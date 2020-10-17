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
#include <vector>
#include <cstdio>

#include <algorithm>

// #define PERF_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/environment.hpp>
#include <jau/basic_algos.hpp>

#include "BTIoctl.hpp"

#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "HCIHandler.hpp"
#include "DBTTypes.hpp"

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

HCIEnv::HCIEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.hci") ),
  HCI_READER_THREAD_POLL_TIMEOUT( jau::environment::getInt32Property("direct_bt.hci.reader.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  HCI_COMMAND_STATUS_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.hci.cmd.status.timeout", 3000, 1500 /* min */, INT32_MAX /* max */) ),
  HCI_COMMAND_COMPLETE_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.hci.cmd.complete.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  HCI_EVT_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.hci.ringsize", 64, 64 /* min */, 1024 /* max */) ),
  DEBUG_EVENT( jau::environment::getBooleanProperty("direct_bt.debug.hci.event", false) ),
  HCI_READ_PACKET_MAX_RETRY( HCI_EVT_RING_CAPACITY )
{
}

const pid_t HCIHandler::pidSelf = getpid();

struct hci_rp_status {
    __u8    status;
} __packed;

HCIConnectionRef HCIHandler::addOrUpdateHCIConnection(std::vector<HCIConnectionRef> &list,
                                                      const EUI48 & address, BDAddressType addrType, const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    // remove all old entry with given address first
    for (auto it = list.begin(); it != list.end(); ) {
        HCIConnectionRef conn = *it;
        if ( conn->equals(address, addrType) ) {
            // reuse same entry
            WORDY_PRINT("HCIHandler::addTrackerConnection: address[%s, %s], handle %s: reuse entry %s",
               address.toString().c_str(), getBDAddressTypeString(addrType).c_str(), jau::uint16HexString(handle).c_str(), conn->toString().c_str());
            // Overwrite tracked connection handle with given _valid_ handle only, i.e. non zero!
            if( 0 != handle ) {
                if( 0 != conn->getHandle() && handle != conn->getHandle() ) {
                    WARN_PRINT("HCIHandler::addTrackerConnection: address[%s, %s], handle %s: reusing entry %s, overwriting non-zero handle",
                       address.toString().c_str(), getBDAddressTypeString(addrType).c_str(), jau::uint16HexString(handle).c_str(), conn->toString().c_str());
                }
                conn->setHandle( handle );
            }
            return conn; // done
        } else {
            ++it;
        }
    }
    HCIConnectionRef res( new HCIConnection(address, addrType, handle) );
    list.push_back( res );
    return res;
}

HCIConnectionRef HCIHandler::findHCIConnection(std::vector<HCIConnectionRef> &list, const EUI48 & address, BDAddressType addrType) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    const size_t size = list.size();
    for (size_t i = 0; i < size; i++) {
        HCIConnectionRef & e = list[i];
        if( e->equals(address, addrType) ) {
            return e;
        }
    }
    return nullptr;
}

HCIConnectionRef HCIHandler::findTrackerConnection(const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    const size_t size = connectionList.size();
    for (size_t i = 0; i < size; i++) {
        HCIConnectionRef & e = connectionList[i];
        if ( handle == e->getHandle() ) {
            return e;
        }
    }
    return nullptr;
}

HCIConnectionRef HCIHandler::removeTrackerConnection(const HCIConnectionRef conn) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    for (auto it = connectionList.begin(); it != connectionList.end(); ) {
        HCIConnectionRef e = *it;
        if ( *e == *conn ) {
            it = connectionList.erase(it);
            return e; // done
        } else {
            ++it;
        }
    }
    return nullptr;
}

HCIConnectionRef HCIHandler::removeHCIConnection(std::vector<HCIConnectionRef> &list, const uint16_t handle) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    for (auto it = list.begin(); it != list.end(); ) {
        HCIConnectionRef e = *it;
        if ( e->getHandle() == handle ) {
            it = list.erase(it);
            return e; // done
        } else {
            ++it;
        }
    }
    return nullptr;
}

void HCIHandler::clearConnectionLists() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
    connectionList.clear();
    disconnectList.clear();
}

