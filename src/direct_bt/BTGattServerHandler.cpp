/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

class NopGattServerHandler : public BTGattHandler::GattServerHandler {
    private:
        void close_impl() noexcept {}

    public:
        NopGattServerHandler() noexcept = default;

        ~NopGattServerHandler() override { close_impl(); }

        void close() noexcept override { close_impl(); }

        DBGattServer::Mode getMode() noexcept override { return DBGattServer::Mode::NOP; }

        bool replyExchangeMTUReq(const AttExchangeMTU * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyReadReq(const AttPDUMsg * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyWriteReq(const AttPDUMsg * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyFindInfoReq(const AttFindInfoReq * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyFindByTypeValueReq(const AttFindByTypeValueReq * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyReadByTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            (void)pdu;
            return true;
        }

        bool replyReadByGroupTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            (void)pdu;
            return true;
        }
};

class DBGattServerHandler : public BTGattHandler::GattServerHandler {
    private:
        BTGattHandler& gh;
        DBGattServerRef gattServerData;

        jau::darray<AttPrepWrite> writeDataQueue;
        jau::darray<uint16_t> writeDataQueueHandles;

        void close_impl() noexcept {
            BTDeviceRef device = gh.getDeviceUnchecked();
            if( nullptr == device ) {
                ERR_PRINT("null device: %s", gh.toString().c_str());
            } else {
                int i=0;
                jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                    try {
                        l->disconnected(device);
                    } catch (std::exception &e) {
                        ERR_PRINT("%d/%zd: %s: Caught exception %s",
                                i+1, gattServerData->listener().size(),
                                gh.toString().c_str(), e.what());
                    }
                    i++;
                });
            }
            writeDataQueue.clear();
            writeDataQueueHandles.clear();
        }

    public:
        DBGattServerHandler(BTGattHandler& gh_, DBGattServerRef gsd) noexcept
        : gh(gh_), gattServerData(std::move(gsd)) {}

        ~DBGattServerHandler() override { close_impl(); }

        void close() noexcept override { close_impl(); }

    private:
        bool hasServerHandle(const uint16_t handle) noexcept {
            for(DBGattServiceRef& s : gattServerData->getServices()) {
                if( s->getHandle() <= handle && handle <= s->getEndHandle() ) {
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( c->getHandle() <= handle && handle <= c->getEndHandle() ) {
                            if( handle == c->getValueHandle() ) {
                                return true;
                            }
                            for(DBGattDescRef& d : c->getDescriptors()) {
                                if( handle == d->getHandle() ) {
                                    return true;
                                }
                            }
                        } // if characteristics-range
                    } // for characteristics
                } // if service-range
            } // for services
            return false;
        }

        DBGattCharRef findServerGattCharByValueHandle(const uint16_t char_value_handle) noexcept {
            return gattServerData->findGattCharByValueHandle(char_value_handle);
        }

