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
// PERF3_PRINT_ON for close
// #define PERF3_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "BTIoctl.hpp"

#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "BTTypes1.hpp"
#include "SMPHandler.hpp"

#include "BTAdapter.hpp"
#include "BTManager.hpp"
#include "DBTConst.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
    #include <signal.h>
}

using namespace direct_bt;

MgmtEnv::MgmtEnv() noexcept
: DEBUG_GLOBAL( jau::environment::get("direct_bt").debug ),
  exploding( jau::environment::getExplodingProperties("direct_bt.mgmt") ),
  MGMT_READER_THREAD_POLL_TIMEOUT( jau::environment::getInt32Property("direct_bt.mgmt.reader.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_COMMAND_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.mgmt.cmd.timeout", 3000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_SET_POWER_COMMAND_TIMEOUT( jau::environment::getInt32Property("direct_bt.mgmt.setpower.timeout",
                                                                  std::max(MGMT_COMMAND_REPLY_TIMEOUT, 6000) /* default */,
                                                                  MGMT_COMMAND_REPLY_TIMEOUT /* min */, INT32_MAX /* max */) ),
  MGMT_EVT_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.mgmt.ringsize", 64, 64 /* min */, 1024 /* max */) ),
  DEBUG_EVENT( jau::environment::getBooleanProperty("direct_bt.debug.mgmt.event", false) ),
  MGMT_READ_PACKET_MAX_RETRY( MGMT_EVT_RING_CAPACITY )
{
    // Kick off singleton initialization of all environments.
    HCIEnv::get();
    L2CAPEnv::get();
    BTGattEnv::get();
    SMPEnv::get();
}

const pid_t BTManager::pidSelf = getpid();
std::mutex BTManager::mtx_singleton;

void BTManager::mgmtReaderThreadImpl() noexcept {
    {
        const std::lock_guard<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        mgmtReaderShallStop = false;
        mgmtReaderRunning = true;
        DBG_PRINT("BTManager::reader: Started");
    }
    cv_mgmtReaderInit.notify_all(); // have mutex unlocked before notify_all to avoid pessimistic re-block of notified wait() thread.

    thread_local jau::call_on_release thread_cleanup([&]() {
        DBG_PRINT("BTManager::mgmtReaderThreadCleanup: mgmtReaderRunning %d -> 0", mgmtReaderRunning.load());
        mgmtReaderRunning = false;
        cv_mgmtReaderInit.notify_all();
    });

    while( !mgmtReaderShallStop ) {
        jau::snsize_t len;
        if( !comm.isOpen() ) {
            // not open
            ERR_PRINT("BTManager::reader: Not connected");
            mgmtReaderShallStop = true;
            break;
        }

        len = comm.read(rbuffer.get_wptr(), rbuffer.size(), env.MGMT_READER_THREAD_POLL_TIMEOUT);
        if( 0 < len ) {
            const jau::nsize_t len2 = static_cast<jau::nsize_t>(len);
            const jau::nsize_t paramSize = len2 >= MGMT_HEADER_SIZE ? rbuffer.get_uint16_nc(4) : 0;
            if( len2 < MGMT_HEADER_SIZE + paramSize ) {
                WARN_PRINT("BTManager::reader: length mismatch %zu < MGMT_HEADER_SIZE(%u) + %u", len2, MGMT_HEADER_SIZE, paramSize);
                continue; // discard data
            }
            std::unique_ptr<MgmtEvent> event = MgmtEvent::getSpecialized(rbuffer.get_ptr(), len2);
            const MgmtEvent::Opcode opc = event->getOpcode();
            if( MgmtEvent::Opcode::CMD_COMPLETE == opc || MgmtEvent::Opcode::CMD_STATUS == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV (CMD) %s", event->toString().c_str());
                if( mgmtEventRing.isFull() ) {
                    const jau::nsize_t dropCount = mgmtEventRing.capacity()/4;
                    mgmtEventRing.drop(dropCount);
                    WARN_PRINT("BTManager-IO RECV Drop (%u oldest elements of %u capacity, ring full)", dropCount, mgmtEventRing.capacity());
                }
                mgmtEventRing.putBlocking( std::move( event ) );
            } else if( MgmtEvent::Opcode::INDEX_ADDED == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV (ADD) %s", event->toString().c_str());
                std::thread adapterAddedThread(&BTManager::processAdapterAdded, this, std::move( event) ); // @suppress("Invalid arguments")
                adapterAddedThread.detach();
            } else if( MgmtEvent::Opcode::INDEX_REMOVED == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV (REM) %s", event->toString().c_str());
                std::thread adapterRemovedThread(&BTManager::processAdapterRemoved, this, std::move( event ) ); // @suppress("Invalid arguments")
                adapterRemovedThread.detach();
            } else {
                // issue a callback
                COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV (CB) %s", event->toString().c_str());
                sendMgmtEvent( *event );
            }
        } else if( ETIMEDOUT != errno && !mgmtReaderShallStop ) { // expected exits
            ERR_PRINT("BTManager::reader: HCIComm read error");
        }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        WORDY_PRINT("BTManager::reader: Ended. Ring has %u entries flushed", mgmtEventRing.size());
        mgmtEventRing.clear();
        mgmtReaderRunning = false;
    }
    cv_mgmtReaderInit.notify_all(); // have mutex unlocked before notify_all to avoid pessimistic re-block of notified wait() thread.
}

void BTManager::sendMgmtEvent(const MgmtEvent& event) noexcept {
    const uint16_t dev_id = event.getDevID();
    MgmtAdapterEventCallbackList & mgmtEventCallbackList = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(event.getOpcode())];
    int invokeCount = 0;

    jau::for_each_fidelity(mgmtEventCallbackList, [&](MgmtAdapterEventCallback &cb) {
        if( 0 > cb.getDevID() || dev_id == cb.getDevID() ) {
            try {
                cb.getCallback().invoke(event);
            } catch (std::exception &e) {
                ERR_PRINT("BTManager::sendMgmtEvent-CBs %d/%zd: MgmtAdapterEventCallback %s : Caught exception %s",
                        invokeCount+1, mgmtEventCallbackList.size(),
                        cb.toString().c_str(), e.what());
            }
            invokeCount++;
        }
    });

    COND_PRINT(env.DEBUG_EVENT, "BTManager::sendMgmtEvent: Event %s -> %d/%zd callbacks", event.toString().c_str(), invokeCount, mgmtEventCallbackList.size());
    (void)invokeCount;
}

static void mgmthandler_sigaction(int sig, siginfo_t *info, void *ucontext) noexcept {
    bool pidMatch = info->si_pid == BTManager::pidSelf;
    WORDY_PRINT("BTManager.sigaction: sig %d, info[code %d, errno %d, signo %d, pid %d, uid %d, fd %d], pid-self %d (match %d)",
            sig, info->si_code, info->si_errno, info->si_signo,
            info->si_pid, info->si_uid, info->si_fd,
            BTManager::pidSelf, pidMatch);
    (void)ucontext;

    if( !pidMatch || SIGALRM != sig ) {
        return;
    }
#if 0
    // We do not de-install the handler on single use,
    // as we act for multiple SIGALRM events within direct-bt
    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_handler = SIG_DFL;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = 0;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("BTManager.sigaction: Resetting sighandler");
        }
    }
