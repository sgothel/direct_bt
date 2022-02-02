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
    #include <signal.h>
}

// #define PERF_PRINT_ON 1
// PERF2_PRINT_ON for read/write single values
// #define PERF2_PRINT_ON 1
// PERF3_PRINT_ON for disconnect
// #define PERF3_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "L2CAPIoctl.hpp"
#include "GattNumbers.hpp"

#include "BTGattHandler.hpp"

#include "BTDevice.hpp"

#include "BTManager.hpp"
#include "BTAdapter.hpp"
#include "BTManager.hpp"
#include "DBTConst.hpp"

using namespace direct_bt;

BTGattEnv::BTGattEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.gatt") ),
  GATT_READ_COMMAND_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.gatt.cmd.read.timeout", 550, 550 /* min */, INT32_MAX /* max */) ),
  GATT_WRITE_COMMAND_REPLY_TIMEOUT(  jau::environment::getInt32Property("direct_bt.gatt.cmd.write.timeout", 550, 550 /* min */, INT32_MAX /* max */) ),
  GATT_INITIAL_COMMAND_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.gatt.cmd.init.timeout", 2500, 2000 /* min */, INT32_MAX /* max */) ),
  ATTPDU_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.gatt.ringsize", 128, 64 /* min */, 1024 /* max */) ),
  DEBUG_DATA( jau::environment::getBooleanProperty("direct_bt.debug.gatt.data", false) )
{
}

BTDeviceRef BTGattHandler::getDeviceChecked() const {
    BTDeviceRef ref = wbr_device.lock();
    if( nullptr == ref ) {
        throw jau::IllegalStateException("GATTHandler's device already destructed: "+toString(), E_FILE_LINE);
    }
    return ref;
}

bool BTGattHandler::validateConnected() noexcept {
    const bool l2capIsConnected = l2cap.isOpen();
    const bool l2capHasIOError = l2cap.hasIOError();

    if( has_ioerror || l2capHasIOError ) {
        ERR_PRINT("IOError state: GattHandler %s, l2cap %s: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), toString().c_str());
        has_ioerror = true; // propagate l2capHasIOError -> has_ioerror
        return false;
    }

    if( !is_connected || !l2capIsConnected ) {
        ERR_PRINT("Disconnected state: GattHandler %s, l2cap %s: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), toString().c_str());
        return false;
    }
    return true;
}

static jau::cow_darray<std::shared_ptr<BTGattCharListener>>::equal_comparator _btGattCharListenerRefEqComparator =
        [](const std::shared_ptr<BTGattCharListener> &a, const std::shared_ptr<BTGattCharListener> &b) -> bool { return *a == *b; };

bool BTGattHandler::addCharListener(std::shared_ptr<BTGattCharListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("GATTEventListener ref is null", E_FILE_LINE);
    }
    return btGattCharListenerList.push_back_unique(l, _btGattCharListenerRefEqComparator);
}

bool BTGattHandler::removeCharListener(std::shared_ptr<BTGattCharListener> l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    const int count = btGattCharListenerList.erase_matching(l, false /* all_matching */, _btGattCharListenerRefEqComparator);
    return count > 0;
}

bool BTGattHandler::removeCharListener(const BTGattCharListener * l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("GATTCharacteristicListener ref is null");
        return false;
    }
    auto it = btGattCharListenerList.begin(); // lock mutex and copy_store
    for (; !it.is_end(); ++it ) {
        if ( **it == *l ) {
            it.erase();
            it.write_back();
            return true;
        }
    }
    return false;
}

static jau::cow_darray<std::shared_ptr<BTGattHandler::NativeGattCharListener>>::equal_comparator _nativeGattCharListenerRefEqComparator =
        [](const std::shared_ptr<BTGattHandler::NativeGattCharListener> &a, const std::shared_ptr<BTGattHandler::NativeGattCharListener> &b) -> bool { return *a == *b; };

bool BTGattHandler::addCharListener(std::shared_ptr<NativeGattCharListener> l){
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("NativeGattCharListener ref is null", E_FILE_LINE);
    }
    return nativeGattCharListenerList.push_back_unique(l, _nativeGattCharListenerRefEqComparator);
}

bool BTGattHandler::removeCharListener(std::shared_ptr<NativeGattCharListener> l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("NativeGattCharListener ref is null");
        return false;
    }
    const int count = nativeGattCharListenerList.erase_matching(l, false /* all_matching */, _nativeGattCharListenerRefEqComparator);
    return count > 0;
}

void BTGattHandler::printCharListener() noexcept {
    jau::INFO_PRINT("BTGattHandler: BTGattChar %u listener", btGattCharListenerList.size());
    {
        int i=0;
        auto it = btGattCharListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it, ++i ) {
            jau::INFO_PRINT("[%d]: %s", i, (*it)->toString().c_str());
        }
    }
    jau::INFO_PRINT("BTGattHandler: NativeGattChar %u listener", nativeGattCharListenerList.size());
    {
        int i=0;
        auto it = nativeGattCharListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it, ++i ) {
            jau::INFO_PRINT("[%d]: %s", i, (*it)->toString().c_str());
        }
    }
}

int BTGattHandler::removeAllAssociatedCharListener(std::shared_ptr<BTGattChar> associatedCharacteristic) noexcept {
    if( nullptr == associatedCharacteristic ) {
        ERR_PRINT("Given GATTCharacteristic ref is null");
        return false;
    }
    return removeAllAssociatedCharListener( associatedCharacteristic.get() );
}