        AttErrorRsp::ErrorCode applyWrite(BTDeviceRef device, const uint16_t handle, const jau::TROOctets & value, const uint16_t value_offset) noexcept {
            for(DBGattServiceRef& s : gattServerData->getServices()) {
                if( s->getHandle() <= handle && handle <= s->getEndHandle() ) {
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( c->getHandle() <= handle && handle <= c->getEndHandle() ) {
                            if( handle == c->getValueHandle() ) {
                                if( c->getValue().size() < value_offset) { // offset at value-end + 1 OK to append
                                    return AttErrorRsp::ErrorCode::INVALID_OFFSET;
                                }
                                if( c->hasVariableLength() ) {
                                    if( c->getValue().capacity() < value_offset + value.size() ) {
                                        return AttErrorRsp::ErrorCode::INVALID_ATTRIBUTE_VALUE_LEN;
                                    }
                                } else {
                                    if( c->getValue().size() < value_offset + value.size() ) {
                                        return AttErrorRsp::ErrorCode::INVALID_ATTRIBUTE_VALUE_LEN;
                                    }
                                }
                                {
                                    bool allowed = true;
                                    int i=0;
                                    jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                        try {
                                            allowed = l->writeCharValue(device, s, c, value, value_offset) && allowed;
                                        } catch (std::exception &e) {
                                            ERR_PRINT("GATT-REQ: WRITE: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                    c->toString().c_str(), i+1, gattServerData->listener().size(),
                                                    device->toString().c_str(), e.what());
                                        }
                                        i++;
                                    });
                                    if( !allowed ) {
                                        return AttErrorRsp::ErrorCode::NO_WRITE_PERM;
                                    }
                                }
                                if( c->hasVariableLength() ) {
                                    if( c->getValue().size() != value_offset + value.size() ) {
                                        c->getValue().resize( value_offset + value.size() );
                                    }
                                }
                                c->getValue().put_octets_nc(value_offset, value);
                                return AttErrorRsp::ErrorCode::NO_ERROR;
                            }
                            for(DBGattDescRef& d : c->getDescriptors()) {
                                if( handle == d->getHandle() ) {
                                    if( d->getValue().size() < value_offset) { // offset at value-end + 1 OK to append
                                        return AttErrorRsp::ErrorCode::INVALID_OFFSET;
                                    }
                                    if( d->hasVariableLength() ) {
                                        if( d->getValue().capacity() < value_offset + value.size() ) {
                                            return AttErrorRsp::ErrorCode::INVALID_ATTRIBUTE_VALUE_LEN;
                                        }
                                    } else {
                                        if( d->getValue().size() < value_offset + value.size() ) {
                                            return AttErrorRsp::ErrorCode::INVALID_ATTRIBUTE_VALUE_LEN;
                                        }
                                    }
                                    if( d->isUserDescription() ) {
                                        return AttErrorRsp::ErrorCode::NO_WRITE_PERM;
                                    }
                                    const bool isCCCD = d->isClientCharConfig();
                                    if( !isCCCD ) {
                                        bool allowed = true;
                                        int i=0;
                                        jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                            try {
                                                allowed = l->writeDescValue(device, s, c, d, value, value_offset) && allowed;
                                            } catch (std::exception &e) {
                                                ERR_PRINT("GATT-REQ: WRITE: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                        d->toString().c_str(), i+1, gattServerData->listener().size(),
                                                        device->toString().c_str(), e.what());
                                            }
                                            i++;
                                        });
                                        if( !allowed ) {
                                            return AttErrorRsp::ErrorCode::NO_WRITE_PERM;
                                        }
                                    }
                                    if( d->hasVariableLength() ) {
                                        if( d->getValue().size() != value_offset + value.size() ) {
                                            d->getValue().resize( value_offset + value.size() );
                                        }
                                    }
                                    if( isCCCD ) {
                                        if( value.size() == 0 ) {
                                            // no change, exit
                                            return AttErrorRsp::ErrorCode::NO_ERROR;
                                        }
                                        const uint8_t old_v = d->getValue().get_uint8_nc(0);
                                        const bool oldEnableNotification = old_v & 0b001;
                                        const bool oldEnableIndication = old_v & 0b010;

                                        const uint8_t req_v = value.get_uint8_nc(0);
                                        const bool reqEnableNotification = req_v & 0b001;
                                        const bool reqEnableIndication = req_v & 0b010;
                                        const bool hasNotification = c->hasProperties(BTGattChar::PropertyBitVal::Notify);
                                        const bool hasIndication = c->hasProperties(BTGattChar::PropertyBitVal::Indicate);
                                        const bool enableNotification = reqEnableNotification && hasNotification;
                                        const bool enableIndication = reqEnableIndication && hasIndication;

                                        if( oldEnableNotification == enableNotification &&
                                            oldEnableIndication == enableIndication ) {
                                            // no change, exit
                                            return AttErrorRsp::ErrorCode::NO_ERROR;
                                        }
                                        const uint16_t new_v = enableNotification | ( enableIndication << 1 );
                                        d->getValue().put_uint8_nc(0, new_v);
                                        {
                                            int i=0;
                                            jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                                try {
                                                    l->clientCharConfigChanged(device, s, c, d, enableNotification, enableIndication);
                                                } catch (std::exception &e) {
                                                    ERR_PRINT("GATT-REQ: WRITE CCCD: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                            d->toString().c_str(), i+1, gattServerData->listener().size(),
                                                            device->toString().c_str(), e.what());
                                                }
                                                i++;
                                            });
                                        }
                                    } else {
                                        // all other types ..
                                        d->getValue().put_octets_nc(value_offset, value);
                                    }
                                    return AttErrorRsp::ErrorCode::NO_ERROR;
                                }
                            }
                        } // if characteristics-range
                    } // for characteristics
                } // if service-range
            } // for services
            return AttErrorRsp::ErrorCode::INVALID_HANDLE;
        }

        void signalWriteDone(BTDeviceRef device, const uint16_t handle) noexcept {
            for(DBGattServiceRef& s : gattServerData->getServices()) {
                if( s->getHandle() <= handle && handle <= s->getEndHandle() ) {
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( c->getHandle() <= handle && handle <= c->getEndHandle() ) {
                            if( handle == c->getValueHandle() ) {
                                {
                                    int i=0;
                                    jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                        try {
                                            l->writeCharValueDone(device, s, c);
                                        } catch (std::exception &e) {
                                            ERR_PRINT("GATT-REQ: WRITE-Done: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                    c->toString().c_str(), i+1, gattServerData->listener().size(),
                                                    device->toString().c_str(), e.what());
                                        }
                                        i++;
                                    });
                                }
                                return;
                            }
                            for(DBGattDescRef& d : c->getDescriptors()) {
                                if( handle == d->getHandle() ) {
                                    if( d->isUserDescription() ) {
                                        return;
                                    }
                                    const bool isCCCD = d->isClientCharConfig();
                                    if( !isCCCD ) {
                                        int i=0;
                                        jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                            try {
                                                l->writeDescValueDone(device, s, c, d);
                                            } catch (std::exception &e) {
                                                ERR_PRINT("GATT-REQ: WRITE-Done: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                        d->toString().c_str(), i+1, gattServerData->listener().size(),
                                                        device->toString().c_str(), e.what());
                                            }
                                            i++;
                                        });
                                    }
                                    return;
                                }
                            }
                        } // if characteristics-range
                    } // for characteristics
                } // if service-range
            } // for services
            return;
        }


    public:
        DBGattServer::Mode getMode() noexcept override { return DBGattServer::Mode::DB; }

        bool replyExchangeMTUReq(const AttExchangeMTU * pdu) noexcept override {
            const uint16_t clientMTU = pdu->getMTUSize();
            gh.setUsedMTU( std::min(gh.getServerMTU(), clientMTU) );
            const AttExchangeMTU rsp(AttPDUMsg::ReqRespType::RESPONSE, gh.getUsedMTU());
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: MTU recv: %u, %s  -> %u %s from %s",
                    clientMTU, pdu->toString().c_str(),
                    gh.getUsedMTU(), rsp.toString().c_str(), gh.toString().c_str());
            if( nullptr != gattServerData ) {
                BTDeviceRef device = gh.getDeviceUnchecked();
                if( nullptr != device ) {
                    int i=0;
                    jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                        try {
                            l->mtuChanged(device, gh.getUsedMTU());
                        } catch (std::exception &e) {
                            ERR_PRINT("%d/%zd: %s: Caught exception %s",
                                    i+1, gattServerData->listener().size(),
                                    gh.toString().c_str(), e.what());
                        }
                        i++;
                    });
                }
            }
            return gh.send(rsp);
        }

        bool replyWriteReq(const AttPDUMsg * pdu) noexcept override {
            /**
             * Without Response:
             *   BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.3 ATT_WRITE_CMD
             *   BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value without Response
             *
             * With Response:
             *   BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.1 ATT_WRITE_REQ
             *   BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             *   BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             *
             *   BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.2 ATT_WRITE_RSP
             *   BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             */
            BTDeviceRef device = gh.getDeviceUnchecked();
            if( nullptr == device ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNLIKELY_ERROR, pdu->getOpcode(), 0);
                ERR_PRINT("GATT-Req: WRITE.0, null device: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }

            if( AttPDUMsg::Opcode::PREPARE_WRITE_REQ == pdu->getOpcode() ) {
                const AttPrepWrite * req = static_cast<const AttPrepWrite*>(pdu);
                if( !hasServerHandle( req->getHandle() ) ) {
                    AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, req->getOpcode(), req->getHandle());
                    WARN_PRINT("GATT-Req: WRITE.10: %s -> %s from %s", req->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                    return gh.send(err);
                }
                const uint16_t handle = req->getHandle();
                AttPrepWrite rsp(false, *req);
                writeDataQueue.push_back(rsp);
                if( writeDataQueueHandles.cend() == jau::find_if(writeDataQueueHandles.cbegin(), writeDataQueueHandles.cend(),
                                                                 [&](const uint16_t it)->bool { return handle == it; }) )
                {
                    // new entry
                    writeDataQueueHandles.push_back(handle);
                }
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.11: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                return gh.send(rsp);
            } else if( AttPDUMsg::Opcode::EXECUTE_WRITE_REQ == pdu->getOpcode() ) {
                const AttExeWriteReq * req = static_cast<const AttExeWriteReq*>(pdu);
                if( 0x01 == req->getFlags() ) { // immediately write all pending prepared values
                    AttErrorRsp::ErrorCode res = AttErrorRsp::ErrorCode::NO_ERROR;
                    for( auto iter_handle = writeDataQueueHandles.cbegin(); iter_handle < writeDataQueueHandles.cend(); ++iter_handle ) {
                        for( auto iter_prep_write = writeDataQueue.cbegin(); iter_prep_write < writeDataQueue.cend(); ++iter_prep_write ) {
                            const AttPrepWrite &p = *iter_prep_write;
                            const uint16_t handle = p.getHandle();
                            if( handle == *iter_handle ) {
                                const jau::TOctetSlice &p_value = p.getValue();
                                jau::TROOctets p_val(p_value.get_ptr_nc(0), p_value.size(), p_value.byte_order());
                                res = applyWrite(device, handle, p_val, p.getValueOffset());

                                if( AttErrorRsp::ErrorCode::NO_ERROR != res ) {
                                    writeDataQueue.clear();
                                    writeDataQueueHandles.clear();
                                    AttErrorRsp err(res, pdu->getOpcode(), handle);
                                    WARN_PRINT("GATT-Req: WRITE.12: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                                    return gh.send(err);
                                }
                            }
                        }
                    }
                    for( auto iter_handle = writeDataQueueHandles.cbegin(); iter_handle < writeDataQueueHandles.cend(); ++iter_handle ) {
                        signalWriteDone(device, *iter_handle);
                    }
                } // else 0x00 == req->getFlags() -> cancel all prepared writes
                writeDataQueue.clear();
                writeDataQueueHandles.clear();
                AttExeWriteRsp rsp;
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.13: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                return gh.send(rsp);
            }

            uint16_t handle = 0;
            const jau::TOctetSlice * vslice = nullptr;
            bool withResp;
            if( AttPDUMsg::Opcode::WRITE_REQ == pdu->getOpcode() ) {
                const AttWriteReq * req = static_cast<const AttWriteReq*>(pdu);
                handle = req->getHandle();
                vslice = &req->getValue();
                withResp = true;
            } else if( AttPDUMsg::Opcode::WRITE_CMD == pdu->getOpcode() ) {
                const AttWriteCmd * req = static_cast<const AttWriteCmd*>(pdu);
                handle = req->getHandle();
                vslice = &req->getValue();
                withResp = false;
            } else {
                // Actually an internal error, method should not have been called
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST, pdu->getOpcode(), 0);
                WARN_PRINT("GATT-Req: WRITE.20: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            jau::TROOctets req_val(vslice->get_ptr_nc(0), vslice->size(), vslice->byte_order());
            AttErrorRsp::ErrorCode res = applyWrite(device, handle, req_val, 0);
            if( AttErrorRsp::ErrorCode::NO_ERROR != res ) {
                AttErrorRsp err(res, pdu->getOpcode(), handle);
                WARN_PRINT("GATT-Req: WRITE.21: %s -> %s (sent %d) from %s",
                        pdu->toString().c_str(), err.toString().c_str(), (int)withResp, gh.toString().c_str());
                if( withResp ) {
                    return gh.send(err);
                }
                return true;;
            }
            if( withResp ) {
                AttWriteRsp rsp;
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.22: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                if( !gh.send(rsp) ) {
                    return false;
                }
            }
            signalWriteDone(device, handle);
            return true;
        }

        bool replyReadReq(const AttPDUMsg * pdu) noexcept override {
            /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value */
            /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value */
            /* For any follow up request, which previous request reply couldn't fit in ATT_MTU */
            BTDeviceRef device = gh.getDeviceUnchecked();
            if( nullptr == device ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNLIKELY_ERROR, pdu->getOpcode(), 0);
                ERR_PRINT("GATT-Req: READ, null device: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            uint16_t handle = 0;
            uint16_t value_offset = 0;
            bool isBlobReq;
            if( AttPDUMsg::Opcode::READ_REQ == pdu->getOpcode() ) {
                const AttReadReq * req = static_cast<const AttReadReq*>(pdu);
                handle = req->getHandle();
                isBlobReq = false;
            } else if( AttPDUMsg::Opcode::READ_BLOB_REQ == pdu->getOpcode() ) {
                /**
                 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
                 *
                 * If the Characteristic Value is not longer than (ATT_MTU – 1)
                 * an ATT_ERROR_RSP PDU with the error
                 * code set to Attribute Not Long shall be received on the first
                 * ATT_READ_BLOB_REQ PDU.
                 */
                const AttReadBlobReq * req = static_cast<const AttReadBlobReq*>(pdu);
                handle = req->getHandle();
                value_offset = req->getValueOffset();
                isBlobReq = true;
            } else {
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST, pdu->getOpcode(), 0);
                WARN_PRINT("GATT-Req: READ: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            if( 0 == handle ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), 0);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            const jau::nsize_t rspMaxSize = gh.getUsedMTU()-1;
            (void)rspMaxSize;

            for(DBGattServiceRef& s : gattServerData->getServices()) {
                if( s->getHandle() <= handle && handle <= s->getEndHandle() ) {
                    /**
                     * AttReadByGroupTypeRsp (1 opcode + 1 element_size + 2 handle + 2 handle + 16 uuid128_t = 22 bytes)
                     * always fits in minimum ATT_PDU 23
                     */
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( c->getHandle() <= handle && handle <= c->getEndHandle() ) {
                            if( handle == c->getValueHandle() ) {
                                if( isBlobReq ) {
#if SEND_ATTRIBUTE_NOT_LONG
                                    if( c->getValue().size() <= rspMaxSize ) {
                                        AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_LONG, pdu->getOpcode(), handle);
                                        COND_PRINT(env.DEBUG_DATA, "GATT-Req: READ.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), toString().c_str());
                                        gh.send(err);
                                        return;
                                    }
#endif
                                    if( value_offset > c->getValue().size() ) {
                                        AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_OFFSET, pdu->getOpcode(), handle);
                                        COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                                        return gh.send(err);
                                    }
                                }
                                {
                                    bool allowed = true;
                                    int i=0;
                                    jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                        try {
                                            allowed = l->readCharValue(device, s, c) && allowed;
                                        } catch (std::exception &e) {
                                            ERR_PRINT("GATT-REQ: READ: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                    c->toString().c_str(), i+1, gattServerData->listener().size(),
                                                    device->toString().c_str(), e.what());
                                        }
                                        i++;
                                    });
                                    if( !allowed ) {
                                        AttErrorRsp err(AttErrorRsp::ErrorCode::NO_READ_PERM, pdu->getOpcode(), handle);
                                        COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.2: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                                        return gh.send(err);
                                    }
                                }
                                AttReadNRsp rsp(isBlobReq, c->getValue(), value_offset); // Blob: value_size == value_offset -> OK, ends communication
                                if( rsp.getPDUValueSize() > rspMaxSize ) {
                                    rsp.pdu.resize(gh.getUsedMTU()); // requires another READ_BLOB_REQ
                                }
                                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.3: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                                return gh.send(rsp);
                            }
                            for(DBGattDescRef& d : c->getDescriptors()) {
                                if( handle == d->getHandle() ) {
                                    if( isBlobReq ) {
#if SEND_ATTRIBUTE_NOT_LONG
                                        if( isBlobReq && d->getValue().size() <= rspMaxSize ) {
                                            AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_LONG, pdu->getOpcode(), handle);
                                            COND_PRINT(env.DEBUG_DATA, "GATT-Req: READ.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), toString().c_str());
                                            gh.send(err);
                                            return;
                                        }
#endif
                                        if( value_offset > c->getValue().size() ) {
                                            AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_OFFSET, pdu->getOpcode(), handle);
                                            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                                            return gh.send(err);
                                        }
                                    }
                                    {
                                        bool allowed = true;
                                        int i=0;
                                        jau::for_each_fidelity(gattServerData->listener(), [&](DBGattServer::ListenerRef &l) {
                                            try {
                                                allowed = l->readDescValue(device, s, c, d) && allowed;
                                            } catch (std::exception &e) {
                                                ERR_PRINT("GATT-REQ: READ: (%s) %d/%zd: %s of %s: Caught exception %s",
                                                        d->toString().c_str(), i+1, gattServerData->listener().size(),
                                                        device->toString().c_str(), e.what());
                                            }
                                            i++;
                                        });
                                        if( !allowed ) {
                                            AttErrorRsp err(AttErrorRsp::ErrorCode::NO_READ_PERM, pdu->getOpcode(), handle);
                                            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.4: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                                            return gh.send(err);
                                        }
                                    }
                                    AttReadNRsp rsp(isBlobReq, d->getValue(), value_offset); // Blob: value_size == value_offset -> OK, ends communication
                                    if( rsp.getPDUValueSize() > rspMaxSize ) {
                                        rsp.pdu.resize(gh.getUsedMTU()); // requires another READ_BLOB_REQ
                                    }
                                    COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.5: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                                    return gh.send(rsp);
                                }
                            }
                        } // if characteristics-range
                    } // for characteristics
                } // if service-range
            } // for services
            AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), handle);
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ.6: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
            return gh.send(err);
        }

        bool replyFindInfoReq(const AttFindInfoReq * pdu) noexcept override {
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.1 ATT_FIND_INFORMATION_REQ
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.2 ATT_FIND_INFORMATION_RSP
            // BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
            if( 0 == pdu->getStartHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), 0);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            if( pdu->getStartHandle() > pdu->getEndHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            const uint16_t end_handle = pdu->getEndHandle();
            const uint16_t start_handle = pdu->getStartHandle();

            const jau::nsize_t rspMaxSize = std::min<jau::nsize_t>(255, gh.getUsedMTU()-2);
            AttFindInfoRsp rsp(gh.getUsedMTU()); // maximum size
            jau::nsize_t rspElemSize = 0;
            jau::nsize_t rspSize = 0;
            jau::nsize_t rspCount = 0;

            for(DBGattServiceRef& s : gattServerData->getServices()) {
                for(DBGattCharRef& c : s->getCharacteristics()) {
                    for(DBGattDescRef& d : c->getDescriptors()) {
                        if( start_handle <= d->getHandle() && d->getHandle() <= end_handle ) {
                            const jau::nsize_t size = 2 + d->getType()->getTypeSizeInt();
                            if( 0 == rspElemSize ) {
                                // initial setting or reset
                                rspElemSize = size;
                                rsp.setElementSize(rspElemSize);
                            }
                            if( rspSize + size > rspMaxSize || rspElemSize != size ) {
                                // send if rsp is full - or - element size changed
                                rsp.setElementCount(rspCount);
                                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO.2: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                                return gh.send(rsp); // Client shall issue additional FIND_INFORMATION_REQ
                            }
                            rsp.setElementHandle(rspCount, d->getHandle());
                            rsp.setElementValueUUID(rspCount, *d->getType());
                            rspSize += size;
                            ++rspCount;
                        }
                    }
                }
            }
            if( 0 < rspCount ) { // loop completed, elements added and all fitting in ATT_MTU
                rsp.setElementCount(rspCount);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO.3: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                return gh.send(rsp);
            }
            AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), start_handle);
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO.4: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
            return gh.send(err);
        }

        bool replyFindByTypeValueReq(const AttFindByTypeValueReq * pdu) noexcept override {
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.3 ATT_FIND_BY_TYPE_VALUE_REQ
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.4 ATT_FIND_BY_TYPE_VALUE_RSP
            // BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.2 Discover Primary Service by Service UUID
            if( 0 == pdu->getStartHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), 0);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            if( pdu->getStartHandle() > pdu->getEndHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            const jau::uuid16_t uuid_prim_service = jau::uuid16_t(GattAttributeType::PRIMARY_SERVICE);
            const jau::uuid16_t uuid_secd_service = jau::uuid16_t(GattAttributeType::SECONDARY_SERVICE);
            const uint16_t end_handle = pdu->getEndHandle();
            const uint16_t start_handle = pdu->getStartHandle();
            const jau::uuid16_t att_type = pdu->getAttType();
            std::unique_ptr<const jau::uuid_t> att_value = pdu->getAttValue();

            uint16_t req_group_type;
            if( att_type.equivalent( uuid_prim_service ) ) {
                req_group_type = GattAttributeType::PRIMARY_SERVICE;
            } else if( att_type.equivalent( uuid_secd_service ) ) {
                req_group_type = GattAttributeType::SECONDARY_SERVICE;
            } else {
                // not handled
                req_group_type = 0;
            }

            // const jau::nsize_t rspMaxSize = std::min<jau::nsize_t>(255, getUsedMTU()-2);
            AttFindByTypeValueRsp rsp(gh.getUsedMTU()); // maximum size
            // jau::nsize_t rspSize = 0;
            jau::nsize_t rspCount = 0;

            try {
                // const jau::nsize_t size = 2 + 2;

                for(DBGattServiceRef& s : gattServerData->getServices()) {
                    if( start_handle <= s->getHandle() && s->getHandle() <= end_handle ) {
                        if( ( ( GattAttributeType::PRIMARY_SERVICE   == req_group_type &&  s->isPrimary() ) ||
                            ( GattAttributeType::SECONDARY_SERVICE == req_group_type && !s->isPrimary() )
                            ) &&
                            s->getType()->equivalent(*att_value) )
                        {
                            rsp.setElementHandles(rspCount, s->getHandle(), s->getEndHandle());
                            // rspSize += size;
                            ++rspCount;
                            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE.4: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                            return gh.send(rsp); // done
                        }
                    }
                }
                if( 0 < rspCount ) { // loop completed, elements added and all fitting in ATT_MTU
                    rsp.setElementCount(rspCount);
                    COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE.5: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                    return gh.send(rsp);
                }
            } catch (const jau::ExceptionBase &e) {
                ERR_PRINT("invalid att uuid: %s", e.message().c_str());
            } catch (...) {
                ERR_PRINT("invalid att uuid: Unknown exception");
            }
            try {
                AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), start_handle);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE.6: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            } catch (const jau::ExceptionBase &e) {
                ERR_PRINT("invalid att uuid: %s", e.message().c_str());
            } catch (...) {
                ERR_PRINT("invalid att uuid: Unknown exception");
            }
            return false;
        }

        bool replyReadByTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.1 ATT_READ_BY_TYPE_REQ
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.2 ATT_READ_BY_TYPE_RSP
            if( 0 == pdu->getStartHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), 0);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            if( pdu->getStartHandle() > pdu->getEndHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            const jau::uuid16_t uuid_characteristic = jau::uuid16_t(GattAttributeType::CHARACTERISTIC);
            const jau::uuid16_t uuid_incl_service = jau::uuid16_t(GattAttributeType::INCLUDE_DECLARATION);
            std::unique_ptr<const jau::uuid_t> req_attribute = pdu->getNType();
            uint16_t req_type;
            if( req_attribute->equivalent( uuid_characteristic ) ) {
                // BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
                req_type = GattAttributeType::CHARACTERISTIC;
            } else if( req_attribute->equivalent( uuid_incl_service ) ) {
                req_type = GattAttributeType::INCLUDE_DECLARATION;
            } else {
                // BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.2 Read Using Characteristic UUID
                req_type = 0;
            }
            if( GattAttributeType::CHARACTERISTIC == req_type ) {
                // BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
                const uint16_t end_handle = pdu->getEndHandle();
                const uint16_t start_handle = pdu->getStartHandle();

                const jau::nsize_t rspMaxSize = std::min<jau::nsize_t>(255, gh.getUsedMTU()-2);
                // Attribute Handle and Attribute Value pairs corresponding to the Characteristic
                // - Attribute Handle is the handle for the Characteristic
                // - Attribute Value contains Properties, Value-Handle and UUID of the Characteristic
                AttReadByTypeRsp rsp(gh.getUsedMTU()); // maximum size
                jau::nsize_t rspElemSize = 0;
                jau::nsize_t rspSize = 0;
                jau::nsize_t rspCount = 0;

                for(DBGattServiceRef& s : gattServerData->getServices()) {
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( start_handle <= c->getHandle() && c->getHandle() <= end_handle ) {
                            const jau::nsize_t size = 2 + 1 + 2 + c->getValueType()->getTypeSizeInt();
                            if( 0 == rspElemSize ) {
                                // initial setting or reset
                                rspElemSize = size;
                                rsp.setElementSize(rspElemSize);
                            }
                            if( rspSize + size > rspMaxSize || rspElemSize != size ) {
                                // send if rsp is full - or - element size changed
                                rsp.setElementCount(rspCount);
                                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.2: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                                return gh.send(rsp); // Client shall issue additional READ_BY_TYPE_REQ
                            }
                            jau::nsize_t ePDUOffset = rsp.getElementPDUOffset(rspCount);
                            rsp.setElementHandle(rspCount, c->getHandle()); // Characteristic Handle
                            ePDUOffset += 2;
                            rsp.pdu.put_uint8_nc(ePDUOffset, c->getProperties()); // Characteristics Property
                            ePDUOffset += 1;
                            rsp.pdu.put_uint16_nc(ePDUOffset, c->getValueHandle()); // Characteristics Value Handle
                            ePDUOffset += 2;
                            c->getValueType()->put(rsp.pdu.get_wptr_nc(ePDUOffset) + 0, jau::lb_endian::little); // Characteristics Value Type UUID
                            ePDUOffset += c->getValueType()->getTypeSizeInt();
                            rspSize += size;
                            ++rspCount;
                            (void)ePDUOffset;
                        }
                    }
                }
                if( 0 < rspCount ) { // loop completed, elements added and all fitting in ATT_MTU
                    rsp.setElementCount(rspCount);
                    COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.3: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                    return gh.send(rsp);
                }
                AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.4: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            } else if( GattAttributeType::INCLUDE_DECLARATION == req_type ) {
                // TODO: Support INCLUDE_DECLARATION ??
                AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.5: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            } else { // TODO: Add other group types ???
                // BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.2 Read Using Characteristic UUID
                const uint16_t end_handle = pdu->getEndHandle();
                const uint16_t start_handle = pdu->getStartHandle();

                const jau::nsize_t rspMaxSize = std::min<jau::nsize_t>(255, gh.getUsedMTU()-2);
                // Attribute Handle and Attribute Value pairs corresponding to the Characteristic
                // - Attribute Handle is the handle for the Characteristic
                // - Attribute Value contains the value of the Characteristic
                AttReadByTypeRsp rsp(gh.getUsedMTU()); // maximum size

                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.6: Searching for %s, req %s from %s",
                        req_attribute->toString().c_str(), pdu->toString().c_str(), gh.toString().c_str());
                for(DBGattServiceRef& s : gattServerData->getServices()) {
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        if( start_handle <= c->getHandle() && c->getHandle() <= end_handle && c->getValueType()->equivalent(*req_attribute) ) {
                            jau::POctets& value = c->getValue();
                            const jau::nsize_t value_size_max = std::min(value.size(), rspMaxSize-2);
                            const jau::nsize_t size = 2 + value_size_max;
                            rsp.setElementSize(size);
                            jau::nsize_t ePDUOffset = rsp.getElementPDUOffset(0);
                            rsp.setElementHandle(0, c->getHandle()); // Characteristic Handle
                            ePDUOffset += 2;
                            rsp.pdu.put_bytes(ePDUOffset, value.get_ptr(), value_size_max);
                            (void)ePDUOffset;
                            rsp.setElementCount(1);
                            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.6: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                            return gh.send(rsp);
                        }
                    }
                }
                AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE.7: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
        }

        bool replyReadByGroupTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.9 ATT_READ_BY_GROUP_TYPE_REQ
            // BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.10 ATT_READ_BY_GROUP_TYPE_RSP
            // BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
            if( 0 == pdu->getStartHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), 0);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.0: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            if( pdu->getStartHandle() > pdu->getEndHandle() ) {
                AttErrorRsp err(AttErrorRsp::ErrorCode::INVALID_HANDLE, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.1: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
            const jau::uuid16_t uuid_prim_service = jau::uuid16_t(GattAttributeType::PRIMARY_SERVICE);
            const jau::uuid16_t uuid_secd_service = jau::uuid16_t(GattAttributeType::SECONDARY_SERVICE);
            std::unique_ptr<const jau::uuid_t> req_attribute_group = pdu->getNType();
            uint16_t req_group_type;
            if( req_attribute_group->equivalent( uuid_prim_service ) ) {
                req_group_type = GattAttributeType::PRIMARY_SERVICE;
            } else if( req_attribute_group->equivalent( uuid_secd_service ) ) {
                req_group_type = GattAttributeType::SECONDARY_SERVICE;
            } else {
                // not handled
                req_group_type = 0;
            }
            if( 0 != req_group_type ) {
                const uint16_t end_handle = pdu->getEndHandle();
                const uint16_t start_handle = pdu->getStartHandle();

                const jau::nsize_t rspMaxSize = std::min<jau::nsize_t>(255, gh.getUsedMTU()-2);
                AttReadByGroupTypeRsp rsp(gh.getUsedMTU()); // maximum size
                jau::nsize_t rspElemSize = 0;
                jau::nsize_t rspSize = 0;
                jau::nsize_t rspCount = 0;

                for(DBGattServiceRef& s : gattServerData->getServices()) {
                    if( ( ( GattAttributeType::PRIMARY_SERVICE   == req_group_type &&  s->isPrimary() ) ||
                          ( GattAttributeType::SECONDARY_SERVICE == req_group_type && !s->isPrimary() )
                        ) &&
                        start_handle <= s->getHandle() && s->getHandle() <= end_handle )
                    {
                        const jau::nsize_t size = 2 + 2 + s->getType()->getTypeSizeInt();
                        if( 0 == rspElemSize ) {
                            // initial setting or reset
                            rspElemSize = size;
                            rsp.setElementSize(rspElemSize);
                        }
                        if( rspSize + size > rspMaxSize || rspElemSize != size ) {
                            // send if rsp is full - or - element size changed
                            /**
                             * AttReadByGroupTypeRsp (1 opcode + 1 element_size + 2 handle + 2 handle + 16 uuid128_t = 22 bytes)
                             * always fits in minimum ATT_PDU 23
                             */
                            rsp.setElementCount(rspCount);
                            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.3: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                            return gh.send(rsp); // Client shall issue additional READ_BY_TYPE_REQ
                        }
                        rsp.setElementStartHandle(rspCount, s->getHandle());
                        rsp.setElementEndHandle(rspCount, s->getEndHandle());
                        rsp.setElementValueUUID(rspCount, *s->getType());
                        rspSize += size;
                        ++rspCount;
                    }
                }
                if( 0 < rspCount ) { // loop completed, elements added and all fitting in ATT_MTU
                    rsp.setElementCount(rspCount);
                    COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.4: %s -> %s from %s", pdu->toString().c_str(), rsp.toString().c_str(), gh.toString().c_str());
                    return gh.send(rsp);
                }
                AttErrorRsp err(AttErrorRsp::ErrorCode::ATTRIBUTE_NOT_FOUND, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.5: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            } else {
                // TODO: Add other group types ???
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNSUPPORTED_GROUP_TYPE, pdu->getOpcode(), pdu->getStartHandle());
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE.6: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                return gh.send(err);
            }
        }
};

