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

#include "SMPHandler.hpp"

#include "BTDevice.hpp"
#include "BTAdapter.hpp"
#include "DBTConst.hpp"

using namespace direct_bt;

SMPEnv::SMPEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.smp") ),
  SMP_READ_COMMAND_REPLY_TIMEOUT( jau::environment::getInt32Property("direct_bt.smp.cmd.read.timeout", 500, 250 /* min */, INT32_MAX /* max */) ),
  SMP_WRITE_COMMAND_REPLY_TIMEOUT(  jau::environment::getInt32Property("direct_bt.smp.cmd.write.timeout", 500, 250 /* min */, INT32_MAX /* max */) ),
  SMPPDU_RING_CAPACITY( jau::environment::getInt32Property("direct_bt.smp.ringsize", 128, 64 /* min */, 1024 /* max */) ),
  DEBUG_DATA( jau::environment::getBooleanProperty("direct_bt.debug.smp.data", false) )
{
}

bool SMPHandler::IS_SUPPORTED_BY_OS = SMP_SUPPORTED_BY_OS ? true : false;

std::shared_ptr<BTDevice> SMPHandler::getDeviceChecked() const {
    std::shared_ptr<BTDevice> ref = wbr_device.lock();
    if( nullptr == ref ) {
        throw jau::IllegalStateException("SMPHandler's device already destructed: "+deviceString, E_FILE_LINE);
    }
    return ref;
}