int BTGattHandler::removeAllAssociatedCharListener(const BTGattChar * associatedCharacteristic) noexcept {
    if( nullptr == associatedCharacteristic ) {
        ERR_PRINT("Given GATTCharacteristic ref is null");
        return false;
    }
    int count = 0;
    auto it = btGattCharListenerList.begin(); // lock mutex and copy_store
    while( !it.is_end() ) {
        if ( (*it)->match(*associatedCharacteristic) ) {
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

int BTGattHandler::removeAllCharListener() noexcept {
    int count = btGattCharListenerList.size();
    btGattCharListenerList.clear();
    count += nativeGattCharListenerList.size();
    nativeGattCharListenerList.clear();
    return count;
}

void BTGattHandler::setSendIndicationConfirmation(const bool v) {
    sendIndicationConfirmation = v;
}

bool BTGattHandler::getSendIndicationConfirmation() noexcept {
    return sendIndicationConfirmation;
}

void BTGattHandler::replyAttPDUReq(std::unique_ptr<const AttPDUMsg> && pdu) noexcept {
    if( !validateConnected() ) { // shall not happen
        DBG_PRINT("GATT-Req: disconnected: req %s from %s",
                pdu->toString().c_str(), toString().c_str());
        return;
    }
    switch( pdu->getOpcode() ) {
        case AttPDUMsg::Opcode::EXCHANGE_MTU_REQ: { // 2
            gattServerHandler->replyExchangeMTUReq( static_cast<const AttExchangeMTU*>( pdu.get() ) );
            return;
        }

        case AttPDUMsg::Opcode::FIND_INFORMATION_REQ: { // 4
            gattServerHandler->replyFindInfoReq( static_cast<const AttFindInfoReq*>( pdu.get() ) );
            return;
        }

        case AttPDUMsg::Opcode::FIND_BY_TYPE_VALUE_REQ: { // 6
            gattServerHandler->replyFindByTypeValueReq( static_cast<const AttFindByTypeValueReq*>( pdu.get() ) );
            return;
        }

        case AttPDUMsg::Opcode::READ_BY_TYPE_REQ: { // 8
            gattServerHandler->replyReadByTypeReq( static_cast<const AttReadByNTypeReq*>( pdu.get() ) );
            return;
        }

        case AttPDUMsg::Opcode::READ_REQ: // 10
            [[fallthrough]];
        case AttPDUMsg::Opcode::READ_BLOB_REQ: { // 12
            gattServerHandler->replyReadReq( pdu.get() );
            return;
        }

        case AttPDUMsg::Opcode::READ_BY_GROUP_TYPE_REQ: { // 16
            gattServerHandler->replyReadByGroupTypeReq( static_cast<const AttReadByNTypeReq*>( pdu.get() ) );
            return;
        }

        case AttPDUMsg::Opcode::WRITE_REQ: // 18
            [[fallthrough]];
        case AttPDUMsg::Opcode::WRITE_CMD: // 18 + 64 = 82
            [[fallthrough]];
        case AttPDUMsg::Opcode::PREPARE_WRITE_REQ: // 22
            [[fallthrough]];
        case AttPDUMsg::Opcode::EXECUTE_WRITE_REQ: { // 24
            gattServerHandler->replyWriteReq( pdu.get() );
            return;
        }

        // TODO: Add support for the following requests

        case AttPDUMsg::Opcode::READ_MULTIPLE_REQ: // 14
            [[fallthrough]];
        case AttPDUMsg::Opcode::READ_MULTIPLE_VARIABLE_REQ: // 32
            [[fallthrough]];
        case AttPDUMsg::Opcode::SIGNED_WRITE_CMD: { // 18 + 64 + 128 = 210
            AttErrorRsp rsp(AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST, pdu->getOpcode(), 0);
            WARN_PRINT("GATT Req: Ignored: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), toString().c_str());
            send(rsp);
            return;
        }

        default:
            AttErrorRsp rsp(AttErrorRsp::ErrorCode::FORBIDDEN_VALUE, pdu->getOpcode(), 0);
            ERR_PRINT("GATT Req: Unhandled: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), toString().c_str());
            send(rsp);
            return;
    }
}

void BTGattHandler::l2capReaderWork(jau::service_runner& sr) noexcept {
    jau::snsize_t len;
    if( !validateConnected() ) {
        ERR_PRINT("GATTHandler::reader: Invalid IO state -> Stop");
        sr.set_shall_stop();
        return;
    }

    len = l2cap.read(rbuffer.get_wptr(), rbuffer.size());
    if( 0 < len ) {
        std::unique_ptr<const AttPDUMsg> attPDU = AttPDUMsg::getSpecialized(rbuffer.get_ptr(), static_cast<jau::nsize_t>(len));
        COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: Got %s", attPDU->toString().c_str());

        const AttPDUMsg::Opcode opc = attPDU->getOpcode();
        const AttPDUMsg::OpcodeType opc_type = AttPDUMsg::get_type(opc);

        if( AttPDUMsg::Opcode::MULTIPLE_HANDLE_VALUE_NTF == opc ) { // AttPDUMsg::OpcodeType::NOTIFICATION
            // FIXME TODO ..
            ERR_PRINT("GATTHandler::reader: MULTI-NTF not implemented: %s", attPDU->toString().c_str());
        } else if( AttPDUMsg::Opcode::HANDLE_VALUE_NTF == opc ) { // AttPDUMsg::OpcodeType::NOTIFICATION
            const AttHandleValueRcv * a = static_cast<const AttHandleValueRcv*>(attPDU.get());
            COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: NTF: %s, listener [native %zd, bt %zd]",
                    a->toString().c_str(), nativeGattCharListenerList.size(), btGattCharListenerList.size());
            const uint64_t a_timestamp = a->ts_creation;
            const uint16_t a_handle = a->getHandle();
            const jau::TOctetSlice& a_value = a->getValue();
            const jau::TROOctets a_data_view(a_value.get_ptr_nc(0), a_value.size(), a_value.byte_order()); // just a view, still owned by attPDU
            BTDeviceRef device = getDeviceUnchecked();
            if( nullptr != device ) {
                int i=0;
                jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<NativeGattCharListener> &l) {
                    try {
                        l->notificationReceived(device, a_handle, a_data_view, a_timestamp);
                    } catch (std::exception &e) {
                        ERR_PRINT("GATTHandler::notificationReceived-CBs %d/%zd: NativeGattCharListener %s: Caught exception %s",
                                i+1, nativeGattCharListenerList.size(),
                                jau::to_hexstring((void*)l.get()).c_str(), e.what());
                    }
                    i++;
                });
            }
            BTGattCharRef decl = findCharacterisicsByValueHandle(services, a_handle);
            if( nullptr != decl ) {
                int i=0;
                jau::for_each_fidelity(btGattCharListenerList, [&](std::shared_ptr<BTGattCharListener> &l) {
                    try {
                        if( l->match(*decl) ) {
                            l->notificationReceived(decl, a_data_view, a_timestamp);
                        }
                    } catch (std::exception &e) {
                        ERR_PRINT("GATTHandler::notificationReceived-CBs %d/%zd: BTGattCharListener %s: Caught exception %s",
                                i+1, btGattCharListenerList.size(),
                                jau::to_hexstring((void*)l.get()).c_str(), e.what());
                    }
                    i++;
                });
            }
        } else if( AttPDUMsg::Opcode::HANDLE_VALUE_IND == opc ) { // AttPDUMsg::OpcodeType::INDICATION
            const AttHandleValueRcv * a = static_cast<const AttHandleValueRcv*>(attPDU.get());
            COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: IND: %s, sendIndicationConfirmation %d, listener [native %zd, bt %zd]",
                    a->toString().c_str(), sendIndicationConfirmation.load(), nativeGattCharListenerList.size(), btGattCharListenerList.size());
            bool cfmSent = false;
            if( sendIndicationConfirmation ) {
                AttHandleValueCfm cfm;
                send(cfm);
                cfmSent = true;
            }
            const uint64_t a_timestamp = a->ts_creation;
            const uint16_t a_handle = a->getHandle();
            const jau::TOctetSlice& a_value = a->getValue();
            const jau::TROOctets a_data_view(a_value.get_ptr_nc(0), a_value.size(), a_value.byte_order()); // just a view, still owned by attPDU
            BTDeviceRef device = getDeviceUnchecked();
            if( nullptr != device ) {
                int i=0;
                jau::for_each_fidelity(nativeGattCharListenerList, [&](std::shared_ptr<NativeGattCharListener> &l) {
                    try {
                        l->indicationReceived(device, a_handle, a_data_view, a_timestamp, cfmSent);
                    } catch (std::exception &e) {
                        ERR_PRINT("GATTHandler::indicationReceived-CBs %d/%zd: NativeGattCharListener %s: Caught exception %s",
                                i+1, nativeGattCharListenerList.size(),
                                jau::to_hexstring((void*)l.get()).c_str(), e.what());
                    }
                    i++;
                });
            }
            BTGattCharRef decl = findCharacterisicsByValueHandle(services, a_handle);
            if( nullptr != decl ) {
                int i=0;
                jau::for_each_fidelity(btGattCharListenerList, [&](std::shared_ptr<BTGattCharListener> &l) {
                    try {
                        if( l->match(*decl) ) {
                            l->indicationReceived(decl, a_data_view, a_timestamp, cfmSent);
                        }
                    } catch (std::exception &e) {
                        ERR_PRINT("GATTHandler::indicationReceived-CBs %d/%zd: BTGattCharListener %s, cfmSent %d: Caught exception %s",
                                i+1, btGattCharListenerList.size(),
                                jau::to_hexstring((void*)l.get()).c_str(), cfmSent, e.what());
                    }
                    i++;
                });
            }
        } else if( AttPDUMsg::OpcodeType::RESPONSE == opc_type ) {
            COND_PRINT(env.DEBUG_DATA, "GATTHandler::reader: Ring: %s", attPDU->toString().c_str());
            attPDURing.putBlocking( std::move(attPDU) );
        } else if( AttPDUMsg::OpcodeType::REQUEST == opc_type ) {
            replyAttPDUReq( std::move( attPDU ) );
        } else {
            ERR_PRINT("GATTHandler::reader: Unhandled: %s", attPDU->toString().c_str());
        }
    } else if( 0 > len && ETIMEDOUT != errno && !sr.get_shall_stop() ) { // expected exits
        IRQ_PRINT("GATTHandler::reader: l2cap read error -> Stop; l2cap.read %d (%s); %s",
                len, L2CAPComm::getRWExitCodeString(len).c_str(),
                getStateString().c_str());
        sr.set_shall_stop();
        has_ioerror = true;
    } else if( len != L2CAPComm::number(L2CAPComm::RWExitCode::POLL_TIMEOUT) ) { // expected POLL_TIMEOUT if idle
        WORDY_PRINT("GATTHandler::reader: l2cap read: l2cap.read %d (%s); %s",
                len, L2CAPComm::getRWExitCodeString(len).c_str(),
                getStateString().c_str());
    }
}

void BTGattHandler::l2capReaderEndLocked(jau::service_runner& sr) noexcept {
    (void)sr;
    WORDY_PRINT("GATTHandler::reader: Ended. Ring has %u entries flushed", attPDURing.size());
    attPDURing.clear();
}
void BTGattHandler::l2capReaderEndFinal(jau::service_runner& sr) noexcept {
    (void)sr;
    disconnect(true /* disconnectDevice */, has_ioerror);
}

BTGattHandler::BTGattHandler(const BTDeviceRef &device, L2CAPComm& l2cap_att, const int32_t supervision_timeout_) noexcept
: supervision_timeout(supervision_timeout_),
  env(BTGattEnv::get()),
  read_cmd_reply_timeout(std::max<int32_t>(env.GATT_READ_COMMAND_REPLY_TIMEOUT, supervision_timeout+50)),
  write_cmd_reply_timeout(std::max<int32_t>(env.GATT_WRITE_COMMAND_REPLY_TIMEOUT, supervision_timeout+50)),
  wbr_device(device),
  role(device->getLocalGATTRole()),
  l2cap(l2cap_att),
  deviceString(device->getAddressAndType().toString()),
  rbuffer(number(Defaults::MAX_ATT_MTU), jau::endian::little),
  is_connected(l2cap.isOpen()), has_ioerror(false),
  l2cap_reader_service("GATTHandler::reader", THREAD_SHUTDOWN_TIMEOUT_MS,
                       jau::bindMemberFunc(this, &BTGattHandler::l2capReaderWork),
                       jau::service_runner::Callback() /* init */,
                       jau::bindMemberFunc(this, &BTGattHandler::l2capReaderEndLocked),
                       jau::bindMemberFunc(this, &BTGattHandler::l2capReaderEndFinal)),
  attPDURing(env.ATTPDU_RING_CAPACITY),
  serverMTU(number(Defaults::MIN_ATT_MTU)), usedMTU(number(Defaults::MIN_ATT_MTU)), clientMTUExchanged(false),
  gattServerData( GATTRole::Server == role ? device->getAdapter().getGATTServerData() : nullptr ),
  gattServerHandler( selectGattServerHandler(*this, gattServerData) )
{
    if( !validateConnected() ) {
        ERR_PRINT("GATTHandler.ctor: L2CAP could not connect");
        is_connected = false;
        return;
    }
    DBG_PRINT("GATTHandler::ctor: Start Connect: GattHandler[%s], l2cap[%s]: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), toString().c_str());

    /**
     * We utilize DBTManager's mgmthandler_sigaction SIGALRM handler,
     * as we only can install one handler.
     */
    l2cap_reader_service.start();

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
            int i=0;
            jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                try {
                    l->connected(device, usedMTU);
                } catch (std::exception &e) {
                    ERR_PRINT("GATTHandler::connected: %d/%zd: %s: Caught exception %s",
                            i+1, gattServerData->listener().size(),
                            toString().c_str(), e.what());
                }
                i++;
            });
        }
    }
}

