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

#include <algorithm>

// #define PERF_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/environment.hpp>
#include <jau/basic_algos.hpp>
#include <jau/packed_attribute.hpp>

#include "BTIoctl.hpp"

#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "HCIHandler.hpp"
#include "BTTypes1.hpp"
#include "SMPHandler.hpp"
#include "DBTConst.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
    #include <signal.h>
    #ifdef __linux__
        #include <sys/ioctl.h>
    #endif
}

using namespace direct_bt;
using namespace jau::fractions_i64_literals;

HCIEnv::HCIEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.hci") ),
  HCI_READER_THREAD_POLL_TIMEOUT( jau::environment::getFractionProperty("direct_bt.hci.reader.timeout", 10_s, 1500_ms /* min */, 365_d /* max */) ),
  HCI_COMMAND_STATUS_REPLY_TIMEOUT( jau::environment::getFractionProperty("direct_bt.hci.cmd.status.timeout", 3_s, 1500_ms /* min */, 365_d /* max */) ),
  HCI_COMMAND_COMPLETE_REPLY_TIMEOUT( jau::environment::getFractionProperty("direct_bt.hci.cmd.complete.timeout", 10_s, 1500_ms /* min */, 365_d /* max */) ),
  HCI_COMMAND_POLL_PERIOD( jau::environment::getFractionProperty("direct_bt.hci.cmd.poll.period", 125_ms, 50_ms, 365_d) ),
  HCI_EVT_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.hci.ringsize", 64, 64 /* min */, 1024 /* max */) ),
  DEBUG_EVENT( jau::environment::getBooleanProperty("direct_bt.debug.hci.event", false) ),
  DEBUG_SCAN_AD_EIR( jau::environment::getBooleanProperty("direct_bt.debug.hci.scan_ad_eir", false) ),
  HCI_READ_PACKET_MAX_RETRY( HCI_EVT_RING_CAPACITY )
{
}

__pack( struct hci_rp_status {
    __u8    status;
} );

HCIHandler::HCIConnectionRef HCIHandler::setResolvHCIConnectionAddr(jau::darray<HCIConnectionRef> &list,
                                                                    const BDAddressAndType& visibleAddressAndType,
                                                                    const BDAddressAndType& addressAndType) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    auto end = list.end();
    for (auto it = list.begin(); it != end; ++it) {
        HCIConnectionRef conn = *it;
        if ( conn->equals(visibleAddressAndType) ) {
            conn->setResolvAddrAndType(addressAndType);
            return conn; // done
        }
    }
    return nullptr;
}

HCIHandler::HCIConnectionRef HCIHandler::addOrUpdateHCIConnection(jau::darray<HCIConnectionRef> &list,
                                                                  const BDAddressAndType& addressAndType, const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    auto end = list.end();
    for (auto it = list.begin(); it != end; ++it) {
        HCIConnectionRef conn = *it;
        if ( conn->equals(addressAndType) ) {
            // reuse same entry
            WORDY_PRINT("HCIHandler<%u>::addTrackerConnection: address%s, handle %s: reuse entry %s - %s",
               dev_id, addressAndType.toString().c_str(), jau::to_hexstring(handle).c_str(),
               conn->toString().c_str(), toString().c_str());
            // Overwrite tracked connection handle with given _valid_ handle only, i.e. non zero!
            if( 0 != handle ) {
                if( 0 != conn->getHandle() && handle != conn->getHandle() ) {
                    WARN_PRINT("address%s, handle %s: reusing entry %s, overwriting non-zero handle - %s",
                       addressAndType.toString().c_str(), jau::to_hexstring(handle).c_str(),
                       conn->toString().c_str(), toString().c_str());
                }
                conn->setHandle( handle );
            }
            return conn; // done
        }
    }
    try {
        HCIConnectionRef res( std::make_shared<HCIConnection>(addressAndType, handle) );
        list.push_back( res );
        return res;
    } catch (const std::bad_alloc &e) {
        ABORT("Error: bad_alloc: HCIConnectionRef allocation failed");
        return nullptr; // unreachable
    }
}

HCIHandler::HCIConnectionRef HCIHandler::findHCIConnection(jau::darray<HCIConnectionRef> &list, const BDAddressAndType& addressAndType) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    const jau::nsize_t size = list.size();
    for (jau::nsize_t i = 0; i < size; i++) {
        HCIConnectionRef & e = list[i];
        if( e->equals(addressAndType) ) {
            return e;
        }
    }
    return nullptr;
}

HCIHandler::HCIConnectionRef HCIHandler::findTrackerConnection(const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    const jau::nsize_t size = connectionList.size();
    for (jau::nsize_t i = 0; i < size; i++) {
        HCIConnectionRef & e = connectionList[i];
        if ( handle == e->getHandle() ) {
            return e;
        }
    }
    return nullptr;
}

HCIHandler::HCIConnectionRef HCIHandler::removeTrackerConnection(const HCIConnectionRef& conn) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    auto end = connectionList.end();
    for (auto it = connectionList.begin(); it != end; ++it) {
        HCIConnectionRef e = *it;
        if ( *e == *conn ) {
            connectionList.erase(it);
            return e; // done
        }
    }
    return nullptr;
}
HCIHandler::size_type HCIHandler::countPendingTrackerConnections() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    size_type count = 0;
    for (const auto& e : connectionList) {
        if ( e->getHandle() == 0 ) {
            count++;
        }
    }
    return count;
}
HCIHandler::size_type HCIHandler::getTrackerConnectionCount() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    return connectionList.size();
}
HCIHandler::HCIConnectionRef HCIHandler::removeHCIConnection(jau::darray<HCIConnectionRef> &list, const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    auto end = list.end();
    for (auto it = list.begin(); it != end; ++it) {
        HCIConnectionRef e = *it;
        if ( e->getHandle() == handle ) {
            list.erase(it);
            return e; // done
        }
    }
    return nullptr;
}

MgmtEvent::Opcode HCIHandler::translate(HCIEventType evt, HCIMetaEventType met) noexcept {
    if( HCIEventType::LE_META == evt ) {
        switch( met ) {
            case HCIMetaEventType::LE_CONN_COMPLETE:
                [[fallthrough]];
            case HCIMetaEventType::LE_EXT_CONN_COMPLETE:
                return MgmtEvent::Opcode::DEVICE_CONNECTED;

            default:
                return MgmtEvent::Opcode::INVALID;
        }
    }
    switch( evt ) {
        case HCIEventType::CONN_COMPLETE: return MgmtEvent::Opcode::DEVICE_CONNECTED;
        case HCIEventType::DISCONN_COMPLETE: return MgmtEvent::Opcode::DEVICE_DISCONNECTED;
        case HCIEventType::CMD_COMPLETE: return MgmtEvent::Opcode::CMD_COMPLETE;
        case HCIEventType::CMD_STATUS: return MgmtEvent::Opcode::CMD_STATUS;
        default: return MgmtEvent::Opcode::INVALID;
    }
}

