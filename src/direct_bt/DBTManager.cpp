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
// PERF3_PRINT_ON for close
// #define PERF3_PRINT_ON 1
#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "BTIoctl.hpp"

#include "DBTManager.hpp"
#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "DBTTypes.hpp"

#include "SMPHandler.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
    #include <signal.h>
}

using namespace direct_bt;

BTMode MgmtEnv::getEnvBTMode() {
    // Environment variable is 'direct_bt.mgmt.btmode' or 'org.tinyb.btmode'
    // Default is BTMode::LE, if non of the above environment variable is set.
    std::string val = jau::environment::getProperty("direct_bt.mgmt.btmode");
    if( val.empty() ) {
        val = jau::environment::getProperty("org.tinyb.btmode");
    }
    const BTMode res = direct_bt::getBTMode(val);
    return BTMode::NONE != res ? res : BTMode::LE; // fallback to default LE
}

MgmtEnv::MgmtEnv() noexcept
: DEBUG_GLOBAL( jau::environment::get("direct_bt").debug ),
  exploding( jau::environment::getExplodingProperties("direct_bt.mgmt") ),
  MGMT_READER_THREAD_POLL_TIMEOUT( jau::environment::getInt32Property("direct_bt.mgmt.reader.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_COMMAND_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.mgmt.cmd.timeout", 3000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_EVT_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.mgmt.ringsize", 64, 64 /* min */, 1024 /* max */) ),
  DEBUG_EVENT( jau::environment::getBooleanProperty("direct_bt.debug.mgmt.event", false) ),
  DEFAULT_BTMODE( getEnvBTMode() ),
  MGMT_READ_PACKET_MAX_RETRY( MGMT_EVT_RING_CAPACITY )
{
}

const pid_t DBTManager::pidSelf = getpid();
std::mutex DBTManager::mtx_singleton;

void DBTManager::mgmtReaderThreadImpl() noexcept {
    {
        const std::lock_guard<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        mgmtReaderShallStop = false;
        mgmtReaderRunning = true;
        DBG_PRINT("DBTManager::reader: Started");
        cv_mgmtReaderInit.notify_all();
    }
    thread_local jau::call_on_release thread_cleanup([&]() {
        DBG_PRINT("DBTManager::mgmtReaderThreadCleanup: mgmtReaderRunning %d -> 0", mgmtReaderRunning.load());
        mgmtReaderRunning = false;
    });

    while( !mgmtReaderShallStop ) {
        jau::snsize_t len;
        if( !comm.isOpen() ) {
            // not open
            ERR_PRINT("DBTManager::reader: Not connected");
            mgmtReaderShallStop = true;
            break;
        }

        len = comm.read(rbuffer.get_wptr(), rbuffer.getSize(), env.MGMT_READER_THREAD_POLL_TIMEOUT);
        if( 0 < len ) {
            const jau::nsize_t len2 = static_cast<jau::nsize_t>(len);
            const jau::nsize_t paramSize = len2 >= MGMT_HEADER_SIZE ? rbuffer.get_uint16_nc(4) : 0;
            if( len2 < MGMT_HEADER_SIZE + paramSize ) {
                WARN_PRINT("DBTManager::reader: length mismatch %zu < MGMT_HEADER_SIZE(%u) + %u", len2, MGMT_HEADER_SIZE, paramSize);
                continue; // discard data
            }
            std::shared_ptr<MgmtEvent> event = MgmtEvent::getSpecialized(rbuffer.get_ptr(), len2);
            const MgmtEvent::Opcode opc = event->getOpcode();
            if( MgmtEvent::Opcode::CMD_COMPLETE == opc || MgmtEvent::Opcode::CMD_STATUS == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (CMD) %s", event->toString().c_str());
                if( mgmtEventRing.isFull() ) {
                    const jau::nsize_t dropCount = mgmtEventRing.capacity()/4;
                    mgmtEventRing.drop(dropCount);
                    WARN_PRINT("DBTManager-IO RECV Drop (%u oldest elements of %u capacity, ring full)", dropCount, mgmtEventRing.capacity());
                }
                mgmtEventRing.putBlocking( event );
            } else if( MgmtEvent::Opcode::INDEX_ADDED == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (ADD) %s", event->toString().c_str());
                std::thread adapterAddedThread(&DBTManager::processAdapterAdded, this, event); // @suppress("Invalid arguments")
                adapterAddedThread.detach();
            } else if( MgmtEvent::Opcode::INDEX_REMOVED == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (REM) %s", event->toString().c_str());
                std::thread adapterRemovedThread(&DBTManager::processAdapterRemoved, this, event); // @suppress("Invalid arguments")
                adapterRemovedThread.detach();
            } else {
                // issue a callback
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (CB) %s", event->toString().c_str());
                sendMgmtEvent(event);
            }
        } else if( ETIMEDOUT != errno && !mgmtReaderShallStop ) { // expected exits
            ERR_PRINT("DBTManager::reader: HCIComm read error");
        }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        WORDY_PRINT("DBTManager::reader: Ended. Ring has %u entries flushed", mgmtEventRing.getSize());
        mgmtEventRing.clear();
        mgmtReaderRunning = false;
        cv_mgmtReaderInit.notify_all();
    }


}

void DBTManager::sendMgmtEvent(std::shared_ptr<MgmtEvent> event) noexcept {
    const uint16_t dev_id = event->getDevID();
    MgmtAdapterEventCallbackList & mgmtEventCallbackList = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(event->getOpcode())];
    int invokeCount = 0;

    jau::for_each_cow(mgmtEventCallbackList, [&](MgmtAdapterEventCallback &cb) {
        if( 0 > cb.getDevID() || dev_id == cb.getDevID() ) {
            try {
                cb.getCallback().invoke(event);
            } catch (std::exception &e) {
                ERR_PRINT("DBTManager::sendMgmtEvent-CBs %d/%zd: MgmtAdapterEventCallback %s : Caught exception %s",
                        invokeCount+1, mgmtEventCallbackList.size(),
                        cb.toString().c_str(), e.what());
            }
            invokeCount++;
        }
    });

    COND_PRINT(env.DEBUG_EVENT, "DBTManager::sendMgmtEvent: Event %s -> %d/%zd callbacks", event->toString().c_str(), invokeCount, mgmtEventCallbackList.size());
    (void)invokeCount;
}

static void mgmthandler_sigaction(int sig, siginfo_t *info, void *ucontext) noexcept {
    bool pidMatch = info->si_pid == DBTManager::pidSelf;
    WORDY_PRINT("DBTManager.sigaction: sig %d, info[code %d, errno %d, signo %d, pid %d, uid %d, fd %d], pid-self %d (match %d)",
            sig, info->si_code, info->si_errno, info->si_signo,
            info->si_pid, info->si_uid, info->si_fd,
            DBTManager::pidSelf, pidMatch);
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
            ERR_PRINT("DBTManager.sigaction: Resetting sighandler");
        }
    }