BTGattHandler::~BTGattHandler() noexcept {
    DBG_PRINT("GATTHandler::dtor: Start: %s", toString().c_str());
    disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    btGattCharListenerList.clear();
    nativeGattCharListenerList.clear();
    services.clear();
    genericAccess = nullptr;
    DBG_PRINT("GATTHandler::dtor: End: %s", toString().c_str());
}

bool BTGattHandler::disconnect(const bool disconnectDevice, const bool ioErrorCause) noexcept {
    PERF3_TS_T0();
    // Interrupt GATT's L2CAP::connect(..) and L2CAP::read(..), avoiding prolonged hang
    // and pull all underlying l2cap read operations!
    l2cap.close();

    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !is_connected.compare_exchange_strong(expConn, false) ) {
        // not connected
        DBG_PRINT("GATTHandler::disconnect: Not connected: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s]: %s",
                  disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(), toString().c_str());
        btGattCharListenerList.clear();
        nativeGattCharListenerList.clear();
        return false;
    }

    gattServerHandler->close();

    // Lock to avoid other threads using instance while disconnecting
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("GATTHandler::disconnect: Start: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s]: %s",
              disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(), toString().c_str());
    btGattCharListenerList.clear();
    nativeGattCharListenerList.clear();

    PERF3_TS_TD("GATTHandler::disconnect.1");
    l2cap_reader_service.stop();
    PERF3_TS_TD("GATTHandler::disconnect.2");

    clientMTUExchanged = false;

    if( disconnectDevice ) {
        BTDeviceRef device = getDeviceUnchecked();
        if( nullptr != device ) {
            // Cleanup device resources, proper connection state
            // Intentionally giving the POWER_OFF reason for the device in case of ioErrorCause!
            const HCIStatusCode reason = ioErrorCause ?
                                   HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF :
                                   HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION;
            device->disconnect(reason);
        }
    }

    PERF3_TS_TD("GATTHandler::disconnect.X");
    DBG_PRINT("GATTHandler::disconnect: End: %s", toString().c_str());
    return true;
}