class FwdGattServerHandler : public BTGattHandler::GattServerHandler {
    private:
        BTGattHandler& gh;
        BTDeviceRef fwdServer;
        std::shared_ptr<BTGattHandler> fwd_gh;

        jau::darray<AttPrepWrite> writeDataQueue;
        jau::darray<uint16_t> writeDataQueueHandles;

        void close_impl() noexcept {
            writeDataQueue.clear();
            writeDataQueueHandles.clear();
        }

    public:
        FwdGattServerHandler(BTGattHandler& gh_, BTDeviceRef fwdServer_) noexcept
        : gh(gh_), fwdServer(std::move(fwdServer_)) 
        {
            fwd_gh = fwdServer->getGattHandler();
        }

        ~FwdGattServerHandler() override { close_impl(); }

        void close() noexcept override { close_impl(); }

        DBGattServer::Mode getMode() noexcept override { return DBGattServer::Mode::FWD; }

        bool replyExchangeMTUReq(const AttExchangeMTU * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            const uint16_t clientMTU = pdu->getMTUSize();
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.write_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: MTU: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            if( AttPDUMsg::Opcode::EXCHANGE_MTU_RSP == rsp->getOpcode() ) {
                const AttExchangeMTU* mtuRsp = static_cast<const AttExchangeMTU*>( rsp.get() );
                const uint16_t serverMTU = mtuRsp->getMTUSize();
                gh.setUsedMTU( std::min(gh.getServerMTU(), std::min(clientMTU, serverMTU) ) );
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: MTU: %u -> %u -> %u", clientMTU, serverMTU, gh.getUsedMTU());
                fwd_gh->notifyNativeMTUResponse(clientMTU, *rsp, AttErrorRsp::ErrorCode::NO_ERROR, serverMTU, gh.getUsedMTU(), clientSource);
            } else {
                const AttErrorRsp::ErrorCode error_code = AttPDUMsg::Opcode::ERROR_RSP == rsp->getOpcode() ?
                        static_cast<const AttErrorRsp*>(rsp.get())->getErrorCode() : AttErrorRsp::ErrorCode::NO_ERROR;
                fwd_gh->notifyNativeMTUResponse(clientMTU, *rsp, error_code, 0, 0, clientSource);
            }
            return gh.send(*rsp);
        }