#endif
}

bool BTManager::send(MgmtCommand &req) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    COND_PRINT(env.DEBUG_EVENT, "BTManager-IO SENT %s", req.toString().c_str());
    jau::TROOctets & pdu = req.getPDU();
    if ( comm.write( pdu.get_ptr(), pdu.size() ) < 0 ) {
        ERR_PRINT("BTManager::sendWithReply: HCIComm write error, req %s", req.toString().c_str());
        return false;
    }
    return true;
}

std::unique_ptr<MgmtEvent> BTManager::sendWithReply(MgmtCommand &req, const int timeoutMS) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    if( !send(req) ) {
        return nullptr;
    }

    // Ringbuffer read is thread safe
    int32_t retryCount = 0;
    while( retryCount < env.MGMT_READ_PACKET_MAX_RETRY ) {
        /* timeoutMS default: env.MGMT_COMMAND_REPLY_TIMEOUT */
        std::unique_ptr<MgmtEvent> res;
        if( !mgmtEventRing.getBlocking(res, timeoutMS) || nullptr == res ) {
            errno = ETIMEDOUT;
            ERR_PRINT("BTManager::sendWithReply.X: nullptr result (timeout -> abort): req %s", req.toString().c_str());
            return nullptr;
        } else if( !res->validate(req) ) {
            // This could occur due to an earlier timeout w/ a nullptr == res (see above),
            // i.e. the pending reply processed here and naturally not-matching.
            COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV sendWithReply: res mismatch (drop evt, retryCount %d): res %s; req %s",
                    retryCount, res->toString().c_str(), req.toString().c_str());
            retryCount++;
        } else {
            COND_PRINT(env.DEBUG_EVENT, "BTManager-IO RECV sendWithReply: res %s; req %s", res->toString().c_str(), req.toString().c_str());
            return res;
        }
    }
    return nullptr;
}

std::unique_ptr<AdapterInfo> BTManager::readAdapterInfo(const uint16_t dev_id) noexcept {
    std::unique_ptr<AdapterInfo> adapterInfo(nullptr); // nullptr
    MgmtCommand req0(MgmtCommand::Opcode::READ_INFO, dev_id);
    {
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getTotalSize() < MgmtEvtAdapterInfo::getRequiredTotalSize()) {
            ERR_PRINT("Insufficient data for adapter info: req %d, res %s", MgmtEvtAdapterInfo::getRequiredTotalSize(), res->toString().c_str());
            goto fail;
        }
        const MgmtEvtAdapterInfo * res1 = static_cast<MgmtEvtAdapterInfo*>(res.get());
        adapterInfo = res1->toAdapterInfo();
        if( dev_id != adapterInfo->dev_id ) {
            ABORT("readAdapterSettings dev_id=%d != dev_id=%d: %s", adapterInfo->dev_id, dev_id, adapterInfo->toString().c_str());
        }
    }
    DBG_PRINT("readAdapterSettings[%d]: End: %s", dev_id, adapterInfo->toString().c_str());

fail:
    return adapterInfo;
}

HCIStatusCode BTManager::initializeAdapter(AdapterInfo& adapterInfo, const uint16_t dev_id,
                                           const BTRole btRole, const BTMode btMode) noexcept {
    /**
     * We set BTManager::defaultIOCapability, i.e. SMPIOCapability::NO_INPUT_NO_OUTPUT,
     * which may be overridden for each connection by BTDevice/BTAdapter!
     *
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.8
     *
     * See SMPTypes.cpp: getPairingMode(const bool le_sc_pairing, const SMPIOCapability ioCap_init, const SMPIOCapability ioCap_resp) noexcept
     */
#if USE_LINUX_BT_SECURITY
    const uint8_t debug_keys = 0;
    const uint8_t ssp_on_param = 0x01; // SET_SSP 0x00 disabled, 0x01 enable Secure Simple Pairing. SSP only available for BREDR >= 2.1 not single-mode LE.
    const uint8_t sc_on_param = 0x01; // SET_SECURE_CONN 0x00 disabled, 0x01 enables SC mixed, 0x02 enables SC only mode
#endif

    AdapterSetting current_settings;
    MgmtCommand req0(MgmtCommand::Opcode::READ_INFO, dev_id);
    {
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getTotalSize() < MgmtEvtAdapterInfo::getRequiredTotalSize()) {
            ERR_PRINT("Insufficient data for adapter info: req %d, res %s", MgmtEvtAdapterInfo::getRequiredTotalSize(), res->toString().c_str());
            goto fail;
        }
        const MgmtEvtAdapterInfo * res1 = static_cast<MgmtEvtAdapterInfo*>(res.get());
        res1->updateAdapterInfo(adapterInfo);
        if( dev_id != adapterInfo.dev_id ) {
            ABORT("initializeAdapter dev_id=%d != dev_id=%d: %s", adapterInfo.dev_id, dev_id, adapterInfo.toString().c_str());
        }
    }
    DBG_PRINT("initializeAdapter[%d, BTMode %s]: Start: %s", dev_id, to_string(btMode).c_str(), adapterInfo.toString().c_str());
    current_settings = adapterInfo.getCurrentSettingMask();

    setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, 0, current_settings);

    switch ( btMode ) {
        case BTMode::DUAL:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 1, current_settings);
            setDiscoverable(dev_id, 0, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 1, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, sc_on_param, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, ssp_on_param, current_settings);