bool SMPHandler::validateConnected() noexcept {
    bool l2capIsConnected = l2cap.isOpen();
    bool l2capHasIOError = l2cap.hasIOError();

    if( has_ioerror || l2capHasIOError ) {
        has_ioerror = true; // propagate l2capHasIOError -> has_ioerror
        ERR_PRINT("IOError state: GattHandler %s, l2cap %s: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());
        return false;
    }

    if( !is_connected || !l2capIsConnected ) {
        ERR_PRINT("Disconnected state: GattHandler %s, l2cap %s: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());
        return false;
    }
    return true;
}

void SMPHandler::l2capReaderThreadImpl() {
    {
        const std::lock_guard<std::mutex> lock(mtx_l2capReaderLifecycle); // RAII-style acquire and relinquish via destructor
        l2capReaderShallStop = false;
        l2capReaderRunning = true;
        DBG_PRINT("SMPHandler::reader Started");
    }
    cv_l2capReaderInit.notify_all(); // have mutex unlocked before notify_all to avoid pessimistic re-block of notified wait() thread.

    thread_local jau::call_on_release thread_cleanup([&]() {
        DBG_PRINT("SMPHandler::l2capReaderThreadCleanup: l2capReaderRunning %d -> 0", l2capReaderRunning.load());
        l2capReaderRunning = false;
        cv_l2capReaderInit.notify_all();
    });

    while( !l2capReaderShallStop ) {
        jau::snsize_t len;
        if( !validateConnected() ) {
            ERR_PRINT("SMPHandler::reader: Invalid IO state -> Stop");
            l2capReaderShallStop = true;
            break;
        }

        len = l2cap.read(rbuffer.get_wptr(), rbuffer.size());
        if( 0 < len ) {
            std::unique_ptr<const SMPPDUMsg> smpPDU = SMPPDUMsg::getSpecialized(rbuffer.get_ptr(), static_cast<jau::nsize_t>(len));
            const SMPPDUMsg::Opcode opc = smpPDU->getOpcode();

            if( SMPPDUMsg::Opcode::SECURITY_REQUEST == opc ) {
                COND_PRINT(env.DEBUG_DATA, "SMPHandler-IO RECV (SEC_REQ) %s", smpPDU->toString().c_str());
                jau::for_each_fidelity(smpSecurityReqCallbackList, [&](SMPSecurityReqCallback &cb) {
                   cb.invoke(*smpPDU);
                });
            } else {
                COND_PRINT(env.DEBUG_DATA, "SMPHandler-IO RECV (MSG) %s", smpPDU->toString().c_str());
                if( smpPDURing.isFull() ) {
                    const jau::nsize_t dropCount = smpPDURing.capacity()/4;
                    smpPDURing.drop(dropCount);
                    WARN_PRINT("SMPHandler-IO RECV Drop (%u oldest elements of %u capacity, ring full)", dropCount, smpPDURing.capacity());
                }
                smpPDURing.putBlocking( std::move(smpPDU) );
            }
        } else if( 0 > len && ETIMEDOUT != errno && !l2capReaderShallStop ) { // expected exits
            IRQ_PRINT("SMPHandler::reader: l2cap read error -> Stop; l2cap.read %d (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len).c_str(),
                    getStateString().c_str());
            l2capReaderShallStop = true;
            has_ioerror = true;
        } else {
            WORDY_PRINT("SMPHandler::reader: l2cap read: l2cap.read %d (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len).c_str(),
                    getStateString().c_str());
        }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_l2capReaderLifecycle); // RAII-style acquire and relinquish via destructor
        WORDY_PRINT("SMPHandler::reader: Ended. Ring has %u entries flushed", smpPDURing.size());
        smpPDURing.clear();
        l2capReaderRunning = false;
    }
    cv_l2capReaderInit.notify_all(); // have mutex unlocked before notify_all to avoid pessimistic re-block of notified wait() thread.

    disconnect(true /* disconnectDevice */, has_ioerror);
}

SMPHandler::SMPHandler(const std::shared_ptr<BTDevice> &device) noexcept
: env(SMPEnv::get()),
  wbr_device(device), deviceString(device->getAddressAndType().toString()),
  rbuffer(number(Defaults::SMP_MTU_BUFFER_SZ), jau::endian::little),
  l2cap(device->getAdapter().getAddressAndType(), L2CAP_PSM::UNDEFINED, L2CAP_CID::SMP),
  is_connected(l2cap.open(*device)), has_ioerror(false),
  smpPDURing(env.SMPPDU_RING_CAPACITY), l2capReaderShallStop(false),
  l2capReaderThreadId(0), l2capReaderRunning(false),
  mtu(number(Defaults::MIN_SMP_MTU))
{
    if( !validateConnected() ) {
        ERR_PRINT("SMPHandler.ctor: L2CAP could not connect");
        is_connected = false;
        return;
    }
    DBG_PRINT("SMPHandler::ctor: Start Connect: GattHandler[%s], l2cap[%s]: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());

    /**
     * We utilize DBTManager's mgmthandler_sigaction SIGALRM handler,
     * as we only can install one handler.
     */
    {
        std::unique_lock<std::mutex> lock(mtx_l2capReaderLifecycle); // RAII-style acquire and relinquish via destructor

        std::thread l2capReaderThread(&SMPHandler::l2capReaderThreadImpl, this); // @suppress("Invalid arguments")
        l2capReaderThreadId = l2capReaderThread.native_handle();
        // Avoid 'terminate called without an active exception'
        // as l2capReaderThread may end due to I/O errors.
        l2capReaderThread.detach();

        while( false == l2capReaderRunning ) {
            cv_l2capReaderInit.wait(lock);
        }
    }

    // FIXME: Determine proper MTU usage: Defaults::MIN_SMP_MTU or Defaults::LE_SECURE_SMP_MTU (if enabled)
    uint16_t mtu_ = number(Defaults::MIN_SMP_MTU);
    mtu = std::min(number(Defaults::LE_SECURE_SMP_MTU), (int)mtu_);
}

SMPHandler::~SMPHandler() noexcept {
    disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    clearAllCallbacks();
}

bool SMPHandler::establishSecurity(const BTSecurityLevel sec_level) {
    // FIXME: Start negotiating security!
    // FIXME: Return true only if security has been established (encryption and optionally authentication)
    (void)sec_level;
    return false;
}

bool SMPHandler::disconnect(const bool disconnectDevice, const bool ioErrorCause) noexcept {
    PERF3_TS_T0();
    // Interrupt SM's L2CAP::connect(..) and L2CAP::read(..), avoiding prolonged hang
    // and pull all underlying l2cap read operations!
    l2cap.close();

    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !is_connected.compare_exchange_strong(expConn, false) ) {
        // not connected
        DBG_PRINT("SMPHandler::disconnect: Not connected: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s]: %s",
                  disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());
        clearAllCallbacks();
        return false;
    }
    // Lock to avoid other threads using instance while disconnecting
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("SMPHandler::disconnect: Start: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s]: %s",
              disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());
    clearAllCallbacks();

    PERF3_TS_TD("SMPHandler::disconnect.1");
    {
        std::unique_lock<std::mutex> lockReader(mtx_l2capReaderLifecycle); // RAII-style acquire and relinquish via destructor
        has_ioerror = false;

        const pthread_t tid_self = pthread_self();
        const pthread_t tid_l2capReader = l2capReaderThreadId;
        l2capReaderThreadId = 0;
        const bool is_l2capReader = tid_l2capReader == tid_self;
        DBG_PRINT("SMPHandler.disconnect: l2capReader[running %d, shallStop %d, isReader %d, tid %p)",
                  l2capReaderRunning.load(), l2capReaderShallStop.load(), is_l2capReader, (void*)tid_l2capReader);
        if( l2capReaderRunning ) {
            l2capReaderShallStop = true;
            if( !is_l2capReader && 0 != tid_l2capReader ) {
                int kerr;
                if( 0 != ( kerr = pthread_kill(tid_l2capReader, SIGALRM) ) ) {
                    ERR_PRINT("SMPHandler::disconnect: pthread_kill %p FAILED: %d", (void*)tid_l2capReader, kerr);
                }
            }
            // Ensure the reader thread has ended, no runaway-thread using *this instance after destruction
            while( true == l2capReaderRunning ) {
                std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                std::cv_status s = cv_l2capReaderInit.wait_until(lockReader, t0 + std::chrono::milliseconds(THREAD_SHUTDOWN_TIMEOUT_MS));
                if( std::cv_status::timeout == s && true == l2capReaderRunning ) {
                    ERR_PRINT("SMPHandler::disconnect::mgmtReader: Timeout");
                }
            }
        }
    }
    PERF3_TS_TD("SMPHandler::disconnect.2");

    if( disconnectDevice ) {
        std::shared_ptr<BTDevice> device = getDeviceUnchecked();
        if( nullptr != device ) {
            // Cleanup device resources, proper connection state
            // Intentionally giving the POWER_OFF reason for the device in case of ioErrorCause!
            const HCIStatusCode reason = ioErrorCause ?
                                   HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF :
                                   HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION;
            device->disconnect(reason);
        }
    }

    PERF3_TS_TD("SMPHandler::disconnect.X");
    DBG_PRINT("SMPHandler::disconnect: End: %s", deviceString.c_str());
    return true;
}

void SMPHandler::send(const SMPPDUMsg & msg) {
    if( !validateConnected() ) {
        throw jau::IllegalStateException("SMPHandler::send: Invalid IO State: req "+msg.toString()+" to "+deviceString, E_FILE_LINE);
    }
    if( msg.pdu.size() > mtu ) {
        throw jau::IllegalArgumentException("clientMaxMTU "+std::to_string(msg.pdu.size())+" > usedMTU "+std::to_string(mtu)+
                                       " to "+deviceString, E_FILE_LINE);
    }

    // Thread safe l2cap.write(..) operation..
    const jau::snsize_t res = l2cap.write(msg.pdu.get_ptr(), msg.pdu.size());
    if( 0 > res ) {
        IRQ_PRINT("SMPHandler::send: l2cap write error -> disconnect: l2cap.write %d (%s); %s; %s to %s",
                res, L2CAPClient::getRWExitCodeString(res).c_str(), getStateString().c_str(),
                msg.toString().c_str(), deviceString.c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */); // state -> Disconnected
        throw BTException("SMPHandler::send: l2cap write error: req "+msg.toString()+" to "+deviceString, E_FILE_LINE);
    }
    if( static_cast<size_t>(res) != msg.pdu.size() ) {
        ERR_PRINT("SMPHandler::send: l2cap write count error, %d != %zu: %s -> disconnect: %s",
                res, msg.pdu.size(), msg.toString().c_str(), deviceString.c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */); // state -> Disconnected
        throw BTException("SMPHandler::send: l2cap write count error, "+std::to_string(res)+" != "+std::to_string(msg.pdu.size())
                                 +": "+msg.toString()+" -> disconnect: "+deviceString, E_FILE_LINE);
    }
}

std::unique_ptr<const SMPPDUMsg> SMPHandler::sendWithReply(const SMPPDUMsg & msg, const int timeout) {
    send( msg );

    // Ringbuffer read is thread safe
    std::unique_ptr<const SMPPDUMsg> res;
    if( !smpPDURing.getBlocking(res, timeout) || nullptr == res ) {
        errno = ETIMEDOUT;
        IRQ_PRINT("SMPHandler::sendWithReply: nullptr result (timeout %d): req %s to %s", timeout, msg.toString().c_str(), deviceString.c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */);
        throw BTException("SMPHandler::sendWithReply: nullptr result (timeout "+std::to_string(timeout)+"): req "+msg.toString()+" to "+deviceString, E_FILE_LINE);
    }
    return res;
}

/**
 * SMPSecurityReqCallback handling
 */

static SMPSecurityReqCallbackList::equal_comparator _changedSMPSecurityReqCallbackEqComp =
        [](const SMPSecurityReqCallback& a, const SMPSecurityReqCallback& b) -> bool { return a == b; };


void SMPHandler::addSMPSecurityReqCallback(const SMPSecurityReqCallback & l) {
    smpSecurityReqCallbackList.push_back(l);
}
int SMPHandler::removeSMPSecurityReqCallback(const SMPSecurityReqCallback & l) {
    return smpSecurityReqCallbackList.erase_matching(l, true /* all_matching */, _changedSMPSecurityReqCallbackEqComp);
}

void SMPHandler::clearAllCallbacks() noexcept {
    smpSecurityReqCallbackList.clear();
}