MgmtEvent::Opcode HCIHandler::translate(HCIEventType evt, HCIMetaEventType met) noexcept {
    if( HCIEventType::LE_META == evt ) {
        switch( met ) {
            case HCIMetaEventType::LE_CONN_COMPLETE: return MgmtEvent::Opcode::DEVICE_CONNECTED;
            default: return MgmtEvent::Opcode::INVALID;
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

std::shared_ptr<MgmtEvent> HCIHandler::translate(std::shared_ptr<HCIEvent> ev) noexcept {
    const HCIEventType evt = ev->getEventType();
    const HCIMetaEventType mevt = ev->getMetaEventType();

    if( HCIEventType::LE_META == evt ) {
        switch( mevt ) {
            case HCIMetaEventType::LE_CONN_COMPLETE: {
                HCIStatusCode status;
                const hci_ev_le_conn_complete * ev_cc = getMetaReplyStruct<hci_ev_le_conn_complete>(ev, mevt, &status);
                if( nullptr == ev_cc ) {
                    ERR_PRINT("HCIHandler::translate(reader): LE_CONN_COMPLETE: Null reply-struct: %s", ev->toString().c_str());
                    return nullptr;
                }
                const HCILEPeerAddressType hciAddrType = static_cast<HCILEPeerAddressType>(ev_cc->bdaddr_type);
                const BDAddressType addrType = getBDAddressType(hciAddrType);
                HCIConnectionRef conn = addOrUpdateTrackerConnection(ev_cc->bdaddr, addrType, ev_cc->handle);
                if( HCIStatusCode::SUCCESS == status ) {
                    return std::shared_ptr<MgmtEvent>( new MgmtEvtDeviceConnected(dev_id, ev_cc->bdaddr, addrType, ev_cc->handle) );
                } else {
                    std::shared_ptr<MgmtEvent> res( new MgmtEvtDeviceConnectFailed(dev_id, ev_cc->bdaddr, addrType, status) );
                    removeTrackerConnection(conn);
                    return res;
                }
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
                ERR_PRINT("HCIHandler::translate(reader): CONN_COMPLETE: Null reply-struct: %s", ev->toString().c_str());
                return nullptr;
            }
            HCIConnectionRef conn = addOrUpdateTrackerConnection(ev_cc->bdaddr, BDAddressType::BDADDR_BREDR, ev_cc->handle);
            if( HCIStatusCode::SUCCESS == status ) {
                return std::shared_ptr<MgmtEvent>( new MgmtEvtDeviceConnected(dev_id, conn->getAddress(), conn->getAddressType(), conn->getHandle()) );
            } else {
                std::shared_ptr<MgmtEvent> res( new MgmtEvtDeviceConnectFailed(dev_id, conn->getAddress(), conn->getAddressType(), status) );
                removeTrackerConnection(conn);
                return res;
            }
        }
        case HCIEventType::DISCONN_COMPLETE: {
            HCIStatusCode status;
            const hci_ev_disconn_complete * ev_cc = getReplyStruct<hci_ev_disconn_complete>(ev, evt, &status);
            if( nullptr == ev_cc ) {
                ERR_PRINT("HCIHandler::translate(reader): DISCONN_COMPLETE: Null reply-struct: %s", ev->toString().c_str());
                return nullptr;
            }
            removeDisconnect(ev_cc->handle);
            HCIConnectionRef conn = removeTrackerConnection(ev_cc->handle);
            if( nullptr == conn ) {
                WORDY_PRINT("HCIHandler::translate(reader): DISCONN_COMPLETE: Not tracked handle %s: %s",
                        jau::uint16HexString(ev_cc->handle).c_str(), ev->toString().c_str());
                return nullptr;
            } else {
                if( HCIStatusCode::SUCCESS != status ) {
                    // FIXME: Ever occuring? Still sending out essential disconnect event!
                    ERR_PRINT("HCIHandler::translate(reader): DISCONN_COMPLETE: !SUCCESS[%s, %s], %s: %s",
                            jau::uint8HexString(static_cast<uint8_t>(status)).c_str(), getHCIStatusCodeString(status).c_str(),
                            conn->toString().c_str(), ev->toString().c_str());
                }
                const HCIStatusCode hciRootReason = static_cast<HCIStatusCode>(ev_cc->reason);
                return std::shared_ptr<MgmtEvent>( new MgmtEvtDeviceDisconnected(dev_id, conn->getAddress(), conn->getAddressType(), hciRootReason, conn->getHandle()) );
            }
        }
        default:
            return nullptr;
    }
}

void HCIHandler::hciReaderThreadImpl() noexcept {
    {
        const std::lock_guard<std::mutex> lock(mtx_hciReaderLifecycle); // RAII-style acquire and relinquish via destructor
        hciReaderShallStop = false;
        hciReaderRunning = true;
        DBG_PRINT("HCIHandler::reader: Started");
        cv_hciReaderInit.notify_all();
    }

    while( !hciReaderShallStop ) {
        int len;
        if( !comm.isOpen() ) {
            // not open
            ERR_PRINT("HCIHandler::reader: Not connected");
            hciReaderShallStop = true;
            break;
        }

        len = comm.read(rbuffer.get_wptr(), rbuffer.getSize(), env.HCI_READER_THREAD_POLL_TIMEOUT);
        if( 0 < len ) {
            const uint16_t paramSize = len >= number(HCIConstU8::EVENT_HDR_SIZE) ? rbuffer.get_uint8_nc(2) : 0;
            if( len < number(HCIConstU8::EVENT_HDR_SIZE) + paramSize ) {
                WARN_PRINT("HCIHandler::reader: length mismatch %d < EVENT_HDR_SIZE(%d) + %d",
                        len, number(HCIConstU8::EVENT_HDR_SIZE), paramSize);
                continue; // discard data
            }
            std::shared_ptr<HCIEvent> event = HCIEvent::getSpecialized(rbuffer.get_ptr(), len);
            if( nullptr == event ) {
                // not an event ...
                ERR_PRINT("HCIHandler-IO RECV Drop (non-event) %s", jau::bytesHexString(rbuffer.get_ptr(), 0, len, true /* lsbFirst*/).c_str());
                continue;
            }

            const HCIMetaEventType mec = event->getMetaEventType();
            if( HCIMetaEventType::INVALID != mec && !filter_test_metaev(mec) ) {
                // DROP
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV Drop (meta filter) %s", event->toString().c_str());
                continue; // next packet
            }

            if( event->isEvent(HCIEventType::CMD_STATUS) || event->isEvent(HCIEventType::CMD_COMPLETE) )
            {
                COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV (CMD) %s", event->toString().c_str());
                if( hciEventRing.isFull() ) {
                    const int dropCount = hciEventRing.capacity()/4;
                    hciEventRing.drop(dropCount);
                    WARN_PRINT("HCIHandler-IO RECV Drop (%d oldest elements of %d capacity, ring full)", dropCount, hciEventRing.capacity());
                }
                hciEventRing.putBlocking( event );
            } else if( event->isMetaEvent(HCIMetaEventType::LE_ADVERTISING_REPORT) ) {
                // issue callbacks for the translated AD events
                std::vector<std::shared_ptr<EInfoReport>> eirlist = EInfoReport::read_ad_reports(event->getParam(), event->getParamSize());
                jau::for_each_idx(eirlist, [&](std::shared_ptr<EInfoReport> & eir) {
                    // COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV (AD EIR) %s", eir->toString().c_str());
                    sendMgmtEvent( std::shared_ptr<MgmtEvent>( new MgmtEvtDeviceFound(dev_id, eir) ) );
                });
            } else {
                // issue a callback for the translated event
                std::shared_ptr<MgmtEvent> mevent = translate(event);
                if( nullptr != mevent ) {
                    COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV (CB) %s", event->toString().c_str());
                    sendMgmtEvent( mevent );
                } else {
                    COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV Drop (no translation) %s", event->toString().c_str());
                }
            }
        } else if( ETIMEDOUT != errno && !hciReaderShallStop ) { // expected exits
            ERR_PRINT("HCIHandler::reader: HCIComm read error");
        }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_hciReaderLifecycle); // RAII-style acquire and relinquish via destructor
        WORDY_PRINT("HCIHandler::reader: Ended. Ring has %d entries flushed", hciEventRing.getSize());
        hciEventRing.clear();
        hciReaderRunning = false;
        cv_hciReaderInit.notify_all();
    }
}

void HCIHandler::sendMgmtEvent(std::shared_ptr<MgmtEvent> event) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    MgmtEventCallbackList & mgmtEventCallbackList = mgmtEventCallbackLists[static_cast<uint16_t>(event->getOpcode())];
    int invokeCount = 0;
    if( mgmtEventCallbackList.size() > 0 ) {
        for (auto it = mgmtEventCallbackList.begin(); it != mgmtEventCallbackList.end(); ++it) {
            try {
                it->invoke(event);
            } catch (std::exception &e) {
                ERR_PRINT("HCIHandler::sendMgmtEvent-CBs %d/%zd: MgmtEventCallback %s : Caught exception %s",
                        invokeCount+1, mgmtEventCallbackList.size(),
                        it->toString().c_str(), e.what());
            }
            invokeCount++;
        }
    }
    COND_PRINT(env.DEBUG_EVENT, "HCIHandler::sendMgmtEvent: Event %s -> %d/%zd callbacks", event->toString().c_str(), invokeCount, mgmtEventCallbackList.size());
    (void)invokeCount;
}

bool HCIHandler::sendCommand(HCICommand &req) noexcept {
    COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO SENT %s", req.toString().c_str());

    TROOctets & pdu = req.getPDU();
    if ( comm.write( pdu.get_ptr(), pdu.getSize() ) < 0 ) {
        ERR_PRINT("HCIHandler::sendCommand: HCIComm write error, req %s", req.toString().c_str());
        return false;
    }
    return true;
}

std::shared_ptr<HCIEvent> HCIHandler::getNextReply(HCICommand &req, int32_t & retryCount, const int32_t replyTimeoutMS) noexcept
{
    // Ringbuffer read is thread safe
    while( retryCount < env.HCI_READ_PACKET_MAX_RETRY ) {
        std::shared_ptr<HCIEvent> ev = hciEventRing.getBlocking(replyTimeoutMS);
        if( nullptr == ev ) {
            errno = ETIMEDOUT;
            ERR_PRINT("HCIHandler::getNextReply: nullptr result (timeout %d ms -> abort): req %s",
                    replyTimeoutMS, req.toString().c_str());
            return nullptr;
        } else if( !ev->validate(req) ) {
            // This could occur due to an earlier timeout w/ a nullptr == res (see above),
            // i.e. the pending reply processed here and naturally not-matching.
            retryCount++;
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV getNextReply: res mismatch (drop, retry %d): res %s; req %s",
                       retryCount, ev->toString().c_str(), req.toString().c_str());
        } else {
            COND_PRINT(env.DEBUG_EVENT, "HCIHandler-IO RECV getNextReply: res %s; req %s", ev->toString().c_str(), req.toString().c_str());
            return ev;
        }
    }
    return nullptr;
}