#endif
            break;
        case BTMode::BREDR:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 1, current_settings);
            setDiscoverable(dev_id, 0, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 0, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, ssp_on_param, current_settings);
#endif
            break;
        case BTMode::NONE:
            [[fallthrough]]; // map NONE -> LE
        case BTMode::LE:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 1, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, sc_on_param, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 0, current_settings); // SSP not available in LE single mode
#endif
            break;
    }

#if USE_LINUX_BT_SECURITY
    setMode(dev_id, MgmtCommand::Opcode::SET_DEBUG_KEYS, debug_keys, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_IO_CAPABILITY, direct_bt::number(BTManager::defaultIOCapability), current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, 1, current_settings); // required for pairing
#else
    setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_DEBUG_KEYS, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, 0, current_settings);
#endif

#if 0
    if( BTRole::Slave == btRole ) {
        setMode(dev_id, MgmtCommand::Opcode::SET_CONNECTABLE, 1, current_settings);
    } else {
        setMode(dev_id, MgmtCommand::Opcode::SET_CONNECTABLE, 0, current_settings);
    }
#else
    (void)btRole;
    setMode(dev_id, MgmtCommand::Opcode::SET_CONNECTABLE, 0, current_settings); // '1' not required for BTRole::Slave
#endif
    setMode(dev_id, MgmtCommand::Opcode::SET_FAST_CONNECTABLE, 0, current_settings);

    removeDeviceFromWhitelist(dev_id, BDAddressAndType::ANY_BREDR_DEVICE); // flush whitelist!

    setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, 1, current_settings);

    /**
     * Update AdapterSettings post settings
     */
    if( AdapterSetting::NONE != current_settings ) {
        adapterInfo.setCurrentSettingMask(current_settings);
    } else {
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getTotalSize() < MgmtEvtAdapterInfo::getRequiredTotalSize()) {
            ERR_PRINT("Insufficient data for adapter info: req %d, res %s", MgmtEvtAdapterInfo::getRequiredTotalSize(), res->toString().c_str());
            goto fail;
        }
        const MgmtEvtAdapterInfo * res1 = static_cast<MgmtEvtAdapterInfo*>(res.get());
        res1->updateAdapterInfo(adapterInfo);
        if( dev_id != adapterInfo.dev_id ) {
            ABORT("initializeAdapter dev_id=%d != dev_id=%d: %s", adapterInfo.dev_id, dev_id, adapterInfo.toString().c_str());
        }
    }
    if( !adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        ERR_PRINT("initializeAdapter[%d, BTMode %s]: Fail: Couldn't power-on: %s",
                dev_id, to_string(btMode).c_str(), adapterInfo.toString().c_str());
        goto fail;
    }
    DBG_PRINT("initializeAdapter[%d, BTMode %s]: OK: %s", dev_id, to_string(btMode).c_str(), adapterInfo.toString().c_str());
    return HCIStatusCode::SUCCESS;

fail:
    return HCIStatusCode::FAILED;
}

BTManager::BTManager() noexcept
: env(MgmtEnv::get()),
  rbuffer(ClientMaxMTU, jau::endian::little), comm(HCI_DEV_NONE, HCI_CHANNEL_CONTROL),
  mgmtEventRing(env.MGMT_EVT_RING_CAPACITY), mgmtReaderShallStop(false),
  mgmtReaderThreadId(0), mgmtReaderRunning(false),
  allowClose( comm.isOpen() )
{
    WORDY_PRINT("BTManager.ctor: pid %d", BTManager::pidSelf);
    if( !allowClose ) {
        ERR_PRINT("BTManager::open: Could not open mgmt control channel");
        return;
    }

    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_sigaction = mgmthandler_sigaction;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = SA_SIGINFO;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("BTManager::ctor: Setting sighandler");
        }
    }
    {
        std::unique_lock<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor

        std::thread mgmtReaderThread(&BTManager::mgmtReaderThreadImpl, this); // @suppress("Invalid arguments")
        mgmtReaderThreadId = mgmtReaderThread.native_handle();
        // Avoid 'terminate called without an active exception'
        // as hciReaderThreadImpl may end due to I/O errors.
        mgmtReaderThread.detach();

        while( false == mgmtReaderRunning ) {
            cv_mgmtReaderInit.wait(lock);
        }
    }

    PERF_TS_T0();

    // Mandatory
    {
        MgmtCommand req0(MgmtCommand::Opcode::READ_VERSION, MgmtConstU16::MGMT_INDEX_NONE);
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getDataSize() < 3) {
            ERR_PRINT("Wrong version response: %s", res->toString().c_str());
            goto fail;
        }
        const uint8_t *data = res->getData();
        const uint8_t version = data[0];
        const uint16_t revision = jau::get_uint16(data, 1, true /* littleEndian */);
        WORDY_PRINT("Bluetooth version %d.%d", version, revision);
        if( version < 1 ) {
            ERR_PRINT("Bluetooth version >= 1.0 required");
            goto fail;
        }
    }
    // Optional
    {
        MgmtCommand req0(MgmtCommand::Opcode::READ_COMMANDS, MgmtConstU16::MGMT_INDEX_NONE);
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto next1;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE == res->getOpcode() && res->getDataSize() >= 4) {
            const uint8_t *data = res->getData();
            const uint16_t num_commands = jau::get_uint16(data, 0, true /* littleEndian */);
            const uint16_t num_events = jau::get_uint16(data, 2, true /* littleEndian */);
            WORDY_PRINT("Bluetooth %d commands, %d events", num_commands, num_events);
#ifdef VERBOSE_ON
            const int expDataSize = 4 + num_commands * 2 + num_events * 2;
            if( res->getDataSize() >= expDataSize ) {
                for(int i=0; i< num_commands; i++) {
                    const MgmtCommand::Opcode op = static_cast<MgmtCommand::Opcode>( get_uint16(data, 4+i*2, true /* littleEndian */) );
                    DBG_PRINT("kernel op %d: %s", i, toString(op).c_str());
                }
            }
#endif
        }
    }

