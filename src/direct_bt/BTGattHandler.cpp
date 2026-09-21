/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include  <algorithm>

extern "C" {
    #include <unistd.h>
    #include <sys/socket.h>
    #include <poll.h>
}

// #define PERF_PRINT_ON 1
// PERF2_PRINT_ON for read/write single values
// #define PERF2_PRINT_ON 1
// PERF3_PRINT_ON for disconnect
// #define PERF3_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "GattNumbers.hpp"

#include "BTGattHandler.hpp"

#include "BTDevice.hpp"

#include "BTAdapter.hpp"
#include "DBTConst.hpp"

#include "BTGattService.hpp"
#include "BTGattChar.hpp"
#include "BTGattDesc.hpp"

using namespace direct_bt;

BTGattEnv::BTGattEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.gatt") ),
  GATT_READ_COMMAND_REPLY_TIMEOUT( jau::environment::getFractionProperty("direct_bt.gatt.cmd.read.timeout", 550_ms, 550_ms /* min */, 365_d /* max */) ),
  GATT_WRITE_COMMAND_REPLY_TIMEOUT(  jau::environment::getFractionProperty("direct_bt.gatt.cmd.write.timeout", 550_ms, 550_ms /* min */, 365_d /* max */) ),
  GATT_INITIAL_COMMAND_REPLY_TIMEOUT( jau::environment::getFractionProperty("direct_bt.gatt.cmd.init.timeout", 2500_ms, 2000_ms /* min */, 365_d /* max */) ),
  ATTPDU_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.gatt.ringsize", 128, 64 /* min */, 1024 /* max */) ),
  DEBUG_DATA( jau::environment::getBooleanProperty("direct_bt.debug.gatt.data", false) )
{
}

BTDeviceRef BTGattHandler::getDeviceChecked() const {
    BTDeviceRef ref = wbr_device.lock();
    if( nullptr == ref ) {
        throw jau::IllegalStateError("GATTHandler's device already destructed: "+toString(), E_FILE_LINE);
    }
    return ref;
}

bool BTGattHandler::validateConnected() noexcept {
    const bool l2capIsConnected = l2cap.is_open();
    const bool l2capHasIOError = l2cap.hasIOError();

    if( has_ioerror || l2capHasIOError ) {
        jau_DBG_PRINT("ioerr state: GattHandler %s, l2cap %s: %s",
                getStateString(), l2cap.getStateString(), toString());
        has_ioerror = true; // propagate l2capHasIOError -> has_ioerror
        return false;
    }

    if( !is_connected || !l2capIsConnected ) {
        jau_DBG_PRINT("Disconnected state: GattHandler %s, l2cap %s: %s",
                getStateString(), l2cap.getStateString(), toString());
        return false;
    }
    return true;
}

BTGattHandler::gattCharListenerList_t::equal_comparator BTGattHandler::gattCharListenerRefEqComparator =
        [](const GattCharListenerPair& a, const GattCharListenerPair& b) noexcept -> bool { return *a.listener == *b.listener; };

bool BTGattHandler::addCharListener(const BTGattCharListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    return gattCharListenerList.push_back_unique(GattCharListenerPair{l, std::weak_ptr<BTGattChar>{} },
                                                 gattCharListenerRefEqComparator);
}

bool BTGattHandler::addCharListener(const BTGattCharListenerRef& l, const BTGattCharRef& d) {
    if( nullptr == l ) {
        jau_ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    if( nullptr == d ) {
        jau_ERR_PRINT("BTGattChar ref is null");
        return false;
    }
    return gattCharListenerList.push_back_unique(GattCharListenerPair{l, d},
                                                 gattCharListenerRefEqComparator);
}

bool BTGattHandler::removeCharListener(const BTGattCharListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    const size_type count = gattCharListenerList.erase_matching(GattCharListenerPair{l, std::weak_ptr<BTGattChar>{}},
                                                        false /* all_matching */,
                                                        gattCharListenerRefEqComparator);
    return count > 0;
}

bool BTGattHandler::removeCharListener(const BTGattCharListener * l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    auto it = gattCharListenerList.begin(); // lock mutex and copy_store
    for (; !it.is_end(); ++it ) {
        if ( *it->listener == *l ) {
            it.erase();
            it.write_back();
            return true;
        }
    }
    return false;
}

static jau::cow_darray<BTGattHandler::NativeGattCharListenerRef>::equal_comparator _nativeGattCharListenerRefEqComparator =
        [](const BTGattHandler::NativeGattCharListenerRef& a, const BTGattHandler::NativeGattCharListenerRef& b) noexcept -> bool { return *a == *b; };

bool BTGattHandler::addCharListener(const BTGattHandler::NativeGattCharListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("NativeGattCharListener ref is null");
        return false;
    }
    return nativeGattCharListenerList.push_back_unique(l, _nativeGattCharListenerRefEqComparator);
}

bool BTGattHandler::removeCharListener(const BTGattHandler::NativeGattCharListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("NativeGattCharListener ref is null");
        return false;
    }
    const size_type count = nativeGattCharListenerList.erase_matching(l, false /* all_matching */, _nativeGattCharListenerRefEqComparator);
    return count > 0;
}

void BTGattHandler::printCharListener() {
    jau_INFO_PRINT("BTGattHandler: BTGattChar %zu listener", gattCharListenerList.size());
    {
        int i=0;
        auto it = gattCharListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it, ++i ) {
            jau_INFO_PRINT("[%d]: %s", i, it->listener->toString());
        }
    }
    jau_INFO_PRINT("BTGattHandler: NativeGattChar %zu listener", nativeGattCharListenerList.size());
    {
        int i=0;
        auto it = nativeGattCharListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it, ++i ) {
            jau_INFO_PRINT("[%d]: %s", i, (*it)->toString());
        }
    }
}

BTGattHandler::size_type BTGattHandler::removeAllAssociatedCharListener(const BTGattCharRef& associatedCharacteristic) {
    if( nullptr == associatedCharacteristic ) {
        jau_ERR_PRINT("Given GATTCharacteristic ref is null");
        return false;
    }
    return removeAllAssociatedCharListener( associatedCharacteristic.get() );
}

BTGattHandler::size_type BTGattHandler::removeAllAssociatedCharListener(const BTGattChar * associatedCharacteristic) {
    if( nullptr == associatedCharacteristic ) {
        jau_ERR_PRINT("Given GATTCharacteristic ref is null");
        return false;
    }
    size_type count = 0;
    auto it = gattCharListenerList.begin(); // lock mutex and copy_store
    while( !it.is_end() ) {
        if ( it->match(*associatedCharacteristic) ) {
            it.erase();
            ++count;
        } else {
            ++it;
        }
    }
    if( 0 < count ) {
        it.write_back();
    }
    return count;
}

BTGattHandler::size_type BTGattHandler::removeAllCharListener() noexcept {
    size_type count = gattCharListenerList.size();
    gattCharListenerList.clear();
    count += nativeGattCharListenerList.size();
    nativeGattCharListenerList.clear();
    return count;
}