std::shared_ptr<HCIEvent> HCIHandler::sendWithCmdCompleteReply(HCICommand &req, HCICommandCompleteEvent **res) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    *res = nullptr;

    int32_t retryCount = 0;
    std::shared_ptr<HCIEvent> ev = nullptr;

    if( !sendCommand(req) ) {
        goto exit;
    }

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
                WARN_PRINT("HCIHandler::sendWithCmdCompleteReply: CMD_STATUS 0x%2.2X (%s), errno %d %s: res %s, req %s",
                        number(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno),
                        ev_cs->toString().c_str(), req.toString().c_str());
                break; // error status, leave loop
            } else {
                DBG_PRINT("HCIHandler::sendWithCmdCompleteReply: CMD_STATUS 0x%2.2X (%s, retryCount %d), errno %d %s: res %s, req %s",
                        number(status), getHCIStatusCodeString(status).c_str(), retryCount, errno, strerror(errno),
                        ev_cs->toString().c_str(), req.toString().c_str());
            }
            retryCount++;
            continue; // next packet
        } else {
            retryCount++;
            DBG_PRINT("HCIHandler::sendWithCmdCompleteReply: !(CMD_COMPLETE, CMD_STATUS) (drop, retry %d): res %s; req %s",
                       retryCount, ev->toString().c_str(), req.toString().c_str());
            continue; // next packet
        }
    }