next1:
    // Mandatory
    {
        MgmtCommand req0(MgmtCommand::Opcode::READ_INDEX_LIST, MgmtConstU16::MGMT_INDEX_NONE);
        std::unique_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getDataSize() < 2) {
            ERR_PRINT("Insufficient data for adapter index: res %s", res->toString().c_str());
            goto fail;
        }
        const uint8_t *data = res->getData();
        const uint16_t num_adapter = jau::get_uint16(data, 0, true /* littleEndian */);
        WORDY_PRINT("Bluetooth %d adapter", num_adapter);

        const jau::nsize_t expDataSize = 2 + num_adapter * 2;
        if( res->getDataSize() < expDataSize ) {
            ERR_PRINT("Insufficient data for %d adapter indices: res %s", num_adapter, res->toString().c_str());
            goto fail;
        }
        for(int i=0; i < num_adapter; i++) {
            const uint16_t dev_id = jau::get_uint16(data, 2+i*2, true /* littleEndian */);
            std::unique_ptr<AdapterInfo> adapterInfo = readAdapterInfo(dev_id);
            if( nullptr != adapterInfo ) {
                std::shared_ptr<BTAdapter> adapter = BTAdapter::make_shared(*this, *adapterInfo);
                adapters.push_back( adapter );
                adapterIOCapability.push_back(BTManager::defaultIOCapability);
                DBG_PRINT("BTManager::adapters %d/%d: dev_id %d: %s", i, num_adapter, dev_id, adapter->toString().c_str());
            } else {
                DBG_PRINT("BTManager::adapters %d/%d: dev_id %d: FAILED", i, num_adapter, dev_id);
            }
        }
    }

    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_SETTINGS,  jau::bindMemberFunc(this, &BTManager::mgmtEvNewSettingsCB));

    if( jau::environment::get().debug ) {
        addMgmtEventCallback(-1, MgmtEvent::Opcode::CONTROLLER_ERROR, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::CLASS_OF_DEV_CHANGED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_LINK_KEY, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::CONNECT_FAILED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::AUTH_FAILED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_FOUND, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_BLOCKED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNBLOCKED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::PASSKEY_NOTIFY, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_IRK, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CSRK, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_ADDED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_REMOVED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CONN_PARAM, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));

        addMgmtEventCallback(-1, MgmtEvent::Opcode::LOCAL_OOB_DATA_UPDATED, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE, jau::bindMemberFunc(this, &BTManager::mgmtEventAnyCB));
    }
    PERF_TS_TD("BTManager::ctor.ok");
    DBG_PRINT("BTManager::ctor: OK");
    return;

fail:
    close();
    PERF_TS_TD("BTManager::ctor.fail");
    DBG_PRINT("BTManager::ctor: FAIL");
    return;
}

void BTManager::close() noexcept {
    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowClose.compare_exchange_strong(expConn, false) ) {
        // not open
        DBG_PRINT("BTManager::close: Not open");
        whitelist.clear();
        clearAllCallbacks();
        adapters.clear();
        adapterIOCapability.clear();
        comm.close();
        return;
    }
    PERF3_TS_T0();

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("BTManager::close: Start");
    removeAllDevicesFromWhitelist();
    clearAllCallbacks();

    {
        int i=0;
        jau::for_each_fidelity(adapters, [&](std::shared_ptr<BTAdapter> & a) {
            DBG_PRINT("BTManager::close -> adapter::close(): %d/%d processing: %s", i, adapters.size(), a->toString().c_str());
            a->close(); // also issues removeMgmtEventCallback(dev_id);
            ++i;
        });
    }

    adapters.clear();
    adapterIOCapability.clear();

    // Interrupt BTManager's HCIComm::read(..), avoiding prolonged hang
    // and pull all underlying hci read operations!
    comm.close();

    PERF3_TS_TD("BTManager::close.1");
    {
        std::unique_lock<std::mutex> lockReader(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        const pthread_t tid_self = pthread_self();
        const pthread_t tid_reader = mgmtReaderThreadId;
        mgmtReaderThreadId = 0;
        const bool is_reader = tid_reader == tid_self;
        DBG_PRINT("BTManager::close: mgmtReader[running %d, shallStop %d, isReader %d, tid %p)",
                mgmtReaderRunning.load(), mgmtReaderShallStop.load(), is_reader, (void*)tid_reader);
        if( mgmtReaderRunning ) {
            mgmtReaderShallStop = true;
            if( !is_reader && 0 != tid_reader ) {
                int kerr;
                if( 0 != ( kerr = pthread_kill(tid_reader, SIGALRM) ) ) {
                    ERR_PRINT("BTManager::close: pthread_kill %p FAILED: %d", (void*)tid_reader, kerr);
                }
            }
            // Ensure the reader thread has ended, no runaway-thread using *this instance after destruction
            while( true == mgmtReaderRunning ) {
                std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                std::cv_status s = cv_mgmtReaderInit.wait_until(lockReader, t0 + std::chrono::milliseconds(THREAD_SHUTDOWN_TIMEOUT_MS));
                if( std::cv_status::timeout == s && true == mgmtReaderRunning ) {
                    ERR_PRINT("BTManager::close::mgmtReader: Timeout: %s", toString().c_str());
                }
            }
        }
    }
    PERF3_TS_TD("BTManager::close.2");

    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_handler = SIG_DFL;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = 0;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("BTManager.sigaction: Resetting sighandler");
        }
    }

    PERF3_TS_TD("BTManager::close.X");
    DBG_PRINT("BTManager::close: End");
}

std::shared_ptr<BTAdapter> BTManager::getDefaultAdapter() const noexcept {
    typename adapters_t::const_iterator it = adapters.cbegin();
    for (; !it.is_end(); ++it) {
        if( (*it)->isPowered() ) {
            return *it;
        }
    }
    return nullptr;
}

std::shared_ptr<BTAdapter> BTManager::getAdapter(const uint16_t dev_id) const noexcept {
    typename adapters_t::const_iterator it = adapters.cbegin();
    for (; !it.is_end(); ++it) {
        if ( (*it)->dev_id == dev_id ) {
            return *it;
        }
    }
    return nullptr;
}