void BTGattHandler::notifyNativeRequestSent(const AttPDUMsg& pduRequest, const BTDeviceRef& clientSource) noexcept {
    BTDeviceRef serverDest = getDeviceUnchecked();
    if( nullptr != serverDest ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->requestSent(pduRequest, serverDest, clientSource);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::requestSent-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::notifyNativeReplyReceived(const AttPDUMsg& pduReply, const BTDeviceRef& clientDest) noexcept {
    BTDeviceRef serverSource = getDeviceUnchecked();
    if( nullptr != serverSource ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->replyReceived(pduReply, serverSource, clientDest);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::replyReceived-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::notifyNativeMTUResponse(const uint16_t clientMTU_,
                                            const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_reply,
                                            const uint16_t serverMTU_, const uint16_t usedMTU_,
                                            const BTDeviceRef& clientRequester) noexcept
{
    BTDeviceRef serverReplier = getDeviceUnchecked();
    if( nullptr != serverReplier ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->mtuResponse(clientMTU_, pduReply, error_reply, serverMTU_, usedMTU_, serverReplier, clientRequester);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::mtuResponse-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::notifyNativeWriteRequest(const uint16_t handle, const jau::TROOctets& data, const NativeGattCharSections_t& sections,
                                             const bool with_response, const BTDeviceRef& clientSource) noexcept
{
    BTDeviceRef serverDest = getDeviceUnchecked();
    if( nullptr != serverDest ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->writeRequest(handle, data, sections, with_response, serverDest, clientSource);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::writeRequest-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::notifyNativeWriteResponse(const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_code, const BTDeviceRef& clientDest) noexcept {
    BTDeviceRef serverSource = getDeviceUnchecked();
    if( nullptr != serverSource ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->writeResponse(pduReply, error_code, serverSource, clientDest);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::writeResponse-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::notifyNativeReadResponse(const uint16_t handle, const uint16_t value_offset,
                                             const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_code, const jau::TROOctets& data_reply,
                                             const BTDeviceRef& clientRequester) noexcept {
    BTDeviceRef serverReplier = getDeviceUnchecked();
    if( nullptr != serverReplier ) {
        size_t i=0;
        jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<BTGattHandler::NativeGattCharListener> &l) {
            try {
                l->readResponse(handle, value_offset, pduReply, error_code, data_reply, serverReplier, clientRequester);
            } catch (std::exception &e) {
                jau_ERR_PRINT("GATTHandler::readResponse-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                        i+1, nativeGattCharListenerList.size(),
                        jau::toHexString((void*)l.get()), e.what());
            }
            i++;
        });
    }
}

void BTGattHandler::setSendIndicationConfirmation(const bool v) noexcept {
    sendIndicationConfirmation = v;
}

bool BTGattHandler::getSendIndicationConfirmation() noexcept {
    return sendIndicationConfirmation;
}

bool BTGattHandler::replyAttPDUReq(std::unique_ptr<const AttPDUMsg> && pdu) noexcept {
    if( !validateConnected() ) { // shall not happen
        jau_DBG_PRINT("GATT-Req: disconnected: req %s from %s",
                pdu->toString(), toString());
        return false;
    }
    switch( pdu->getOpcode() ) {
        case AttPDUMsg::Opcode::EXCHANGE_MTU_REQ: { // 2
            return gattServerHandler->replyExchangeMTUReq( static_cast<const AttExchangeMTU*>( pdu.get() ) );
        }

        case AttPDUMsg::Opcode::FIND_INFORMATION_REQ: { // 4
            return gattServerHandler->replyFindInfoReq( static_cast<const AttFindInfoReq*>( pdu.get() ) );
        }

        case AttPDUMsg::Opcode::FIND_BY_TYPE_VALUE_REQ: { // 6
            return gattServerHandler->replyFindByTypeValueReq( static_cast<const AttFindByTypeValueReq*>( pdu.get() ) );
        }

        case AttPDUMsg::Opcode::READ_BY_TYPE_REQ: { // 8
            return gattServerHandler->replyReadByTypeReq( static_cast<const AttReadByNTypeReq*>( pdu.get() ) );
        }

        case AttPDUMsg::Opcode::READ_REQ: // 10
            [[fallthrough]];
        case AttPDUMsg::Opcode::READ_BLOB_REQ: { // 12
            return gattServerHandler->replyReadReq( pdu.get() );
        }

        case AttPDUMsg::Opcode::READ_BY_GROUP_TYPE_REQ: { // 16
            return gattServerHandler->replyReadByGroupTypeReq( static_cast<const AttReadByNTypeReq*>( pdu.get() ) );
        }

        case AttPDUMsg::Opcode::WRITE_REQ: // 18
            [[fallthrough]];
        case AttPDUMsg::Opcode::WRITE_CMD: // 18 + 64 = 82
            [[fallthrough]];
        case AttPDUMsg::Opcode::PREPARE_WRITE_REQ: // 22
            [[fallthrough]];
        case AttPDUMsg::Opcode::EXECUTE_WRITE_REQ: { // 24
            return gattServerHandler->replyWriteReq( pdu.get() );
        }

        // TODO: Add support for the following requests

        case AttPDUMsg::Opcode::READ_MULTIPLE_REQ: // 14
            [[fallthrough]];
        case AttPDUMsg::Opcode::READ_MULTIPLE_VARIABLE_REQ: // 32
            [[fallthrough]];
        case AttPDUMsg::Opcode::SIGNED_WRITE_CMD: { // 18 + 64 + 128 = 210
            AttErrorRsp rsp(AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST, pdu->getOpcode(), 0);
            jau_WARN_PRINT("GATT Req: Ignored: %s -> %s from %s", pdu->toString(), rsp, toString());
            if( !send(rsp) ) {
                jau_ERR_PRINT2("l2cap send: Error req %s; %s", rsp, toString());
                return false;
            }
            return true;
        }

        default:
            AttErrorRsp rsp(AttErrorRsp::ErrorCode::FORBIDDEN_VALUE, pdu->getOpcode(), 0);
            jau_ERR_PRINT("GATT Req: Unhandled: %s -> %s from %s", pdu->toString(), rsp, toString());
            if( !send(rsp) ) {
                jau_ERR_PRINT2("l2cap send: Error req %s; %s", rsp, toString());
                return false;
            }
            return true;
    }
}

void BTGattHandler::l2capReaderWork(jau::service_runner& sr) noexcept {
    jau::snsize_t len;
    if( !validateConnected() ) {
        jau_DBG_PRINT("GATTHandler::reader: Invalid IO state -> Stop");
        sr.set_shall_stop();
        return;
    }

    len = l2cap.read(rbuffer.get_wptr(), rbuffer.size());
    if( 0 < len ) {
        std::unique_ptr<const AttPDUMsg> attPDU = AttPDUMsg::getSpecialized(rbuffer.get_ptr(), static_cast<jau::nsize_t>(len));
        jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: Got %s", attPDU->toString());

        const AttPDUMsg::Opcode opc = attPDU->getOpcode();
        const AttPDUMsg::OpcodeType opc_type = AttPDUMsg::get_type(opc);

        if( AttPDUMsg::Opcode::MULTIPLE_HANDLE_VALUE_NTF == opc ) { // AttPDUMsg::OpcodeType::NOTIFICATION
            // FIXME TODO ..
            jau_ERR_PRINT("MULTI-NTF not implemented: %s", attPDU->toString());
        } else if( AttPDUMsg::Opcode::HANDLE_VALUE_NTF == opc ) { // AttPDUMsg::OpcodeType::NOTIFICATION
            const AttHandleValueRcv * a = static_cast<const AttHandleValueRcv*>(attPDU.get());
            jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: NTF: %s, listener [native %zu, bt %zu]",
                    a->toString(), nativeGattCharListenerList.size(), gattCharListenerList.size());
            const uint64_t a_timestamp = a->ts_creation;
            const uint16_t a_handle = a->getHandle();
            const jau::TOctetSlice& a_value = a->getValue();
            const jau::TROOctets a_data_view(a_value.get_ptr_nc(0), a_value.size(), a_value.byte_order()); // just a view, still owned by attPDU
            BTDeviceRef device = getDeviceUnchecked();
            if( nullptr != device ) {
                size_t i=0;
                jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<NativeGattCharListener> &l) {
                    try {
                        l->notificationReceived(device, a_handle, a_data_view, a_timestamp);
                    } catch (const std::exception &e) {
                        jau_ERR_PRINT("GATTHandler::notificationReceived-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                                i+1, nativeGattCharListenerList.size(),
                                jau::toHexString((void*)l.get()), e.what());
                    }
                    i++;
                });
            }
            BTGattCharRef characteristic = findCharacterisicsByValueHandle(services, a_handle);
            if( nullptr != characteristic ) {
                size_t i=0;
                jau::for_each_fidelity(gattCharListenerList, [&](GattCharListenerPair &p) {
                    try {
                        if( p.match(*characteristic) ) {
                            p.listener->notificationReceived(characteristic, a_data_view, a_timestamp);
                        }
                    } catch (const std::exception &e) {
                        jau_ERR_PRINT("GATTHandler::notificationReceived-CBs %zu/%zu: BTGattCharListener %s: Caught exception %s",
                                i+1, gattCharListenerList.size(),
                                jau::toHexString((void*)p.listener.get()), e.what());
                    }
                    i++;
                });
            }
        } else if( AttPDUMsg::Opcode::HANDLE_VALUE_IND == opc ) { // AttPDUMsg::OpcodeType::INDICATION
            const AttHandleValueRcv * a = static_cast<const AttHandleValueRcv*>(attPDU.get());
            jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: IND: %s, sendIndicationConfirmation %d, listener [native %zu, bt %zu]",
                    a->toString(), sendIndicationConfirmation.load(), nativeGattCharListenerList.size(), gattCharListenerList.size());
            bool cfmSent = false;
            if( sendIndicationConfirmation ) {
                AttHandleValueCfm cfm;
                if( !send(cfm) ) {
                    jau_ERR_PRINT2("Indication Confirmation: Error req %s; %s", cfm, toString());
                    sr.set_shall_stop();
                    has_ioerror = true;
                    return;
                }
                cfmSent = true;
            }
            const uint64_t a_timestamp = a->ts_creation;
            const uint16_t a_handle = a->getHandle();
            const jau::TOctetSlice& a_value = a->getValue();
            const jau::TROOctets a_data_view(a_value.get_ptr_nc(0), a_value.size(), a_value.byte_order()); // just a view, still owned by attPDU
            BTDeviceRef device = getDeviceUnchecked();
            if( nullptr != device ) {
                size_t i=0;
                jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<NativeGattCharListener> &l) {
                    try {
                        l->indicationReceived(device, a_handle, a_data_view, a_timestamp, cfmSent);
                    } catch (const std::exception &e) {
                        jau_ERR_PRINT("GATTHandler::indicationReceived-CBs %zu/%zu: NativeGattCharListener %s: Caught exception %s",
                                i+1, nativeGattCharListenerList.size(),
                                jau::toHexString((void*)l.get()), e.what());
                    }
                    i++;
                });
            }
            BTGattCharRef characteristic = findCharacterisicsByValueHandle(services, a_handle);
            if( nullptr != characteristic ) {
                size_t i=0;
                jau::for_each_fidelity(gattCharListenerList, [&](GattCharListenerPair &p) {
                    try {
                        if( p.match(*characteristic) ) {
                            p.listener->indicationReceived(characteristic, a_data_view, a_timestamp, cfmSent);
                        }
                    } catch (const std::exception &e) {
                        jau_ERR_PRINT("GATTHandler::indicationReceived-CBs %zu/%zu: BTGattCharListener %s, cfmSent %d: Caught exception %s",
                                i+1, gattCharListenerList.size(),
                                jau::toHexString((void*)p.listener.get()), cfmSent, e.what());
                    }
                    i++;
                });
            }
        } else if( AttPDUMsg::OpcodeType::RESPONSE == opc_type ) {
            jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: Ring: %s", attPDU->toString());
            if( !attPDURing.putBlocking( std::move(attPDU), 0_s ) ) {
                jau_ERR_PRINT2("attPDURing put: %s", attPDURing);
                sr.set_shall_stop();
                return;
            }
        } else if( AttPDUMsg::OpcodeType::REQUEST == opc_type ) {
            if( !replyAttPDUReq( std::move( attPDU ) ) ) {
                jau_ERR_PRINT2("ATT Reply: %s", toString());
                sr.set_shall_stop();
                has_ioerror = true;
                return;
            }
        } else {
            jau_ERR_PRINT("Unhandled: %s", attPDU->toString());
        }
    } else if( len == L2CAPClient::number(L2CAPClient::RWExitCode::INTERRUPTED) ) {
        jau_WORDY_PRINT("GATTHandler::reader: l2cap read: IRQed res %zd (%s); %s",
                len, L2CAPClient::getRWExitCodeString(len), getStateString());
        if( !sr.shall_stop() ) {
            // need to stop service_runner if interrupted externally
            sr.set_shall_stop();
        }
    } else if( len != L2CAPClient::number(L2CAPClient::RWExitCode::POLL_TIMEOUT) &&
               len != L2CAPClient::number(L2CAPClient::RWExitCode::READ_TIMEOUT) ) { // expected TIMEOUT if idle
        if( 0 > len ) { // actual error case
            jau_IRQ_PRINT("GATTHandler::reader: l2cap read: Error res %zd (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len), getStateString());
            sr.set_shall_stop();
            has_ioerror = true;
        } else { // zero size
            jau_WORDY_PRINT("GATTHandler::reader: l2cap read: Zero res %zd (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len), getStateString());
        }
    }
}

void BTGattHandler::l2capReaderEndLocked(jau::service_runner& sr) noexcept {
    (void)sr;
    jau_WORDY_PRINT("GATTHandler::reader: EndLocked. Ring has %zu entries flushed: %s", attPDURing.size(), toString());
    attPDURing.clear();
#if 0
    // Disabled: BT host is sending out disconnect -> simplify tear down
    if( has_ioerror ) {
        // Don't rely on receiving a disconnect
        BTDeviceRef device = getDeviceUnchecked();
        if( nullptr != device ) {
            std::thread dc(&BTDevice::disconnect, device.get(), HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF);
            dc.detach();
        }
    }
#endif
}

bool BTGattHandler::l2capReaderInterrupted(int dummy) /* const */ noexcept {
    (void)dummy;
    if( l2cap_reader_service.shall_stop() || !is_connected ) {
        return true;
    }
    BTDeviceRef device = getDeviceUnchecked();
    if( nullptr == device ) {
        return true;
    }
    return !device->getConnected();
}

BTGattHandler::BTGattHandler(const BTDeviceRef &device, L2CAPClient& l2cap_att, const int32_t supervision_timeout_) noexcept
: supervision_timeout(supervision_timeout_),
  env(BTGattEnv::get()),
  read_cmd_reply_timeout(jau::max(env.GATT_READ_COMMAND_REPLY_TIMEOUT, 1_ms*(supervision_timeout+50_i64))),
  write_cmd_reply_timeout(jau::max(env.GATT_WRITE_COMMAND_REPLY_TIMEOUT, 1_ms*(supervision_timeout+50_i64))),
  wbr_device(device),
  role(device->getLocalGATTRole()),
  l2cap(l2cap_att),
  deviceString(device->getAddressAndType().address.toString()),
  rbuffer(number(Defaults::MAX_ATT_MTU), jau::lb_endian_t::little),
  is_connected(l2cap.is_open()), has_ioerror(false),
  l2cap_reader_service("GATTHandler::reader_"+deviceString, THREAD_SHUTDOWN_TIMEOUT_MS,
                       jau::bind_member(this, &BTGattHandler::l2capReaderWork),
                       jau::service_runner::Callback() /* init */,
                       jau::bind_member(this, &BTGattHandler::l2capReaderEndLocked)),
  attPDURing(env.ATTPDU_RING_CAPACITY),
  serverMTU(number(Defaults::MIN_ATT_MTU)), usedMTU(number(Defaults::MIN_ATT_MTU)), clientMTUExchanged(false),
  gattServerData( device->getAdapter().getGATTServerData() ),
  gattServerHandler( selectGattServerHandler(*this, gattServerData) )
{
    if( !validateConnected() ) {
        jau_ERR_PRINT("L2CAP could not connect");
        is_connected = false;
        return;
    }

    /**
     * We utilize DBTManager's mgmthandler_sigaction SIGALRM handler,
     * as we only can install one handler.
     */
    // l2cap.set_interrupted_query( jau::bind_member(&l2cap_reader_service, &jau::service_runner::shall_stop2) );
    l2cap.set_interrupted_query( jau::bind_member(this, &BTGattHandler::l2capReaderInterrupted) );
    l2cap_reader_service.start();

    jau_DBG_PRINT("GATTHandler::ctor: Started: GattHandler[%s], l2cap[%s]: %s",
                getStateString(), l2cap.getStateString(), toString());

    if( GATTRole::Client == getRole() ) {
        // MTU to be negotiated via initClientGatt() from this GATT client later
        serverMTU = number(Defaults::MAX_ATT_MTU);
        usedMTU = number(Defaults::MIN_ATT_MTU);
    } else {
        // MTU to be negotiated via client request on this GATT server
        if( nullptr != gattServerData ) {
            serverMTU = std::max( std::min( gattServerData->getMaxAttMTU(), number(Defaults::MAX_ATT_MTU) ), number(Defaults::MIN_ATT_MTU) );
        } else {
            serverMTU = number(Defaults::MAX_ATT_MTU);
        }
        usedMTU = number(Defaults::MIN_ATT_MTU);

        if( nullptr != gattServerData ) {
            size_t i=0;
            jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                try {
                    l->connected(device, usedMTU);
                } catch (std::exception &e) {
                    jau_ERR_PRINT("GATTHandler::connected: %zu/%zu: %s: Caught exception %s",
                            i+1, gattServerData->listener().size(),
                            toString(), e.what());
                }
                i++;
            });
        }
    }
}

BTGattHandler::~BTGattHandler() noexcept {
    jau_DBG_PRINT("GATTHandler::dtor: Start: %s", toString());
    disconnect(false /* disconnect_device */, false /* ioerr_cause */);
    gattCharListenerList.clear();
    nativeGattCharListenerList.clear();
    services.clear();
    genericAccess = nullptr;
    jau_DBG_PRINT("GATTHandler::dtor: End: %s", toString());
}

std::string BTGattHandler::getStateString() const noexcept {
    return L2CAPComm::getStateString(is_connected, has_ioerror);
}

bool BTGattHandler::disconnect(const bool disconnect_device, const bool ioerr_cause) noexcept {
    BTDeviceRef device = getDeviceUnchecked();
    if( nullptr == device ) {
        // If the device has been pulled already, so its l2cap instance.
        jau_ERR_PRINT("BTDevice null");
        return false;
    }
    jau_PERF3_TS_T0();

    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !is_connected.compare_exchange_strong(expConn, false) ) {
        // not connected
        const bool l2cap_service_stopped = l2cap_reader_service.join(); // [data] race: wait until disconnecting thread has stopped service
        l2cap.close(); // owned by BTDevice.
        jau_DBG_PRINT("GATTHandler::disconnect: Not connected: disconnect_device %d, ioerr %d: GattHandler[%s], l2cap[%s], stopped %d: %s",
                  disconnect_device, ioerr_cause, getStateString(), l2cap.getStateString(),
                  l2cap_service_stopped, toString());
        gattCharListenerList.clear();
        nativeGattCharListenerList.clear();
        return false;
    }

    jau_PERF3_TS_TD("GATTHandler::disconnect.1");
    const bool l2cap_service_stop_res = l2cap_reader_service.stop();
    l2cap.close(); // owned by BTDevice.
    jau_PERF3_TS_TD("GATTHandler::disconnect.X");

    gattServerHandler->close();

    // Lock to avoid other threads using instance while disconnecting
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    jau_DBG_PRINT("GATTHandler::disconnect: Start: disconnect_device %d, ioerr %d: GattHandler[%s], l2cap[%s]: %s",
              disconnect_device, ioerr_cause, getStateString(), l2cap.getStateString(), toString());
    gattCharListenerList.clear();
    nativeGattCharListenerList.clear();

    clientMTUExchanged = false;

    jau_DBG_PRINT("GATTHandler::disconnect: End: stopped %d, disconnect_device %d, %s",
            l2cap_service_stop_res, disconnect_device, toString());

    if( disconnect_device ) {
        // Cleanup device resources, proper connection state
        // Intentionally giving the POWER_OFF reason for the device in case of ioerr_cause!
        const HCIStatusCode reason = ioerr_cause ?
                               HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF :
                               HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION;
        device->disconnect(reason);
    }
    return true;
}

bool BTGattHandler::send(const AttPDUMsg & msg) noexcept {
    if( !validateConnected() ) {
        if( !l2capReaderInterrupted() ) {
            jau_ERR_PRINT("Invalid IO State: req %s to %s", msg, toString());
        }
        return false;
    }
    // [1 .. ATT_MTU-1] BT Core Spec v5.2: Vol 3, Part F 3.2.9 Long attribute values
    if( msg.pdu.size() > usedMTU ) {
        jau_ERR_PRINT("Msg PDU size %zu >= used MTU %u, req %s to $s",
                msg.pdu.size(), usedMTU.load(), msg, toString());
        return false;
    }

    // Thread safe l2cap.write(..) operation..
    const jau::snsize_t len = l2cap.write(msg.pdu.get_ptr(), msg.pdu.size());
    if( 0 > len ) {
        if( len == L2CAPClient::number(L2CAPClient::RWExitCode::INTERRUPTED) ) { // expected exits
            jau_WORDY_PRINT("GATTHandler::reader: l2cap read: IRQed res %zd (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len), getStateString());
        } else {
            jau_ERR_PRINT("l2cap write: Error res %zd (%s); %s; %s -> disconnect: %s",
                    len, L2CAPClient::getRWExitCodeString(len), getStateString(),
                    msg, toString());
            has_ioerror = true;
            disconnect(true /* disconnect_device */, true /* ioerr_cause */); // state -> Disconnected
        }
        return false;
    }
    if( static_cast<size_t>(len) != msg.pdu.size() ) {
        jau_ERR_PRINT("l2cap write: Error: Message size has %zd != exp %zu: %s -> disconnect: %s",
                len, msg.pdu.size(), msg, toString());
        has_ioerror = true;
        disconnect(true /* disconnect_device */, true /* ioerr_cause */); // state -> Disconnected
        return false;
    }
    return true;
}

std::unique_ptr<const AttPDUMsg> BTGattHandler::sendWithReply(const AttPDUMsg & msg, const jau::fraction_i64& timeout) noexcept {
    if( !send( msg ) ) {
        return nullptr;
    }

    // Ringbuffer read is thread safe
    std::unique_ptr<const AttPDUMsg> res;
    if( !attPDURing.getBlocking(res, timeout) || nullptr == res ) {
        errno = ETIMEDOUT;
        jau_ERR_PRINT("GATTHandler::sendWithReply: nullptr result (timeout %" PRIi64 " ms): req %s to %s", timeout.to_ms(), msg, toString());
        has_ioerror = true;
        disconnect(true /* disconnect_device */, true /* ioerr_cause */);
        return nullptr;
    }
    return res;
}

bool BTGattHandler::sendError(AttErrorRsp::ErrorCode ec, const AttPDUMsg *cause_req, uint16_t cause_handle, LogLevel llvl, std::string_view msg) noexcept {
    try {
        AttErrorRsp err(ec, cause_req->getOpcode(), cause_handle);
        if ( LogLevel::warning == llvl ) {
            jau_WARN_PRINT("%s: %s -> %s from %s", msg, cause_req->toString(), err, toString());
        } else if ( LogLevel::error == llvl ) {
            jau_ERR_PRINT("%s: %s -> %s from %s", msg, cause_req->toString(), err, toString());
        } else if ( LogLevel::cond == llvl ) {
            jau_COND_PRINT(env.DEBUG_DATA, "%s: %s -> %s from %s", msg, cause_req->toString(), err, toString());
        }
        return send(err);
    } catch (...) {
        std::exception_ptr eptr = std::current_exception();
        jau::fput_exception(stderr, eptr, E_FILE_LINE);
        jau_ERR_PRINT3("Exception caught while sending error: %s", toString());
        return false;
    }
}

uint16_t BTGattHandler::clientMTUExchange(const jau::fraction_i64& timeout) noexcept {
    if( GATTRole::Client != getRole() ) {
        jau_ERR_PRINT("GATT MTU exchange only allowed in client mode");
        return usedMTU;
    }
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.3.1 Exchange MTU (Server configuration)
     */
    const AttExchangeMTU req(AttPDUMsg::ReqRespType::REQUEST, number(Defaults::MAX_ATT_MTU));
    const std::lock_guard<std::recursive_mutex> lock(mtx_command);
    jau_PERF_TS_T0();

    uint16_t mtu = 0;
    jau_DBG_PRINT("GATT MTU-REQ send: %s to %s", req, toString());

    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, timeout);

    if( nullptr == pdu ) {
        jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
    } else if( pdu->getOpcode() == AttPDUMsg::Opcode::EXCHANGE_MTU_RSP ) {
        const AttExchangeMTU * p = static_cast<const AttExchangeMTU*>(pdu.get());
        mtu = p->getMTUSize();
        jau_DBG_PRINT("GATT MTU-RSP recv: %u, %s from %s", mtu, pdu->toString(), toString());
    } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
        /**
         * If the ATT_ERROR_RSP PDU is sent by the server
         * with the error code set to 'Request Not Supported',
         * the Attribute Opcode is not supported and the default MTU shall be used.
         */
        const AttErrorRsp * p = static_cast<const AttErrorRsp *>(pdu.get());
        if( AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST == p->getErrorCode() ) {
            mtu = number(Defaults::MIN_ATT_MTU); // OK by spec: Use default MTU
            jau_DBG_PRINT("GATT MTU handled error -> ATT_MTU %u, %s from %s", mtu, pdu->toString(), toString());
        } else {
            jau_WORDY_PRINT("GATT MTU unexpected error %s; req %s from %s", pdu->toString(), req, toString());
        }
    } else {
        jau_ERR_PRINT("GATT MTU unexpected reply %s; req %s from %s", pdu->toString(), req, toString());
    }
    jau_PERF_TS_TD("GATT exchangeMTU");

    return mtu;
}