exit:
    return ev;
}

HCIHandler::HCIHandler(const uint16_t dev_id, const BTMode btMode) noexcept
: env(HCIEnv::get()),
  dev_id(dev_id),
  rbuffer(HCI_MAX_MTU),
  comm(dev_id, HCI_CHANNEL_RAW),
  hciEventRing(env.HCI_EVT_RING_CAPACITY), hciReaderShallStop(false),
  hciReaderThreadId(0), hciReaderRunning(false),
  allowClose( comm.isOpen() ),
  btMode(btMode)
{
    WORDY_PRINT("HCIHandler.ctor: pid %d", HCIHandler::pidSelf);
    if( !allowClose ) {
        ERR_PRINT("HCIHandler::ctor: Could not open hci control channel");
        return;
    }

    {
        std::unique_lock<std::mutex> lock(mtx_hciReaderLifecycle); // RAII-style acquire and relinquish via destructor

        std::thread hciReaderThread(&HCIHandler::hciReaderThreadImpl, this); // @suppress("Invalid arguments")
        hciReaderThreadId = hciReaderThread.native_handle();
        // Avoid 'terminate called without an active exception'
        // as hciReaderThreadImpl may end due to I/O errors.
        hciReaderThread.detach();

        while( false == hciReaderRunning ) {
            cv_hciReaderInit.wait(lock);
        }
    }

    PERF_TS_T0();

    // Mandatory socket filter (not adapter filter!)
    {
#if 0
        // No use for pre-existing hci_ufilter
        hci_ufilter of;
        socklen_t olen;

        olen = sizeof(of);
        if (getsockopt(comm.getSocketDescriptor(), SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
            ERR_PRINT("HCIHandler::ctor: getsockopt");
            goto fail;
        }
#endif
        HCIComm::filter_clear(&filter_mask);
        HCIComm::filter_set_ptype(number(HCIPacketType::EVENT),  &filter_mask); // only EVENTs
        // Setup generic filter mask for all events, this is also required for
        // HCIComm::filter_all_events(&filter_mask); // all events
        HCIComm::filter_set_event(number(HCIEventType::CONN_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::DISCONN_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::CMD_COMPLETE), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::CMD_STATUS), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::HARDWARE_ERROR), &filter_mask);
        HCIComm::filter_set_event(number(HCIEventType::LE_META), &filter_mask);
        // HCIComm::filter_set_event(number(HCIEventType::DISCONN_PHY_LINK_COMPLETE), &filter_mask);
        // HCIComm::filter_set_event(number(HCIEventType::DISCONN_LOGICAL_LINK_COMPLETE), &filter_mask);
        HCIComm::filter_set_opcode(0, &filter_mask); // all opcode

        if (setsockopt(comm.getSocketDescriptor(), SOL_HCI, HCI_FILTER, &filter_mask, sizeof(filter_mask)) < 0) {
            ERR_PRINT("HCIHandler::ctor: setsockopt");
            goto fail;
        }
    }
    // Mandatory own LE_META filter
    {
        uint32_t mask = 0;
        // filter_all_metaevs(mask);
        filter_set_metaev(HCIMetaEventType::LE_CONN_COMPLETE, mask);
        filter_set_metaev(HCIMetaEventType::LE_ADVERTISING_REPORT, mask);
        filter_put_metaevs(mask);
    }
    // Mandatory own HCIOpcodeBit/HCIOpcode filter
    {
        uint64_t mask = 0;
        // filter_all_opcbit(mask);
        filter_set_opcbit(HCIOpcodeBit::CREATE_CONN, mask);
        filter_set_opcbit(HCIOpcodeBit::DISCONNECT, mask);
        filter_set_opcbit(HCIOpcodeBit::RESET, mask);
        filter_set_opcbit(HCIOpcodeBit::READ_LOCAL_VERSION, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_SCAN_PARAM, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_SET_SCAN_ENABLE, mask);
        filter_set_opcbit(HCIOpcodeBit::LE_CREATE_CONN, mask);
        filter_put_opcbit(mask);
    }

    PERF_TS_TD("HCIHandler::open.ok");
    return;

fail:
    close();
    PERF_TS_TD("HCIHandler::open.fail");
    return;
}