void BTGattHandler::send(const AttPDUMsg & msg) {
    if( !validateConnected() ) {
        throw jau::IllegalStateException("GATTHandler::send: Invalid IO State: req "+msg.toString()+" to "+toString(), E_FILE_LINE);
    }
    // [1 .. ATT_MTU-1] BT Core Spec v5.2: Vol 3, Part F 3.2.9 Long attribute values
    if( msg.pdu.size() > usedMTU ) {
        throw jau::IllegalArgumentException("Msg PDU size "+std::to_string(msg.pdu.size())+" >= usedMTU "+std::to_string(usedMTU)+
                                       ", "+msg.toString()+" to "+toString(), E_FILE_LINE);
    }

    // Thread safe l2cap.write(..) operation..
    const jau::snsize_t res = l2cap.write(msg.pdu.get_ptr(), msg.pdu.size());
    if( 0 > res ) {
        IRQ_PRINT("GATTHandler::send: l2cap write error -> disconnect: l2cap.write %d (%s); %s; %s to %s",
                res, L2CAPComm::getRWExitCodeString(res).c_str(), getStateString().c_str(),
                msg.toString().c_str(), toString().c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */); // state -> Disconnected
        throw BTException("GATTHandler::send: l2cap write error: req "+msg.toString()+" to "+toString(), E_FILE_LINE);
    }
    if( static_cast<size_t>(res) != msg.pdu.size() ) {
        ERR_PRINT("GATTHandler::send: l2cap write count error, %d != %zu: %s -> disconnect: %s",
                res, msg.pdu.size(), msg.toString().c_str(), toString().c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */); // state -> Disconnected
        throw BTException("GATTHandler::send: l2cap write count error, "+std::to_string(res)+" != "+std::to_string(msg.pdu.size())
                                 +": "+msg.toString()+" -> disconnect: "+toString(), E_FILE_LINE);
    }
}

std::unique_ptr<const AttPDUMsg> BTGattHandler::sendWithReply(const AttPDUMsg & msg, const int timeout) {
    send( msg );

    // Ringbuffer read is thread safe
    std::unique_ptr<const AttPDUMsg> res;
    if( !attPDURing.getBlocking(res, timeout) || nullptr == res ) {
        errno = ETIMEDOUT;
        IRQ_PRINT("GATTHandler::sendWithReply: nullptr result (timeout %d): req %s to %s", timeout, msg.toString().c_str(), toString().c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */);
        throw BTException("GATTHandler::sendWithReply: nullptr result (timeout "+std::to_string(timeout)+"): req "+msg.toString()+" to "+toString(), E_FILE_LINE);
    }
    return res;
}

uint16_t BTGattHandler::clientMTUExchange(const int32_t timeout) {
    if( GATTRole::Client != getRole() ) {
        ERR_PRINT("GATT MTU exchange only allowed in client mode");
        return usedMTU;
    }
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.3.1 Exchange MTU (Server configuration)
     */
    const AttExchangeMTU req(AttPDUMsg::ReqRespType::REQUEST, number(Defaults::MAX_ATT_MTU));
    const std::lock_guard<std::recursive_mutex> lock(mtx_command);
    PERF_TS_T0();

    uint16_t mtu = 0;
    DBG_PRINT("GATT MTU-REQ send: %s to %s", req.toString().c_str(), toString().c_str());

    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, timeout); // valid reply or exception

    if( pdu->getOpcode() == AttPDUMsg::Opcode::EXCHANGE_MTU_RSP ) {
        const AttExchangeMTU * p = static_cast<const AttExchangeMTU*>(pdu.get());
        mtu = p->getMTUSize();
        DBG_PRINT("GATT MTU-RSP recv: %u, %s from %s", mtu, pdu->toString().c_str(), toString().c_str());
    } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
        /**
         * If the ATT_ERROR_RSP PDU is sent by the server
         * with the error code set to 'Request Not Supported',
         * the Attribute Opcode is not supported and the default MTU shall be used.
         */
        const AttErrorRsp * p = static_cast<const AttErrorRsp *>(pdu.get());
        if( AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST == p->getErrorCode() ) {
            mtu = number(Defaults::MIN_ATT_MTU); // OK by spec: Use default MTU
            DBG_PRINT("GATT MTU handled error -> ATT_MTU %u, %s from %s", mtu, pdu->toString().c_str(), toString().c_str());
        } else {
            WORDY_PRINT("GATT MTU unexpected error %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
        }
    } else {
        ERR_PRINT("GATT MTU unexpected reply %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
    }
    PERF_TS_TD("GATT exchangeMTU");

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
        ERR_PRINT("BTDevice::sendNotification: GATTRole not server");
        return false;
    }
    if( DBGattServer::Mode::DB == gattServerHandler->getMode() &&
        nullptr == findServerGattCharByValueHandle(char_value_handle) )
    {
        ERR_PRINT("BTDevice::sendNotification: invalid char handle %s", jau::to_hexstring(char_value_handle).c_str());
        return false;
    }
    if( 0 == value.size() ) {
        COND_PRINT(env.DEBUG_DATA, "GATT SEND NTF: Zero size, skipped sending to %s", toString().c_str());
        return true;
    }
    AttHandleValueRcv data(true /* isNotify */, char_value_handle, value, usedMTU);
    COND_PRINT(env.DEBUG_DATA, "GATT SEND NTF: %s to %s", data.toString().c_str(), toString().c_str());
    send(data);
    return true;
}