DBGattCharRef BTGattHandler::findServerGattCharByValueHandle(const uint16_t char_value_handle) noexcept {
    if( nullptr != gattServerData ) {
        return gattServerData->findGattCharByValueHandle(char_value_handle);
    } else {
        return nullptr;
    }
}

bool BTGattHandler::sendNotification(const uint16_t char_value_handle, const jau::TROOctets & value) {
    if( GATTRole::Server != role ) {
        jau_ERR_PRINT("GATTRole not server");
        return false;
    }
    if( DBGattServer::Mode::DB == gattServerHandler->getMode() &&
        nullptr == findServerGattCharByValueHandle(char_value_handle) )
    {
        jau_ERR_PRINT("Invalid char handle %s", jau::toHexString(char_value_handle));
        return false;
    }
    if( 0 == value.size() ) {
        jau_COND_PRINT(env.DEBUG_DATA, "GATT SEND NTF: Zero size, skipped sending to %s", toString());
        return true;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    AttHandleValueRcv data(true /* isNotify */, char_value_handle, value, usedMTU);
    jau_COND_PRINT(env.DEBUG_DATA, "GATT SEND NTF: %s to %s", data, toString());
    return send(data);
}

bool BTGattHandler::sendIndication(const uint16_t char_value_handle, const jau::TROOctets & value) {
    if( GATTRole::Server != role ) {
        jau_ERR_PRINT("GATTRole not server");
        return false;
    }
    if( DBGattServer::Mode::DB == gattServerHandler->getMode() &&
        nullptr == findServerGattCharByValueHandle(char_value_handle) )
    {
        jau_ERR_PRINT("Invalid char handle %s", jau::toHexString(char_value_handle));
        return false;
    }
    if( 0 == value.size() ) {
        jau_COND_PRINT(env.DEBUG_DATA, "GATT SEND IND: Zero size, skipped sending to %s", toString());
        return true;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    AttHandleValueRcv req(false /* isNotify */, char_value_handle, value, usedMTU);
    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, write_cmd_reply_timeout);
    if( nullptr == pdu ) {
        jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
        return false;
    }
    if( pdu->getOpcode() == AttPDUMsg::Opcode::HANDLE_VALUE_CFM ) {
        jau_COND_PRINT(env.DEBUG_DATA, "GATT SENT IND: %s -> %s to/from %s",
                req, pdu->toString(), toString());
        return true;
    } else {
        jau_WARN_PRINT("GATT SENT IND: Failed, no CFM reply: %s -> %s to/from %s",
                req, pdu->toString(), toString());
        return false;
    }
}

BTGattCharRef BTGattHandler::findCharacterisicsByValueHandle(const jau::darray<BTGattServiceRef> &services_, const uint16_t charValueHandle) noexcept {
    for(const auto & service : services_) {
        BTGattCharRef decl = findCharacterisicsByValueHandle(service, charValueHandle);
        if( nullptr != decl ) {
            return decl;
        }
    }
    return nullptr;
}

BTGattCharRef BTGattHandler::findCharacterisicsByValueHandle(const BTGattServiceRef& service, const uint16_t charValueHandle) noexcept {
    for(auto decl : service->characteristicList) {
        if( charValueHandle == decl->value_handle ) {
            return decl;
        }
    }
    return nullptr;
}

bool BTGattHandler::initClientGatt(const std::shared_ptr<BTGattHandler>& shared_this, bool& already_init) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command);
    already_init = clientMTUExchanged && services.size() > 0 && nullptr != genericAccess;
    if( already_init ) {
        return true;
    }
    if( !isConnected() ) {
        jau_DBG_PRINT("GATTHandler::initClientGatt: Not connected: %s", toString());
        return false;
    }
    if( !clientMTUExchanged) {
        // First point of failure if remote device exposes no GATT functionality. Allow a longer timeout!
        const jau::fraction_i64 initial_command_reply_timeout = jau::min(10_s, jau::max(env.GATT_INITIAL_COMMAND_REPLY_TIMEOUT, 1_ms*(2_i64*supervision_timeout)));
        jau_DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: MTU Exchange Start: %s", toString());
        uint16_t mtu = clientMTUExchange(initial_command_reply_timeout);
        if( 0 == mtu ) {
            jau_ERR_PRINT2("Local GATT Client: Zero serverMTU -> disconnect: %s", toString());
            disconnect(true /* disconnect_device */, false /* ioerr_cause */);
            return false;
        }
        serverMTU = mtu;
        usedMTU = std::min(number(Defaults::MAX_ATT_MTU), serverMTU.load());
        clientMTUExchanged = true;
        jau_DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: MTU Exchanged: server %u -> used %u, %s", serverMTU.load(), usedMTU.load(), toString());
    }

    if( services.size() > 0 && nullptr != genericAccess ) {
        // already initialized
        return true;
    }
    services.clear();

    // Service discovery may consume 500ms - 2000ms, depending on bandwidth
    jau_DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: Service Discovery Start: %s", toString());
    if( !discoverCompletePrimaryServices(shared_this) ) {
        jau_ERR_PRINT2("Failed service discovery");
        services.clear();
        disconnect(true /* disconnect_device */, true /* ioerr_cause */);
        return false;
    }
    if( services.size() == 0 ) { // nothing discovered
        jau_ERR_PRINT2("No services discovered");
        services.clear();
        disconnect(true /* disconnect_device */, false /* ioerr_cause */);
        return false;
    }
    genericAccess = getGenericAccess(services);
    if( nullptr == genericAccess ) {
        jau_ERR_PRINT2("No GenericAccess discovered");
        services.clear();
        disconnect(true /* disconnect_device */, false /* ioerr_cause */);
        return false;
    }
    jau_DBG_PRINT("GATTHandler::initClientGatt: End: %zu services discovered: %s, %s",
            services.size(), genericAccess->toString(), toString());
    return true;
}