void HCIHandler::close() noexcept {
    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowClose.compare_exchange_strong(expConn, false) ) {
        // not open
        DBG_PRINT("HCIHandler::close: Not open");
        clearAllMgmtEventCallbacks();
        clearConnectionLists();
        comm.close();
        return;
    }
    PERF_TS_T0();
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("HCIHandler::close: Start");
    clearAllMgmtEventCallbacks();
    clearConnectionLists();

    // Interrupt HCIHandler's HCIComm::read(..), avoiding prolonged hang
    // and pull all underlying hci read operations!
    comm.close();

    PERF_TS_TD("HCIHandler::close.1");
    {
        std::unique_lock<std::mutex> lockReader(mtx_hciReaderLifecycle); // RAII-style acquire and relinquish via destructor
        const pthread_t tid_self = pthread_self();
        const pthread_t tid_reader = hciReaderThreadId;
        hciReaderThreadId = 0;
        const bool is_reader = tid_reader == tid_self;
        DBG_PRINT("HCIHandler::close: hciReader[running %d, shallStop %d, isReader %d, tid %p)",
                hciReaderRunning, hciReaderShallStop.load(), is_reader, (void*)tid_reader);
        if( hciReaderRunning ) {
            hciReaderShallStop = true;
            if( !is_reader && 0 != tid_reader ) {
                int kerr;
                if( 0 != ( kerr = pthread_kill(tid_reader, SIGALRM) ) ) {
                    ERR_PRINT("HCIHandler::close: pthread_kill %p FAILED: %d", (void*)tid_reader, kerr);
                }
            }
            // Ensure the reader thread has ended, no runaway-thread using *this instance after destruction
            while( true == hciReaderRunning ) {
                cv_hciReaderInit.wait(lockReader);
            }
        }
    }
    PERF_TS_TD("HCIHandler::close.X");
    DBG_PRINT("HCIHandler::close: End");
}

HCIStatusCode HCIHandler::startAdapter() {
    #ifdef __linux__
        int res;
        if( ( res = ioctl(comm.getSocketDescriptor(), HCIDEVUP, dev_id) ) < 0 ) {
            if (errno != EALREADY) {
                ERR_PRINT("HCIHandler::startAdapter(dev_id %d): FAILED: %d", dev_id, res);
                return HCIStatusCode::INTERNAL_FAILURE;
            }
        }
        return HCIStatusCode::SUCCESS;
    #else
        #warning add implementation
    #endif
    return HCIStatusCode::INTERNAL_FAILURE;
}

HCIStatusCode HCIHandler::stopAdapter() {
    HCIStatusCode status;
    #ifdef __linux__
        int res;
        if( ( res = ioctl(comm.getSocketDescriptor(), HCIDEVDOWN, dev_id) ) < 0) {
            ERR_PRINT("HCIHandler::stopAdapter(dev_id %d): FAILED: %d", dev_id, res);
            status = HCIStatusCode::INTERNAL_FAILURE;
        } else {
            status = HCIStatusCode::SUCCESS;
        }
    #else
        #warning add implementation
        status = HCIStatusCode::INTERNAL_FAILURE;
    #endif
    if( HCIStatusCode::SUCCESS == status ) {
        clearConnectionLists();
    }
    return status;
}

HCIStatusCode HCIHandler::resetAdapter() {
    #ifdef __linux__
        if( HCIStatusCode::SUCCESS == stopAdapter() && HCIStatusCode::SUCCESS == startAdapter() ) {
            return HCIStatusCode::SUCCESS;
        }
    #else
        #warning add implementation
    #endif
    return HCIStatusCode::INTERNAL_FAILURE;
}

HCIStatusCode HCIHandler::reset() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::reset: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    HCICommand req0(HCIOpcode::RESET, 0);
    HCICommandCompleteEvent * ev_cc;
    std::shared_ptr<HCIEvent> ev = sendWithCmdCompleteReply(req0, &ev_cc);
    if( nullptr == ev || nullptr == ev_cc ) {
        return HCIStatusCode::INTERNAL_TIMEOUT; // timeout
    }
    const HCIStatusCode status = ev_cc->getReturnStatus(0);
    if( HCIStatusCode::SUCCESS == status ) {
        clearConnectionLists();
    }
    return status;
}

HCIStatusCode HCIHandler::getLocalVersion(HCILocalVersion &version) noexcept {
    HCICommand req0(HCIOpcode::READ_LOCAL_VERSION, 0);
    const hci_rp_read_local_version * ev_lv;
    HCIStatusCode status;
    std::shared_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_lv, &status);
    if( nullptr == ev || nullptr == ev_lv || HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("HCIHandler::getLocalVersion: READ_LOCAL_VERSION: 0x%x (%s)", number(status), getHCIStatusCodeString(status).c_str());
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
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::le_set_scan_param: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    DBG_PRINT("HCI Scan Param: scan [interval %.3f ms, window %.3f ms]",
            0.625f * (float)le_scan_interval, 0.625f * (float)le_scan_window);

    HCIStructCommand<hci_cp_le_set_scan_param> req0(HCIOpcode::LE_SET_SCAN_PARAM);
    hci_cp_le_set_scan_param * cp = req0.getWStruct();
    cp->type = le_scan_active ? LE_SCAN_ACTIVE : LE_SCAN_PASSIVE;
    cp->interval = jau::cpu_to_le(le_scan_interval);
    cp->window = jau::cpu_to_le(le_scan_window);
    cp->own_address_type = static_cast<uint8_t>(own_mac_type);
    cp->filter_policy = filter_policy;

    const hci_rp_status * ev_status;
    HCIStatusCode status;
    std::shared_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);
    return status;
}