#endif
}

std::shared_ptr<MgmtEvent> DBTManager::sendWithReply(MgmtCommand &req) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    {
        COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO SENT %s", req.toString().c_str());
        TROOctets & pdu = req.getPDU();
        if ( comm.write( pdu.get_ptr(), pdu.getSize() ) < 0 ) {
            ERR_PRINT("DBTManager::sendWithReply: HCIComm write error, req %s", req.toString().c_str());
            return nullptr;
        }
    }

    // Ringbuffer read is thread safe
    int32_t retryCount = 0;
    while( retryCount < env.MGMT_READ_PACKET_MAX_RETRY ) {
        std::shared_ptr<MgmtEvent> res = mgmtEventRing.getBlocking(env.MGMT_COMMAND_REPLY_TIMEOUT);
        // std::shared_ptr<MgmtEvent> res = receiveNext();
        if( nullptr == res ) {
            errno = ETIMEDOUT;
            ERR_PRINT("DBTManager::sendWithReply.X: nullptr result (timeout -> abort): req %s", req.toString().c_str());
            return nullptr;
        } else if( !res->validate(req) ) {
            // This could occur due to an earlier timeout w/ a nullptr == res (see above),
            // i.e. the pending reply processed here and naturally not-matching.
            COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV sendWithReply: res mismatch (drop evt, retryCount %d): res %s; req %s",
                    retryCount, res->toString().c_str(), req.toString().c_str());
            retryCount++;
        } else {
            COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV sendWithReply: res %s; req %s", res->toString().c_str(), req.toString().c_str());
            return res;
        }
    }
    return nullptr;
}

std::shared_ptr<AdapterInfo> DBTManager::initAdapter(const uint16_t dev_id, const BTMode btMode) noexcept {
    const SMPIOCapability iocap { SMPIOCapability::KEYBOARD_DISPLAY };
    // const uint8_t debug_keys = 1;
    const uint8_t debug_keys = 0;
    std::shared_ptr<AdapterInfo> adapterInfo = nullptr;
    AdapterSetting current_settings;
    MgmtCommand req0(MgmtCommand::Opcode::READ_INFO, dev_id);
    {
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
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
            ABORT("AdapterInfo dev_id=%d != dev_id=%d: %s", adapterInfo->dev_id, dev_id, adapterInfo->toString().c_str());
        }
    }
    DBG_PRINT("initAdapter[%d, BTMode %s]: Start: %s", dev_id, getBTModeString(btMode).c_str(), adapterInfo->toString().c_str());
    current_settings = adapterInfo->getCurrentSettingMask();

    switch ( btMode ) {
        case BTMode::DUAL:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 1, current_settings);
            setDiscoverable(dev_id, 0, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 1, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 1, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 1, current_settings);
#endif
            break;
        case BTMode::BREDR:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 1, current_settings);
            setDiscoverable(dev_id, 0, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 0, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 1, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 0, current_settings);
#endif
            break;
        case BTMode::NONE:
            [[fallthrough]]; // map NONE -> LE
        case BTMode::LE:
            setMode(dev_id, MgmtCommand::Opcode::SET_BREDR, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_LE, 1, current_settings);
#if USE_LINUX_BT_SECURITY
            setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 0, current_settings);
            setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 1, current_settings);
#endif
            break;
    }

#if USE_LINUX_BT_SECURITY
    setMode(dev_id, MgmtCommand::Opcode::SET_DEBUG_KEYS, debug_keys, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_IO_CAPABILITY, direct_bt::number(iocap), current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, 1, current_settings); // required for pairing