bool BTGattHandler::discoverCompletePrimaryServices(const std::shared_ptr<BTGattHandler>& shared_this) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    if( !discoverPrimaryServices(shared_this, services) ) {
        return false;
    }
    for(auto primSrv : services) {
        if( !discoverCharacteristics(primSrv) ) {
            return false;
        }
        if( primSrv->characteristicList.size() > 0 ) {
            if( !discoverDescriptors(primSrv) ) {
                return false;
            }
        }
    }
    return true;
}

bool BTGattHandler::discoverPrimaryServices(const std::shared_ptr<BTGattHandler>& shared_this, jau::darray<BTGattServiceRef> & result) {
    {
        // validate shared_this first!
        BTGattHandler *given_this = shared_this.get();
        if( given_this != this ) {
            jau_ABORT("Given shared GATTHandler reference %s not matching this %s, %s",
                    jau::toHexString(given_this), jau::toHexString(this), toString());
        }
    }
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
     *
     * This sub-procedure is complete when the ATT_ERROR_RSP PDU is received
     * and the error code is set to Attribute Not Found or when the End Group Handle
     * in the Read by Type Group Response is 0xFFFF.
     */
    const jau::uuid16_t groupType = jau::uuid16_t(GattAttributeType::PRIMARY_SERVICE);
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    jau_PERF_TS_T0();

    bool done=false;
    uint16_t startHandle=0x0001;
    result.clear();
    while(!done) {
        const AttReadByNTypeReq req(true /* group */, startHandle, 0xffff, groupType);
        jau_COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discover send: %s to %s", req, toString());

        std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout);
        if( nullptr == pdu ) {
            jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
            return false;
        }
        jau_COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discover recv: %s on %s", pdu->toString(), toString());

        if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_BY_GROUP_TYPE_RSP ) {
            const AttReadByGroupTypeRsp * p = static_cast<const AttReadByGroupTypeRsp*>(pdu.get());
            const size_type esz = p->getElementSize();
            const size_type count = p->getElementCount();

            for(size_type i=0; i<count; ++i) {
                const size_type ePDUOffset = p->getElementPDUOffset(i);
                try {
                    result.push_back( std::make_shared<BTGattService>( shared_this, true,
                            p->pdu.get_uint16(ePDUOffset), // start-handle
                            p->pdu.get_uint16(ePDUOffset + 2), // end-handle
                            p->pdu.get_uuid( ePDUOffset + 2 + 2, jau::uuid_t::toTypeSize(esz-2-2) ) // uuid
                        ) );
                } catch (const std::bad_alloc &e) {
                    jau_ABORT("Error: bad_alloc: BTGattServiceRef allocation failed");
                    return false; // unreachable
                }
                jau_COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discovered[%zu/%zu]: %s on %s", i,
                        count, result.at(result.size()-1)->toString(), toString());
            }
            startHandle = p->getElementEndHandle(count-1);
            if( startHandle < 0xffff ) {
                startHandle++;
            } else {
                done = true; // OK by spec: End of communication
            }
        } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
            done = true; // OK by spec: End of communication
        } else {
            jau_ERR_PRINT("GATT discoverPrimary unexpected reply %s, req %s from %s",
                    pdu->toString(), req, toString());
            done = true;
        }
    }
    jau_PERF_TS_TD("GATT discoverPrimaryServices");
    return true;
}