std::unique_ptr<MgmtEvent> HCIHandler::translate(HCIEvent& ev) noexcept {
    const HCIEventType evt = ev.getEventType();
    const HCIMetaEventType mevt = ev.getMetaEventType();

    if( HCIEventType::LE_META == evt ) {
        switch( mevt ) {
            case HCIMetaEventType::LE_CONN_COMPLETE: {
                HCIStatusCode status;
                const hci_ev_le_conn_complete * ev_cc = getMetaReplyStruct<hci_ev_le_conn_complete>(ev, mevt, &status);
                if( nullptr == ev_cc ) {
                    ERR_PRINT("LE_CONN_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                    return nullptr;
                }
                const HCILEPeerAddressType hciAddrType = static_cast<HCILEPeerAddressType>(ev_cc->bdaddr_type);
                const BDAddressAndType addressAndType(jau::le_to_cpu(ev_cc->bdaddr), to_BDAddressType(hciAddrType));
                const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
                const HCIConnectionRef conn = addOrUpdateTrackerConnection(addressAndType, handle);
                if( HCIStatusCode::SUCCESS == status ) {
                    advertisingEnabled = false;
                    return std::make_unique<MgmtEvtDeviceConnected>(dev_id, addressAndType, handle);
                } else {
                    removeTrackerConnection(conn);
                    return std::make_unique<MgmtEvtDeviceConnectFailed>(dev_id, addressAndType, status);
                }
            }
            case HCIMetaEventType::LE_LTK_REQUEST: {
                const HCILELTKReqEvent & ev2 = *static_cast<const HCILELTKReqEvent*>( &ev );
                const HCIConnectionRef conn = findTrackerConnection(ev2.getHandle());
                if( nullptr == conn ) {
                    WARN_PRINT("dev_id %u: LE_LTK_REQUEST: Not tracked conn_handle of %s", dev_id, ev2.toString().c_str());
                    return nullptr;
                }
                return std::make_unique<MgmtEvtHCILELTKReq>(dev_id, conn->getAddressAndType(), ev2.getRand(), ev2.getEDIV());
            }
            case HCIMetaEventType::LE_EXT_CONN_COMPLETE: {
                HCIStatusCode status;
                const hci_ev_le_enh_conn_complete * ev_cc = getMetaReplyStruct<hci_ev_le_enh_conn_complete>(ev, mevt, &status);
                if( nullptr == ev_cc ) {
                    ERR_PRINT("LE_EXT_CONN_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                    return nullptr;
                }
                const HCILEPeerAddressType hciAddrType = static_cast<HCILEPeerAddressType>(ev_cc->bdaddr_type);
                const BDAddressAndType addressAndType(jau::le_to_cpu(ev_cc->bdaddr), to_BDAddressType(hciAddrType));
                const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
                const HCIConnectionRef conn = addOrUpdateTrackerConnection(addressAndType, handle);
                if( HCIStatusCode::SUCCESS == status ) {
                    advertisingEnabled = false;
                    return std::make_unique<MgmtEvtDeviceConnected>(dev_id, addressAndType, handle);
                } else {
                    removeTrackerConnection(conn);
                    return std::make_unique<MgmtEvtDeviceConnectFailed>(dev_id, addressAndType, status);
                }
            }
            case HCIMetaEventType::LE_REMOTE_FEAT_COMPLETE: {
                HCIStatusCode status;
                const hci_ev_le_remote_feat_complete * ev_cc = getMetaReplyStruct<hci_ev_le_remote_feat_complete>(ev, mevt, &status);
                if( nullptr == ev_cc ) {
                    ERR_PRINT("LE_REMOTE_FEAT_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                    return nullptr;
                }
                const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
                const LE_Features features = static_cast<LE_Features>(jau::get_uint64(ev_cc->features + 0, jau::lb_endian_t::little));
                const HCIConnectionRef conn = findTrackerConnection(handle);
                if( nullptr == conn ) {
                    WARN_PRINT("dev_id %u:: LE_REMOTE_FEAT_COMPLETE: Not tracked conn_handle %s of %s",
                            dev_id, jau::to_hexstring(handle).c_str(), ev.toString().c_str());
                    return nullptr;
                }
                return std::make_unique<MgmtEvtHCILERemoteFeatures>(dev_id, conn->getAddressAndType(), status, features);
            }
            case HCIMetaEventType::LE_PHY_UPDATE_COMPLETE: {
                HCIStatusCode status;
                struct le_phy_update_complete {
                    uint8_t  status;
                    uint16_t handle;
                    uint8_t  tx;
                    uint8_t  rx;
                } __packed;
                const le_phy_update_complete * ev_cc = getMetaReplyStruct<le_phy_update_complete>(ev, mevt, &status);
                if( nullptr == ev_cc ) {
                    ERR_PRINT("LE_PHY_UPDATE_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                    return nullptr;
                }
                const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
                const LE_PHYs Tx = static_cast<LE_PHYs>(ev_cc->tx);
                const LE_PHYs Rx = static_cast<LE_PHYs>(ev_cc->rx);
                const HCIConnectionRef conn = findTrackerConnection(handle);
                if( nullptr == conn ) {
                    WARN_PRINT("dev_id %u:: LE_PHY_UPDATE_COMPLETE: Not tracked conn_handle %s of %s",
                            dev_id, jau::to_hexstring(handle).c_str(), ev.toString().c_str());
                    return nullptr;
                }
                return std::make_unique<MgmtEvtHCILEPhyUpdateComplete>(dev_id, conn->getAddressAndType(), status, Tx, Rx);
            }
            default:
                return nullptr;
        }
    }
    switch( evt ) {
        case HCIEventType::CONN_COMPLETE: {
            HCIStatusCode status;
            const hci_ev_conn_complete * ev_cc = getReplyStruct<hci_ev_conn_complete>(ev, evt, &status);
            if( nullptr == ev_cc ) {
                ERR_PRINT("CONN_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                return nullptr;
            }
            const BDAddressAndType addressAndType(jau::le_to_cpu(ev_cc->bdaddr), BDAddressType::BDADDR_BREDR);
            HCIConnectionRef conn = addOrUpdateTrackerConnection(addressAndType, ev_cc->handle);
            if( HCIStatusCode::SUCCESS == status ) {
                advertisingEnabled = false;
                return std::make_unique<MgmtEvtDeviceConnected>(dev_id, conn->getAddressAndType(), conn->getHandle());
            } else {
                try {
                    std::unique_ptr<MgmtEvent> res( std::make_unique<MgmtEvtDeviceConnectFailed>(dev_id, conn->getAddressAndType(),status) );
                    removeTrackerConnection(conn);
                    return res;
                } catch (const std::bad_alloc &e) {
                    ABORT("Error: bad_alloc: MgmtEvtDeviceConnectFailedRef allocation failed");
                    return nullptr; // unreachable
                }
            }
        }
        case HCIEventType::DISCONN_COMPLETE: {
            HCIStatusCode status;
            const hci_ev_disconn_complete * ev_cc = getReplyStruct<hci_ev_disconn_complete>(ev, evt, &status);
            if( nullptr == ev_cc ) {
                ERR_PRINT("DISCONN_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                return nullptr;
            }
            removeDisconnectCmd(ev_cc->handle);
            HCIConnectionRef conn = removeTrackerConnection(ev_cc->handle);
            if( nullptr == conn ) {
                WORDY_PRINT("HCIHandler<%u>::translate(evt): DISCONN_COMPLETE: Not tracked handle %s: %s of %s",
                        dev_id, jau::to_hexstring(ev_cc->handle).c_str(), ev.toString().c_str(), toString().c_str());
                return nullptr;
            } else {
                if( HCIStatusCode::SUCCESS != status ) {
                    // FIXME: Ever occuring? Still sending out essential disconnect event!
                    ERR_PRINT("DISCONN_COMPLETE: !SUCCESS[%s, %s], %s: %s - %s",
                            jau::to_hexstring(static_cast<uint8_t>(status)).c_str(), to_string(status).c_str(),
                            conn->toString().c_str(), ev.toString().c_str(), toString().c_str());
                }
                const HCIStatusCode hciRootReason = static_cast<HCIStatusCode>(ev_cc->reason);
                return std::make_unique<MgmtEvtDeviceDisconnected>(dev_id, conn->getAddressAndType(), hciRootReason, conn->getHandle());
            }
        }
        case HCIEventType::ENCRYPT_CHANGE: {
            HCIStatusCode status;
            const hci_ev_encrypt_change * ev_cc = getReplyStruct<hci_ev_encrypt_change>(ev, evt, &status);
            if( nullptr == ev_cc ) {
                ERR_PRINT("ENCRYPT_CHANGE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                return nullptr;
            }
            const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
            const HCIConnectionRef conn = findTrackerConnection(handle);
            if( nullptr == conn ) {
                WARN_PRINT("dev_id %u:: ENCRYPT_CHANGE: Not tracked conn_handle %s of %s",
                        dev_id, jau::to_hexstring(handle).c_str(), ev.toString().c_str());
                return nullptr;
            }
            return std::make_unique<MgmtEvtHCIEncryptionChanged>(dev_id, conn->getAddressAndType(), status, ev_cc->encrypt);
        }
        case HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE: {
            HCIStatusCode status;
            const hci_ev_key_refresh_complete * ev_cc = getReplyStruct<hci_ev_key_refresh_complete>(ev, evt, &status);
            if( nullptr == ev_cc ) {
                ERR_PRINT("ENCRYPT_KEY_REFRESH_COMPLETE: Null reply-struct: %s - %s", ev.toString().c_str(), toString().c_str());
                return nullptr;
            }
            const uint16_t handle = jau::le_to_cpu(ev_cc->handle);
            const HCIConnectionRef conn = findTrackerConnection(handle);
            if( nullptr == conn ) {
                WARN_PRINT("dev_id %u:: ENCRYPT_KEY_REFRESH_COMPLETE: Not tracked conn_handle %s of %s",
                        dev_id, jau::to_hexstring(handle).c_str(), ev.toString().c_str());
                return nullptr;
            }
            return std::make_unique<MgmtEvtHCIEncryptionKeyRefreshComplete>(dev_id, conn->getAddressAndType(), status);
        }
        // TODO: AUTH_COMPLETE
        // 7.7.6 AUTH_COMPLETE 0x06

        default:
            return nullptr;
    }
}

std::unique_ptr<MgmtEvent> HCIHandler::translate(HCICommand& ev) noexcept {
    const HCIOpcode opc = ev.getOpcode();
    switch( opc ) {
        case HCIOpcode::LE_ENABLE_ENC: {
            const HCILEEnableEncryptionCmd & ev2 = *static_cast<const HCILEEnableEncryptionCmd*>( &ev );
            const HCIConnectionRef conn = findTrackerConnection(ev2.getHandle());
            if( nullptr == conn ) {
                WARN_PRINT("dev_id %u:: LE_ENABLE_ENC: Not tracked conn_handle %s", dev_id, ev2.toString().c_str());
                return nullptr;
            }
            return std::make_unique<MgmtEvtHCILEEnableEncryptionCmd>(dev_id, conn->getAddressAndType(),
                                                                     ev2.getRand(), ev2.getEDIV(), ev2.getLTK());
        }
        case HCIOpcode::LE_LTK_REPLY_ACK: {
            const HCILELTKReplyAckCmd & ev2 = *static_cast<const HCILELTKReplyAckCmd*>( &ev );
            const HCIConnectionRef conn = findTrackerConnection(ev2.getHandle());
            if( nullptr == conn ) {
                WARN_PRINT("dev_id %u:: LE_LTK_REPLY_ACK: Not tracked conn_handle %s", dev_id, ev2.toString().c_str());
                return nullptr;
            }
            return std::make_unique<MgmtEvtHCILELTKReplyAckCmd>(dev_id, conn->getAddressAndType(), ev2.getLTK());
        }
        case HCIOpcode::LE_LTK_REPLY_REJ: {
            const HCILELTKReplyRejCmd & ev2 = *static_cast<const HCILELTKReplyRejCmd*>( &ev );
            const HCIConnectionRef conn = findTrackerConnection(ev2.getHandle());
            if( nullptr == conn ) {
                WARN_PRINT("dev_id %u:: LE_LTK_REPLY_REJ: Not tracked conn_handle %s", dev_id, ev2.toString().c_str());
                return nullptr;
            }
            return std::make_unique<MgmtEvtHCILELTKReplyRejCmd>(dev_id, conn->getAddressAndType());
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<const SMPPDUMsg> HCIHandler::getSMPPDUMsg(const HCIACLData::l2cap_frame & l2cap, const uint8_t * l2cap_data) const noexcept {
    if( nullptr != l2cap_data && 0 < l2cap.len && l2cap.isSMP() ) {
        return SMPPDUMsg::getSpecialized(l2cap_data, l2cap.len);
    }
    return nullptr;
}

void HCIHandler::hciReaderWork(jau::service_runner& sr) noexcept {
    jau::snsize_t len;
    if( !isOpen() ) {
        // not open
        ERR_PRINT("Not connected %s", toString().c_str());
        sr.set_shall_stop();
        return;
    }

    len = comm.read(rbuffer.get_wptr(), rbuffer.size(), env.HCI_READER_THREAD_POLL_TIMEOUT);
    if( 0 < len ) {
        const jau::nsize_t len2 = static_cast<jau::nsize_t>(len);
        const HCIPacketType pc = static_cast<HCIPacketType>( rbuffer.get_uint8_nc(0) );

        // ACL
        if( HCIPacketType::ACLDATA == pc ) {
            std::unique_ptr<HCIACLData> acldata = HCIACLData::getSpecialized(rbuffer.get_ptr(), len2);
            if( nullptr == acldata ) {
                // not valid acl-data ...
                if( jau::environment::get().verbose ) {
                    WARN_PRINT("dev_id %u: IO RECV Drop ACL (non-acl-data) %s - %s",
                            dev_id, jau::bytesHexString(rbuffer.get_ptr(), 0, len2, true /* lsbFirst*/).c_str(), toString().c_str());
                }
                return;
            }
            const uint8_t* l2cap_data = nullptr; // owned by acldata
            HCIACLData::l2cap_frame l2cap = acldata->getL2CAPFrame(l2cap_data);
            std::unique_ptr<const SMPPDUMsg> smpPDU = getSMPPDUMsg(l2cap, l2cap_data);
            if( nullptr != smpPDU ) {
                HCIConnectionRef conn = findTrackerConnection(l2cap.handle);

                if( nullptr != conn ) {
                    COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV ACL (SMP) %s for %s",
                            dev_id, smpPDU->toString().c_str(), conn->toString().c_str());
                    jau::for_each_fidelity(hciSMPMsgCallbackList, [&](HCISMPMsgCallback &cb) {
                       cb(conn->getAddressAndType(), *smpPDU, l2cap);
                    });
                } else {
                    WARN_PRINT("dev_id %u: IO RECV ACL Drop (SMP): Not tracked conn_handle %s: %s, %s",
                            dev_id, jau::to_hexstring(l2cap.handle).c_str(),
                            l2cap.toString().c_str(), smpPDU->toString().c_str());
                }
            } else if( !l2cap.isGATT() ) { // ignore handled GATT packages
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV ACL Drop (L2CAP): ???? %s",
                        dev_id, acldata->toString(l2cap, l2cap_data).c_str());
            }
            return;
        }

        // COMMAND
        if( HCIPacketType::COMMAND == pc ) {
            std::unique_ptr<HCICommand> event = HCICommand::getSpecialized(rbuffer.get_ptr(), len2);
            if( nullptr == event ) {
                // not a valid event ...
                ERR_PRINT("IO RECV CMD Drop (non-command) %s - %s",
                        jau::bytesHexString(rbuffer.get_ptr(), 0, len2, true /* lsbFirst*/).c_str(), toString().c_str());
                return;
            }
            std::unique_ptr<MgmtEvent> mevent = translate(*event);
            if( nullptr != mevent ) {
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV CMD (CB) %s\n    -> %s", dev_id, event->toString().c_str(), mevent->toString().c_str());
                sendMgmtEvent( *mevent );
            } else {
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV CMD Drop (no translation) %s", dev_id, event->toString().c_str());
            }
            return;
        }

        if( HCIPacketType::EVENT != pc ) {
            WARN_PRINT("dev_id %u: IO RECV EVT Drop (not event, nor command, nor acl-data) %s - %s",
                    dev_id, jau::bytesHexString(rbuffer.get_ptr(), 0, len2, true /* lsbFirst*/).c_str(), toString().c_str());
            return;
        }

        // EVENT
        std::unique_ptr<HCIEvent> event = HCIEvent::getSpecialized(rbuffer.get_ptr(), len2);
        if( nullptr == event ) {
            // not a valid event ...
            ERR_PRINT("IO RECV EVT Drop (non-event) %s - %s",
                    jau::bytesHexString(rbuffer.get_ptr(), 0, len2, true /* lsbFirst*/).c_str(), toString().c_str());
            return;
        }

        const HCIMetaEventType mec = event->getMetaEventType();
        if( HCIMetaEventType::INVALID != mec && !filter_test_metaev(mec) ) {
            // DROP
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV EVT Drop (meta filter) %s", dev_id, event->toString().c_str());
            return; // next packet
        }

        if( event->isEvent(HCIEventType::CMD_STATUS) || event->isEvent(HCIEventType::CMD_COMPLETE) )
        {
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV EVT (CMD REPLY) %s", dev_id, event->toString().c_str());
            if( hciEventRing.isFull() ) {
                const jau::nsize_t dropCount = hciEventRing.capacity()/4;
                hciEventRing.drop(dropCount);
                WARN_PRINT("dev_id %u: IO RECV Drop (%u oldest elements of %u capacity, ring full) - %s",
                        dev_id, dropCount, hciEventRing.capacity(), toString().c_str());
            }
            if( !hciEventRing.putBlocking( std::move( event ), jau::fractions_i64::zero ) ) {
                ERR_PRINT2("hciEventRing put: %s", hciEventRing.toString().c_str());
                sr.set_shall_stop();
                return;
            }
        } else if( event->isMetaEvent(HCIMetaEventType::LE_ADVERTISING_REPORT) ) {
            // issue callbacks for the translated AD events
            jau::darray<std::unique_ptr<EInfoReport>> eirlist = EInfoReport::read_ad_reports(event->getParam(), event->getParamSize());
            for(jau::nsize_t eircount = 0; eircount < eirlist.size(); ++eircount) {
                const MgmtEvtDeviceFound e(dev_id, std::move( eirlist[eircount] ) );
                COND_PRINT(env.DEBUG_SCAN_AD_EIR, "HCIHandler<%u>-IO RECV EVT (AD EIR) [%d] %s",
                        dev_id, eircount, e.getEIR()->toString().c_str());
                sendMgmtEvent( e );
            }
        } else if( event->isMetaEvent(HCIMetaEventType::LE_EXT_ADV_REPORT) ) {
            // issue callbacks for the translated EAD events
            jau::darray<std::unique_ptr<EInfoReport>> eirlist = EInfoReport::read_ext_ad_reports(event->getParam(), event->getParamSize());
            for(jau::nsize_t eircount = 0; eircount < eirlist.size(); ++eircount) {
                const MgmtEvtDeviceFound e(dev_id, std::move( eirlist[eircount] ) );
                COND_PRINT(env.DEBUG_SCAN_AD_EIR, "HCIHandler<%u>-IO RECV EVT (EAD EIR (ext)) [%d] %s",
                        dev_id, eircount, e.getEIR()->toString().c_str());
                sendMgmtEvent( e );
            }
        } else {
            // issue a callback for the translated event
            std::unique_ptr<MgmtEvent> mevent = translate(*event);
            if( nullptr != mevent ) {
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV EVT (CB) %s\n    -> %s", dev_id, event->toString().c_str(), mevent->toString().c_str());
                sendMgmtEvent( *mevent );
            } else {
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV EVT Drop (no translation) %s", dev_id, event->toString().c_str());
            }
        }
    } else if( 0 > len && ETIMEDOUT != errno && !comm.interrupted() ) { // expected exits
        ERR_PRINT("HCIComm read: Error res %d, %s", len, toString().c_str());
        // Keep alive - sr.set_shall_stop();
    } else if( ETIMEDOUT != errno && !comm.interrupted() ) { // expected TIMEOUT if idle
        WORDY_PRINT("HCIHandler<%u>::reader: HCIComm read: IRQed res %d, %s", dev_id, len, toString().c_str());
    }
}

void HCIHandler::hciReaderEndLocked(jau::service_runner& sr) noexcept {
    (void)sr;
    WORDY_PRINT("HCIHandler<%u>::reader: Ended. Ring has %u entries flushed - %s", dev_id, hciEventRing.size(), toString().c_str());
    hciEventRing.clear();
}


void HCIHandler::sendMgmtEvent(const MgmtEvent& event) noexcept {
    MgmtEventCallbackList & mgmtEventCallbackList = mgmtEventCallbackLists[static_cast<uint16_t>(event.getOpcode())];
    int invokeCount = 0;

    jau::for_each_fidelity(mgmtEventCallbackList, [&](MgmtEventCallback &cb) {
        try {
            cb(event);
        } catch (std::exception &e) {
            ERR_PRINT("CBs %d/%zd: MgmtEventCallback %s : Caught exception %s - %s",
                    invokeCount+1, mgmtEventCallbackList.size(),
                    cb.toString().c_str(), e.what(), toString().c_str());
        }
        invokeCount++;
    });

    COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>::sendMgmtEvent: Event %s -> %d/%zd callbacks",
            dev_id, event.toString().c_str(), invokeCount, mgmtEventCallbackList.size());
    (void)invokeCount;
}

bool HCIHandler::sendCommand(HCICommand &req, const bool quiet) noexcept {
    COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO SENT %s", dev_id, req.toString().c_str());

    jau::TROOctets & pdu = req.getPDU();
    if ( comm.write( pdu.get_ptr(), pdu.size() ) < 0 ) {
        if( !quiet || jau::environment::get().verbose ) {
            ERR_PRINT("HCIComm write error, req %s - %s", req.toString().c_str(), toString().c_str());
        }
        return false;
    }
    return true;
}

std::unique_ptr<HCIEvent> HCIHandler::getNextReply(HCICommand &req, int32_t & retryCount, const jau::fraction_i64& replyTimeout) noexcept
{
    // Ringbuffer read is thread safe
    while( retryCount < env.HCI_READ_PACKET_MAX_RETRY ) {
        std::unique_ptr<HCIEvent> ev;
        if( !hciEventRing.getBlocking(ev, replyTimeout) || nullptr == ev ) {
            errno = ETIMEDOUT;
            ERR_PRINT("nullptr result (timeout %" PRIi64 " ms -> abort): req %s - %s", replyTimeout.to_ms(), req.toString().c_str(), toString().c_str());
            return nullptr;
        } else if( !ev->validate(req) ) {
            // This could occur due to an earlier timeout w/ a nullptr == res (see above),
            // i.e. the pending reply processed here and naturally not-matching.
            retryCount++;
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV getNextReply: res mismatch (drop, retry %d): res %s; req %s",
                       dev_id, retryCount, ev->toString().c_str(), req.toString().c_str());
        } else {
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler<%u>-IO RECV getNextReply: res %s; req %s", dev_id, ev->toString().c_str(), req.toString().c_str());
            return ev;
        }
    }
    return nullptr;
}

std::unique_ptr<HCIEvent> HCIHandler::getNextCmdCompleteReply(HCICommand &req, HCICommandCompleteEvent **res) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    *res = nullptr;

    int32_t retryCount = 0;
    std::unique_ptr<HCIEvent> ev = nullptr;

    while( retryCount < env.HCI_READ_PACKET_MAX_RETRY ) {
        ev = getNextReply(req, retryCount, env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT);
        if( nullptr == ev ) {
            break;  // timeout, leave loop
        } else if( ev->isEvent(HCIEventType::CMD_COMPLETE) ) {
            // gotcha, leave loop
            *res = static_cast<HCICommandCompleteEvent*>(ev.get());
            break;
        } else if( ev->isEvent(HCIEventType::CMD_STATUS) ) {
            // pending command .. wait for result
            HCICommandStatusEvent * ev_cs = static_cast<HCICommandStatusEvent*>(ev.get());
            HCIStatusCode status = ev_cs->getStatus();
            if( HCIStatusCode::SUCCESS != status ) {
                DBG_WARN_PRINT("dev_id %u: CMD_STATUS 0x%2.2X (%s), errno %d %s: res %s, req %s - %s",
                        dev_id, number(status), to_string(status).c_str(), errno, strerror(errno),
                        ev_cs->toString().c_str(), req.toString().c_str(), toString().c_str());
                break; // error status, leave loop
            } else {
                DBG_PRINT("HCIHandler<%u>::getNextCmdCompleteReply: CMD_STATUS 0x%2.2X (%s, retryCount %d), errno %d %s: res %s, req %s - %s",
                        dev_id, number(status), to_string(status).c_str(), retryCount, errno, strerror(errno),
                        ev_cs->toString().c_str(), req.toString().c_str(), toString().c_str());
            }
            retryCount++;
            continue; // next packet
        } else {
            retryCount++;
            DBG_PRINT("HCIHandler<%u>::getNextCmdCompleteReply: !(CMD_COMPLETE, CMD_STATUS) (drop, retry %d): res %s; req %s - %s",
                       dev_id, retryCount, ev->toString().c_str(), req.toString().c_str(), toString().c_str());
            continue; // next packet
        }
    }
    return ev;
}

HCIHandler::HCIHandler(const uint16_t dev_id_, const BTMode btMode_) noexcept
: env(HCIEnv::get()),
  dev_id(dev_id_),
  rbuffer(HCI_MAX_MTU, jau::lb_endian_t::little),
  comm(dev_id_, HCI_CHANNEL_RAW),
  hci_reader_service("HCIHandler::reader", THREAD_SHUTDOWN_TIMEOUT_MS,
                     jau::bind_member(this, &HCIHandler::hciReaderWork),
                     jau::service_runner::Callback() /* init */,
                     jau::bind_member(this, &HCIHandler::hciReaderEndLocked)),
  hciEventRing(env.HCI_EVT_RING_CAPACITY),
  le_ll_feats( LE_Features::NONE ),
  sup_commands_set( false ),
  allowClose( comm.is_open() ),
  btMode(btMode_),
  currentScanType(ScanType::NONE),
  advertisingEnabled(false)
{
    zeroSupCommands();

    WORDY_PRINT("HCIHandler<%u>.ctor: Start %s", dev_id, toString().c_str());
    if( !allowClose ) {
        ERR_PRINT("Could not open hci control channel %s", toString().c_str());
        return;
    }

    comm.set_interrupted_query( jau::bind_member(&hci_reader_service, &jau::service_runner::shall_stop2) );
    hci_reader_service.start();

    PERF_TS_T0();

#if 0
    {
        int opt = 1;
        if (setsockopt(comm.socket(), SOL_SOCKET, SO_TIMESTAMP, &opt, sizeof(opt)) < 0) {
            ERR_PRINT("setsockopt SO_TIMESTAMP %s", toString().c_str());
            goto fail;
        }

        if (setsockopt(comm.socket(), SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt)) < 0) {
            ERR_PRINT("setsockopt SO_PASSCRED %s", toString().c_str());
            goto fail;
        }
    }
#endif

#define FILTER_ALL_EVENTS 0

    // Mandatory socket filter (not adapter filter!)
    {
#if 0
        // No use for pre-existing hci_ufilter
        hci_ufilter of;
        socklen_t olen;

        olen = sizeof(of);
        if (getsockopt(comm.socket(), SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
            ERR_PRINT("getsockopt %s", toString().c_str());
            goto fail;
        }
#endif
        HCIComm::filter_clear(&filter_mask);
        if constexpr ( CONSIDER_HCI_CMD_FOR_SMP_STATE ) {
            // Currently only used to determine ENCRYPTION STATE, if at all.
            HCIComm::filter_set_ptype(number(HCIPacketType::COMMAND), &filter_mask); // COMMANDs
        }
        HCIComm::filter_set_ptype(number(HCIPacketType::EVENT),  &filter_mask); // EVENTs
        HCIComm::filter_set_ptype(number(HCIPacketType::ACLDATA),  &filter_mask); // SMP via ACL DATA

        // Setup generic filter mask for all events, this is also required for
#if FILTER_ALL_EVENTS
        HCIComm::filter_all_events(&filter_mask); // all events
#else
        HCIComm::filter_set_event(number(HCIEventType::CONN_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::DISCONN_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::AUTH_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::ENCRYPT_CHANGE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::CMD_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::CMD_STATUS), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::HARDWARE_ERROR), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE), &filter_mask);

        // HCIComm::filter_set_event(number(HCIEventType::IO_CAPABILITY_REQUEST), &filter_mask);
        // HCIComm::filter_set_event(number(HCIEventType::IO_CAPABILITY_RESPONSE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::LE_META), &filter_mask);
        // HCIComm::filter_set_event(number(HCIEventType::DISCONN_PHY_LINK_COMPLETE), &filter_mask);
        // HCIComm::filter_set_event(number(HCIEventType::DISCONN_LOGICAL_LINK_COMPLETE), &filter_mask);
#endif
        HCIComm::filter_set_opcode(0, &filter_mask); // all opcode

        if (setsockopt(comm.socket(), SOL_HCI, HCI_FILTER, &filter_mask, sizeof(filter_mask)) < 0) {
            ERR_PRINT("setsockopt HCI_FILTER %s", toString().c_str());
            goto fail;
        }
    }
    // Mandatory own LE_META filter
    {
        uint32_t mask = 0;
#if FILTER_ALL_EVENTS
        filter_all_metaevs(mask);
#else
        filter_set_metaev(HCIMetaEventType::LE_CONN_COMPLETE, mask);
        filter_set_metaev(HCIMetaEventType::LE_ADVERTISING_REPORT, mask);
        filter_set_metaev(HCIMetaEventType::LE_REMOTE_FEAT_COMPLETE, mask);
        filter_set_metaev(HCIMetaEventType::LE_LTK_REQUEST, mask);
        filter_set_metaev(HCIMetaEventType::LE_EXT_CONN_COMPLETE, mask);
        filter_set_metaev(HCIMetaEventType::LE_PHY_UPDATE_COMPLETE, mask);
        filter_set_metaev(HCIMetaEventType::LE_EXT_ADV_REPORT, mask);
        // filter_set_metaev(HCIMetaEventType::LE_CHANNEL_SEL_ALGO, mask);

#endif
        filter_put_metaevs(mask);
    }
    // Own HCIOpcodeBit/HCIOpcode filter (not functional yet!)
    {
        uint64_t mask = 0;
#if FILTER_ALL_EVENTS
        filter_all_opcbit(mask);
#else
        filter_set_opcbit(HCIOpcodeBit::CREATE_CONN, mask);
        filter_set_opcbit(HCIOpcodeBit::DISCONNECT, mask);
        // filter_set_opcbit(HCIOpcodeBit::IO_CAPABILITY_REQ_REPLY, mask);
        // filter_set_opcbit(HCIOpcodeBit::IO_CAPABILITY_REQ_NEG_REPLY, mask);
        filter_set_opcbit(HCIOpcodeBit::RESET, mask);
        filter_set_opcbit(HCIOpcodeBit::READ_LOCAL_VERSION, mask);
        filter_set_opcbit(HCIOpcodeBit::READ_LOCAL_COMMANDS, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_ADV_PARAM, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_ADV_DATA, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_SCAN_RSP_DATA, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_ADV_ENABLE, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_SCAN_PARAM, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_SCAN_ENABLE, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_CREATE_CONN, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_READ_REMOTE_FEATURES, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_ENABLE_ENC, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_LTK_REPLY_ACK, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_LTK_REPLY_REJ, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_READ_PHY, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_DEFAULT_PHY, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_PHY, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_ADV_PARAMS, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_ADV_DATA, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_SCAN_RSP_DATA, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_ADV_ENABLE, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_SCAN_PARAMS, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_EXT_SCAN_ENABLE, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_EXT_CREATE_CONN, mask);
#endif
        filter_put_opcbit(mask);
    }
    zeroSupCommands();

    PERF_TS_TD("HCIHandler::ctor.ok");
    WORDY_PRINT("HCIHandler.ctor: End OK - %s", toString().c_str());
    return;

fail:
    close();
    PERF_TS_TD("HCIHandler::ctor.fail");
    WORDY_PRINT("HCIHandler.ctor: End failure - %s", toString().c_str());
    return;
}

void HCIHandler::zeroSupCommands() noexcept {
    bzero(sup_commands, sizeof(sup_commands));
    sup_commands_set = false;
    le_ll_feats = LE_Features::NONE;
}
bool HCIHandler::initSupCommands() noexcept {
    // We avoid using a lock or an atomic-switch as we rely on sensible calls.
    if( !isOpen() ) {
        zeroSupCommands();
        return false;
    }
    HCIStatusCode status;

    le_ll_feats = LE_Features::NONE;
    {
        HCICommand req0(HCIOpcode::LE_READ_LOCAL_FEATURES, 0);
        const hci_rp_le_read_local_features * ev_lf;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_lf, &status, true /* quiet */);
        if( nullptr == ev || nullptr == ev_lf || HCIStatusCode::SUCCESS != status ) {
            DBG_PRINT("HCIHandler<%u>::initSupCommands: LE_READ_LOCAL_FEATURES: 0x%x (%s) - %s",
                    number(status), to_string(status).c_str(), toString().c_str());
            zeroSupCommands();
            return false;
        }
        le_ll_feats = static_cast<LE_Features>( jau::get_uint64(ev_lf->features + 0, jau::lb_endian_t::little) );
    }

    HCICommand req0(HCIOpcode::READ_LOCAL_COMMANDS, 0);
    const hci_rp_read_local_commands * ev_cmds;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_cmds, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_cmds || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("HCIHandler<%u>::initSupCommands: READ_LOCAL_COMMANDS: 0x%x (%s) - %s",
                dev_id, number(status), to_string(status).c_str(), toString().c_str());
        zeroSupCommands();
        return false;
    } else {
        memcpy(sup_commands, ev_cmds->commands, sizeof(sup_commands));
        sup_commands_set = true;
        return true;
    }
}

HCIStatusCode HCIHandler::check_open_connection(const std::string& caller,
                                                const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                                const bool addUntrackedConn) {
    if( !isOpen() ) {
        ERR_PRINT("%s: Not connected %s", caller.c_str(), toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    if( 0 == conn_handle ) {
        ERR_PRINT("%s: Null conn_handle given address%s (drop) - %s",
                caller.c_str(), peerAddressAndType.toString().c_str(), toString().c_str());
        return HCIStatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    HCIConnectionRef conn = findTrackerConnection(conn_handle);
    if( nullptr == conn ) {
        // called w/o being connected through this HCIHandler
        if( addUntrackedConn ) {
            // add unknown connection to tracker
            conn = addOrUpdateTrackerConnection(peerAddressAndType, conn_handle);
            WORDY_PRINT("HCIHandler::%s: Not tracked address%s, added %s - %s",
                       caller.c_str(), peerAddressAndType.toString().c_str(),
                       conn->toString().c_str(), toString().c_str());
        } else {
            ERR_PRINT("%s: Not tracked handle %s (address%s) (drop) - %s",
                       caller.c_str(), jau::to_hexstring(conn_handle).c_str(),
                       peerAddressAndType.toString().c_str(), toString().c_str());
            return HCIStatusCode::INVALID_HCI_COMMAND_PARAMETERS;
        }
    } else if( !conn->equals(peerAddressAndType) ) {
        ERR_PRINT("%s: Mismatch given address%s and tracked %s (drop) - %s",
                   caller.c_str(), peerAddressAndType.toString().c_str(),
                   conn->toString().c_str(), toString().c_str());
        return HCIStatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    }
    DBG_PRINT("HCIHandler<%u>::%s: address%s, handle %s, %s - %s",
               dev_id, caller.c_str(), peerAddressAndType.toString().c_str(),
               jau::to_hexstring(conn_handle).c_str(),
               conn->toString().c_str(), toString().c_str());

    return HCIStatusCode::SUCCESS;
}

HCIStatusCode HCIHandler::le_read_remote_features(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType) noexcept {
    HCIStatusCode status = check_open_connection("le_read_remote_features", conn_handle, peerAddressAndType);
    if( HCIStatusCode::SUCCESS != status ) {
        return status;
    }
    HCIStructCommand<hci_cp_le_read_remote_features> req0(HCIOpcode::LE_READ_REMOTE_FEATURES);
    hci_cp_le_read_remote_features * cp = req0.getWStruct();
    cp->handle = jau::cpu_to_le(conn_handle);
    std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);

    if( nullptr == ev || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("le_read_remote_features: LE_READ_PHY: 0x%x (%s) - %s",
                number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

void HCIHandler::close() noexcept {
    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowClose.compare_exchange_strong(expConn, false) ) {
        // not open
        const bool hci_service_stopped = hci_reader_service.join(); // [data] race: wait until disconnecting thread has stopped service
        comm.close();
        DBG_PRINT("HCIHandler<%u>::close: Not open: stopped %d, %s", dev_id, hci_service_stopped, toString().c_str());
        clearAllCallbacks();
        resetAllStates(false);
        comm.close();
        return;
    }
    PERF_TS_T0();
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler<%u>::close: Start %s", dev_id, toString().c_str());
    clearAllCallbacks();
    resetAllStates(false);

    PERF_TS_TD("HCIHandler::close.1");
    hci_reader_service.stop();
    comm.close();
    PERF_TS_TD("HCIHandler::close.X");

    DBG_PRINT("HCIHandler<%u>::close: End %s", dev_id, toString().c_str());
}

std::string HCIHandler::toString() const noexcept {
    return "HCIHandler["+std::to_string(dev_id)+", BTMode "+to_string(btMode)+", open "+std::to_string(isOpen())+
            ", adv "+std::to_string(advertisingEnabled)+", scan "+to_string(currentScanType)+
            ", ext[init "+std::to_string(sup_commands_set)+", adv "+std::to_string(use_ext_adv())+", scan "+std::to_string(use_ext_scan())+", conn "+std::to_string(use_ext_conn())+
            "], ring[entries "+std::to_string(hciEventRing.size())+"]]";
}

HCIStatusCode HCIHandler::startAdapter() {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    HCIStatusCode res = HCIStatusCode::INTERNAL_FAILURE;

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler<%u>::startAdapter.0: %s", dev_id, toString().c_str());

    #if defined(__linux__)
        int res_ioctl;
        if( ( res_ioctl = ioctl(comm.socket(), HCIDEVUP, dev_id) ) < 0 ) {
            if (errno != EALREADY) {
                ERR_PRINT("FAILED: %d - %s", res_ioctl, toString().c_str());
            } else {
                res = HCIStatusCode::SUCCESS;
            }
        } else {
            res = HCIStatusCode::SUCCESS;
        }
    #elif defined(__FreeBSD__)
        // #warning add implementation
        ABORT("add implementation for FreeBSD");
    #else
        #warning add implementation
        ABORT("add implementation");
    #endif
    if( HCIStatusCode::SUCCESS == res ) {
        res = resetAllStates(true) ? HCIStatusCode::SUCCESS : HCIStatusCode::FAILED;
    }
    DBG_PRINT("HCIHandler<%u>::startAdapter.X: %s - %s", dev_id, to_string(res).c_str(), toString().c_str());
    return res;
}

HCIStatusCode HCIHandler::stopAdapter() {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    HCIStatusCode res = HCIStatusCode::INTERNAL_FAILURE;

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler<%u>::stopAdapter.0: %s", dev_id, toString().c_str());

    #if defined(__linux__)
        int res_ioctl;
        if( ( res_ioctl = ioctl(comm.socket(), HCIDEVDOWN, dev_id) ) < 0) {
            ERR_PRINT("FAILED: %d - %s", res_ioctl, toString().c_str());
        } else {
            res = HCIStatusCode::SUCCESS;
        }
    #elif defined(__FreeBSD__)
        // #warning add implementation
        ABORT("add implementation for FreeBSD");
    #else
        #warning add implementation
        ABORT("add implementation");
    #endif
    if( HCIStatusCode::SUCCESS == res ) {
        resetAllStates(false);
    }
    DBG_PRINT("HCIHandler<%u>::stopAdapter.X: %s - %s", dev_id, to_string(res).c_str(), toString().c_str());
    return res;
}

HCIStatusCode HCIHandler::resetAdapter(const HCIHandler::PostShutdownFunc& user_post_shutdown) {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    HCIStatusCode res = HCIStatusCode::INTERNAL_FAILURE;
    bool user_called = false;
    bool user_abort = false;

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler<%u>::resetAdapter.0: %s", dev_id, toString().c_str());

    #if defined(__linux__)
        res = stopAdapter();
        if( HCIStatusCode::SUCCESS == res ) {
            if( nullptr != user_post_shutdown ) {
                user_called = true;
                res = user_post_shutdown();
                user_abort = HCIStatusCode::SUCCESS != res;
            }
            if( !user_abort ) {
                res = startAdapter();
            }
        }
    #elif defined(__FreeBSD__)
        // #warning add implementation
        (void)user_post_shutdown;
        ABORT("add implementation for FreeBSD");
    #else
        #warning add implementation
        (void)user_post_shutdown;
        ABORT("add implementation");
    #endif
    DBG_PRINT("HCIHandler<%u>::resetAdapter.X: %s user[called %d, abort %d] - %s", dev_id, to_string(res).c_str(), user_called, user_abort, toString().c_str());
    return res;
}

bool HCIHandler::resetAllStates(const bool powered_on) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    connectionList.clear();
    disconnectCmdList.clear();
    currentScanType = ScanType::NONE;
    advertisingEnabled = false;
    zeroSupCommands();
    if( powered_on ) {
        return initSupCommands();
    } else {
        return true;
    }
}

HCIStatusCode HCIHandler::resetHCI() noexcept {
    HCIStatusCode res = HCIStatusCode::INTERNAL_FAILURE;

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler<%u>::Reset HCI.0: %s", dev_id, toString().c_str());

    HCICommand req0(HCIOpcode::RESET, 0);

    const hci_rp_status * ev_status;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &res);
    if( nullptr == ev ) {
        res = HCIStatusCode::INTERNAL_TIMEOUT; // timeout
    }
    DBG_PRINT("HCIHandler<%u>::Reset HCI.X: %s - %s", dev_id, to_string(res).c_str(), toString().c_str());
    return res;
}

HCIStatusCode HCIHandler::getLocalVersion(HCILocalVersion &version) noexcept {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    HCICommand req0(HCIOpcode::READ_LOCAL_VERSION, 0);
    const hci_rp_read_local_version * ev_lv;
    HCIStatusCode status;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_lv, &status);
    if( nullptr == ev || nullptr == ev_lv || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("%s: 0x%x (%s) - %s",
                to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
        bzero(&version, sizeof(version));
    } else {
        version.hci_ver = ev_lv->hci_ver;
        version.hci_rev = jau::le_to_cpu(ev_lv->hci_rev);
        version.manufacturer = jau::le_to_cpu(ev_lv->manufacturer);
        version.lmp_ver = ev_lv->lmp_ver;
        version.lmp_subver = jau::le_to_cpu(ev_lv->lmp_subver);
    }
    return status;
}

HCIStatusCode HCIHandler::le_set_scan_param(const bool le_scan_active,
                                            const HCILEOwnAddressType own_mac_type,
                                            const uint16_t le_scan_interval, const uint16_t le_scan_window,
                                            const uint8_t filter_policy) noexcept {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    if( is_set(currentScanType, ScanType::LE) ) {
        WARN_PRINT("Not allowed: LE Scan Enabled: %s - tried scan [interval %.3f ms, window %.3f ms]",
                toString().c_str(), 0.625f * (float)le_scan_interval, 0.625f * (float)le_scan_window);
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    DBG_PRINT("HCIHandler<%u>::le_set_scan_param: scan [active %d, interval %.3f ms, window %.3f ms, filter %d] - %s",
            dev_id, le_scan_active,
            0.625f * (float)le_scan_interval, 0.625f * (float)le_scan_window,
            filter_policy, toString().c_str());

    HCIStatusCode status;
    if( use_ext_scan() ) {
        struct le_set_ext_scan_params {
            __u8    own_address_type;
            __u8    filter_policy;
            __u8    scanning_phys;
            hci_cp_le_scan_phy_params p1;
            // hci_cp_le_scan_phy_params p2;
        } __packed;
        HCIStructCommand<le_set_ext_scan_params> req0(HCIOpcode::LE_SET_EXT_SCAN_PARAMS);
        le_set_ext_scan_params * cp = req0.getWStruct();
        cp->own_address_type = static_cast<uint8_t>(own_mac_type);
        cp->filter_policy = filter_policy;
        cp->scanning_phys = direct_bt::number(LE_PHYs::LE_1M); // Only scan on LE_1M for compatibility

        cp->p1.type = le_scan_active ? LE_SCAN_ACTIVE : LE_SCAN_PASSIVE;
        cp->p1.interval = jau::cpu_to_le(le_scan_interval);
        cp->p1.window = jau::cpu_to_le(le_scan_window);
        // TODO: Support LE_1M + LE_CODED combo?

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
    } else {
        HCIStructCommand<hci_cp_le_set_scan_param> req0(HCIOpcode::LE_SET_SCAN_PARAM);
        hci_cp_le_set_scan_param * cp = req0.getWStruct();
        cp->type = le_scan_active ? LE_SCAN_ACTIVE : LE_SCAN_PASSIVE;
        cp->interval = jau::cpu_to_le(le_scan_interval);
        cp->window = jau::cpu_to_le(le_scan_window);
        cp->own_address_type = static_cast<uint8_t>(own_mac_type);
        cp->filter_policy = filter_policy;

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
    }
    return status;
}

HCIStatusCode HCIHandler::le_enable_scan(const bool enable, const bool filter_dup) noexcept {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    if( enable && advertisingEnabled ) {
        WARN_PRINT("dev_id %u: Not allowed: Advertising is enabled %s", dev_id, toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    ScanType nextScanType = changeScanType(currentScanType, ScanType::LE, enable);
    DBG_PRINT("HCIHandler<%u>::le_enable_scan: enable %s -> %s, filter_dup %d - %s",
            dev_id, to_string(currentScanType).c_str(), to_string(nextScanType).c_str(), filter_dup, toString().c_str());

    HCIStatusCode status;
    if( currentScanType != nextScanType ) {
        if( use_ext_scan() ) {
            HCIStructCommand<hci_cp_le_set_ext_scan_enable> req0(HCIOpcode::LE_SET_EXT_SCAN_ENABLE);
            hci_cp_le_set_ext_scan_enable * cp = req0.getWStruct();
            cp->enable = enable ? LE_SCAN_ENABLE : LE_SCAN_DISABLE;
            cp->filter_dup = filter_dup ? LE_SCAN_FILTER_DUP_ENABLE : LE_SCAN_FILTER_DUP_DISABLE;
            // cp->duration = 0; // until disabled
            // cp->period = 0; // until disabled
            const hci_rp_status * ev_status;
            std::unique_ptr<HCIEvent> evComplete = processCommandComplete(req0, &ev_status, &status);
        } else {
            HCIStructCommand<hci_cp_le_set_scan_enable> req0(HCIOpcode::LE_SET_SCAN_ENABLE);
            hci_cp_le_set_scan_enable * cp = req0.getWStruct();
            cp->enable = enable ? LE_SCAN_ENABLE : LE_SCAN_DISABLE;
            cp->filter_dup = filter_dup ? LE_SCAN_FILTER_DUP_ENABLE : LE_SCAN_FILTER_DUP_DISABLE;
            const hci_rp_status * ev_status;
            std::unique_ptr<HCIEvent> evComplete = processCommandComplete(req0, &ev_status, &status);
        }
    } else {
        status = HCIStatusCode::SUCCESS;
        WARN_PRINT("dev_id %u: current %s == next %s, OK, skip command - %s",
                dev_id, to_string(currentScanType).c_str(), to_string(nextScanType).c_str(), toString().c_str());
    }

    if( HCIStatusCode::SUCCESS == status ) {
        currentScanType = nextScanType;
        const MgmtEvtDiscovering e(dev_id, ScanType::LE, enable);
        sendMgmtEvent( e );
    }
    return status;
}

HCIStatusCode HCIHandler::le_start_scan(const bool filter_dup,
                                        const bool le_scan_active,
                                        const HCILEOwnAddressType own_mac_type,
                                        const uint16_t le_scan_interval, const uint16_t le_scan_window,
                                        const uint8_t filter_policy) noexcept {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    if( advertisingEnabled ) {
        WARN_PRINT("dev_id %u: Not allowed: Advertising is enabled %s", dev_id, toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    if( is_set(currentScanType, ScanType::LE) ) {
        WARN_PRINT("dev_id %u: Not allowed: LE Scan Enabled: %s", dev_id, toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    HCIStatusCode status = le_set_scan_param(le_scan_active, own_mac_type, le_scan_interval, le_scan_window, filter_policy);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("dev_id %u: le_set_scan_param failed: %s - %s",
                dev_id, to_string(status).c_str(), toString().c_str());
        return status;
    }
    status = le_enable_scan(true /* enable */, filter_dup);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("dev_id %u: le_enable_scan failed: %s - %s",
                dev_id, to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_create_conn(const EUI48 &peer_bdaddr,
                            const HCILEPeerAddressType peer_mac_type,
                            const HCILEOwnAddressType own_mac_type,
                            const uint16_t le_scan_interval, const uint16_t le_scan_window,
                            const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                            const uint16_t conn_latency, const uint16_t supervision_timeout) noexcept {
    /**
     * As we rely on consistent 'pending tracker connections',
     * i.e. avoid a race condition on issuing connections via this command,
     * we need to synchronize this method.
     */
    const std::lock_guard<std::mutex> lock(mtx_connect_cmd); // RAII-style acquire and relinquish via destructor

    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    if( advertisingEnabled ) {
        WARN_PRINT("Not allowed: Advertising is enabled %s", toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }

    const uint16_t min_ce_length = 0x0000;
    const uint16_t max_ce_length = 0x0000;
    const uint8_t initiator_filter = 0x00; // whitelist not used but peer_bdaddr*

    DBG_PRINT("HCIHandler<%u>::le_create_conn: scan [interval %.3f ms, window %.3f ms]", 0.625f *
            dev_id, (float)le_scan_interval, 0.625f * (float)le_scan_window);
    DBG_PRINT("HCIHandler<%u>::le_create_conn: conn [interval [%.3f ms - %.3f ms], latency %d, sup_timeout %d ms] - %s",
            dev_id, 1.25f * (float)conn_interval_min, 1.25f * (float)conn_interval_max,
            conn_latency, supervision_timeout*10, toString().c_str());

    size_type pendingConnections = countPendingTrackerConnections();
    if( 0 < pendingConnections ) {
        DBG_PRINT("HCIHandler<%u>::le_create_conn: %zu connections pending - %s", dev_id, (size_t)pendingConnections, toString().c_str());
        jau::fraction_i64 td = 0_s;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && 0 < pendingConnections ) {
            sleep_for(env.HCI_COMMAND_POLL_PERIOD);
            td += env.HCI_COMMAND_POLL_PERIOD;
            pendingConnections = countPendingTrackerConnections();
        }
        if( 0 < pendingConnections ) {
            WARN_PRINT("%zu connections pending after %" PRIi64 " ms - %s", (size_t)pendingConnections, td.to_ms(), toString().c_str());
        } else {
            DBG_PRINT("HCIHandler<%u>::le_create_conn: pending connections resolved after %" PRIi64 " ms - %s", dev_id, td.to_ms(), toString().c_str());
        }
    }
    const BDAddressAndType addressAndType(peer_bdaddr, to_BDAddressType(peer_mac_type));
    HCIConnectionRef disconn = findDisconnectCmd(addressAndType);
    if( nullptr != disconn ) {
        DBG_PRINT("HCIHandler<%u>::le_create_conn: disconnect pending %s - %s",
                dev_id, disconn->toString().c_str(), toString().c_str());
        jau::fraction_i64 td = 0_s;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && nullptr != disconn ) {
            sleep_for(env.HCI_COMMAND_POLL_PERIOD);
            td += env.HCI_COMMAND_POLL_PERIOD;
            disconn = findDisconnectCmd(addressAndType);
        }
        if( nullptr != disconn ) {
            WARN_PRINT("disconnect persisting after %" PRIi64 " ms: %s - %s",
                    td.to_ms(), disconn->toString().c_str(), toString().c_str());
        } else {
            DBG_PRINT("HCIHandler<%u>::le_create_conn: disconnect resolved after %" PRIi64 " ms - %s", dev_id, td.to_ms(), toString().c_str());
        }
    }
    HCIConnectionRef conn = addOrUpdateTrackerConnection(addressAndType, 0);
    HCIStatusCode status;

    if( use_ext_conn() ) {
        struct le_ext_create_conn {
            __u8      filter_policy;
            __u8      own_address_type;
            __u8      peer_addr_type;
            bdaddr_t  peer_addr;
            __u8      phys;
            hci_cp_le_ext_conn_param p1;
            // hci_cp_le_ext_conn_param p2;
            // hci_cp_le_ext_conn_param p3;
        } __packed;
        HCIStructCommand<le_ext_create_conn> req0(HCIOpcode::LE_EXT_CREATE_CONN);
        le_ext_create_conn * cp = req0.getWStruct();
        cp->filter_policy = initiator_filter;
        cp->own_address_type = static_cast<uint8_t>(own_mac_type);
        cp->peer_addr_type = static_cast<uint8_t>(peer_mac_type);
        cp->peer_addr = jau::cpu_to_le(peer_bdaddr);
        cp->phys = direct_bt::number(LE_PHYs::LE_1M); // Only scan on LE_1M for compatibility

        cp->p1.scan_interval = jau::cpu_to_le(le_scan_interval);
        cp->p1.scan_window = jau::cpu_to_le(le_scan_window);
        cp->p1.conn_interval_min = jau::cpu_to_le(conn_interval_min);
        cp->p1.conn_interval_max = jau::cpu_to_le(conn_interval_max);
        cp->p1.conn_latency = jau::cpu_to_le(conn_latency);
        cp->p1.supervision_timeout = jau::cpu_to_le(supervision_timeout);
        cp->p1.min_ce_len = jau::cpu_to_le(min_ce_length);
        cp->p1.max_ce_len = jau::cpu_to_le(max_ce_length);
        // TODO: Support some PHYs combo settings?

        std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
        // Events on successful connection:
        // - HCI_LE_Enhanced_Connection_Complete
        // - HCI_LE_Channel_Selection_Algorithm
    } else {
        HCIStructCommand<hci_cp_le_create_conn> req0(HCIOpcode::LE_CREATE_CONN);
        hci_cp_le_create_conn * cp = req0.getWStruct();
        cp->scan_interval = jau::cpu_to_le(le_scan_interval);
        cp->scan_window = jau::cpu_to_le(le_scan_window);
        cp->filter_policy = initiator_filter;
        cp->peer_addr_type = static_cast<uint8_t>(peer_mac_type);
        cp->peer_addr = jau::cpu_to_le(peer_bdaddr);
        cp->own_address_type = static_cast<uint8_t>(own_mac_type);
        cp->conn_interval_min = jau::cpu_to_le(conn_interval_min);
        cp->conn_interval_max = jau::cpu_to_le(conn_interval_max);
        cp->conn_latency = jau::cpu_to_le(conn_latency);
        cp->supervision_timeout = jau::cpu_to_le(supervision_timeout);
        cp->min_ce_len = jau::cpu_to_le(min_ce_length);
        cp->max_ce_len = jau::cpu_to_le(max_ce_length);

        std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
        // Events on successful connection:
        // - HCI_LE_Connection_Complete
    }
    if( HCIStatusCode::SUCCESS != status ) {
        removeTrackerConnection(conn);

        if( HCIStatusCode::CONNECTION_ALREADY_EXISTS == status ) {
            const std::string s0 = nullptr != disconn ? disconn->toString() : "null";
            WARN_PRINT("%s: disconnect pending: %s - %s",
                    to_string(status).c_str(), s0.c_str(), toString().c_str());
        }
    }
    return status;
}

HCIStatusCode HCIHandler::create_conn(const EUI48 &bdaddr,
                                     const uint16_t pkt_type,
                                     const uint16_t clock_offset, const uint8_t role_switch) noexcept {
    /**
     * As we rely on consistent 'pending tracker connections',
     * i.e. avoid a race condition on issuing connections via this command,
     * we need to synchronize this method.
     */
    const std::lock_guard<std::mutex> lock(mtx_connect_cmd); // RAII-style acquire and relinquish via destructor

    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    if( advertisingEnabled ) {
        WARN_PRINT("Not allowed: Advertising is enabled %s", toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }

    HCIStructCommand<hci_cp_create_conn> req0(HCIOpcode::CREATE_CONN);
    hci_cp_create_conn * cp = req0.getWStruct();
    cp->bdaddr = jau::cpu_to_le(bdaddr);
    cp->pkt_type = jau::cpu_to_le((uint16_t)(pkt_type & (uint16_t)ACL_PTYPE_MASK)); /* TODO OK excluding SCO_PTYPE_MASK   (HCI_HV1 | HCI_HV2 | HCI_HV3) ? */
    cp->pscan_rep_mode = 0x02; /* TODO magic? */
    cp->pscan_mode = 0x00; /* TODO magic? */
    cp->clock_offset = jau::cpu_to_le(clock_offset);
    cp->role_switch = role_switch;

    size_type pendingConnections = countPendingTrackerConnections();
    if( 0 < pendingConnections ) {
        DBG_PRINT("HCIHandler<%u>::create_conn: %zu connections pending - %s", dev_id, (size_t)pendingConnections, toString().c_str());
        jau::fraction_i64 td = 0_s;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && 0 < pendingConnections ) {
            sleep_for(env.HCI_COMMAND_POLL_PERIOD);
            td += env.HCI_COMMAND_POLL_PERIOD;
            pendingConnections = countPendingTrackerConnections();
        }
        if( 0 < pendingConnections ) {
            WARN_PRINT("%zu connections pending after %" PRIi64 " ms - %s", (size_t)pendingConnections, td.to_ms(), toString().c_str());
        } else {
            DBG_PRINT("HCIHandler<%u>::create_conn: pending connections resolved after %" PRIi64 " ms - %s", dev_id, td.to_ms(), toString().c_str());
        }
    }
    const BDAddressAndType addressAndType(bdaddr, BDAddressType::BDADDR_BREDR);
    HCIConnectionRef disconn = findDisconnectCmd(addressAndType);
    if( nullptr != disconn ) {
        DBG_PRINT("HCIHandler<%u>::create_conn: disconnect pending %s - %s",
                dev_id, disconn->toString().c_str(), toString().c_str());
        jau::fraction_i64 td = 0_s;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && nullptr != disconn ) {
            sleep_for(env.HCI_COMMAND_POLL_PERIOD);
            td += env.HCI_COMMAND_POLL_PERIOD;
            disconn = findDisconnectCmd(addressAndType);
        }
        if( nullptr != disconn ) {
            WARN_PRINT("disconnect persisting after %" PRIi64 " ms: %s - %s",
                    td.to_ms(), disconn->toString().c_str(), toString().c_str());
        } else {
            DBG_PRINT("HCIHandler<%u>::create_conn: disconnect resolved after %" PRIi64 " ms - %s", dev_id, td.to_ms(), toString().c_str());
        }
    }
    HCIConnectionRef conn = addOrUpdateTrackerConnection(addressAndType, 0);
    HCIStatusCode status;
    std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
    if( HCIStatusCode::SUCCESS != status ) {
        removeTrackerConnection(conn);

        if( HCIStatusCode::CONNECTION_ALREADY_EXISTS == status ) {
            const std::string s0 = nullptr != disconn ? disconn->toString() : "null";
            WARN_PRINT("%s: disconnect pending: %s - %s",
                    to_string(status).c_str(), s0.c_str(), toString().c_str());
        }
    }
    return status;
}

HCIStatusCode HCIHandler::disconnect(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                     const HCIStatusCode reason) noexcept
{
    HCIStatusCode status = check_open_connection("disconnect", conn_handle, peerAddressAndType, true /* addUntrackedConn */);
    if( HCIStatusCode::SUCCESS != status ) {
        return status;
    }

    // Always issue DISCONNECT command, even in case of an ioError (lost-connection),
    // see Issue #124 fast re-connect on CSR adapter.
    // This will always notify the adapter of a disconnected device.
    {
        HCIStructCommand<hci_cp_disconnect> req0(HCIOpcode::DISCONNECT);
        hci_cp_disconnect * cp = req0.getWStruct();
        cp->handle = jau::cpu_to_le(conn_handle);
        cp->reason = number(reason);

        std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
    }
    if( HCIStatusCode::SUCCESS == status ) {
        addOrUpdateDisconnectCmd(peerAddressAndType, conn_handle);
    }
    return status;
}

HCIStatusCode HCIHandler::le_add_to_resolv_list(const BDAddressAndType& peerIdentityAddressAndType,
                                    jau::uint128dp_t& peer_irk, jau::uint128dp_t& local_irk) noexcept {
    if( !use_resolv_add() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_add_to_resolv_list> req0(HCIOpcode::LE_ADD_TO_RESOLV_LIST);
    hci_cp_le_add_to_resolv_list * cp = req0.getWStruct();
    cp->bdaddr_type = static_cast<uint8_t>(peerIdentityAddressAndType.type);
    cp->bdaddr      = jau::cpu_to_le(peerIdentityAddressAndType.address);
    jau::put_uint128(cp->peer_irk + 0, peer_irk, jau::lb_endian_t::little);
    jau::put_uint128(cp->local_irk + 0, local_irk, jau::lb_endian_t::little);
    const hci_rp_status * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_del_from_resolv_list(const BDAddressAndType& peerIdentityAddressAndType) noexcept {
    if( !use_resolv_del() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_del_from_resolv_list> req0(HCIOpcode::LE_DEL_FROM_RESOLV_LIST);
    hci_cp_le_del_from_resolv_list * cp = req0.getWStruct();
    cp->bdaddr_type = static_cast<uint8_t>(peerIdentityAddressAndType.type);
    cp->bdaddr      = jau::cpu_to_le(peerIdentityAddressAndType.address);
    const hci_rp_status * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_clear_resolv_list() noexcept {
    if( !use_resolv_clear() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    HCIStatusCode status;
    HCICommand req0(HCIOpcode::LE_CLEAR_RESOLV_LIST, 0);
    const hci_rp_status * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_read_resolv_list_size(uint32_t& size_res) noexcept {
    if( !use_resolv_size() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    size_res = 0;
    HCIStatusCode status;
    HCICommand req0(HCIOpcode::LE_READ_RESOLV_LIST_SIZE, 0);
    const hci_rp_le_read_resolv_list_size * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    } else if( nullptr != ev_res && HCIStatusCode::SUCCESS != status ) {
        size_res = ev_res->size;
    }
    return status;
}

HCIStatusCode HCIHandler::le_read_peer_resolv_addr(const BDAddressAndType& peerIdentityAddressAndType,
                                                   jau::EUI48& peerResolvableAddress) noexcept {
    if( !use_resolv_readPeerRA() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    peerResolvableAddress.clear();
    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_read_peer_resolv_addr> req0(HCIOpcode::LE_READ_PEER_RESOLV_ADDR);
    hci_cp_le_read_peer_resolv_addr * cp = req0.getWStruct();
    cp->peer_id_addr_type = static_cast<uint8_t>(peerIdentityAddressAndType.type);
    cp->peer_id_addr = jau::cpu_to_le(peerIdentityAddressAndType.address);
    const hci_rp_le_read_peer_resolv_addr * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    } else if( nullptr != ev_res && HCIStatusCode::SUCCESS != status ) {
        peerResolvableAddress = jau::le_to_cpu(ev_res->peer_resolv_addr);
    }
    return status;
}

HCIStatusCode HCIHandler::le_read_local_resolv_addr(const BDAddressAndType& peerIdentityAddressAndType,
                                                    jau::EUI48& localResolvableAddress) noexcept {
    if( !use_resolv_readLocalRA() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    localResolvableAddress.clear();
    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_read_local_resolv_addr> req0(HCIOpcode::LE_READ_LOCAL_RESOLV_ADDR);
    hci_cp_le_read_local_resolv_addr * cp = req0.getWStruct();
    cp->peer_id_addr_type = static_cast<uint8_t>(peerIdentityAddressAndType.type);
    cp->peer_id_addr = jau::cpu_to_le(peerIdentityAddressAndType.address);
    const hci_rp_le_read_local_resolv_addr * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    } else if( nullptr != ev_res && HCIStatusCode::SUCCESS != status ) {
        localResolvableAddress = jau::le_to_cpu(ev_res->local_resolv_addr);
    }
    return status;
}

HCIStatusCode HCIHandler::le_set_addr_resolv_enable(const bool enable) noexcept {
    if( !use_resolv_enable() ) { return HCIStatusCode::UNKNOWN_COMMAND; }
    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_set_addr_resolv_enable> req0(HCIOpcode::LE_SET_ADDR_RESOLV_ENABLE);
    hci_cp_le_set_addr_resolv_enable * cp = req0.getWStruct();
    cp->enable = enable ? 0x01 : 0x00;
    const hci_rp_status * ev_res;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_res, &status, true /* quiet */);
    if( nullptr == ev || nullptr == ev_res || HCIStatusCode::SUCCESS != status ) {
        DBG_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_read_phy(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                      LE_PHYs& resTx, LE_PHYs& resRx) noexcept {
    if( !is_set(le_ll_feats, LE_Features::LE_2M_PHY) ) {
        resTx = LE_PHYs::LE_1M;
        resRx = LE_PHYs::LE_1M;
        return HCIStatusCode::SUCCESS;
    }
    resTx = LE_PHYs::NONE;
    resRx = LE_PHYs::NONE;

    HCIStatusCode status = check_open_connection("le_read_phy", conn_handle, peerAddressAndType);
    if( HCIStatusCode::SUCCESS != status ) {
        return status;
    }

    struct hci_cp_le_read_phy {
        __le16   handle;
    } __packed;
    struct hci_rp_le_read_phy {
        __u8     status;
        __le16   handle;
        __u8    tx_phys;
        __u8    rx_phys;
    } __packed;

    HCIStructCommand<hci_cp_le_read_phy> req0(HCIOpcode::LE_READ_PHY);
    hci_cp_le_read_phy * cp = req0.getWStruct();
    cp->handle = jau::cpu_to_le(conn_handle);
    const hci_rp_le_read_phy * ev_phy;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_phy, &status);

    if( nullptr == ev || nullptr == ev_phy || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(),
                number(status), to_string(status).c_str(), toString().c_str());
    } else {
        const uint16_t conn_handle_rcvd = jau::le_to_cpu(ev_phy->handle);
        if( conn_handle != conn_handle_rcvd ) {
            ERR_PRINT("Mismatch given address%s conn_handle (req) %s != %s (res) (drop) - %s",
                       peerAddressAndType.toString().c_str(),
                       jau::to_hexstring(conn_handle).c_str(), jau::to_hexstring(conn_handle_rcvd).c_str(), toString().c_str());
            return HCIStatusCode::INTERNAL_FAILURE;
        }
        switch( ev_phy->tx_phys ) {
            case 0x01: resTx = LE_PHYs::LE_1M; break;
            case 0x02: resTx = LE_PHYs::LE_2M; break;
            case 0x03: resTx = LE_PHYs::LE_CODED; break;
        }
        switch( ev_phy->rx_phys ) {
            case 0x01: resRx = LE_PHYs::LE_1M; break;
            case 0x02: resRx = LE_PHYs::LE_2M; break;
            case 0x03: resRx = LE_PHYs::LE_CODED; break;
        }
    }
    return status;
}

HCIStatusCode HCIHandler::le_set_default_phy(const LE_PHYs Tx, const LE_PHYs Rx) noexcept {
    if( !is_set(le_ll_feats, LE_Features::LE_2M_PHY) ) {
        if( is_set(Tx, LE_PHYs::LE_2M) ) {
            WARN_PRINT("dev_id %u: LE_2M_PHY no supported, requested Tx %s", dev_id, to_string(Tx).c_str());
            return HCIStatusCode::INVALID_PARAMS;
        }
        if( is_set(Rx, LE_PHYs::LE_2M) ) {
            WARN_PRINT("dev_id %u: LE_2M_PHY no supported, requested Rx %s", dev_id, to_string(Rx).c_str());
            return HCIStatusCode::INVALID_PARAMS;
        }
    }

    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }

    HCIStatusCode status;
    HCIStructCommand<hci_cp_le_set_default_phy> req0(HCIOpcode::LE_SET_DEFAULT_PHY);
    hci_cp_le_set_default_phy * cp = req0.getWStruct();
    cp->all_phys = ( Tx != LE_PHYs::NONE ? 0b000 : 0b001 ) |
                   ( Rx != LE_PHYs::NONE ? 0b000 : 0b010 );
    cp->tx_phys = number( Tx );
    cp->rx_phys = number( Rx );

    const hci_rp_status * ev_status;
    std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    if( nullptr == ev || nullptr == ev || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_set_phy(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                     const LE_PHYs Tx, const LE_PHYs Rx) noexcept {
    if( !is_set(le_ll_feats, LE_Features::LE_2M_PHY) ) {
        if( is_set(Tx, LE_PHYs::LE_2M) ) {
            WARN_PRINT("dev_id %u: LE_2M_PHY no supported, requested Tx %s", dev_id, to_string(Tx).c_str());
            return HCIStatusCode::INVALID_PARAMS;
        }
        if( is_set(Rx, LE_PHYs::LE_2M) ) {
            WARN_PRINT("dev_id %u: LE_2M_PHY no supported, requested Rx %s", dev_id, to_string(Rx).c_str());
            return HCIStatusCode::INVALID_PARAMS;
        }
    }

    HCIStatusCode status = check_open_connection("le_set_phy", conn_handle, peerAddressAndType);
    if( HCIStatusCode::SUCCESS != status ) {
        return status;
    }

    struct hci_cp_le_set_phy {
        uint16_t handle;
        __u8    all_phys;
        __u8    tx_phys;
        __u8    rx_phys;
        uint16_t phy_options;
    } __packed;

    HCIStructCommand<hci_cp_le_set_phy> req0(HCIOpcode::LE_SET_PHY);
    hci_cp_le_set_phy * cp = req0.getWStruct();
    cp->handle = jau::cpu_to_le(conn_handle);
    cp->all_phys = ( Tx != LE_PHYs::NONE ? 0b000 : 0b001 ) |
                   ( Rx != LE_PHYs::NONE ? 0b000 : 0b010 );
    cp->tx_phys = number( Tx );
    cp->rx_phys = number( Rx );
    cp->phy_options = 0;

    std::unique_ptr<HCIEvent> ev = processCommandStatus(req0, &status);

    if( nullptr == ev || nullptr == ev || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("%s: 0x%x (%s) - %s", to_string(req0.getOpcode()).c_str(), number(status), to_string(status).c_str(), toString().c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::le_set_adv_param(const EUI48 &peer_bdaddr,
                                           const HCILEOwnAddressType own_mac_type,
                                           const HCILEOwnAddressType peer_mac_type,
                                           const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                                           const AD_PDU_Type adv_type,
                                           const uint8_t adv_chan_map,
                                           const uint8_t filter_policy) noexcept {
    DBG_PRINT("HCIHandler<%u>::le_set_adv_param: adv-interval[%.3f ms .. %.3f ms], filter %d - %s",
            dev_id, 0.625f * (float)adv_interval_min, 0.625f * (float)adv_interval_max,
            filter_policy, toString().c_str());

    HCIStatusCode status;
    if( use_ext_adv() ) {
        HCIStructCommand<hci_cp_le_set_ext_adv_params> req0(HCIOpcode::LE_SET_EXT_ADV_PARAMS);
        hci_cp_le_set_ext_adv_params * cp = req0.getWStruct();
        cp->handle = 0x00; // TODO: Support more than one advertising sets?
        AD_PDU_Type adv_type2;
        switch( adv_type ) {
            // Connectable
            case AD_PDU_Type::ADV_IND:
                [[fallthrough]];
            case AD_PDU_Type::ADV_SCAN_IND:
                [[fallthrough]];
            case AD_PDU_Type::ADV_IND2:
                adv_type2 = AD_PDU_Type::ADV_IND2;
                break;

            // Non Connectable
            case AD_PDU_Type::SCAN_IND2:
                adv_type2 = AD_PDU_Type::SCAN_IND2;
                break;

            case AD_PDU_Type::ADV_NONCONN_IND:
                [[fallthrough]];
            case AD_PDU_Type::NONCONN_IND2:
                adv_type2 = AD_PDU_Type::NONCONN_IND2;
                break;

            default:
                WARN_PRINT("dev_id %u: Invalid AD_PDU_Type %x (%s)", dev_id, adv_type, to_string(adv_type).c_str());
                return HCIStatusCode::INVALID_PARAMS;
        }
        cp->evt_properties = number(adv_type2);
        // Actually .. but struct uses uint8_t[3] duh ..
        // cp->min_interval = jau::cpu_to_le(adv_interval_min);
        // cp->max_interval = jau::cpu_to_le(adv_interval_max);
        jau::put_uint16(cp->min_interval + 0, adv_interval_min, jau::lb_endian_t::little);
        jau::put_uint16(cp->max_interval + 0, adv_interval_max, jau::lb_endian_t::little);
        cp->channel_map = adv_chan_map;
        cp->own_addr_type = static_cast<uint8_t>(own_mac_type);
        cp->peer_addr_type = static_cast<uint8_t>(peer_mac_type);
        cp->peer_addr = jau::cpu_to_le(peer_bdaddr);
        cp->filter_policy = filter_policy;
        cp->tx_power = 0x7f; // Host has no preference (default); -128 to +20 [dBm]
        cp->primary_phy = direct_bt::number(LE_PHYs::LE_1M);
        // TODO: Support LE_1M + LE_CODED combo? Then must not use legacy PDU adv_type!
        // cp->secondary_max_skip;
        cp->secondary_phy = direct_bt::number(LE_PHYs::LE_1M);
        cp->sid = 0x00; // TODO: Support more than one advertising SID subfield?
        cp->notif_enable = 0x01;
        const hci_rp_le_set_ext_adv_params * ev_reply;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_reply, &status);
        // Not using `ev_reply->tx_power` yet.
    } else {
        HCIStructCommand<hci_cp_le_set_adv_param> req0(HCIOpcode::LE_SET_ADV_PARAM);
        hci_cp_le_set_adv_param * cp = req0.getWStruct();
        cp->min_interval = jau::cpu_to_le(adv_interval_min);
        cp->max_interval = jau::cpu_to_le(adv_interval_max);
        cp->type = number(adv_type);
        cp->own_address_type = static_cast<uint8_t>(own_mac_type);
        cp->direct_addr_type = static_cast<uint8_t>(peer_mac_type);
        cp->direct_addr = jau::cpu_to_le(peer_bdaddr);
        cp->channel_map = adv_chan_map;
        cp->filter_policy = filter_policy;
        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
    }
    return status;
}


HCIStatusCode HCIHandler::le_set_adv_data(const EInfoReport &eir, const EIRDataType mask) noexcept {
    DBG_PRINT("HCIHandler<%u>::le_set_adv_data: %d", dev_id, eir.toString(true).c_str());

    HCIStatusCode status;
    if( use_ext_adv() ) {

        HCIStructCommand<hci_cp_le_set_ext_adv_data> req0(HCIOpcode::LE_SET_EXT_ADV_DATA);
        hci_cp_le_set_ext_adv_data * cp = req0.getWStruct();
        const jau::nsize_t max_data_len = HCI_MAX_AD_LENGTH; // not sizeof(cp->data), as we use legacy PDU
        cp->handle = 0x00; // TODO: Support more than one advertising sets?
        cp->operation = LE_SET_ADV_DATA_OP_COMPLETE;
        cp->frag_pref = LE_SET_ADV_DATA_NO_FRAG;
        cp->length = eir.write_data(mask, cp->data, max_data_len);
        req0.trimParamSize( req0.getParamSize() + cp->length - sizeof(cp->data) );

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    } else {

        HCIStructCommand<hci_cp_le_set_adv_data> req0(HCIOpcode::LE_SET_ADV_DATA);
        hci_cp_le_set_adv_data * cp = req0.getWStruct();
        cp->length = eir.write_data(mask, cp->data, sizeof(cp->data));
        // No param-size trimming for BT4, fixed 31 bytes

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    }
    return status;
}

HCIStatusCode HCIHandler::le_set_scanrsp_data(const EInfoReport &eir, const EIRDataType mask) noexcept {
    DBG_PRINT("HCIHandler<%u>::le_set_scanrsp_data: %d", dev_id, eir.toString(true).c_str());

    HCIStatusCode status;
    if( use_ext_adv() ) {

        HCIStructCommand<hci_cp_le_set_ext_scan_rsp_data> req0(HCIOpcode::LE_SET_EXT_SCAN_RSP_DATA);
        hci_cp_le_set_ext_scan_rsp_data * cp = req0.getWStruct();
        const jau::nsize_t max_data_len = HCI_MAX_AD_LENGTH; // not sizeof(cp->data), as we use legacy PDU
        cp->handle = 0x00; // TODO: Support more than one advertising sets?
        cp->operation = LE_SET_ADV_DATA_OP_COMPLETE;
        cp->frag_pref = LE_SET_ADV_DATA_NO_FRAG;
        cp->length = eir.write_data(mask, cp->data, max_data_len);
        req0.trimParamSize( req0.getParamSize() + cp->length - sizeof(cp->data) );

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    } else {

        HCIStructCommand<hci_cp_le_set_scan_rsp_data> req0(HCIOpcode::LE_SET_SCAN_RSP_DATA);
        hci_cp_le_set_scan_rsp_data * cp = req0.getWStruct();
        cp->length = eir.write_data(mask, cp->data, sizeof(cp->data));
        // No param-size trimming for BT4, fixed 31 bytes

        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    }
    return status;
}

HCIStatusCode HCIHandler::le_enable_adv(const bool enable) noexcept {
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    if( enable ) {
        if( ScanType::NONE != currentScanType ) {
            WARN_PRINT("Not allowed (scan enabled): %s", toString().c_str());
            return HCIStatusCode::COMMAND_DISALLOWED;
        }
        const size_type connCount = getTrackerConnectionCount();
        if( 0 < connCount ) {
            WARN_PRINT("Not allowed (%zu connections open/pending): %s", (size_t)connCount, toString().c_str());
            return HCIStatusCode::COMMAND_DISALLOWED;
        }
    }
    DBG_PRINT("HCIHandler<%u>::le_enable_adv: enable %d - %s", dev_id, enable, toString().c_str());

    HCIStatusCode status = HCIStatusCode::SUCCESS;

    if( use_ext_adv() ) {
        const hci_rp_status * ev_status;
        if( enable ) {
            struct hci_cp_le_set_ext_adv_enable_1 {
                __u8  enable;
                __u8  num_of_sets;
                hci_cp_ext_adv_set sets[1];
            } __packed;

            HCIStructCommand<hci_cp_le_set_ext_adv_enable_1> req0(HCIOpcode::LE_SET_EXT_ADV_ENABLE);
            hci_cp_le_set_ext_adv_enable_1 * cp = req0.getWStruct();
            cp->enable = 0x01;
            cp->num_of_sets = 1;
            cp->sets[0].handle = 0x00;
            cp->sets[0].duration = 0; // continue adv until host disables
            cp->sets[0].max_events = 0; // no maximum number of adv events
            std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
        } else {
            HCIStructCommand<hci_cp_le_set_ext_adv_enable> req0(HCIOpcode::LE_SET_EXT_ADV_ENABLE);
            hci_cp_le_set_ext_adv_enable * cp = req0.getWStruct();
            cp->enable = 0x00;
            cp->num_of_sets = 0; // disable all advertising sets
            std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
        }
    } else {
        struct hci_cp_le_set_adv_enable {
            uint8_t enable;
        } __packed;
        HCIStructCommand<hci_cp_le_set_adv_enable> req0(HCIOpcode::LE_SET_ADV_ENABLE);
        hci_cp_le_set_adv_enable * cp = req0.getWStruct();
        cp->enable = enable ? 0x01 : 0x00;
        const hci_rp_status * ev_status;
        std::unique_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
    }
    if( HCIStatusCode::SUCCESS == status ) {
        advertisingEnabled = enable;
    } else if( advertisingEnabled == enable ) {
        // Override erroneous HCI failure when
        // - disabling advertising when already disabled, or
        // - enabling advertising when already enabled
        // as stated in spec 'BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command'
        WARN_PRINT("enable-arg %d == enabled-state %d: Unchanged request, overriding failure: %s -> %s - %s",
                enable, advertisingEnabled.load(), to_string(status).c_str(), to_string(HCIStatusCode::SUCCESS).c_str(), toString().c_str());
        status = HCIStatusCode::SUCCESS;
    }
    return status;
}

HCIStatusCode HCIHandler::le_start_adv(const EInfoReport &eir,
                                       const EIRDataType adv_mask, const EIRDataType scanrsp_mask,
                                       const EUI48 &peer_bdaddr,
                                       const HCILEOwnAddressType own_mac_type,
                                       const HCILEOwnAddressType peer_mac_type,
                                       const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                                       const AD_PDU_Type adv_type,
                                       const uint8_t adv_chan_map,
                                       const uint8_t filter_policy) noexcept
{
    if( !isOpen() ) {
        ERR_PRINT("Not connected %s", toString().c_str());
        return HCIStatusCode::DISCONNECTED;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    if( ScanType::NONE != currentScanType ) {
        WARN_PRINT("Not allowed (scan enabled): %s", toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    const size_type connCount = getTrackerConnectionCount();
    if( 0 < connCount ) {
        WARN_PRINT("Not allowed (%zu connections open/pending): %s", (size_t)connCount, toString().c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }

    HCIStatusCode status = le_set_adv_data(eir, adv_mask);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("le_set_adv_data: %s - %s",
                to_string(status).c_str(), toString().c_str());
        return status;
    }

    status = le_set_scanrsp_data(eir, scanrsp_mask);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("le_set_scanrsp_data: %s - %s",
                to_string(status).c_str(), toString().c_str());
        return status;
    }
    status = le_set_adv_param(peer_bdaddr, own_mac_type, peer_mac_type,
                              adv_interval_min, adv_interval_max,
                              adv_type, adv_chan_map, filter_policy);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("le_set_adv_param: %s - %s",
                to_string(status).c_str(), toString().c_str());
        return status;
    }

    status = le_enable_adv(true);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("le_enable_adv failed: %s - %s",
                to_string(status).c_str(), toString().c_str());
    }

    return status;
}

std::unique_ptr<HCIEvent> HCIHandler::processCommandStatus(HCICommand &req, HCIStatusCode *status, const bool quiet) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    *status = HCIStatusCode::INTERNAL_FAILURE;

    int32_t retryCount = 0;
    std::unique_ptr<HCIEvent> ev = nullptr;

    if( !sendCommand(req) ) {
        goto exit;
    }

    while( retryCount < env.HCI_READ_PACKET_MAX_RETRY ) {
        ev = getNextReply(req, retryCount, env.HCI_COMMAND_STATUS_REPLY_TIMEOUT);
        if( nullptr == ev ) {
            *status = HCIStatusCode::INTERNAL_TIMEOUT;
            break; // timeout, leave loop
        } else if( ev->isEvent(HCIEventType::CMD_STATUS) ) {
            HCICommandStatusEvent * ev_cs = static_cast<HCICommandStatusEvent*>(ev.get());
            *status = ev_cs->getStatus();
            DBG_PRINT("HCIHandler<%u>::processCommandStatus %s -> Status 0x%2.2X (%s), errno %d %s: res %s, req %s - %s",
                    dev_id, to_string(req.getOpcode()).c_str(),
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    ev_cs->toString().c_str(), req.toString().c_str(), toString().c_str());
            break; // gotcha, leave loop - pending completion result handled via callback
        } else {
            retryCount++;
            DBG_PRINT("HCIHandler<%u>::processCommandStatus: !CMD_STATUS (drop, retry %d): res %s; req %s - %s",
                       dev_id, retryCount, ev->toString().c_str(), req.toString().c_str(), toString().c_str());
            continue; // next packet
        }
    }
    if( nullptr == ev ) {
        // timeout exit
        if( !quiet || jau::environment::get().verbose ) {
            WARN_PRINT("%s -> Status 0x%2.2X (%s), errno %d %s: res nullptr, req %s - %s",
                    to_string(req.getOpcode()).c_str(),
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    req.toString().c_str(), toString().c_str());
        }
    }

exit:
    return ev;
}

template<typename hci_cmd_event_struct>
std::unique_ptr<HCIEvent> HCIHandler::processCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status,
                                                             const bool quiet) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    *res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    if( !sendCommand(req, quiet) ) {
        if( !quiet || jau::environment::get().verbose ) {
            WARN_PRINT("Send failed: Status 0x%2.2X (%s), errno %d %s: res nullptr, req %s - %s",
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    req.toString().c_str(), toString().c_str());
        }
        return nullptr; // timeout
    }

    return receiveCommandComplete(req, res, status, quiet);
}

template<typename hci_cmd_event_struct>
std::unique_ptr<HCIEvent> HCIHandler::receiveCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status,
                                                             const bool quiet) noexcept
{
    *res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    const HCIEventType evc = HCIEventType::CMD_COMPLETE;
    HCICommandCompleteEvent * ev_cc;
    std::unique_ptr<HCIEvent> ev = getNextCmdCompleteReply(req, &ev_cc);
    if( nullptr == ev ) {
        *status = HCIStatusCode::INTERNAL_TIMEOUT;
        if( !quiet || jau::environment::get().verbose ) {
            WARN_PRINT("%s -> %s: Status 0x%2.2X (%s), errno %d %s: res nullptr, req %s - %s",
                    to_string(req.getOpcode()).c_str(), to_string(evc).c_str(),
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    req.toString().c_str(), toString().c_str());
        }
        return nullptr; // timeout
    } else if( nullptr == ev_cc ) {
        if( ev->isEvent(HCIEventType::CMD_STATUS) ) {
            HCICommandStatusEvent * ev_cs = static_cast<HCICommandStatusEvent*>(ev.get());
            *status = ev_cs->getStatus();
        }
        if( !quiet || jau::environment::get().verbose ) {
            WARN_PRINT("%s -> %s: Status 0x%2.2X (%s), errno %d %s: res %s, req %s - %s",
                    to_string(req.getOpcode()).c_str(), to_string(evc).c_str(),
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    ev->toString().c_str(), req.toString().c_str(), toString().c_str());
        }
        return ev;
    }
    const uint8_t returnParamSize = ev_cc->getReturnParamSize();
    if( returnParamSize < sizeof(hci_cmd_event_struct) ) {
        if( !quiet || jau::environment::get().verbose ) {
            WARN_PRINT("%s -> %s: Status 0x%2.2X (%s), errno %d %s: res %s, req %s - %s",
                    to_string(req.getOpcode()).c_str(), to_string(evc).c_str(),
                    number(*status), to_string(*status).c_str(), errno, strerror(errno),
                    ev_cc->toString().c_str(), req.toString().c_str(), toString().c_str());
        }
        return ev;
    }
    *res = (const hci_cmd_event_struct*)(ev_cc->getReturnParam());
    *status = static_cast<HCIStatusCode>((*res)->status);
    DBG_PRINT("HCIHandler<%u>::receiveCommandComplete %s -> %s: Status 0x%2.2X (%s): res %s, req %s - %s",
            dev_id, to_string(req.getOpcode()).c_str(), to_string(evc).c_str(),
            number(*status), to_string(*status).c_str(),
            ev_cc->toString().c_str(), req.toString().c_str(), toString().c_str());
    return ev;
}

template<typename hci_cmd_event_struct>
const hci_cmd_event_struct* HCIHandler::getReplyStruct(HCIEvent& event, HCIEventType evc, HCIStatusCode *status) noexcept
{
    const hci_cmd_event_struct* res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    typedef HCIStructCmdCompleteEvtWrap<hci_cmd_event_struct> HCITypeCmdCompleteEvtWrap;
    HCITypeCmdCompleteEvtWrap ev_cc( event );
    if( ev_cc.isTypeAndSizeValid(evc) ) {
        *status = ev_cc.getStatus();
        res = ev_cc.getStruct();
    } else {
        WARN_PRINT("%s: Type or size mismatch: Status 0x%2.2X (%s), errno %d %s: res %s - %s",
                to_string(evc).c_str(),
                number(*status), to_string(*status).c_str(), errno, strerror(errno),
                ev_cc.toString().c_str(), toString().c_str());
    }
    return res;
}

template<typename hci_cmd_event_struct>
const hci_cmd_event_struct* HCIHandler::getMetaReplyStruct(HCIEvent& event, HCIMetaEventType mec, HCIStatusCode *status) noexcept
{
    const hci_cmd_event_struct* res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    typedef HCIStructCmdCompleteMetaEvtWrap<hci_cmd_event_struct> HCITypeCmdCompleteMetaEvtWrap;
    const HCITypeCmdCompleteMetaEvtWrap ev_cc( *static_cast<HCIMetaEvent*>( &event ) );
    if( ev_cc.isTypeAndSizeValid(mec) ) {
        *status = ev_cc.getStatus();
        res = ev_cc.getStruct();
    } else {
        WARN_PRINT("%s: Type or size mismatch: Status 0x%2.2X (%s), errno %d %s: res %s - %s",
                  to_string(mec).c_str(),
                  number(*status), to_string(*status).c_str(), errno, strerror(errno),
                  ev_cc.toString().c_str(), toString().c_str());
    }
    return res;
}

/***
 *
 * MgmtEventCallback section
 *
 */

static MgmtEventCallbackList::equal_comparator _mgmtEventCallbackEqComparator =
        [](const MgmtEventCallback &a, const MgmtEventCallback &b) noexcept -> bool { return a == b; };

bool HCIHandler::addMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d - %s", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size(), toString().c_str());
        return false;
    }
    MgmtEventCallbackList &l = mgmtEventCallbackLists[static_cast<uint16_t>(opc)];
    /* const bool added = */ l.push_back_unique(cb, _mgmtEventCallbackEqComparator);
    return true;
}
HCIHandler::size_type HCIHandler::removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d - %s", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size(), toString().c_str());
        return 0;
    }
    MgmtEventCallbackList &l = mgmtEventCallbackLists[static_cast<uint16_t>(opc)];
    return l.erase_matching(cb, true /* all_matching */, _mgmtEventCallbackEqComparator);
}
void HCIHandler::clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d - %s", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size(), toString().c_str());
        return;
    }
    mgmtEventCallbackLists[static_cast<uint16_t>(opc)].clear();
}
void HCIHandler::clearAllCallbacks() noexcept {
    for(auto & mgmtEventCallbackList : mgmtEventCallbackLists) {
        mgmtEventCallbackList.clear();
    }
    hciSMPMsgCallbackList.clear();
}

/**
 * SMPMsgCallback handling
 */

static HCISMPMsgCallbackList::equal_comparator _changedHCISMPMsgCallbackEqComp =
        [](const HCISMPMsgCallback& a, const HCISMPMsgCallback& b) noexcept -> bool { return a == b; };


void HCIHandler::addSMPMsgCallback(const HCISMPMsgCallback & l) {
    hciSMPMsgCallbackList.push_back(l);
}
HCIHandler::size_type HCIHandler::removeSMPMsgCallback(const HCISMPMsgCallback & l) {
    return hciSMPMsgCallbackList.erase_matching(l, true /* all_matching */, _changedHCISMPMsgCallbackEqComp);
}