#endif

    setMode(dev_id, MgmtCommand::Opcode::SET_CONNECTABLE, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_FAST_CONNECTABLE, 0, current_settings);

    removeDeviceFromWhitelist(dev_id, EUI48_ANY_DEVICE, BDAddressType::BDADDR_BREDR); // flush whitelist!

    setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, 1, current_settings);

    /**
     * Update AdapterSettings post settings
     */
    if( AdapterSetting::NONE != current_settings ) {
        adapterInfo->setCurrentSettingMask(current_settings);
    } else {
        adapterInfo = nullptr; // flush
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
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
            ABORT("AdapterInfo dev_id=%d != dev_id=%d: %s", adapterInfo->dev_id, dev_id, adapterInfo->toString().c_str());
        }
    }
    DBG_PRINT("initAdapter[%d, BTMode %s]: End: %s", dev_id, getBTModeString(btMode).c_str(), adapterInfo->toString().c_str());

fail:
    return adapterInfo;
}

void DBTManager::shutdownAdapter(const uint16_t dev_id) noexcept {
    AdapterSetting current_settings;
    setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, 0, current_settings);

    setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_CONNECTABLE, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_FAST_CONNECTABLE, 0, current_settings);

    setMode(dev_id, MgmtCommand::Opcode::SET_DEBUG_KEYS, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_IO_CAPABILITY, direct_bt::number(SMPIOCapability::DISPLAY_ONLY), current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_SSP, 0, current_settings);
    setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, 0, current_settings);
}

DBTManager::DBTManager(const BTMode _defaultBTMode) noexcept
: env(MgmtEnv::get()),
  defaultBTMode(BTMode::NONE != _defaultBTMode ? _defaultBTMode : env.DEFAULT_BTMODE),
  rbuffer(ClientMaxMTU), comm(HCI_DEV_NONE, HCI_CHANNEL_CONTROL),
  mgmtEventRing(env.MGMT_EVT_RING_CAPACITY), mgmtReaderShallStop(false),
  mgmtReaderThreadId(0), mgmtReaderRunning(false),
  allowClose( comm.isOpen() )
{
    WORDY_PRINT("DBTManager.ctor: BTMode %s, pid %d", getBTModeString(defaultBTMode).c_str(), DBTManager::pidSelf);
    if( !allowClose ) {
        ERR_PRINT("DBTManager::open: Could not open mgmt control channel");
        return;
    }

    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_sigaction = mgmthandler_sigaction;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = SA_SIGINFO;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("DBTManager::ctor: Setting sighandler");
        }
    }
    {
        std::unique_lock<std::mutex> lock(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor

        std::thread mgmtReaderThread(&DBTManager::mgmtReaderThreadImpl, this); // @suppress("Invalid arguments")
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
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
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
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
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
                    DBG_PRINT("kernel op %d: %s", i, getMgmtOpcodeString(op).c_str());
                }
            }
#endif
        }
    }