bool BTGattHandler::discoverCharacteristics(BTGattServiceRef & service) {
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     * </p>
     */
    const jau::uuid16_t characteristicTypeReq = jau::uuid16_t(GattAttributeType::CHARACTERISTIC);
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    jau_COND_PRINT(env.DEBUG_DATA, "GATT discoverCharacteristics Service: %s on %s", service->toString(), toString());

    jau_PERF_TS_T0();

    bool done=false;
    uint16_t handle=service->handle;
    service->characteristicList.clear();
    while(!done) {
        const AttReadByNTypeReq req(false /* group */, handle, service->end_handle, characteristicTypeReq);
        jau_COND_PRINT(env.DEBUG_DATA, "GATT C discover send: %s to %s", req, toString());

        std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout);
        if( nullptr == pdu ) {
            jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
            return false;
        }
        jau_COND_PRINT(env.DEBUG_DATA, "GATT C discover recv: %s from %s", pdu->toString(), toString());

        if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_BY_TYPE_RSP ) {
            const AttReadByTypeRsp * p = static_cast<const AttReadByTypeRsp*>(pdu.get());
            const size_type esz = p->getElementSize();
            const size_type e_count = p->getElementCount();

            for(size_type e_iter=0; e_iter<e_count; ++e_iter) {
                // handle: handle for the Characteristics declaration
                // value: Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID
                const size_type  ePDUOffset = p->getElementPDUOffset(e_iter);
                try {
                    service->characteristicList.push_back( std::make_shared<BTGattChar>(
                        service,
                        p->getElementHandle(e_iter), // Characteristic Handle
                        static_cast<BTGattChar::PropertyBitVal>(p->pdu.get_uint8(ePDUOffset  + 2)), // Characteristics Property
                        p->pdu.get_uint16(ePDUOffset + 2 + 1), // Characteristics Value Handle
                        p->pdu.get_uuid(ePDUOffset   + 2 + 1 + 2, jau::uuid_t::toTypeSize(esz-2-1-2) ) ) ); // Characteristics Value Type UUID
                } catch (const std::bad_alloc &e) {
                    jau_ABORT("Error: bad_alloc: BTGattCharRef allocation failed");
                    return false; // unreachable
                }
                jau_COND_PRINT(env.DEBUG_DATA, "GATT C discovered[%zu/%zu]: char%s on %s", e_iter, e_count,
                        service->characteristicList.at(service->characteristicList.size()-1)->toString(), toString());
            }
            handle = p->getElementHandle(e_count-1); // Last Characteristic Handle
            if( handle < service->end_handle ) {
                handle++;
            } else {
                done = true; // OK by spec: End of communication
            }
        } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
            done = true; // OK by spec: End of communication
        } else {
            jau_ERR_PRINT("GATT discoverCharacteristics unexpected reply %s, req %s within service%s from %s",
                    pdu->toString(), req, service->toString(), toString());
            done = true;
        }
    }

    jau_PERF_TS_TD("GATT discoverCharacteristics");
    return true;
}