bool BTGattHandler::sendIndication(const uint16_t char_value_handle, const jau::TROOctets & value) {
    if( GATTRole::Server != role ) {
        ERR_PRINT("BTDevice::sendIndication: GATTRole not server");
        return false;
    }
    if( DBGattServer::Mode::DB == gattServerHandler->getMode() &&
        nullptr == findServerGattCharByValueHandle(char_value_handle) )
    {
        ERR_PRINT("BTDevice::sendIndication: invalid char handle %s", jau::to_hexstring(char_value_handle).c_str());
        return false;
    }
    if( 0 == value.size() ) {
        COND_PRINT(env.DEBUG_DATA, "GATT SEND IND: Zero size, skipped sending to %s", toString().c_str());
        return true;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    AttHandleValueRcv req(false /* isNotify */, char_value_handle, value, usedMTU);
    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, write_cmd_reply_timeout); // valid reply or exception
    if( pdu->getOpcode() == AttPDUMsg::Opcode::HANDLE_VALUE_CFM ) {
        COND_PRINT(env.DEBUG_DATA, "GATT SENT IND: %s -> %s to/from %s",
                req.toString().c_str(), pdu->toString().c_str(), toString().c_str());
        return true;
    } else {
        WARN_PRINT("GATT SENT IND: Failed, no CFM reply: %s -> %s to/from %s",
                req.toString().c_str(), pdu->toString().c_str(), toString().c_str());
        return false;
    }
}

BTGattCharRef BTGattHandler::findCharacterisicsByValueHandle(const jau::darray<BTGattServiceRef> &services_, const uint16_t charValueHandle) noexcept {
    for(auto it = services_.cbegin(); it != services_.cend(); it++) {
        BTGattCharRef decl = findCharacterisicsByValueHandle(*it, charValueHandle);
        if( nullptr != decl ) {
            return decl;
        }
    }
    return nullptr;
}

BTGattCharRef BTGattHandler::findCharacterisicsByValueHandle(const BTGattServiceRef service, const uint16_t charValueHandle) noexcept {
    for(auto it = service->characteristicList.begin(); it != service->characteristicList.end(); it++) {
        BTGattCharRef decl = *it;
        if( charValueHandle == decl->value_handle ) {
            return decl;
        }
    }
    return nullptr;
}

bool BTGattHandler::initClientGatt(std::shared_ptr<BTGattHandler> shared_this, bool& already_init) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command);
    already_init = clientMTUExchanged && services.size() > 0;
    if( already_init ) {
        return true;
    }
    if( !isConnected() ) {
        DBG_PRINT("GATTHandler::initClientGatt: Not connected: %s", toString().c_str());
        return false;
    }
    if( !clientMTUExchanged) {
        // First point of failure if remote device exposes no GATT functionality. Allow a longer timeout!
        const int32_t initial_command_reply_timeout = std::min<int32_t>(10000, std::max<int32_t>(env.GATT_INITIAL_COMMAND_REPLY_TIMEOUT, 2*supervision_timeout));
        uint16_t mtu = 0;
        try {
            DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: MTU Exchange Start: %s", toString().c_str());
            mtu = clientMTUExchange(initial_command_reply_timeout);
        } catch (std::exception &e) {
            ERR_PRINT2("GattHandler.initClientGatt: exchangeMTU failed: %s", e.what());
        } catch (std::string &msg) {
            ERR_PRINT2("GattHandler.initClientGatt: exchangeMTU failed: %s", msg.c_str());
        } catch (const char *msg) {
            ERR_PRINT2("GattHandler.initClientGatt: exchangeMTU failed: %s", msg);
        }
        if( 0 == mtu ) {
            ERR_PRINT2("GATTHandler::initClientGatt: Local GATT Client: Zero serverMTU -> disconnect: %s", toString().c_str());
            disconnect(true /* disconnectDevice */, false /* ioErrorCause */);
            return false;
        }
        serverMTU = mtu;
        usedMTU = std::min(number(Defaults::MAX_ATT_MTU), serverMTU.load());
        clientMTUExchanged = true;
        DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: MTU Exchanged: server %u -> used %u, %s", serverMTU.load(), usedMTU.load(), toString().c_str());
    }

    if( services.size() > 0 ) {
        // already initialized
        return true;
    }

    try {
        // Service discovery may consume 500ms - 2000ms, depending on bandwidth
        DBG_PRINT("GATTHandler::initClientGatt: Local GATT Client: Service Discovery Start: %s", toString().c_str());
        jau::darray<BTGattServiceRef>& gattServices = discoverCompletePrimaryServices(shared_this);
        if( gattServices.size() == 0 ) { // nothing discovered
            ERR_PRINT2("GATTHandler::initClientGatt: No primary services discovered");
            disconnect(true /* disconnectDevice */, false /* ioErrorCause */);
            return false;
        }
        DBG_PRINT("GATTHandler::initClientGatt: %zu Services Discovered: %s", gattServices.size(), toString().c_str());
        return true;
    } catch (std::exception &e) {
        WARN_PRINT("GATTHandler::initClientGatt: Caught exception: '%s' on %s", e.what(), toString().c_str());
    }
    return false;
}

jau::darray<BTGattServiceRef> & BTGattHandler::discoverCompletePrimaryServices(std::shared_ptr<BTGattHandler> shared_this) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    if( !discoverPrimaryServices(shared_this, services) ) {
        return services;
    }
    for(auto it = services.begin(); it != services.end(); it++) {
        BTGattServiceRef primSrv = *it;
        if( discoverCharacteristics(primSrv) ) {
            discoverDescriptors(primSrv);
        }
    }
    genericAccess = getGenericAccess(services);
    return services;
}