next1:
    // Mandatory
    {
        MgmtCommand req0(MgmtCommand::Opcode::READ_INDEX_LIST, MgmtConstU16::MGMT_INDEX_NONE);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
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
        {
            // Not required: CTOR: const std::lock_guard<std::recursive_mutex> lock(adapterInfos.get_write_mutex());
            std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> snapshot = adapterInfos.get_snapshot();

            for(int i=0; i < num_adapter; i++) {
                const uint16_t dev_id = jau::get_uint16(data, 2+i*2, true /* littleEndian */);
                std::shared_ptr<AdapterInfo> adapterInfo = initAdapter(dev_id, defaultBTMode);
                if( nullptr != adapterInfo ) {
                    snapshot->push_back(adapterInfo);
                    DBG_PRINT("DBTManager::adapters %d/%d: dev_id %d: %s", i, num_adapter, dev_id, adapterInfo->toString().c_str());
                } else {
                    DBG_PRINT("DBTManager::adapters %d/%d: dev_id %d: FAILED", i, num_adapter, dev_id);
                }
            }
            // Not required: CTOR: adapterInfos.set_store(std::move(snapshot));
        }
    }

    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_SETTINGS,  jau::bindMemberFunc(this, &DBTManager::mgmtEvNewSettingsCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::CONTROLLER_ERROR, jau::bindMemberFunc(this, &DBTManager::mgmtEvControllerErrorCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_LINK_KEY, jau::bindMemberFunc(this, &DBTManager::mgmtEvNewLinkKeyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bindMemberFunc(this, &DBTManager::mgmtEvNewLongTermKeyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bindMemberFunc(this, &DBTManager::mgmtEvPinCodeRequestCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bindMemberFunc(this, &DBTManager::mgmtEvUserPasskeyRequestCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::AUTH_FAILED, jau::bindMemberFunc(this, &DBTManager::mgmtEvAuthFailedCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceUnpairedCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::PASSKEY_NOTIFY, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_IRK, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CSRK, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::LOCAL_OOB_DATA_UPDATED, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));
    addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CSRK, jau::bindMemberFunc(this, &DBTManager::mgmtEventAnyCB));


    if( env.DEBUG_EVENT ) {
        addMgmtEventCallback(-1, MgmtEvent::Opcode::CLASS_OF_DEV_CHANGED, jau::bindMemberFunc(this, &DBTManager::mgmtEvClassOfDeviceChangedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceDiscoveringCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_FOUND, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceFoundCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceDisconnectedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceConnectedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::CONNECT_FAILED, jau::bindMemberFunc(this, &DBTManager::mgmtEvConnectFailedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_BLOCKED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceBlockedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNBLOCKED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceUnblockedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CONN_PARAM, jau::bindMemberFunc(this, &DBTManager::mgmtEvNewConnectionParamCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_ADDED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceWhitelistAddedCB));
        addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_REMOVED, jau::bindMemberFunc(this, &DBTManager::mgmtEvDeviceWhilelistRemovedCB));
    }
    PERF_TS_TD("DBTManager::ctor.ok");
    DBG_PRINT("DBTManager::ctor: OK");
    return;

fail:
    close();
    PERF_TS_TD("DBTManager::ctor.fail");
    DBG_PRINT("DBTManager::ctor: FAIL");
    return;
}

void DBTManager::close() noexcept {
    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowClose.compare_exchange_strong(expConn, false) ) {
        // not open
        DBG_PRINT("DBTManager::close: Not open");
        whitelist.clear();
        clearAllCallbacks();
        adapterInfos.clear();
        comm.close();
        return;
    }
    PERF3_TS_T0();

    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("DBTManager::close: Start");
    removeAllDevicesFromWhitelist();
    clearAllCallbacks();

    jau::for_each_cow(adapterInfos, [&](std::shared_ptr<AdapterInfo> & a) {
        shutdownAdapter(a->dev_id);
    });
    adapterInfos.clear();

    // Interrupt DBTManager's HCIComm::read(..), avoiding prolonged hang
    // and pull all underlying hci read operations!
    comm.close();

    PERF3_TS_TD("DBTManager::close.1");
    {
        std::unique_lock<std::mutex> lockReader(mtx_mgmtReaderLifecycle); // RAII-style acquire and relinquish via destructor
        const pthread_t tid_self = pthread_self();
        const pthread_t tid_reader = mgmtReaderThreadId;
        mgmtReaderThreadId = 0;
        const bool is_reader = tid_reader == tid_self;
        DBG_PRINT("DBTManager::close: mgmtReader[running %d, shallStop %d, isReader %d, tid %p)",
                mgmtReaderRunning.load(), mgmtReaderShallStop.load(), is_reader, (void*)tid_reader);
        if( mgmtReaderRunning ) {
            mgmtReaderShallStop = true;
            if( !is_reader && 0 != tid_reader ) {
                int kerr;
                if( 0 != ( kerr = pthread_kill(tid_reader, SIGALRM) ) ) {
                    ERR_PRINT("DBTManager::close: pthread_kill %p FAILED: %d", (void*)tid_reader, kerr);
                }
            }
            // Ensure the reader thread has ended, no runaway-thread using *this instance after destruction
            while( true == mgmtReaderRunning ) {
                cv_mgmtReaderInit.wait(lockReader);
            }
        }
    }
    PERF3_TS_TD("DBTManager::close.2");

    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_handler = SIG_DFL;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = 0;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("DBTManager.sigaction: Resetting sighandler");
        }
    }

    PERF3_TS_TD("DBTManager::close.X");
    DBG_PRINT("DBTManager::close: End");
}

int DBTManager::findAdapterInfoDevId(const EUI48 &mac) const noexcept {
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> snapshot = adapterInfos.get_snapshot();
    auto begin = snapshot->begin();
    auto it = std::find_if(begin, snapshot->end(), [&](std::shared_ptr<AdapterInfo> const& p) -> bool {
        return p->address == mac;
    });
    if ( it == std::end(*snapshot) ) {
        return -1;
    } else {
        return (*it)->dev_id;
    }
}
std::shared_ptr<AdapterInfo> DBTManager::findAdapterInfo(const EUI48 &mac) const noexcept {
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> snapshot = adapterInfos.get_snapshot();
    auto begin = snapshot->begin();
    auto it = std::find_if(begin, snapshot->end(), [&](std::shared_ptr<AdapterInfo> const& p) -> bool {
        return p->address == mac;
    });
    if ( it == std::end(*snapshot) ) {
        return nullptr;
    } else {
        return *it;
    }
}
std::shared_ptr<AdapterInfo> DBTManager::getAdapterInfo(const uint16_t dev_id) const noexcept {
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> snapshot = adapterInfos.get_snapshot();
    auto begin = snapshot->begin();
    auto it = std::find_if(begin, snapshot->end(), [&](std::shared_ptr<AdapterInfo> const& p) -> bool {
        return p->dev_id == dev_id;
    });
    if ( it == std::end(*snapshot) ) {
        return nullptr;
    } else {
        return *it;
    }
}
bool DBTManager::addAdapterInfo(std::shared_ptr<AdapterInfo> ai) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(adapterInfos.get_write_mutex());
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> store = adapterInfos.copy_store();

    auto begin = store->begin();
    auto it = std::find_if(begin, store->end(), [&](std::shared_ptr<AdapterInfo> const& p) -> bool {
        return p->dev_id == ai->dev_id;
    });
    if ( it != std::end(*store) ) {
        // already existing
        return false;
    }
    store->push_back(ai);
    adapterInfos.set_store(std::move(store));
    return true;
}
std::shared_ptr<AdapterInfo> DBTManager::removeAdapterInfo(const uint16_t dev_id) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(adapterInfos.get_write_mutex());
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> store = adapterInfos.copy_store();

    for(auto it = store->begin(); it != store->end(); ) {
        std::shared_ptr<AdapterInfo> & ai = *it;
        if( ai->dev_id == dev_id ) {
            std::shared_ptr<AdapterInfo> res = ai;
            it = store->erase(it);
            adapterInfos.set_store(std::move(store));
            return res;
        } else {
            ++it;
        }
    }
    return nullptr;
}