bool BTGattHandler::discoverDescriptors(BTGattServiceRef & service) {
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     */
    jau_COND_PRINT(env.DEBUG_DATA, "GATT discoverDescriptors Service: %s on %s", service->toString(), toString());
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    jau_PERF_TS_T0();

    const size_type charCount = service->characteristicList.size();
    for(size_type charIter=0; charIter < charCount; ++charIter ) {
        BTGattCharRef charDecl = service->characteristicList[charIter];
        charDecl->clearDescriptors();
        jau_COND_PRINT(env.DEBUG_DATA, "GATT discoverDescriptors Characteristic[%zu/%zu]: %s on %s", charIter, charCount, charDecl->toString(), toString());

        uint16_t cd_handle_iter = charDecl->value_handle + 1; // Start @ Characteristic Value Handle + 1
        uint16_t cd_handle_end;
        if( charIter+1 < charCount ) {
            cd_handle_end = service->characteristicList.at(charIter+1)->handle - 1; // // Next Characteristic Handle (excluding)
        } else {
            cd_handle_end = service->end_handle; // End of service handle (including)
        }

        bool done=false;

        while( !done && cd_handle_iter <= cd_handle_end ) {
            const AttFindInfoReq req(cd_handle_iter, cd_handle_end);
            jau_COND_PRINT(env.DEBUG_DATA, "GATT CD discover send: %s", req);

            std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout);
            if( nullptr == pdu ) {
                jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
                return false;
            }
            jau_COND_PRINT(env.DEBUG_DATA, "GATT CD discover recv: %s from ", pdu->toString(), toString());

            if( pdu->getOpcode() == AttPDUMsg::Opcode::FIND_INFORMATION_RSP ) {
                const AttFindInfoRsp * p = static_cast<const AttFindInfoRsp*>(pdu.get());
                const size_type e_count = p->getElementCount();

                for(size_type e_iter=0; e_iter<e_count; ++e_iter) {
                    // handle: handle of Characteristic Descriptor.
                    // value: Characteristic Descriptor UUID.
                    const uint16_t cd_handle = p->getElementHandle(e_iter);
                    std::unique_ptr<const jau::uuid_t> cd_uuid = p->getElementValue(e_iter);

                    std::shared_ptr<BTGattDesc> cd( std::make_shared<BTGattDesc>(charDecl, std::move(cd_uuid), cd_handle) );
                    if( cd_handle <= charDecl->value_handle || cd_handle > cd_handle_end ) { // should never happen!
                        jau_ERR_PRINT("GATT discoverDescriptors CD handle %s not in range ]%s..%s]: descr%s within char%s on %s",
                                jau::toHexString(cd_handle),
                                jau::toHexString(charDecl->value_handle), jau::toHexString(cd_handle_end),
                                cd->toString(), charDecl->toString(), toString());
                        done = true;
                        break;

                    }
                    if( !readDescriptorValue(*cd, 0) ) {
                        jau_WORDY_PRINT("GATT discoverDescriptors readDescriptorValue failed: req %s, descr%s within char%s on %s",
                                   req, cd->toString(), charDecl->toString(), toString());
                        done = true;
                        break;
                    }
                    if( cd->isClientCharConfig() ) {
                        charDecl->clientCharConfigIndex = (BTGattChar::ssize_type) charDecl->descriptorList.size();
                    } else if( cd->isUserDescription() ) {
                        charDecl->userDescriptionIndex = (BTGattChar::ssize_type) charDecl->descriptorList.size();
                    }
                    charDecl->descriptorList.push_back(cd);
                    jau_COND_PRINT(env.DEBUG_DATA, "GATT CD discovered[%zu/%zu]: %s", e_iter, e_count, cd->toString());
                }
                cd_handle_iter = p->getElementHandle(e_count-1); // Last Descriptor Handle
                if( cd_handle_iter < cd_handle_end ) {
                    cd_handle_iter++;
                } else {
                    done = true; // OK by spec: End of communication
                }
            } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
                done = true; // OK by spec: End of communication
            } else {
                jau_ERR_PRINT("GATT discoverDescriptors unexpected reply %s; req %s within char%s from %s",
                        pdu->toString(), req, charDecl->toString(), toString());
                done = true;
            }
        }
    }
    jau_PERF_TS_TD("GATT discoverDescriptors");
    return true;
}