        bool replyWriteReq(const AttPDUMsg * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();

            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);

            if( AttPDUMsg::Opcode::PREPARE_WRITE_REQ == pdu->getOpcode() ) {
                {
                    const AttPrepWrite * req = static_cast<const AttPrepWrite*>(pdu);
                    const uint16_t handle = req->getHandle();
                    writeDataQueue.push_back(*req);
                    if( writeDataQueueHandles.cend() == jau::find_if(writeDataQueueHandles.cbegin(), writeDataQueueHandles.cend(),
                                                                     [&](const uint16_t it)->bool { return handle == it; }) )
                    {
                        // new entry
                        writeDataQueueHandles.push_back(handle);
                    }
                }
                std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.write_cmd_reply_timeout); // valid reply or exception
                if( nullptr == rsp ) {
                    ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                    return false;
                }
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.11: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
                fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
                {
                    const AttErrorRsp::ErrorCode error_code = AttPDUMsg::Opcode::ERROR_RSP == rsp->getOpcode() ?
                            static_cast<const AttErrorRsp*>(rsp.get())->getErrorCode() : AttErrorRsp::ErrorCode::NO_ERROR;
                    fwd_gh->notifyNativeWriteResponse(*rsp, error_code, clientSource);
                }
                return gh.send(*rsp);
            } else if( AttPDUMsg::Opcode::EXECUTE_WRITE_REQ == pdu->getOpcode() ) {
                {
                    const AttExeWriteReq * req = static_cast<const AttExeWriteReq*>(pdu);
                    if( 0x01 == req->getFlags() ) { // immediately write all pending prepared values
                        for( auto iter_handle = writeDataQueueHandles.cbegin(); iter_handle < writeDataQueueHandles.cend(); ++iter_handle ) {
                            const jau::lb_endian byte_order = writeDataQueue.size() > 0 ? writeDataQueue[0].getValue().byte_order() : jau::lb_endian::little;
                            jau::POctets data(256, 0, byte_order); // same byte order across all requests
                            BTGattHandler::NativeGattCharSections_t sections;
                            for( auto iter_prep_write = writeDataQueue.cbegin(); iter_prep_write < writeDataQueue.cend(); ++iter_prep_write ) {
                                const AttPrepWrite &p = *iter_prep_write;
                                const uint16_t handle = p.getHandle();
                                if( handle == *iter_handle ) {
                                    const jau::TOctetSlice &p_value = p.getValue();
                                    jau::TROOctets p_val(p_value.get_ptr_nc(0), p_value.size(), p_value.byte_order());
                                    const jau::nsize_t p_end = p.getValueOffset() + p_value.size();
                                    if( p_end > data.capacity() ) {
                                        data.recapacity(p_end);
                                    }
                                    if( p_end > data.size() ) {
                                        data.resize(p_end);
                                    }
                                    data.put_octets_nc(p.getValueOffset(), p_val);
                                    BTGattHandler::NativeGattCharListener::Section section(p.getValueOffset(), (uint16_t)p_end);
                                    if( sections.size() > 0 &&
                                        section.start >= sections[sections.size()-1].start &&
                                        section.start <= sections[sections.size()-1].end )
                                    {
                                        // quick merge of consecutive sections write requests
                                        if( section.end > sections[sections.size()-1].end ) {
                                            sections[sections.size()-1].end = section.end;
                                        } // else section lies within last section
                                    } else {
                                        sections.push_back(section);
                                    }
                                }
                                if( iter_prep_write + 1 == writeDataQueue.cend() ) {
                                    // last entry
                                    fwd_gh->notifyNativeWriteRequest(handle, data, sections, true /* with_response */, clientSource);
                                }
                            }
                        }
                    } // else 0x00 == req->getFlags() -> cancel all prepared writes
                    writeDataQueue.clear();
                    writeDataQueueHandles.clear();
                }
                std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.write_cmd_reply_timeout); // valid reply or exception
                if( nullptr == rsp ) {
                    ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                    return false;
                }
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.13: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
                fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
                {
                    const AttErrorRsp::ErrorCode error_code = AttPDUMsg::Opcode::ERROR_RSP == rsp->getOpcode() ?
                            static_cast<const AttErrorRsp*>(rsp.get())->getErrorCode() : AttErrorRsp::ErrorCode::NO_ERROR;
                    fwd_gh->notifyNativeWriteResponse(*rsp, error_code, clientSource);
                }
                return gh.send(*rsp);
            }

            if( AttPDUMsg::Opcode::WRITE_REQ == pdu->getOpcode() ) {
                {
                    const AttWriteReq &p = *static_cast<const AttWriteReq*>(pdu);
                    const jau::TOctetSlice &p_value = p.getValue();
                    BTGattHandler::NativeGattCharSections_t sections;
                    jau::TROOctets p_val(p_value.get_ptr_nc(0), p_value.size(), p_value.byte_order());
                    sections.emplace_back( 0, (uint16_t)p_value.size() );
                    fwd_gh->notifyNativeWriteRequest(p.getHandle(), p_val, sections, true /* with_response */, clientSource);
                }
                std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.write_cmd_reply_timeout); // valid reply or exception
                if( nullptr == rsp ) {
                    ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                    return false;
                }
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.22: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
                fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
                {
                    const AttErrorRsp::ErrorCode error_code = AttPDUMsg::Opcode::ERROR_RSP == rsp->getOpcode() ?
                            static_cast<const AttErrorRsp*>(rsp.get())->getErrorCode() : AttErrorRsp::ErrorCode::NO_ERROR;
                    fwd_gh->notifyNativeWriteResponse(*rsp, error_code, clientSource);
                }
                return gh.send(*rsp);
            } else if( AttPDUMsg::Opcode::WRITE_CMD == pdu->getOpcode() ) {
                {
                    const AttWriteCmd &p = *static_cast<const AttWriteCmd*>(pdu);
                    const jau::TOctetSlice &p_value = p.getValue();
                    BTGattHandler::NativeGattCharSections_t sections;
                    jau::TROOctets p_val(p_value.get_ptr_nc(0), p_value.size(), p_value.byte_order());
                    sections.emplace_back( 0, (uint16_t)p_value.size() );
                    fwd_gh->notifyNativeWriteRequest(p.getHandle(), p_val, sections, false /* with_response */, clientSource);
                }
                const bool res = fwd_gh->send(*pdu);
                COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: WRITE.21: res %d, %s to %s", res, pdu->toString().c_str(), fwd_gh->toString().c_str());
                return res;
            } else {
                // Actually an internal error, method should not have been called
                AttErrorRsp err(AttErrorRsp::ErrorCode::UNSUPPORTED_REQUEST, pdu->getOpcode(), 0);
                WARN_PRINT("GATT-Req: WRITE.20: %s -> %s from %s", pdu->toString().c_str(), err.toString().c_str(), gh.toString().c_str());
                fwd_gh->notifyNativeReplyReceived(err, clientSource);
                {
                    fwd_gh->notifyNativeWriteResponse(err, err.getErrorCode(), clientSource);
                }
                return gh.send(err);
            }
        }

        bool replyReadReq(const AttPDUMsg * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            uint16_t handle;
            uint16_t value_offset;
            {
                if( AttPDUMsg::Opcode::READ_REQ == pdu->getOpcode() ) {
                    const AttReadReq * req = static_cast<const AttReadReq*>(pdu);
                    handle = req->getHandle();
                    value_offset = 0;
                } else if( AttPDUMsg::Opcode::READ_BLOB_REQ == pdu->getOpcode() ) {
                    /**
                     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
                     *
                     * If the Characteristic Value is not longer than (ATT_MTU – 1)
                     * an ATT_ERROR_RSP PDU with the error
                     * code set to Attribute Not Long shall be received on the first
                     * ATT_READ_BLOB_REQ PDU.
                     */
                    const AttReadBlobReq * req = static_cast<const AttReadBlobReq*>(pdu);
                    handle = req->getHandle();
                    value_offset = req->getValueOffset();
                } else {
                    // Internal error
                    handle = 0;
                    value_offset = 0;
                }
            }
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.read_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: READ: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            {
                if( AttPDUMsg::Opcode::READ_RSP == rsp->getOpcode() ||
                    AttPDUMsg::Opcode::READ_BLOB_RSP == rsp->getOpcode() ) {
                    const AttReadNRsp * p = static_cast<const AttReadNRsp*>(rsp.get());
                    const jau::TOctetSlice& p_value = p->getValue();
                    jau::TROOctets p_val(p_value.get_ptr_nc(0), p_value.size(), p_value.byte_order());
                    fwd_gh->notifyNativeReadResponse(handle, value_offset, *rsp, AttErrorRsp::ErrorCode::NO_ERROR, p_val, clientSource);
                } else {
                    const AttErrorRsp::ErrorCode error_code = AttPDUMsg::Opcode::ERROR_RSP == rsp->getOpcode() ?
                            static_cast<const AttErrorRsp*>(rsp.get())->getErrorCode() : AttErrorRsp::ErrorCode::NO_ERROR;
                    jau::TROOctets p_val;
                    fwd_gh->notifyNativeReadResponse(handle, value_offset, *rsp, error_code, p_val, clientSource);
                }
            }
            return gh.send(*rsp);
        }

        bool replyFindInfoReq(const AttFindInfoReq * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.read_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: INFO: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            return gh.send(*rsp);
        }

        bool replyFindByTypeValueReq(const AttFindByTypeValueReq * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.read_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPEVALUE: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            return gh.send(*rsp);
        }

        bool replyReadByTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.read_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: TYPE: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            return gh.send(*rsp);
        }

        bool replyReadByGroupTypeReq(const AttReadByNTypeReq * pdu) noexcept override {
            if( !fwd_gh->isConnected() ) {
                close();
                return false;
            }
            BTDeviceRef clientSource = gh.getDeviceUnchecked();
            fwd_gh->notifyNativeRequestSent(*pdu, clientSource);
            std::unique_ptr<const AttPDUMsg> rsp = fwd_gh->sendWithReply(*pdu, gh.read_cmd_reply_timeout); // valid reply or exception
            if( nullptr == rsp ) {
                ERR_PRINT2("No reply; req %s from %s", pdu->toString().c_str(), fwd_gh->toString().c_str());
                return false;
            }
            COND_PRINT(gh.env.DEBUG_DATA, "GATT-Req: GROUP_TYPE: %s -> %s from %s", pdu->toString().c_str(), rsp->toString().c_str(), fwd_gh->toString().c_str());
            fwd_gh->notifyNativeReplyReceived(*rsp, clientSource);
            return gh.send(*rsp);
        }
};

std::unique_ptr<BTGattHandler::GattServerHandler> BTGattHandler::selectGattServerHandler(BTGattHandler& gh, const DBGattServerRef& gattServerData) noexcept {
    if( nullptr != gattServerData ) {
        switch( gattServerData->getMode() ) {
            case DBGattServer::Mode::DB: {
                if( gattServerData->getServices().size() > 0 ) {
                    return std::make_unique<DBGattServerHandler>(gh, gattServerData);
                }
                [[fallthrough]];
            }
            case DBGattServer::Mode::FWD: {
                BTDeviceRef fwdServer = gattServerData->getFwdServer();
                if( nullptr != fwdServer ) {
                    return std::make_unique<FwdGattServerHandler>(gh, fwdServer);
                }
                [[fallthrough]];
            }
            default:
                break;
        }
    }
    return std::make_unique<NopGattServerHandler>();
}