BTMode DBTManager::getCurrentBTMode(uint16_t dev_id) const noexcept {
    std::shared_ptr<AdapterInfo> ai = getAdapterInfo(dev_id);
    if( nullptr == ai ) {
        ERR_PRINT("dev_id %d not found", dev_id);
        return BTMode::NONE;
    }
    return ai->getCurrentBTMode();
}

std::shared_ptr<AdapterInfo> DBTManager::getDefaultAdapterInfo() const noexcept {
    std::shared_ptr<std::vector<std::shared_ptr<AdapterInfo>>> snapshot = adapterInfos.get_snapshot();
    auto begin = snapshot->begin();
    auto it = std::find_if(begin, snapshot->end(), [](std::shared_ptr<AdapterInfo> const& p) -> bool {
        return p->isCurrentSettingBitSet(AdapterSetting::POWERED);
    });
    if ( it == std::end(*snapshot) ) {
        return nullptr;
    } else {
        return *it;
    }
}

int DBTManager::getDefaultAdapterDevID() const noexcept {
    std::shared_ptr<AdapterInfo> ai = getDefaultAdapterInfo();
    if( nullptr == ai ) {
        return -1;
    }
    return ai->dev_id;
}

bool DBTManager::setMode(const uint16_t dev_id, const MgmtCommand::Opcode opc, const uint8_t mode, AdapterSetting& current_settings) noexcept {
    MgmtUint8Cmd req(opc, dev_id, mode);
    std::shared_ptr<MgmtEvent> reply = sendWithReply(req);
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
    DBG_PRINT("DBTManager::setMode[%d, %s]: %s, result %s %s", dev_id,
            MgmtCommand::getOpcodeString(opc).c_str(), jau::uint8HexString(mode).c_str(),
            getMgmtStatusString(res).c_str(), getAdapterSettingMaskString(current_settings).c_str());
    return MgmtStatus::SUCCESS == res;
}

MgmtStatus DBTManager::setDiscoverable(const uint16_t dev_id, const uint8_t state, const uint16_t timeout_sec, AdapterSetting& current_settings) noexcept {
    MgmtSetDiscoverableCmd req(dev_id, state, timeout_sec);
    std::shared_ptr<MgmtEvent> reply = sendWithReply(req);
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
    DBG_PRINT("DBTManager::setDiscoverable[%d]: %s, result %s %s", dev_id,
            req.toString().c_str(), getMgmtStatusString(res).c_str(), getAdapterSettingMaskString(current_settings).c_str());
    return res;
}

ScanType DBTManager::startDiscovery(const uint16_t dev_id, const BTMode btMode) noexcept {
    return startDiscovery(dev_id, getScanType(btMode));
}

ScanType DBTManager::startDiscovery(const uint16_t dev_id, const ScanType scanType) noexcept {
    MgmtUint8Cmd req(MgmtCommand::Opcode::START_DISCOVERY, dev_id, number(scanType));
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    ScanType type = ScanType::NONE;
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() && 1 <= res1.getDataSize() ) {
            const uint8_t *p = res1.getData();
            if( nullptr == p ) { // G++ 10: -Werror=null-dereference
                ERR_PRINT("DBTManager::startDiscovery: Impossible MgmtEvtCmdComplete data nullptr: %s - %s", res1.toString().c_str(), req.toString().c_str());
                return type;
            }
            type = static_cast<ScanType>( p[0] );
        }
    }
    return type;
}
bool DBTManager::stopDiscovery(const uint16_t dev_id, const ScanType type) noexcept {
    MgmtUint8Cmd req(MgmtCommand::Opcode::STOP_DISCOVERY, dev_id, number(type));
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return MgmtStatus::SUCCESS == res1.getStatus();
    }
    return false;
}

bool DBTManager::uploadConnParam(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type,
                                 const uint16_t conn_min_interval, const uint16_t conn_max_interval,
                                 const uint16_t conn_latency, const uint16_t supervision_timeout) noexcept {
    MgmtConnParam connParam{ address, address_type, conn_min_interval, conn_max_interval, conn_latency, supervision_timeout };
    MgmtLoadConnParamCmd req(dev_id, connParam);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return MgmtStatus::SUCCESS == res1.getStatus();
    }
    return false;
}

MgmtStatus DBTManager::uploadLinkKey(const uint16_t dev_id, const bool debug_keys, const MgmtLinkKey &key) noexcept {
    MgmtLoadLinkKeyCmd req(dev_id, debug_keys, key);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
}

MgmtStatus DBTManager::uploadLongTermKey(const uint16_t dev_id, const MgmtLongTermKey &key) noexcept {
    MgmtLoadLongTermKeyCmd req(dev_id, key);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
}