HCIStatusCode HCIHandler::le_enable_scan(const bool enable, const bool filter_dup) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::le_enable_scan: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    HCIStructCommand<hci_cp_le_set_scan_enable> req0(HCIOpcode::LE_SET_SCAN_ENABLE);
    hci_cp_le_set_scan_enable * cp = req0.getWStruct();
    cp->enable = enable ? LE_SCAN_ENABLE : LE_SCAN_DISABLE;
    cp->filter_dup = filter_dup ? LE_SCAN_FILTER_DUP_ENABLE : LE_SCAN_FILTER_DUP_DISABLE;

    const hci_rp_status * ev_status;
    HCIStatusCode status;
    std::shared_ptr<HCIEvent> ev = processCommandComplete(req0, &ev_status, &status);

    if( HCIStatusCode::SUCCESS == status ) {
        // SEND_EVENT: Perform off-thread to avoid potential deadlock w/ application callbacks (similar when sent from HCIHandler's reader-thread)
        std::thread bg(&HCIHandler::sendMgmtEvent, this, std::shared_ptr<MgmtEvent>( new MgmtEvtDiscovering(dev_id, ScanType::LE, enable) ) ); // @suppress("Invalid arguments")
        bg.detach();
        // sendMgmtEvent(std::shared_ptr<MgmtEvent>( new MgmtEvtDiscovering(dev_id, ScanType::LE, enable) ) );
    }
    return status;
}

HCIStatusCode HCIHandler::le_create_conn(const EUI48 &peer_bdaddr,
                            const HCILEPeerAddressType peer_mac_type,
                            const HCILEOwnAddressType own_mac_type,
                            const uint16_t le_scan_interval, const uint16_t le_scan_window,
                            const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                            const uint16_t conn_latency, const uint16_t supervision_timeout) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::le_create_conn: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    const uint16_t min_ce_length = 0x0000;
    const uint16_t max_ce_length = 0x0000;
    const uint8_t initiator_filter = 0x00; // whitelist not used but peer_bdaddr*

    DBG_PRINT("HCI Conn Param: scan [interval %.3f ms, window %.3f ms]", 0.625f *
            (float)le_scan_interval, 0.625f * (float)le_scan_window);
    DBG_PRINT("HCI Conn Param: conn [interval [%.3f ms - %.3f ms], latency %d, sup_timeout %d ms]",
            1.25f * (float)conn_interval_min, 1.25f * (float)conn_interval_max,
            conn_latency, supervision_timeout*10);

    HCIStructCommand<hci_cp_le_create_conn> req0(HCIOpcode::LE_CREATE_CONN);
    hci_cp_le_create_conn * cp = req0.getWStruct();
    cp->scan_interval = jau::cpu_to_le(le_scan_interval);
    cp->scan_window = jau::cpu_to_le(le_scan_window);
    cp->filter_policy = initiator_filter;
    cp->peer_addr_type = static_cast<uint8_t>(peer_mac_type);
    cp->peer_addr = peer_bdaddr;
    cp->own_address_type = static_cast<uint8_t>(own_mac_type);
    cp->conn_interval_min = jau::cpu_to_le(conn_interval_min);
    cp->conn_interval_max = jau::cpu_to_le(conn_interval_max);
    cp->conn_latency = jau::cpu_to_le(conn_latency);
    cp->supervision_timeout = jau::cpu_to_le(supervision_timeout);
    cp->min_ce_len = jau::cpu_to_le(min_ce_length);
    cp->max_ce_len = jau::cpu_to_le(max_ce_length);

    BDAddressType bdAddrType = getBDAddressType(peer_mac_type);

    HCIConnectionRef disconn = findDisconnect(peer_bdaddr, bdAddrType);
    if( nullptr != disconn ) {
        DBG_PRINT("HCIHandler::le_create_conn: disconnect pending %s", disconn->toString().c_str());
        const int32_t poll_period = 250; // milliseconds
        int32_t td = 0;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && nullptr != disconn ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_period));
            td += poll_period;
            disconn = findDisconnect(peer_bdaddr, bdAddrType);
        }
        if( nullptr != disconn ) {
            WARN_PRINT("HCIHandler::le_create_conn: disconnect persisting after %d ms poll: %s", td, disconn->toString().c_str());
        } else {
            DBG_PRINT("HCIHandler::le_create_conn: disconnect resolved after %d ms poll", td);
        }
    }
    addOrUpdateTrackerConnection(peer_bdaddr, bdAddrType, 0);
    HCIStatusCode status;
    std::shared_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
    if( HCIStatusCode::CONNECTION_ALREADY_EXISTS == status ) {
        const std::string s0 = nullptr != disconn ? disconn->toString() : "null";
        WARN_PRINT("HCIHandler::le_create_conn: %s: disconnect pending: %s", getHCIStatusCodeString(status).c_str(), s0.c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::create_conn(const EUI48 &bdaddr,
                                     const uint16_t pkt_type,
                                     const uint16_t clock_offset, const uint8_t role_switch) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::create_conn: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    HCIStructCommand<hci_cp_create_conn> req0(HCIOpcode::CREATE_CONN);
    hci_cp_create_conn * cp = req0.getWStruct();
    cp->bdaddr = bdaddr;
    cp->pkt_type = jau::cpu_to_le((uint16_t)(pkt_type & (uint16_t)ACL_PTYPE_MASK)); /* TODO OK excluding SCO_PTYPE_MASK   (HCI_HV1 | HCI_HV2 | HCI_HV3) ? */
    cp->pscan_rep_mode = 0x02; /* TODO magic? */
    cp->pscan_mode = 0x00; /* TODO magic? */
    cp->clock_offset = jau::cpu_to_le(clock_offset);
    cp->role_switch = role_switch;

    HCIConnectionRef disconn = findDisconnect(bdaddr, BDAddressType::BDADDR_BREDR);
    if( nullptr != disconn ) {
        DBG_PRINT("HCIHandler::create_conn: disconnect pending %s", disconn->toString().c_str());
        const int32_t poll_period = 250; // milliseconds
        int32_t td = 0;
        while( env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td && nullptr != disconn ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_period));
            td += poll_period;
            disconn = findDisconnect(bdaddr, BDAddressType::BDADDR_BREDR);
        }
        if( nullptr != disconn ) {
            WARN_PRINT("HCIHandler::create_conn: disconnect persisting after %d ms poll: %s", td, disconn->toString().c_str());
        } else {
            DBG_PRINT("HCIHandler::create_conn: disconnect resolved after %d ms poll", td);
        }
    }
    addOrUpdateTrackerConnection(bdaddr, BDAddressType::BDADDR_BREDR, 0);
    HCIStatusCode status;
    std::shared_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
    if( HCIStatusCode::CONNECTION_ALREADY_EXISTS == status ) {
        const std::string s0 = nullptr != disconn ? disconn->toString() : "null";
        WARN_PRINT("HCIHandler::create_conn: %s: disconnect pending: %s", getHCIStatusCodeString(status).c_str(), s0.c_str());
    }
    return status;
}