bool BTGattHandler::discoverPrimaryServices(std::shared_ptr<BTGattHandler> shared_this, jau::darray<BTGattServiceRef> & result) {
    {
        // validate shared_this first!
        BTGattHandler *given_this = shared_this.get();
        if( given_this != this ) {
            throw jau::IllegalArgumentException("Given shared GATTHandler reference "+
                    jau::to_hexstring(given_this)+" not matching this "+jau::to_hexstring(this), E_FILE_LINE);
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
    PERF_TS_T0();

    bool done=false;
    uint16_t startHandle=0x0001;
    result.clear();
    while(!done) {
        const AttReadByNTypeReq req(true /* group */, startHandle, 0xffff, groupType);
        COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discover send: %s to %s", req.toString().c_str(), toString().c_str());

        std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout); // valid reply or exception
        COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discover recv: %s on %s", pdu->toString().c_str(), toString().c_str());

        if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_BY_GROUP_TYPE_RSP ) {
            const AttReadByGroupTypeRsp * p = static_cast<const AttReadByGroupTypeRsp*>(pdu.get());
            const int esz = p->getElementSize();
            const int count = p->getElementCount();

            for(int i=0; i<count; i++) {
                const int ePDUOffset = p->getElementPDUOffset(i);
                result.push_back( BTGattServiceRef( new BTGattService( shared_this, true,
                        p->pdu.get_uint16(ePDUOffset), // start-handle
                        p->pdu.get_uint16(ePDUOffset + 2), // end-handle
                        p->pdu.get_uuid( ePDUOffset + 2 + 2, jau::uuid_t::toTypeSize(esz-2-2) ) // uuid
                    ) ) );
                COND_PRINT(env.DEBUG_DATA, "GATT PRIM SRV discovered[%d/%d]: %s on %s", i,
                        count, result.at(result.size()-1)->toString().c_str(), toString().c_str());
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
            ERR_PRINT("GATT discoverPrimary unexpected reply %s, req %s from %s",
                    pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
            done = true;
        }
    }
    PERF_TS_TD("GATT discoverPrimaryServices");

    return result.size() > 0;
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
    COND_PRINT(env.DEBUG_DATA, "GATT discoverCharacteristics Service: %s on %s", service->toString().c_str(), toString().c_str());

    PERF_TS_T0();

    bool done=false;
    uint16_t handle=service->handle;
    service->characteristicList.clear();
    while(!done) {
        const AttReadByNTypeReq req(false /* group */, handle, service->end_handle, characteristicTypeReq);
        COND_PRINT(env.DEBUG_DATA, "GATT C discover send: %s to %s", req.toString().c_str(), toString().c_str());

        std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout); // valid reply or exception
        COND_PRINT(env.DEBUG_DATA, "GATT C discover recv: %s from %s", pdu->toString().c_str(), toString().c_str());

        if( pdu->getOpcode() == AttPDUMsg::Opcode::READ_BY_TYPE_RSP ) {
            const AttReadByTypeRsp * p = static_cast<const AttReadByTypeRsp*>(pdu.get());
            const int esz = p->getElementSize();
            const int e_count = p->getElementCount();

            for(int e_iter=0; e_iter<e_count; e_iter++) {
                // handle: handle for the Characteristics declaration
                // value: Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID
                const int ePDUOffset = p->getElementPDUOffset(e_iter);
                service->characteristicList.push_back( BTGattCharRef( new BTGattChar(
                    service,
                    p->getElementHandle(e_iter), // Characteristic Handle
                    static_cast<BTGattChar::PropertyBitVal>(p->pdu.get_uint8(ePDUOffset  + 2)), // Characteristics Property
                    p->pdu.get_uint16(ePDUOffset + 2 + 1), // Characteristics Value Handle
                    p->pdu.get_uuid(ePDUOffset   + 2 + 1 + 2, jau::uuid_t::toTypeSize(esz-2-1-2) ) ) ) ); // Characteristics Value Type UUID
                COND_PRINT(env.DEBUG_DATA, "GATT C discovered[%d/%d]: char%s on %s", e_iter, e_count,
                        service->characteristicList.at(service->characteristicList.size()-1)->toString().c_str(), toString().c_str());
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
            ERR_PRINT("GATT discoverCharacteristics unexpected reply %s, req %s within service%s from %s",
                    pdu->toString().c_str(), req.toString().c_str(), service->toString().c_str(), toString().c_str());
            done = true;
        }
    }

    PERF_TS_TD("GATT discoverCharacteristics");

    return service->characteristicList.size() > 0;
}

bool BTGattHandler::discoverDescriptors(BTGattServiceRef & service) {
    /***
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     */
    COND_PRINT(env.DEBUG_DATA, "GATT discoverDescriptors Service: %s on %s", service->toString().c_str(), toString().c_str());
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    PERF_TS_T0();

    const int charCount = service->characteristicList.size();
    for(int charIter=0; charIter < charCount; charIter++ ) {
        BTGattCharRef charDecl = service->characteristicList[charIter];
        charDecl->clearDescriptors();
        COND_PRINT(env.DEBUG_DATA, "GATT discoverDescriptors Characteristic[%d/%d]: %s on %s", charIter, charCount, charDecl->toString().c_str(), toString().c_str());

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
            COND_PRINT(env.DEBUG_DATA, "GATT CD discover send: %s", req.toString().c_str());

            std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, read_cmd_reply_timeout); // valid reply or exception
            COND_PRINT(env.DEBUG_DATA, "GATT CD discover recv: %s from ", pdu->toString().c_str(), toString().c_str());

            if( pdu->getOpcode() == AttPDUMsg::Opcode::FIND_INFORMATION_RSP ) {
                const AttFindInfoRsp * p = static_cast<const AttFindInfoRsp*>(pdu.get());
                const int e_count = p->getElementCount();

                for(int e_iter=0; e_iter<e_count; e_iter++) {
                    // handle: handle of Characteristic Descriptor.
                    // value: Characteristic Descriptor UUID.
                    const uint16_t cd_handle = p->getElementHandle(e_iter);
                    std::unique_ptr<const jau::uuid_t> cd_uuid = p->getElementValue(e_iter);

                    std::shared_ptr<BTGattDesc> cd( std::make_shared<BTGattDesc>(charDecl, std::move(cd_uuid), cd_handle) );
                    if( cd_handle <= charDecl->value_handle || cd_handle > cd_handle_end ) { // should never happen!
                        ERR_PRINT("GATT discoverDescriptors CD handle %s not in range ]%s..%s]: descr%s within char%s on %s",
                                jau::to_hexstring(cd_handle).c_str(),
                                jau::to_hexstring(charDecl->value_handle).c_str(), jau::to_hexstring(cd_handle_end).c_str(),
                                cd->toString().c_str(), charDecl->toString().c_str(), toString().c_str());
                        done = true;
                        break;

                    }
                    if( !readDescriptorValue(*cd, 0) ) {
                        WORDY_PRINT("GATT discoverDescriptors readDescriptorValue failed: req %s, descr%s within char%s on %s",
                                   req.toString().c_str(), cd->toString().c_str(), charDecl->toString().c_str(), toString().c_str());
                        done = true;
                        break;
                    }
                    if( cd->isClientCharConfig() ) {
                        charDecl->clientCharConfigIndex = charDecl->descriptorList.size();
                    } else if( cd->isUserDescription() ) {
                        charDecl->userDescriptionIndex = charDecl->descriptorList.size();
                    }
                    charDecl->descriptorList.push_back(cd);
                    COND_PRINT(env.DEBUG_DATA, "GATT CD discovered[%d/%d]: %s", e_iter, e_count, cd->toString().c_str());
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
                ERR_PRINT("GATT discoverDescriptors unexpected reply %s; req %s within char%s from %s",
                        pdu->toString().c_str(), req.toString().c_str(), charDecl->toString().c_str(), toString().c_str());
                done = true;
            }
        }
    }
    PERF_TS_TD("GATT discoverDescriptors");

    return service->characteristicList.size() > 0;
}

bool BTGattHandler::readDescriptorValue(BTGattDesc & desc, int expectedLength) {
    COND_PRINT(env.DEBUG_DATA, "GATTHandler::readDescriptorValue expLen %d, desc %s", expectedLength, desc.toString().c_str());
    const bool res = readValue(desc.handle, desc.value, expectedLength);
    if( !res ) {
        WORDY_PRINT("GATT readDescriptorValue error on desc%s within char%s from %s",
                   desc.toString().c_str(), desc.getGattCharChecked()->toString().c_str(), toString().c_str());
    }
    return res;
}

bool BTGattHandler::readCharacteristicValue(const BTGattChar & decl, jau::POctets & resValue, int expectedLength) {
    COND_PRINT(env.DEBUG_DATA, "GATTHandler::readCharacteristicValue expLen %d, decl %s", expectedLength, decl.toString().c_str());
    const bool res = readValue(decl.value_handle, resValue, expectedLength);
    if( !res ) {
        WORDY_PRINT("GATT readCharacteristicValue error on char%s from %s", decl.toString().c_str(), toString().c_str());
    }
    return res;
}

bool BTGattHandler::readValue(const uint16_t handle, jau::POctets & res, int expectedLength) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value */
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    PERF2_TS_T0();

    bool done=false;
    int offset=0;

    COND_PRINT(env.DEBUG_DATA, "GATTHandler::readValue expLen %d, handle %s from %s", expectedLength, jau::to_hexstring(handle).c_str(), toString().c_str());

    while(!done) {
        if( 0 < expectedLength && expectedLength <= offset ) {
            break; // done
        } else if( 0 == expectedLength && 0 < offset ) {
            break; // done w/ only one request
        } // else 0 > expectedLength: implicit

        std::unique_ptr<const AttPDUMsg> pdu = nullptr;

        const AttReadReq req0(handle);
        const AttReadBlobReq req1(handle, offset);
        const AttPDUMsg & req = ( 0 == offset ) ? static_cast<const AttPDUMsg &>(req0) : static_cast<const AttPDUMsg &>(req1);
        COND_PRINT(env.DEBUG_DATA, "GATT RV send: %s", req.toString().c_str());
        pdu = sendWithReply(req, read_cmd_reply_timeout); // valid reply or exception

        COND_PRINT(env.DEBUG_DATA, "GATT RV recv: %s from %s", pdu->toString().c_str(), toString().c_str());
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
                WORDY_PRINT("GATT readValue unexpected error %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
                done = true;
            }
        } else {
            ERR_PRINT("GATT readValue unexpected reply %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
            done = true;
        }
    }
    PERF2_TS_TD("GATT readValue");

    return offset > 0;
}