MgmtStatus DBTManager::userPasskeyReply(const uint16_t dev_id, const EUI48 &address, const BDAddressType addressType, const uint32_t passkey) noexcept {
    MgmtUserPasskeyReplyCmd cmd(dev_id, address, addressType, passkey);
    std::shared_ptr<MgmtEvent> res = sendWithReply(cmd);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
}

MgmtStatus DBTManager::userPasskeyNegativeReply(const uint16_t dev_id, const EUI48 &address, const BDAddressType addressType) noexcept {
    MgmtUserPasskeyNegativeReplyCmd cmd(dev_id, address, addressType);
    std::shared_ptr<MgmtEvent> res = sendWithReply(cmd);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        // FIXME: Analyze address + addressType result?
        return res1.getStatus();
    }
    return MgmtStatus::TIMEOUT;
}

bool DBTManager::isDeviceWhitelisted(const uint16_t dev_id, const EUI48 &address) noexcept {
    for(auto it = whitelist.begin(); it != whitelist.end(); ) {
        std::shared_ptr<WhitelistElem> wle = *it;
        if( wle->dev_id == dev_id && wle->address == address ) {
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

bool DBTManager::addDeviceToWhitelist(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type, const HCIWhitelistConnectType ctype) noexcept {
    MgmtAddDeviceToWhitelistCmd req(dev_id, address, address_type, ctype);

    // Check if already exist in our local whitelist first, reject if so ..
    if( isDeviceWhitelisted(dev_id, address) ) {
        ERR_PRINT("DBTManager::addDeviceToWhitelist: Already in local whitelist, remove first: %s", req.toString().c_str());
        return false;
    }
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<WhitelistElem> wle( new WhitelistElem{dev_id, address, address_type, ctype} );
            whitelist.push_back(wle);
            return true;
        }
    }
    return false;
}

int DBTManager::removeAllDevicesFromWhitelist() noexcept {
#if 0
    std::vector<std::shared_ptr<WhitelistElem>> whitelist_copy = whitelist;
    int count = 0;
    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist.A: Start %zd elements", whitelist_copy.size());

    for(auto it = whitelist_copy.begin(); it != whitelist_copy.end(); ++it) {
        std::shared_ptr<WhitelistElem> wle = *it;
        removeDeviceFromWhitelist(wle->dev_id, wle->address, wle->address_type);
        count++;
    }
#else
    int count = whitelist.size();
    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist.B: Start %d elements", count);
    whitelist.clear();
    jau::for_each_cow(adapterInfos, [&](std::shared_ptr<AdapterInfo> & a) {
        removeDeviceFromWhitelist(a->dev_id, EUI48_ANY_DEVICE, BDAddressType::BDADDR_BREDR); // flush whitelist!
    });
#endif

    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist: End: Removed %d elements, remaining %zd elements",
            count, whitelist.size());
    return count;
}

bool DBTManager::removeDeviceFromWhitelist(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type) noexcept {
    // Remove from our local whitelist first
    {
        for(auto it = whitelist.begin(); it != whitelist.end(); ) {
            std::shared_ptr<WhitelistElem> wle = *it;
            if( wle->dev_id == dev_id && wle->address == address ) {
                it = whitelist.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Actual removal
    MgmtRemoveDeviceFromWhitelistCmd req(dev_id, address, address_type);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            return true;
        }
    }
    return false;
}

bool DBTManager::disconnect(const bool ioErrorCause,
                            const uint16_t dev_id, const EUI48 &peer_bdaddr, const BDAddressType peer_mac_type,
                            const HCIStatusCode reason) noexcept {
    bool bres = false;

    // Always issue DISCONNECT command, even in case of an ioError (lost-connection),
    // see Issue #124 fast re-connect on CSR adapter.
    // This will always notify the adapter of a disconnected device.
    {
        MgmtDisconnectCmd req(dev_id, peer_bdaddr, peer_mac_type);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req);
        if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
            if( MgmtStatus::SUCCESS == res1.getStatus() ) {
                bres = true;
            }
        }
    }
    if( !ioErrorCause ) {
        // In case of an ioError (lost-connection), don't wait for the lagging
        // DISCONN_COMPLETE event but send it directly.
        MgmtEvtDeviceDisconnected *e = new MgmtEvtDeviceDisconnected(dev_id, peer_bdaddr, peer_mac_type, reason, 0xffff);
        sendMgmtEvent(std::shared_ptr<MgmtEvent>(e));
    }
    return bres;
}