HCIStatusCode HCIHandler::disconnect(const uint16_t conn_handle, const EUI48 &peer_bdaddr, const BDAddressType peer_mac_type,
                                     const HCIStatusCode reason) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock(mtx); // RAII-style acquire and relinquish via destructor
    if( !comm.isOpen() ) {
        ERR_PRINT("HCIHandler::create_conn: device not open");
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    if( 0 == conn_handle ) {
        ERR_PRINT("HCIHandler::disconnect: Null conn_handle given address[%s, %s] (drop)",
                   peer_bdaddr.toString().c_str(), getBDAddressTypeString(peer_mac_type).c_str());
        return HCIStatusCode::INVALID_HCI_COMMAND_PARAMETERS;
    }
    HCIConnectionRef conn;
    {
        const std::lock_guard<std::recursive_mutex> lock(mtx_connectionList); // RAII-style acquire and relinquish via destructor
        conn = findTrackerConnection(conn_handle);
        if( nullptr == conn ) {
            // disconnect called w/o being connected through this HCIHandler
            conn = addOrUpdateTrackerConnection(peer_bdaddr, peer_mac_type, conn_handle);
            WORDY_PRINT("HCIHandler::disconnect: Not tracked address[%s, %s], added %s",
                       peer_bdaddr.toString().c_str(), getBDAddressTypeString(peer_mac_type).c_str(),
                       conn->toString().c_str());
        } else if( !conn->equals(peer_bdaddr, peer_mac_type) ) {
            ERR_PRINT("HCIHandler::disconnect: Mismatch given address[%s, %s] and tracked %s (drop)",
                       peer_bdaddr.toString().c_str(), getBDAddressTypeString(peer_mac_type).c_str(),
                       conn->toString().c_str());
            return HCIStatusCode::INVALID_HCI_COMMAND_PARAMETERS;
        }
    }
    DBG_PRINT("HCIHandler::disconnect: address[%s, %s], handle %s, %s",
               peer_bdaddr.toString().c_str(), getBDAddressTypeString(peer_mac_type).c_str(),
               jau::uint16HexString(conn_handle).c_str(),
               conn->toString().c_str());

    HCIStatusCode status;

    // Always issue DISCONNECT command, even in case of an ioError (lost-connection),
    // see Issue #124 fast re-connect on CSR adapter.
    // This will always notify the adapter of a disconnected device.
    {
        HCIStructCommand<hci_cp_disconnect> req0(HCIOpcode::DISCONNECT);
        hci_cp_disconnect * cp = req0.getWStruct();
        cp->handle = jau::cpu_to_le(conn_handle);
        cp->reason = number(reason);

        std::shared_ptr<HCIEvent> ev = processCommandStatus(req0, &status);
    }
    if( HCIStatusCode::SUCCESS == status ) {
        addOrUpdateDisconnect(peer_bdaddr, peer_mac_type, conn_handle);
    }
    return status;
}

std::shared_ptr<HCIEvent> HCIHandler::processCommandStatus(HCICommand &req, HCIStatusCode *status) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor

    *status = HCIStatusCode::INTERNAL_FAILURE;

    int32_t retryCount = 0;
    std::shared_ptr<HCIEvent> ev = nullptr;

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
            DBG_PRINT("HCIHandler::processCommandStatus %s -> Status 0x%2.2X (%s), errno %d %s: res %s, req %s",
                    getHCIOpcodeString(req.getOpcode()).c_str(),
                    number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                    ev_cs->toString().c_str(), req.toString().c_str());
            break; // gotcha, leave loop - pending completion result handled via callback
        } else {
            retryCount++;
            DBG_PRINT("HCIHandler::processCommandStatus: !CMD_STATUS (drop, retry %d): res %s; req %s",
                       retryCount, ev->toString().c_str(), req.toString().c_str());
            continue; // next packet
        }
    }
    if( nullptr == ev ) {
        // timeout exit
        WARN_PRINT("HCIHandler::processCommandStatus %s -> Status 0x%2.2X (%s), errno %d %s: res nullptr, req %s",
                getHCIOpcodeString(req.getOpcode()).c_str(),
                number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                req.toString().c_str());
    }

exit:
    return ev;
}