std::shared_ptr<BTAdapter> BTManager::addAdapter(const AdapterInfo& ai ) noexcept {
    typename adapters_t::iterator it = adapters.begin(); // lock mutex and copy_store
    for (; !it.is_end(); ++it) {
        if ( (*it)->dev_id == ai.dev_id ) {
            break;
        }
    }
    if( it.is_end() ) {
        // new entry
        std::shared_ptr<BTAdapter> adapter = BTAdapter::make_shared(*this, ai);
        it.push_back( adapter );
        adapterIOCapability.push_back(BTManager::defaultIOCapability);
        DBG_PRINT("BTManager::addAdapter: Adding new: %s", adapter->toString().c_str())
        it.write_back();
        return adapter;
    } else {
        // already existing
        std::shared_ptr<BTAdapter> adapter = *it;
        WARN_PRINT("BTManager::addAdapter: Already existing %s, overwriting %s", ai.toString().c_str(), adapter->toString().c_str())
        adapter->adapterInfo = ai;
        return adapter;
    }
}

std::shared_ptr<BTAdapter> BTManager::removeAdapter(const uint16_t dev_id) noexcept {
    typename adapters_t::iterator it = adapters.begin(); // lock mutex and copy_store
    for(; !it.is_end(); ++it ) {
        std::shared_ptr<BTAdapter> & ai = *it;
        if( ai->dev_id == dev_id ) {
            adapterIOCapability.erase( adapterIOCapability.cbegin() + it.dist_begin() );
            std::shared_ptr<BTAdapter> res = ai; // copy
            DBG_PRINT("BTManager::removeAdapter: Remove: %s", res->toString().c_str())
            it.erase();
            it.write_back();
            return res;
        }
    }
    DBG_PRINT("BTManager::removeAdapter: Not found: dev_id %d", dev_id)
    return nullptr;
}

bool BTManager::removeAdapter(BTAdapter* adapter) noexcept {
    typename adapters_t::iterator it = adapters.begin(); // lock mutex and copy_store
    for(; !it.is_end(); ++it ) {
        std::shared_ptr<BTAdapter> & ai = *it;
        if( ai.get() == adapter ) {
            adapterIOCapability.erase( adapterIOCapability.cbegin() + it.dist_begin() );
            DBG_PRINT("BTManager::removeAdapter: Remove: %p -> %s", adapter, ai->toString().c_str())
            it.erase();
            it.write_back();
            return true;
        }
    }
    DBG_PRINT("BTManager::removeAdapter: Not found: %p", adapter)
    return false;
}

bool BTManager::setIOCapability(const uint16_t dev_id, const SMPIOCapability io_cap, SMPIOCapability& pre_io_cap) noexcept {
    if( SMPIOCapability::UNSET != io_cap ) {
#if USE_LINUX_BT_SECURITY
        typename adapters_t::const_iterator it = adapters.cbegin();
        for (; !it.is_end(); ++it) {
            if( (*it)->dev_id == dev_id ) {
                const typename adapters_t::difference_type index = it.dist_begin();
                const SMPIOCapability o = adapterIOCapability.at(index);
                AdapterSetting current_settings { AdapterSetting::NONE }; // throw away return value, unchanged on SET_IO_CAPABILITY
                if( setMode(dev_id, MgmtCommand::Opcode::SET_IO_CAPABILITY, direct_bt::number(io_cap), current_settings) ) {
                    adapterIOCapability.at(index) = io_cap;
                    pre_io_cap = o;
                    return true;
                } else {
                    return false;
                }
            }
        }
#endif
    }
    return false;
}

SMPIOCapability BTManager::getIOCapability(const uint16_t dev_id) const noexcept {
    typename adapters_t::const_iterator it = adapters.cbegin();
    for (; !it.is_end(); ++it) {
        if( (*it)->dev_id == dev_id ) {
            return adapterIOCapability.at( it.dist_begin() );
        }
    }
    return SMPIOCapability::UNSET;
}

bool BTManager::setMode(const uint16_t dev_id, const MgmtCommand::Opcode opc, const uint8_t mode, AdapterSetting& current_settings) noexcept {
    const int timeoutMS = MgmtCommand::Opcode::SET_POWERED == opc ? env.MGMT_SET_POWER_COMMAND_TIMEOUT : env.MGMT_COMMAND_REPLY_TIMEOUT;
    MgmtUint8Cmd req(opc, dev_id, mode);
    std::unique_ptr<MgmtEvent> reply = sendWithReply(req, timeoutMS);
    MgmtStatus res;
    if( nullptr != reply ) {
        if( reply->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            const MgmtEvtCmdComplete &reply1 = *static_cast<const MgmtEvtCmdComplete *>(reply.get());
            res = reply1.getStatus();
            if( MgmtStatus::SUCCESS == res ) {
                reply1.getCurrentSettings(current_settings);
            }
        } else if( reply->getOpcode() == MgmtEvent::Opcode::CMD_STATUS ) {
            const MgmtEvtCmdStatus &reply1 = *static_cast<const MgmtEvtCmdStatus *>(reply.get());
            res = reply1.getStatus();
        } else {
            res = MgmtStatus::UNKNOWN_COMMAND;
        }
    } else {
        res = MgmtStatus::TIMEOUT;
    }
    DBG_PRINT("BTManager::setMode[%d, %s]: %s, result %s %s", dev_id,
            MgmtCommand::getOpcodeString(opc).c_str(), jau::to_hexstring(mode).c_str(),
            to_string(res).c_str(), to_string(current_settings).c_str());
    return MgmtStatus::SUCCESS == res;
}

MgmtStatus BTManager::setDiscoverable(const uint16_t dev_id, const uint8_t state, const uint16_t timeout_sec, AdapterSetting& current_settings) noexcept {
    MgmtSetDiscoverableCmd req(dev_id, state, timeout_sec);
    std::unique_ptr<MgmtEvent> reply = sendWithReply(req);
    MgmtStatus res;
    if( nullptr != reply ) {
        if( reply->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            const MgmtEvtCmdComplete &reply1 = *static_cast<const MgmtEvtCmdComplete *>(reply.get());
            res = reply1.getStatus();
            if( MgmtStatus::SUCCESS == res ) {
                reply1.getCurrentSettings(current_settings);
            }
        } else if( reply->getOpcode() == MgmtEvent::Opcode::CMD_STATUS ) {
            const MgmtEvtCmdStatus &reply1 = *static_cast<const MgmtEvtCmdStatus *>(reply.get());
            res = reply1.getStatus();
        } else {
            res = MgmtStatus::UNKNOWN_COMMAND;
        }
    } else {
        res = MgmtStatus::TIMEOUT;
    }
    DBG_PRINT("BTManager::setDiscoverable[%d]: %s, result %s %s", dev_id,
            req.toString().c_str(), to_string(res).c_str(), to_string(current_settings).c_str());
    return res;
}