std::shared_ptr<ConnectionInfo> DBTManager::getConnectionInfo(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type) noexcept {
    MgmtGetConnectionInfoCmd req(dev_id, address, address_type);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<ConnectionInfo> result = res1.toConnectionInfo();
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<NameAndShortName> DBTManager::setLocalName(const uint16_t dev_id, const std::string & name, const std::string & short_name) noexcept {
    MgmtSetLocalNameCmd req (static_cast<uint16_t>(dev_id), name, short_name);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<NameAndShortName> result = res1.toNameAndShortName();

            // explicit LocalNameChanged event
            MgmtEvtLocalNameChanged * e = new MgmtEvtLocalNameChanged(static_cast<uint16_t>(dev_id), result->getName(), result->getShortName());
            sendMgmtEvent(std::shared_ptr<MgmtEvent>(e));
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

bool DBTManager::addMgmtEventCallback(const int dev_id, const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return false;
    }
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    /* const bool added = */ l.push_back_unique(MgmtAdapterEventCallback(dev_id, opc, cb), _mgmtAdapterEventCallbackEqComp_ID_CB);
    return true;
}
int DBTManager::removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return 0;
    }
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    return l.erase_matching( MgmtAdapterEventCallback( 0, MgmtEvent::Opcode::INVALID, cb ),
                               true /* all_matching */, _mgmtAdapterEventCallbackEqComp_CB);
}
int DBTManager::removeMgmtEventCallback(const int dev_id) noexcept {
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
void DBTManager::clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept {
    if( !isValidMgmtEventCallbackListsIndex(opc) ) {
        ERR_PRINT("Opcode %s >= %d", MgmtEvent::getOpcodeString(opc).c_str(), mgmtAdapterEventCallbackLists.size());
        return;
    }
    mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)].clear();
}
void DBTManager::clearAllCallbacks() noexcept {
    for(size_t i=0; i<mgmtAdapterEventCallbackLists.size(); i++) {
        mgmtAdapterEventCallbackLists[i].clear();
    }
    mgmtChangedAdapterSetCallbackList.clear();
}

void DBTManager::processAdapterAdded(std::shared_ptr<MgmtEvent> e) noexcept {
    const uint16_t dev_id = e->getDevID();
    std::shared_ptr<AdapterInfo> ai = initAdapter(dev_id, defaultBTMode);
    if( nullptr != ai ) {
        const bool added = addAdapterInfo(ai);
        DBG_PRINT("DBTManager::Adapter[%d] Added: Start %s, added %d", dev_id, ai->toString().c_str(), added);
        sendMgmtEvent(e);
        DBG_PRINT("DBTManager::Adapter[%d] Added: User_ %s", dev_id, ai->toString().c_str());
        jau::for_each_cow(mgmtChangedAdapterSetCallbackList, [&](ChangedAdapterSetCallback &cb) {
           cb.invoke(true /* added */, *ai);
        });
        DBG_PRINT("DBTManager::Adapter[%d] Added: End__ %s", dev_id, ai->toString().c_str());
    } else {
        DBG_PRINT("DBTManager::Adapter[%d] Added: InitAI failed", dev_id);
    }
}
void DBTManager::processAdapterRemoved(std::shared_ptr<MgmtEvent> e) noexcept {
    const uint16_t dev_id = e->getDevID();
    std::shared_ptr<AdapterInfo> ai = removeAdapterInfo(dev_id);
    if( nullptr != ai ) {
        DBG_PRINT("DBTManager::Adapter[%d] Removed: Start: %s", dev_id, ai->toString().c_str());
        sendMgmtEvent(e);
        DBG_PRINT("DBTManager::Adapter[%d] Removed: User_: %s", dev_id, ai->toString().c_str());
        jau::for_each_cow(mgmtChangedAdapterSetCallbackList, [&](ChangedAdapterSetCallback &cb) {
           cb.invoke(false /* added */, *ai);
        });
        DBG_PRINT("DBTManager::Adapter[%d] Removed: End__: %s", dev_id, ai->toString().c_str());
    } else {
        DBG_PRINT("DBTManager::Adapter[%d] Removed: RemoveAI failed", dev_id);
    }
}
bool DBTManager::mgmtEvNewSettingsCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(e.get());
    std::shared_ptr<AdapterInfo> adapterInfo = getAdapterInfo(event.getDevID());
    if( nullptr != adapterInfo ) {
        const AdapterSetting old_settings = adapterInfo->getCurrentSettingMask();
        const AdapterSetting new_settings = adapterInfo->setCurrentSettingMask(event.getSettings());
        DBG_PRINT("DBTManager:mgmt:NewSettings: Adapter[%d] %s -> %s - %s",
                event.getDevID(),
                getAdapterSettingMaskString(old_settings).c_str(),
                getAdapterSettingMaskString(new_settings).c_str(),
                e->toString().c_str());
    } else {
        DBG_PRINT("DBTManager:mgmt:NewSettings: Adapter[%d] %s -> adapter not present - %s",
                event.getDevID(),
                getAdapterSettingMaskString(event.getSettings()).c_str(),
                e->toString().c_str());
    }
    return true;
}
bool DBTManager::mgmtEventAnyCB(std::shared_ptr<MgmtEvent> e) noexcept {
    DBG_PRINT("DBTManager:mgmt:Any: %s", e->toString().c_str());
    (void)e;
    return true;
}

bool DBTManager::mgmtEvControllerErrorCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtControllerError &event = *static_cast<const MgmtEvtControllerError *>(e.get());
    DBG_PRINT("DBTManager:mgmt:ControllerError: %s", event.toString().c_str());
    return true;
}