template<typename hci_cmd_event_struct>
std::shared_ptr<HCIEvent> HCIHandler::processCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status) noexcept
{
    *res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    const HCIEventType evc = HCIEventType::CMD_COMPLETE;
    HCICommandCompleteEvent * ev_cc;
    std::shared_ptr<HCIEvent> ev = sendWithCmdCompleteReply(req, &ev_cc);
    if( nullptr == ev ) {
        *status = HCIStatusCode::INTERNAL_TIMEOUT;
        WARN_PRINT("HCIHandler::processCommandComplete %s -> %s: Status 0x%2.2X (%s), errno %d %s: res nullptr, req %s",
                getHCIOpcodeString(req.getOpcode()).c_str(), getHCIEventTypeString(evc).c_str(),
                number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                req.toString().c_str());
        return nullptr; // timeout
    } else if( nullptr == ev_cc ) {
        WARN_PRINT("HCIHandler::processCommandComplete %s -> %s: Status 0x%2.2X (%s), errno %d %s: res %s, req %s",
                getHCIOpcodeString(req.getOpcode()).c_str(), getHCIEventTypeString(evc).c_str(),
                number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                ev->toString().c_str(), req.toString().c_str());
        return ev;
    }
    const uint8_t returnParamSize = ev_cc->getReturnParamSize();
    if( returnParamSize < sizeof(hci_cmd_event_struct) ) {
        WARN_PRINT("HCIHandler::processCommandComplete %s -> %s: Status 0x%2.2X (%s), errno %d %s: res %s, req %s",
                getHCIOpcodeString(req.getOpcode()).c_str(), getHCIEventTypeString(evc).c_str(),
                number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                ev_cc->toString().c_str(), req.toString().c_str());
        return ev;
    }
    *res = (const hci_cmd_event_struct*)(ev_cc->getReturnParam());
    *status = static_cast<HCIStatusCode>((*res)->status);
    DBG_PRINT("HCIHandler::processCommandComplete %s -> %s: Status 0x%2.2X (%s): res %s, req %s",
            getHCIOpcodeString(req.getOpcode()).c_str(), getHCIEventTypeString(evc).c_str(),
            number(*status), getHCIStatusCodeString(*status).c_str(),
            ev_cc->toString().c_str(), req.toString().c_str());
    return ev;
}

template<typename hci_cmd_event_struct>
const hci_cmd_event_struct* HCIHandler::getReplyStruct(std::shared_ptr<HCIEvent> event, HCIEventType evc, HCIStatusCode *status) noexcept
{
    const hci_cmd_event_struct* res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    typedef HCIStructCmdCompleteEvtWrap<hci_cmd_event_struct> HCITypeCmdCompleteEvtWrap;
    HCITypeCmdCompleteEvtWrap ev_cc( *event.get() );
    if( ev_cc.isTypeAndSizeValid(evc) ) {
        *status = ev_cc.getStatus();
        res = ev_cc.getStruct();
    } else {
        WARN_PRINT("HCIHandler::getReplyStruct: %s: Type or size mismatch: Status 0x%2.2X (%s), errno %d %s: res %s",
                getHCIEventTypeString(evc).c_str(),
                number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                ev_cc.toString().c_str());
    }
    return res;
}

template<typename hci_cmd_event_struct>
const hci_cmd_event_struct* HCIHandler::getMetaReplyStruct(std::shared_ptr<HCIEvent> event, HCIMetaEventType mec, HCIStatusCode *status) noexcept
{
    const hci_cmd_event_struct* res = nullptr;
    *status = HCIStatusCode::INTERNAL_FAILURE;

    typedef HCIStructCmdCompleteMetaEvtWrap<hci_cmd_event_struct> HCITypeCmdCompleteMetaEvtWrap;
    HCITypeCmdCompleteMetaEvtWrap ev_cc( *static_cast<HCIMetaEvent*>( event.get() ) );
    if( ev_cc.isTypeAndSizeValid(mec) ) {
        *status = ev_cc.getStatus();
        res = ev_cc.getStruct();
    } else {
        WARN_PRINT("HCIHandler::getMetaReplyStruct: %s: Type or size mismatch: Status 0x%2.2X (%s), errno %d %s: res %s",
                  getHCIMetaEventTypeString(mec).c_str(),
                  number(*status), getHCIStatusCodeString(*status).c_str(), errno, strerror(errno),
                  ev_cc.toString().c_str());
    }
    return res;
}

/***
 *
 * MgmtEventCallback section
 *
 */

bool HCIHandler::addMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size());
        return false;
    }
    MgmtEventCallbackList &l = mgmtEventCallbackLists[static_cast<uint16_t>(opc)];
    for (auto it = l.begin(); it != l.end(); ++it) {
        if ( *it == cb ) {
            // already exists for given adapter
            return true;
        }
    }
    l.push_back( cb );
    return true;
}
int HCIHandler::removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size());
        return 0;
    }
    int count = 0;
    MgmtEventCallbackList &l = mgmtEventCallbackLists[static_cast<uint16_t>(opc)];
    for (auto it = l.begin(); it != l.end(); ) {
        if ( *it == cb ) {
            it = l.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    return count;
}
void HCIHandler::clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtEventCallbackLists.size());
        return;
    }
    mgmtEventCallbackLists[static_cast<uint16_t>(opc)].clear();
}
void HCIHandler::clearAllMgmtEventCallbacks() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    for(size_t i=0; i<mgmtEventCallbackLists.size(); i++) {
        mgmtEventCallbackLists[i].clear();
    }
}