bool BTManager::uploadConnParam(const uint16_t dev_id, const BDAddressAndType & addressAndType,
                                 const uint16_t conn_min_interval, const uint16_t conn_max_interval,
                                 const uint16_t conn_latency, const uint16_t supervision_timeout) noexcept {
    MgmtConnParam connParam{ addressAndType.address, addressAndType.type, conn_min_interval, conn_max_interval, conn_latency, supervision_timeout };
    MgmtLoadConnParamCmd req(dev_id, connParam);
    std::unique_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return MgmtStatus::SUCCESS == res1.getStatus();
    }
    return false;
}

bool BTManager::isValidLongTermKeyAddressAndType(const EUI48 &address, const BDAddressType &address_type) const noexcept {
#if USE_LINUX_BT_SECURITY
    // Linux Kernel `load_long_term_keys(..)` (mgmt.c) require either `BDAddressType::BDADDR_LE_PUBLIC` or
    // `BDAddressType::BDADDR_LE_RANDOM` and `BLERandomAddressType::STATIC_PUBLIC`
    // in ltk_is_valid(..) (mgmt.c).
    if( BDAddressType::BDADDR_LE_PUBLIC == address_type ) {
        return true;
    } else if( BDAddressType::BDADDR_LE_RANDOM == address_type &&
               BLERandomAddressType::STATIC_PUBLIC == BDAddressAndType::getBLERandomAddressType(address, address_type) ) {
        return true;
    } else {
        return false;
    }
#else
    return true;
#endif
}