bool BTGattHandler::readDescriptorValue(BTGattDesc & desc, ssize_type expectedLength) {
    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::readDescriptorValue expLen %zu, desc %s", (size_t)expectedLength, desc);
    const bool res = readValue(desc.handle, desc.value, expectedLength);
    if( !res ) {
        jau_WORDY_PRINT("GATT readDescriptorValue error on desc%s within char%s from %s",
                   desc, desc.getGattCharChecked()->toString(), toString());
    }
    return res;
}

bool BTGattHandler::readCharacteristicValue(const BTGattChar & decl, jau::POctets & resValue, ssize_type expectedLength) {
    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::readCharacteristicValue expLen %zu, decl %s", (size_t)expectedLength, decl);
    const bool res = readValue(decl.value_handle, resValue, expectedLength);
    if( !res ) {
        jau_WORDY_PRINT("GATT readCharacteristicValue error on char%s from %s", decl, toString());
    }
    return res;
}

bool BTGattHandler::readValue(const uint16_t handle, jau::POctets & res, ssize_type expectedLength) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value */
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    jau_PERF2_TS_T0();

    bool done=false;
    size_type offset=0;

    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::readValue expLen %zu, handle %s from %s", (size_t)expectedLength, jau::toHexString(handle), toString());

    while(!done) {
        if( 0 < expectedLength && (size_type)expectedLength <= offset ) { // NOLINT(modernize-use-integer-sign-comparison): wrong warning
            break; // done
        } else if( 0 == expectedLength && 0 < offset ) {
            break; // done w/ only one request
        } // else 0 > expectedLength: implicit

        std::unique_ptr<const AttPDUMsg> pdu = nullptr;

        const AttReadReq req0(handle);
        const AttReadBlobReq req1(handle, offset);
        const AttPDUMsg & req = ( 0 == offset ) ? static_cast<const AttPDUMsg &>(req0) : static_cast<const AttPDUMsg &>(req1);
        jau_COND_PRINT(env.DEBUG_DATA, "GATT RV send: %s", req);
        pdu = sendWithReply(req, read_cmd_reply_timeout);
        if( nullptr == pdu ) {
            jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
            return false;
        }

        jau_COND_PRINT(env.DEBUG_DATA, "GATT RV recv: %s from %s", pdu->toString(), toString());
        if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_RSP ) {
            const AttReadNRsp * p = static_cast<const AttReadNRsp*>(pdu.get());
            const jau::TOctetSlice & v = p->getValue();
            res += v;
            offset += v.size();
            if( p->getPDUValueSize() < p->getMaxPDUValueSize(usedMTU) ) {
                done = true; // No full ATT_MTU PDU used - end of communication
            }
        } else if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_BLOB_RSP ) {
            const AttReadNRsp * p = static_cast<const AttReadNRsp*>(pdu.get());
            const jau::TOctetSlice & v = p->getValue();
            if( 0 == v.size() ) {
                done = true; // OK by spec: No more data - end of communication
            } else {
                res += v;
                offset += v.size();
                if( p->getPDUValueSize() < p->getMaxPDUValueSize(usedMTU) ) {
                    done = true; // No full ATT_MTU PDU used - end of communication
                }
            }
        } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
             *
             * If the Characteristic Value is not longer than (ATT_MTU – 1)
             * an ATT_ERROR_RSP PDU with the error
             * code set to Attribute Not Long shall be received on the first
             * ATT_READ_BLOB_REQ PDU.
             */
            const AttErrorRsp * p = static_cast<const AttErrorRsp *>(pdu.get());
            if( AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_LONG == p->getErrorCode() ) {
                done = true; // OK by spec: No more data - end of communication
            } else {
                jau_WORDY_PRINT("GATT readValue unexpected error %s; req %s from %s", pdu->toString(), req, toString());
                done = true;
            }
        } else {
            jau_ERR_PRINT("GATT readValue unexpected reply %s; req %s from %s", pdu->toString(), req, toString());
            done = true;
        }
    }
    jau_PERF2_TS_TD("GATT readValue");

    return offset > 0;
}

bool BTGattHandler::writeDescriptorValue(const BTGattDesc & cd) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indication */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptor */
    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::writeDesccriptorValue desc %s", cd);
    const bool res = writeValue(cd.handle, cd.value, true);
    if( !res ) {
        jau_WORDY_PRINT("GATT writeDescriptorValue error on desc%s within char%s from %s",
                   cd, cd.getGattCharChecked()->toString(), toString());
    }
    return res;
}

bool BTGattHandler::writeCharacteristicValue(const BTGattChar & c, const jau::TROOctets & value) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::writeCharacteristicValue desc %s, value %s", c, value);
    const bool res = writeValue(c.value_handle, value, true);
    if( !res ) {
        jau_WORDY_PRINT("GATT writeCharacteristicValue error on char%s from %s", c, toString());
    }
    return res;
}

bool BTGattHandler::writeCharacteristicValueNoResp(const BTGattChar & c, const jau::TROOctets & value) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response */
    jau_COND_PRINT(env.DEBUG_DATA, "GATT writeCharacteristicValueNoResp decl %s, value %s", c, value);
    return writeValue(c.value_handle, value, false);
}

bool BTGattHandler::writeValue(const uint16_t handle, const jau::TROOctets & value, const bool withResponse) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indication */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptor */

    if( value.size() <= 0 ) {
        jau_WARN_PRINT("GATT writeValue size <= 0, no-op: %s", value);
        return false;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    // FIXME TODO: Long Value if value.size() > ( ATT_MTU - 3 )
    jau_PERF2_TS_T0();

    if( !withResponse ) {
        AttWriteCmd req(handle, value);
        jau_COND_PRINT(env.DEBUG_DATA, "GATT WV send(resp %d): %s to %s", withResponse, req, toString());

        const bool res = send( req );
        jau_PERF2_TS_TD("GATT writeValue (no-resp)");
        if( !res ) {
            jau_ERR_PRINT2("Send failed; req %s from %s", req, toString());
            return false;
        } else {
            return true;
        }
    }

    AttWriteReq req(handle, value);
    jau_COND_PRINT(env.DEBUG_DATA, "GATT WV send(resp %d): %s to %s", withResponse, req, toString());

    bool res = false;
    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, write_cmd_reply_timeout);
    if( nullptr == pdu ) {
        jau_ERR_PRINT2("No reply; req %s from %s", req, toString());
        return false;
    }
    jau_COND_PRINT(env.DEBUG_DATA, "GATT WV recv: %s from %s", pdu->toString(), toString());

    if( pdu->getOpcode() == AttPDUMsg::Opcode::WRITE_RSP ) {
        // OK
        res = true;
    } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
        jau_WORDY_PRINT("GATT writeValue unexpected error %s; req %s from %s", pdu->toString(), req, toString());
    } else {
        jau_ERR_PRINT("GATT writeValue unexpected reply %s; req %s from %s", pdu->toString(), req, toString());
    }
    jau_PERF2_TS_TD("GATT writeValue (with-resp)");
    return res;
}