bool DBTManager::mgmtEvNewLinkKeyCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtNewLinkKey &event = *static_cast<const MgmtEvtNewLinkKey *>(e.get());
    DBG_PRINT("DBTManager:mgmt:NewLinkKey: %s", event.toString().c_str());
    return true;

}
bool DBTManager::mgmtEvNewLongTermKeyCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtNewLongTermKey &event = *static_cast<const MgmtEvtNewLongTermKey *>(e.get());
    DBG_PRINT("DBTManager:mgmt:NewLongTermKey: %s", event.toString().c_str());
    return true;
}
bool DBTManager::mgmtEvDeviceUnpairedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(e.get());
    DBG_PRINT("DBTManager:mgmt:DeviceUnpaired: %s", event.toString().c_str());
    return true;
}
bool DBTManager::mgmtEvPinCodeRequestCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(e.get());
    DBG_PRINT("DBTManager:mgmt:PinCodeRequest: %s", event.toString().c_str());
    return true;
}
bool DBTManager::mgmtEvAuthFailedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    const MgmtEvtAuthFailed &event = *static_cast<const MgmtEvtAuthFailed *>(e.get());
    DBG_PRINT("DBTManager:mgmt:AuthFailed: %s", event.toString().c_str());
    return true;
}

bool DBTManager::mgmtEvUserConfirmRequestCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtUserConfirmRequest &event = *static_cast<const MgmtEvtUserConfirmRequest *>(e.get());
    DBG_PRINT("DBTManager:mgmt:UserConfirmRequest: %s", event.toString().c_str());
    return true;
}

bool DBTManager::mgmtEvUserPasskeyRequestCB(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(e.get());
    DBG_PRINT("DBTManager:mgmt:UserPasskeyRequest: %s", event.toString().c_str());
    return true;
}

bool DBTManager::mgmtEvClassOfDeviceChangedCB(std::shared_ptr<MgmtEvent> e) noexcept {
    DBG_PRINT("DBTManager:mgmt:ClassOfDeviceChanged: %s", e->toString().c_str());
    (void)e;
    return true;
}
bool DBTManager::mgmtEvDeviceDiscoveringCB(std::shared_ptr<MgmtEvent> e) noexcept {
    DBG_PRINT("DBTManager:mgmt:DeviceDiscovering: %s", e->toString().c_str());
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceFoundCB(std::shared_ptr<MgmtEvent> e) noexcept {
    DBG_PRINT("DBTManager:mgmt:DeviceFound: %s", e->toString().c_str());
    const MgmtEvtDeviceFound &event = *static_cast<const MgmtEvtDeviceFound *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceDisconnectedCB(std::shared_ptr<MgmtEvent> e) noexcept {
    DBG_PRINT("DBTManager:mgmt:DeviceDisconnected: %s", e->toString().c_str());
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceConnectedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:DeviceConnected: %s", e->toString().c_str());
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvConnectFailedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:ConnectFailed: %s", e->toString().c_str());
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceBlockedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:DeviceBlocked: %s", e->toString().c_str());
    const MgmtEvtDeviceBlocked &event = *static_cast<const MgmtEvtDeviceBlocked *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceUnblockedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:DeviceUnblocked: %s", e->toString().c_str());
    const MgmtEvtDeviceUnblocked &event = *static_cast<const MgmtEvtDeviceUnblocked *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvNewConnectionParamCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:NewConnectionParam: %s", e->toString().c_str());
    const MgmtEvtNewConnectionParam &event = *static_cast<const MgmtEvtNewConnectionParam *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceWhitelistAddedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:DeviceWhitelistAdded: %s", e->toString().c_str());
    const MgmtEvtDeviceWhitelistAdded &event = *static_cast<const MgmtEvtDeviceWhitelistAdded *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceWhilelistRemovedCB(std::shared_ptr<MgmtEvent> e) noexcept  {
    DBG_PRINT("DBTManager:mgmt:DeviceWhitelistRemoved: %s", e->toString().c_str());
    const MgmtEvtDeviceWhitelistRemoved &event = *static_cast<const MgmtEvtDeviceWhitelistRemoved *>(e.get());
    (void)event;
    return true;
}

/**
 * ChangedAdapterSetCallback handling
 */

static ChangedAdapterSetCallbackList::equal_comparator _changedAdapterSetCallbackEqComp =
        [](const ChangedAdapterSetCallback& a, const ChangedAdapterSetCallback& b) -> bool { return a == b; };


void DBTManager::addChangedAdapterSetCallback(const ChangedAdapterSetCallback & l) {
    mgmtChangedAdapterSetCallbackList.push_back(l);

    jau::for_each_cow(adapterInfos, [&](std::shared_ptr<AdapterInfo>& ai) {
        jau::for_each_cow(mgmtChangedAdapterSetCallbackList, [&](ChangedAdapterSetCallback &cb) {
           cb.invoke(true /* added */, *ai);
        });
    });
}
int DBTManager::removeChangedAdapterSetCallback(const ChangedAdapterSetCallback & l) {
    return mgmtChangedAdapterSetCallbackList.erase_matching(l, true /* all_matching */, _changedAdapterSetCallbackEqComp);
}

void DBTManager::addChangedAdapterSetCallback(ChangedAdapterSetFunc f) {
    addChangedAdapterSetCallback(
            ChangedAdapterSetCallback(
                    jau::bindPlainFunc<bool, bool, const AdapterInfo&>(f)
            ) );
}
int DBTManager::removeChangedAdapterSetCallback(ChangedAdapterSetFunc f) {
    ChangedAdapterSetCallback l( jau::bindPlainFunc<bool, bool, const AdapterInfo&>(f) );
    return mgmtChangedAdapterSetCallbackList.erase_matching(l, true /* all_matching */, _changedAdapterSetCallbackEqComp);
}