HCIStatusCode BTManager::uploadLongTermKey(const uint16_t dev_id, const MgmtLongTermKeyInfo &key) noexcept {
#if USE_LINUX_BT_SECURITY
    const bool is_valid_ltk_addr = isValidLongTermKeyAddressAndType(key.address, key.address_type);
    MgmtLoadLongTermKeyCmd req(dev_id, key);
    HCIStatusCode res;
    std::unique_ptr<MgmtEvent> reply = sendWithReply(req);
    if( nullptr != reply ) {
        if( reply->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            res = to_HCIStatusCode( static_cast<const MgmtEvtCmdComplete *>(reply.get())->getStatus() );
        } else if( reply->getOpcode() == MgmtEvent::Opcode::CMD_STATUS ) {
            res = to_HCIStatusCode( static_cast<const MgmtEvtCmdStatus *>(reply.get())->getStatus() );
        } else {
            res = HCIStatusCode::UNKNOWN_HCI_COMMAND;
        }
    } else {
        res = HCIStatusCode::TIMEOUT;
    }
    if( HCIStatusCode::SUCCESS != res ) {
        WARN_PRINT("(dev_id %d): %s (valid_ltk_addr %d), result %s", dev_id,
                req.toString().c_str(), is_valid_ltk_addr, to_string(res).c_str());
    } else {
        DBG_PRINT("BTManager::uploadLongTermKeyInfo(dev_id %d): %s (valid_ltk_addr %d), result %s", dev_id,
                req.toString().c_str(), is_valid_ltk_addr, to_string(res).c_str());
    }
    return res;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

HCIStatusCode BTManager::uploadLongTermKey(const uint16_t dev_id, const BDAddressAndType & addressAndType,
                                                const SMPLongTermKey& ltk) noexcept {
#if USE_LINUX_BT_SECURITY
    const MgmtLTKType key_type = to_MgmtLTKType(ltk.properties);
    const MgmtLongTermKeyInfo mgmt_ltk_info { addressAndType.address, addressAndType.type, key_type,
                                              ltk.isResponder(), ltk.enc_size, ltk.ediv, ltk.rand, ltk.ltk };
    return uploadLongTermKey(dev_id, mgmt_ltk_info);
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

HCIStatusCode BTManager::uploadLinkKey(const uint16_t dev_id, const MgmtLinkKeyInfo &key) noexcept {
#if USE_LINUX_BT_SECURITY
    MgmtLoadLinkKeyCmd req(dev_id, false /* debug_keys */, key);
    HCIStatusCode res;
    std::unique_ptr<MgmtEvent> reply = sendWithReply(req);
    if( nullptr != reply ) {
        if( reply->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            res = to_HCIStatusCode( static_cast<const MgmtEvtCmdComplete *>(reply.get())->getStatus() );
        } else if( reply->getOpcode() == MgmtEvent::Opcode::CMD_STATUS ) {
            res = to_HCIStatusCode( static_cast<const MgmtEvtCmdStatus *>(reply.get())->getStatus() );
        } else {
            res = HCIStatusCode::UNKNOWN_HCI_COMMAND;
        }
    } else {
        res = HCIStatusCode::TIMEOUT;
    }
    if( HCIStatusCode::SUCCESS != res ) {
        WARN_PRINT("(dev_id %d): %s, result %s", dev_id,
                req.toString().c_str(), to_string(res).c_str());
    } else {
        DBG_PRINT("BTManager::uploadLinkKeyInfo(dev_id %d): %s, result %s", dev_id,
                req.toString().c_str(), to_string(res).c_str());
    }
    return res;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

HCIStatusCode BTManager::uploadLinkKey(const uint16_t dev_id, const BDAddressAndType & addressAndType, const SMPLinkKey& lk) noexcept {
#if USE_LINUX_BT_SECURITY
    const MgmtLinkKeyInfo mgmt_lk_info { addressAndType.address, addressAndType.type, static_cast<MgmtLinkKeyType>(lk.type),
                                         lk.key, lk.pin_length };
    return uploadLinkKey(dev_id, mgmt_lk_info);
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

MgmtStatus BTManager::userPasskeyReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const uint32_t passkey) noexcept {
#if USE_LINUX_BT_SECURITY
    MgmtUserPasskeyReplyCmd cmd(dev_id, addressAndType, passkey);
    std::unique_ptr<MgmtEvent> res = sendWithReply(cmd);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
#else
    return MgmtStatus::NOT_SUPPORTED;
#endif
}

MgmtStatus BTManager::userPasskeyNegativeReply(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept {
#if USE_LINUX_BT_SECURITY
    MgmtUserPasskeyNegativeReplyCmd cmd(dev_id, addressAndType);
    std::unique_ptr<MgmtEvent> res = sendWithReply(cmd);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
#else
    return MgmtStatus::NOT_SUPPORTED;
#endif
}

MgmtStatus BTManager::userConfirmReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool positive) noexcept {
#if USE_LINUX_BT_SECURITY
    std::unique_ptr<MgmtEvent> res;
    if( positive ) {
        MgmtUserConfirmReplyCmd cmd(dev_id, addressAndType);
        res = sendWithReply(cmd);
    } else {
        MgmtUserConfirmNegativeReplyCmd cmd(dev_id, addressAndType);
        res = sendWithReply(cmd);
    }
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
#else
    return MgmtStatus::NOT_SUPPORTED;
#endif
}

HCIStatusCode BTManager::unpairDevice(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool disconnect) noexcept {
#if USE_LINUX_BT_SECURITY
    MgmtUnpairDeviceCmd cmd(dev_id, addressAndType, disconnect);
    std::unique_ptr<MgmtEvent> res = sendWithReply(cmd);

    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return to_HCIStatusCode( res1.getStatus() );
    }
    return HCIStatusCode::TIMEOUT;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

bool BTManager::isDeviceWhitelisted(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept {
    auto it = whitelist.cbegin();
    for( auto end = whitelist.cend(); it != end; ++it) {
        std::shared_ptr<WhitelistElem> wle = *it;
        if( wle->dev_id == dev_id && wle->address_and_type == addressAndType ) {
            return true;
        }
    }
    return false;
}

bool BTManager::addDeviceToWhitelist(const uint16_t dev_id, const BDAddressAndType & addressAndType, const HCIWhitelistConnectType ctype) noexcept {
    MgmtAddDeviceToWhitelistCmd req(dev_id, addressAndType, ctype);

    // Check if already exist in our local whitelist first, reject if so ..
    if( isDeviceWhitelisted(dev_id, addressAndType) ) {
        ERR_PRINT("BTManager::addDeviceToWhitelist: Already in local whitelist, remove first: %s", req.toString().c_str());
        return false;
    }
    std::unique_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            whitelist.push_back( std::make_shared<WhitelistElem>(dev_id, addressAndType, ctype) );
            return true;
        }
    }
    return false;
}

int BTManager::removeAllDevicesFromWhitelist() noexcept {
#if 0
    jau::darray<std::shared_ptr<WhitelistElem>> whitelist_copy = whitelist;
    int count = 0;
    DBG_PRINT("BTManager::removeAllDevicesFromWhitelist.A: Start %zd elements", whitelist_copy.size());

    for(auto it = whitelist_copy.cbegin(); it != whitelist_copy.cend(); ++it) {
        std::shared_ptr<WhitelistElem> wle = *it;
        removeDeviceFromWhitelist(wle->dev_id, wle->address, wle->address_type);
        ++count;
    }
#else
    int count = 0;
    DBG_PRINT("BTManager::removeAllDevicesFromWhitelist.B: Start %d elements", count);
    whitelist.clear();
    jau::for_each_const(adapters, [&](const std::shared_ptr<BTAdapter> & a) {
        if( removeDeviceFromWhitelist(a->dev_id, BDAddressAndType::ANY_BREDR_DEVICE) ) { // flush whitelist!
            ++count;
        }
    });
#endif

    DBG_PRINT("BTManager::removeAllDevicesFromWhitelist: End: Removed %d elements, remaining %zd elements",
            count, whitelist.size());
    return count;
}

bool BTManager::removeDeviceFromWhitelist(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept {
    // Remove from our local whitelist first
    {
        auto it = whitelist.cbegin();
        for( auto end = whitelist.cend(); it != end; ) {
            std::shared_ptr<WhitelistElem> wle = *it;
            if( wle->dev_id == dev_id && wle->address_and_type == addressAndType ) {
                it = whitelist.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Actual removal
    MgmtRemoveDeviceFromWhitelistCmd req(dev_id, addressAndType);
    std::unique_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<ConnectionInfo> BTManager::getConnectionInfo(const uint16_t dev_id, const BDAddressAndType& addressAndType) noexcept {
    MgmtGetConnectionInfoCmd req(dev_id, addressAndType);
    std::unique_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<ConnectionInfo> result = res1.toConnectionInfo();
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<NameAndShortName> BTManager::setLocalName(const uint16_t dev_id, const std::string & name, const std::string & short_name) noexcept {
    MgmtSetLocalNameCmd req (static_cast<uint16_t>(dev_id), name, short_name);
    std::unique_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<NameAndShortName> result = res1.toNameAndShortName();

            // explicit LocalNameChanged event
            MgmtEvtLocalNameChanged e(static_cast<uint16_t>(dev_id), result->getName(), result->getShortName());
            sendMgmtEvent(e);
            return result;
        }
    }
    return nullptr;
}

/***
 *
 * MgmtEventCallback section
 *
 */

static MgmtAdapterEventCallbackList::equal_comparator _mgmtAdapterEventCallbackEqComp_ID_CB =
        [](const MgmtAdapterEventCallback &a, const MgmtAdapterEventCallback &b) -> bool { return a == b; };

static MgmtAdapterEventCallbackList::equal_comparator _mgmtAdapterEventCallbackEqComp_CB =
        [](const MgmtAdapterEventCallback &a, const MgmtAdapterEventCallback &b) -> bool { return a.getCallback() == b.getCallback(); };

static MgmtAdapterEventCallbackList::equal_comparator _mgmtAdapterEventCallbackEqComp_ID =
        [](const MgmtAdapterEventCallback &a, const MgmtAdapterEventCallback &b) -> bool { return a.getDevID() == b.getDevID(); };

bool BTManager::addMgmtEventCallback(const int dev_id, const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return false;
    }
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    /* const bool added = */ l.push_back_unique(MgmtAdapterEventCallback(dev_id, opc, cb), _mgmtAdapterEventCallbackEqComp_ID_CB);
    return true;
}
int BTManager::removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return 0;
    }
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    return l.erase_matching( MgmtAdapterEventCallback( 0, MgmtEvent::Opcode::INVALID, cb ),
                               true /* all_matching */, _mgmtAdapterEventCallbackEqComp_CB);
}
int BTManager::removeMgmtEventCallback(const int dev_id) noexcept {
    if( 0 > dev_id ) {
        // skip dev_id -1 case, use clearAllMgmtEventCallbacks() here
        return 0;
    }
    int count = 0;
    for(size_t i=0; i<mgmtAdapterEventCallbackLists.size(); i++) {
        MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[i];
        count += l.erase_matching( MgmtAdapterEventCallback( dev_id, MgmtEvent::Opcode::INVALID, MgmtEventCallback() ),
                                     true /* all_matching */, _mgmtAdapterEventCallbackEqComp_ID);
    }
    return count;
}
void BTManager::clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return;
    }
    mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)].clear();
}
void BTManager::clearAllCallbacks() noexcept {
    for(size_t i=0; i<mgmtAdapterEventCallbackLists.size(); i++) {
        mgmtAdapterEventCallbackLists[i].clear();
    }
    mgmtChangedAdapterSetCallbackList.clear();
}