bool BTGattHandler::configNotificationIndication(BTGattDesc & cccd, const bool enableNotification, const bool enableIndication) {
    if( !cccd.isClientCharConfig() ) {
        jau_ERR_PRINT("Not a ClientCharacteristicConfiguration: %s", cccd);
        return false;
    }
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    const uint16_t ccc_value = enableNotification | ( enableIndication << 1 );
    jau_COND_PRINT(env.DEBUG_DATA, "GATTHandler::configNotificationIndication decl %s, enableNotification %d, enableIndication %d",
            cccd, enableNotification, enableIndication);
    cccd.value.resize(2, 2);
    cccd.value.put_uint16_nc(0, ccc_value);
    return writeDescriptorValue(cccd);
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

static const jau::uuid16_t _GENERIC_ACCESS(GattServiceType::GENERIC_ACCESS);
static const jau::uuid16_t _DEVICE_NAME(GattCharacteristicType::DEVICE_NAME);
static const jau::uuid16_t _APPEARANCE(GattCharacteristicType::APPEARANCE);
static const jau::uuid16_t _PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS(GattCharacteristicType::PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS);

static const jau::uuid16_t _DEVICE_INFORMATION(GattServiceType::DEVICE_INFORMATION);
static const jau::uuid16_t _SYSTEM_ID(GattCharacteristicType::SYSTEM_ID);
static const jau::uuid16_t _MODEL_NUMBER_STRING(GattCharacteristicType::MODEL_NUMBER_STRING);
static const jau::uuid16_t _SERIAL_NUMBER_STRING(GattCharacteristicType::SERIAL_NUMBER_STRING);
static const jau::uuid16_t _FIRMWARE_REVISION_STRING(GattCharacteristicType::FIRMWARE_REVISION_STRING);
static const jau::uuid16_t _HARDWARE_REVISION_STRING(GattCharacteristicType::HARDWARE_REVISION_STRING);
static const jau::uuid16_t _SOFTWARE_REVISION_STRING(GattCharacteristicType::SOFTWARE_REVISION_STRING);
static const jau::uuid16_t _MANUFACTURER_NAME_STRING(GattCharacteristicType::MANUFACTURER_NAME_STRING);
static const jau::uuid16_t _REGULATORY_CERT_DATA_LIST(GattCharacteristicType::REGULATORY_CERT_DATA_LIST);
static const jau::uuid16_t _PNP_ID(GattCharacteristicType::PNP_ID);

std::shared_ptr<GattGenericAccessSvc> BTGattHandler::getGenericAccess(jau::darray<BTGattCharRef> & genericAccessCharDeclList) {
    std::shared_ptr<GattGenericAccessSvc> res = nullptr;
    jau::POctets value(number(Defaults::MAX_ATT_MTU), 0, jau::lb_endian_t::little);
    std::string deviceName;
    AppearanceCat appearance = AppearanceCat::UNKNOWN;
    std::shared_ptr<GattPeriphalPreferredConnectionParameters> prefConnParam = nullptr;

    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    for(auto & i : genericAccessCharDeclList) {
        const BTGattChar & charDecl = *i;
        std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
        if( nullptr == service || _GENERIC_ACCESS != *(service->type) ) {
        	continue;
        }
        if( _DEVICE_NAME == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
            	deviceName = GattNameToString(value); // mandatory
            }
        } else if( _APPEARANCE == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) && value.size() >= 2 ) {
            	appearance = static_cast<AppearanceCat>(value.get_uint16(0)); // mandatory
            }
        } else if( _PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
            	prefConnParam = GattPeriphalPreferredConnectionParameters::get(value); // optional
            }
        }
    }
    if( deviceName.size() > 0 ) {
    	res = std::make_shared<GattGenericAccessSvc>(deviceName, appearance, prefConnParam);
    }
    return res;
}

std::shared_ptr<GattGenericAccessSvc> BTGattHandler::getGenericAccess(jau::darray<BTGattServiceRef> & primServices) {
	for(auto & primService : primServices) {
	    BTGattServiceRef service = primService;
	    if( _GENERIC_ACCESS == *service->type ) {
	        return getGenericAccess(primService->characteristicList);
	    }
	}
	return nullptr;
}

bool BTGattHandler::ping() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    bool readOK = true;

    for(size_t i=0; readOK && i<services.size(); i++) {
        jau::darray<BTGattCharRef> & genericAccessCharDeclList = services.at(i)->characteristicList;
        jau::POctets value(32, 0, jau::lb_endian_t::little);

        for(size_t j=0; readOK && j<genericAccessCharDeclList.size(); j++) {
            const BTGattChar & charDecl = *genericAccessCharDeclList.at(j);
            std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
            if( nullptr == service || _GENERIC_ACCESS != *(service->type) ) {
                continue;
            }
            if( _APPEARANCE == *charDecl.value_type ) {
                if( readCharacteristicValue(charDecl, value.clear()) ) {
                    return true; // unique success case
                }
                // read failure, might be disconnected
                readOK = false;
            }
        }
    }
    if( readOK ) {
        jau_INFO_PRINT("GATTHandler::pingGATT: No GENERIC_ACCESS Service with APPEARANCE Characteristic available -> disconnect");
    } else {
        jau_INFO_PRINT("GATTHandler::pingGATT: Read error -> disconnect");
    }
    disconnect(true /* disconnect_device */, true /* ioerr_cause */); // state -> Disconnected
    return false;
}

std::shared_ptr<GattDeviceInformationSvc> BTGattHandler::getDeviceInformation(jau::darray<BTGattCharRef> & characteristicDeclList) {
    std::shared_ptr<GattDeviceInformationSvc> res = nullptr;
    jau::POctets value(number(Defaults::MAX_ATT_MTU), 0, jau::lb_endian_t::little);

    jau::POctets systemID(8, 0, jau::lb_endian_t::little);
    std::string modelNumber;
    std::string serialNumber;
    std::string firmwareRevision;
    std::string hardwareRevision;
    std::string softwareRevision;
    std::string manufacturer;
    jau::POctets regulatoryCertDataList(128, 0, jau::lb_endian_t::little);
    std::shared_ptr<GattPnP_ID> pnpID = nullptr;
    bool found = false;

    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    for(auto & i : characteristicDeclList) {
        const BTGattChar & charDecl = *i;
        std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
        if( nullptr == service || _DEVICE_INFORMATION != *(service->type) ) {
            continue;
        }
        found = true;
        if( _SYSTEM_ID == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, systemID.clear()) ) {
                // nop
            }
        } else if( _REGULATORY_CERT_DATA_LIST == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, regulatoryCertDataList.clear()) ) {
                // nop
            }
        } else if( _PNP_ID == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                pnpID = GattPnP_ID::get(value);
            }
        } else if( _MODEL_NUMBER_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                modelNumber = GattNameToString(value);
            }
        } else if( _SERIAL_NUMBER_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                serialNumber = GattNameToString(value);
            }
        } else if( _FIRMWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                firmwareRevision = GattNameToString(value);
            }
        } else if( _HARDWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                hardwareRevision = GattNameToString(value);
            }
        } else if( _SOFTWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                softwareRevision = GattNameToString(value);
            }
        } else if( _MANUFACTURER_NAME_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.clear()) ) {
                manufacturer = GattNameToString(value);
            }
        }
    }
    if( found ) {
        res = std::make_shared<GattDeviceInformationSvc>(systemID, modelNumber, serialNumber,
                                          firmwareRevision, hardwareRevision, softwareRevision,
                                          manufacturer, regulatoryCertDataList, pnpID);
    }
    return res;
}

std::shared_ptr<GattDeviceInformationSvc> BTGattHandler::getDeviceInformation(jau::darray<BTGattServiceRef> & primServices) {
    for(auto & primService : primServices) {
        BTGattServiceRef service = primService;
        if( _DEVICE_INFORMATION == *service->type ) {
            return getDeviceInformation(primService->characteristicList);
        }
    }
    return nullptr;
}

std::string BTGattHandler::toString() const noexcept {
    return jau_format_string("GattHndlr[%s, %s, mode %s, mtu %u, listener[BTGatt %zu, Native %zu], l2capWorker[running %s, shallStop %s, tid %p], %s]",
        getRole(), deviceString, gattServerHandler->getMode(), usedMTU, gattCharListenerList.size(), nativeGattCharListenerList.size(),
        l2cap_reader_service.is_running(), l2cap_reader_service.shall_stop(),
        (void*)l2cap_reader_service.thread_id(), // NOLINT(performance-no-int-to-ptr)
        getStateString());
}