bool BTGattHandler::writeDescriptorValue(const BTGattDesc & cd) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indication */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptor */
    COND_PRINT(env.DEBUG_DATA, "GATTHandler::writeDesccriptorValue desc %s", cd.toString().c_str());
    const bool res = writeValue(cd.handle, cd.value, true);
    if( !res ) {
        WORDY_PRINT("GATT writeDescriptorValue error on desc%s within char%s from %s",
                   cd.toString().c_str(), cd.getGattCharChecked()->toString().c_str(), toString().c_str());
    }
    return res;
}

bool BTGattHandler::writeCharacteristicValue(const BTGattChar & c, const jau::TROOctets & value) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    COND_PRINT(env.DEBUG_DATA, "GATTHandler::writeCharacteristicValue desc %s, value %s", c.toString().c_str(), value.toString().c_str());
    const bool res = writeValue(c.value_handle, value, true);
    if( !res ) {
        WORDY_PRINT("GATT writeCharacteristicValue error on char%s from %s", c.toString().c_str(), toString().c_str());
    }
    return res;
}

bool BTGattHandler::writeCharacteristicValueNoResp(const BTGattChar & c, const jau::TROOctets & value) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response */
    COND_PRINT(env.DEBUG_DATA, "GATT writeCharacteristicValueNoResp decl %s, value %s", c.toString().c_str(), value.toString().c_str());
    return writeValue(c.value_handle, value, false); // complete or exception
}