void BTManager::processAdapterAdded(std::unique_ptr<MgmtEvent> e) noexcept {
    const uint16_t dev_id = e->getDevID();

    std::unique_ptr<AdapterInfo> adapterInfo = readAdapterInfo(dev_id);

    if( nullptr != adapterInfo ) {
        std::shared_ptr<BTAdapter> adapter = addAdapter( *adapterInfo );
        DBG_PRINT("BTManager::Adapter[%d] Added: Start %s, added %d", dev_id, adapter->toString().c_str());
        sendMgmtEvent(*e);
        DBG_PRINT("BTManager::Adapter[%d] Added: User_ %s", dev_id, adapter->toString().c_str());
        jau::for_each_fidelity(mgmtChangedAdapterSetCallbackList, [&](ChangedAdapterSetCallback &cb) {
           cb.invoke(true /* added */, adapter);
        });
        DBG_PRINT("BTManager::Adapter[%d] Added: End__ %s", dev_id, adapter->toString().c_str());
    } else {
        DBG_PRINT("BTManager::Adapter[%d] Added: InitAI failed", dev_id);
    }
}
void BTManager::processAdapterRemoved(std::unique_ptr<MgmtEvent> e) noexcept {
    const uint16_t dev_id = e->getDevID();
    std::shared_ptr<BTAdapter> ai = removeAdapter(dev_id);
    if( nullptr != ai ) {
        DBG_PRINT("BTManager::Adapter[%d] Removed: Start: %s", dev_id, ai->toString().c_str());
        sendMgmtEvent(*e);
        DBG_PRINT("BTManager::Adapter[%d] Removed: User_: %s", dev_id, ai->toString().c_str());
        jau::for_each_fidelity(mgmtChangedAdapterSetCallbackList, [&](ChangedAdapterSetCallback &cb) {
           cb.invoke(false /* added */, ai);
        });
        ai->close(); // issuing dtor on DBTAdapter
        DBG_PRINT("BTManager::Adapter[%d] Removed: End__: %s", dev_id, ai->toString().c_str());
    } else {
        DBG_PRINT("BTManager::Adapter[%d] Removed: RemoveAI failed", dev_id);
    }
}
bool BTManager::mgmtEvNewSettingsCB(const MgmtEvent& e) noexcept {
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
    std::shared_ptr<BTAdapter> adapter = getAdapter(event.getDevID());
    if( nullptr != adapter ) {
        const AdapterSetting old_settings = adapter->adapterInfo.getCurrentSettingMask();
        const AdapterSetting new_settings = adapter->adapterInfo.setCurrentSettingMask(event.getSettings());
        DBG_PRINT("BTManager:mgmt:NewSettings: Adapter[%d] %s -> %s - %s",
                event.getDevID(),
                to_string(old_settings).c_str(),
                to_string(new_settings).c_str(),
                e.toString().c_str());
    } else {
        DBG_PRINT("BTManager:mgmt:NewSettings: Adapter[%d] %s -> adapter not present - %s",
                event.getDevID(),
                to_string(event.getSettings()).c_str(),
                e.toString().c_str());
    }
    return true;
}

bool BTManager::mgmtEventAnyCB(const MgmtEvent& e) noexcept {
    DBG_PRINT("BTManager:mgmt:Any: %s", e.toString().c_str());
    (void)e;
    return true;
}

/**
 * ChangedAdapterSetCallback handling
 */

static ChangedAdapterSetCallbackList::equal_comparator _changedAdapterSetCallbackEqComp =
        [](const ChangedAdapterSetCallback& a, const ChangedAdapterSetCallback& b) -> bool { return a == b; };


void BTManager::addChangedAdapterSetCallback(const ChangedAdapterSetCallback & l) {
    ChangedAdapterSetCallback* l_p = const_cast<ChangedAdapterSetCallback*>(&l);
    mgmtChangedAdapterSetCallbackList.push_back(l);

    jau::for_each_fidelity(adapters, [&](std::shared_ptr<BTAdapter>& ai) {
        l_p->invoke(true /* added */, ai);
    });
}
int BTManager::removeChangedAdapterSetCallback(const ChangedAdapterSetCallback & l) {
    return mgmtChangedAdapterSetCallbackList.erase_matching(l, true /* all_matching */, _changedAdapterSetCallbackEqComp);
}

void BTManager::addChangedAdapterSetCallback(ChangedAdapterSetFunc f) {
    addChangedAdapterSetCallback(
            ChangedAdapterSetCallback(
                    jau::bindPlainFunc<bool, bool, std::shared_ptr<BTAdapter>&>(f)
            ) );
}
int BTManager::removeChangedAdapterSetCallback(ChangedAdapterSetFunc f) {
    ChangedAdapterSetCallback l( jau::bindPlainFunc<bool, bool, std::shared_ptr<BTAdapter>&>(f) );
    return mgmtChangedAdapterSetCallbackList.erase_matching(l, true /* all_matching */, _changedAdapterSetCallbackEqComp);
}