bool BTGattHandler::writeValue(const uint16_t handle, const jau::TROOctets & value, const bool withResponse) {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indication */
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptor */

    if( value.size() <= 0 ) {
        WARN_PRINT("GATT writeValue size <= 0, no-op: %s", value.toString().c_str());
        return false;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    // FIXME TODO: Long Value if value.size() > ( ATT_MTU - 3 )
    PERF2_TS_T0();

    if( !withResponse ) {
        AttWriteCmd req(handle, value);
        COND_PRINT(env.DEBUG_DATA, "GATT WV send(resp %d): %s to %s", withResponse, req.toString().c_str(), toString().c_str());

        send( req ); // complete or exception
        PERF2_TS_TD("GATT writeValue (no-resp)");
        return true;
    }

    AttWriteReq req(handle, value);
    COND_PRINT(env.DEBUG_DATA, "GATT WV send(resp %d): %s to %s", withResponse, req.toString().c_str(), toString().c_str());

    bool res = false;
    std::unique_ptr<const AttPDUMsg> pdu = sendWithReply(req, write_cmd_reply_timeout); // valid reply or exception
    COND_PRINT(env.DEBUG_DATA, "GATT WV recv: %s from %s", pdu->toString().c_str(), toString().c_str());

    if( pdu->getOpcode() == AttPDUMsg::Opcode::WRITE_RSP ) {
        // OK
        res = true;
    } else if( pdu->getOpcode() == AttPDUMsg::Opcode::ERROR_RSP ) {
        WORDY_PRINT("GATT writeValue unexpected error %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
    } else {
        ERR_PRINT("GATT writeValue unexpected reply %s; req %s from %s", pdu->toString().c_str(), req.toString().c_str(), toString().c_str());
    }
    PERF2_TS_TD("GATT writeValue (with-resp)");
    return res;
}

bool BTGattHandler::configNotificationIndication(BTGattDesc & cccd, const bool enableNotification, const bool enableIndication) {
    if( !cccd.isClientCharConfig() ) {
        throw jau::IllegalArgumentException("Not a ClientCharacteristicConfiguration: "+cccd.toString(), E_FILE_LINE);
    }
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration */
    const uint16_t ccc_value = enableNotification | ( enableIndication << 1 );
    COND_PRINT(env.DEBUG_DATA, "GATTHandler::configNotificationIndication decl %s, enableNotification %d, enableIndication %d",
            cccd.toString().c_str(), enableNotification, enableIndication);
    cccd.value.resize(2, 2);
    cccd.value.put_uint16_nc(0, ccc_value);
    try {
        return writeDescriptorValue(cccd);
    } catch (BTException & bte) {
        if( !enableNotification && !enableIndication ) {
            // OK to have lost connection @ disable
            WORDY_PRINT("GATTHandler::configNotificationIndication(disable) on %s caught exception: %s", toString().c_str(), bte.what());
            return false;
        } else {
            throw; // re-throw current exception
        }
    }
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
    jau::POctets value(number(Defaults::MAX_ATT_MTU), 0, jau::endian::little);
    std::string deviceName = "";
    AppearanceCat appearance = AppearanceCat::UNKNOWN;
    std::shared_ptr<GattPeriphalPreferredConnectionParameters> prefConnParam = nullptr;

    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    for(size_t i=0; i<genericAccessCharDeclList.size(); i++) {
        const BTGattChar & charDecl = *genericAccessCharDeclList.at(i);
        std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
        if( nullptr == service || _GENERIC_ACCESS != *(service->type) ) {
        	continue;
        }
        if( _DEVICE_NAME == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
            	deviceName = GattNameToString(value); // mandatory
            }
        } else if( _APPEARANCE == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) && value.size() >= 2 ) {
            	appearance = static_cast<AppearanceCat>(value.get_uint16(0)); // mandatory
            }
        } else if( _PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
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
	for(size_t i=0; i<primServices.size(); i++) {
	    BTGattServiceRef service = primServices.at(i);
	    if( _GENERIC_ACCESS == *service->type ) {
	        return getGenericAccess(primServices.at(i)->characteristicList);
	    }
	}
	return nullptr;
}

bool BTGattHandler::ping() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    bool readOK = true;

    for(size_t i=0; readOK && i<services.size(); i++) {
        jau::darray<BTGattCharRef> & genericAccessCharDeclList = services.at(i)->characteristicList;
        jau::POctets value(32, 0, jau::endian::little);

        for(size_t j=0; readOK && j<genericAccessCharDeclList.size(); j++) {
            const BTGattChar & charDecl = *genericAccessCharDeclList.at(j);
            std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
            if( nullptr == service || _GENERIC_ACCESS != *(service->type) ) {
                continue;
            }
            if( _APPEARANCE == *charDecl.value_type ) {
                if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                    return true; // unique success case
                }
                // read failure, but not disconnected as no exception thrown from sendWithReply
                readOK = false;
            }
        }
    }
    if( readOK ) {
        jau::INFO_PRINT("GATTHandler::pingGATT: No GENERIC_ACCESS Service with APPEARANCE Characteristic available -> disconnect");
    } else {
        jau::INFO_PRINT("GATTHandler::pingGATT: Read error -> disconnect");
    }
    disconnect(true /* disconnectDevice */, true /* ioErrorCause */); // state -> Disconnected
    return false;
}

std::shared_ptr<GattDeviceInformationSvc> BTGattHandler::getDeviceInformation(jau::darray<BTGattCharRef> & characteristicDeclList) {
    std::shared_ptr<GattDeviceInformationSvc> res = nullptr;
    jau::POctets value(number(Defaults::MAX_ATT_MTU), 0, jau::endian::little);

    jau::POctets systemID(8, 0, jau::endian::little);
    std::string modelNumber;
    std::string serialNumber;
    std::string firmwareRevision;
    std::string hardwareRevision;
    std::string softwareRevision;
    std::string manufacturer;
    jau::POctets regulatoryCertDataList(128, 0, jau::endian::little);
    std::shared_ptr<GattPnP_ID> pnpID = nullptr;
    bool found = false;

    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor

    for(size_t i=0; i<characteristicDeclList.size(); i++) {
        const BTGattChar & charDecl = *characteristicDeclList.at(i);
        std::shared_ptr<BTGattService> service = charDecl.getServiceUnchecked();
        if( nullptr == service || _DEVICE_INFORMATION != *(service->type) ) {
            continue;
        }
        found = true;
        if( _SYSTEM_ID == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, systemID.resize(0)) ) {
                // nop
            }
        } else if( _REGULATORY_CERT_DATA_LIST == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, regulatoryCertDataList.resize(0)) ) {
                // nop
            }
        } else if( _PNP_ID == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                pnpID = GattPnP_ID::get(value);
            }
        } else if( _MODEL_NUMBER_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                modelNumber = GattNameToString(value);
            }
        } else if( _SERIAL_NUMBER_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                serialNumber = GattNameToString(value);
            }
        } else if( _FIRMWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                firmwareRevision = GattNameToString(value);
            }
        } else if( _HARDWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                hardwareRevision = GattNameToString(value);
            }
        } else if( _SOFTWARE_REVISION_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
                softwareRevision = GattNameToString(value);
            }
        } else if( _MANUFACTURER_NAME_STRING == *charDecl.value_type ) {
            if( readCharacteristicValue(charDecl, value.resize(0)) ) {
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
    for(size_t i=0; i<primServices.size(); i++) {
        BTGattServiceRef service = primServices.at(i);
        if( _DEVICE_INFORMATION == *service->type ) {
            return getDeviceInformation(primServices.at(i)->characteristicList);
        }
    }
    return nullptr;
}

std::string BTGattHandler::toString() const noexcept {
    return "GattHndlr["+to_string(getRole())+", mtu "+std::to_string(usedMTU.load())+
           ", listener[BTGatt "+std::to_string(btGattCharListenerList.size())+", "+
           "Native "+std::to_string(nativeGattCharListenerList.size())+"], "+
           deviceString+", "+getStateString()+"]";
}
