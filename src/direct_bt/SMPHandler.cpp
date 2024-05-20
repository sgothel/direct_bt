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
using namespace jau::fractions_i64_literals;

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
        throw jau::IllegalStateError("SMPHandler's device already destructed: "+deviceString, E_FILE_LINE);
    }
    return ref;
}

bool SMPHandler::validateConnected() noexcept {
    bool l2capIsConnected = l2cap.is_open();
    bool l2capHasIOError = l2cap.hasIOError();

    if( has_ioerror || l2capHasIOError ) {
        has_ioerror = true; // propagate l2capHasIOError -> has_ioerror
        ERR_PRINT("ioerr state: GattHandler %s, l2cap %s: %s",
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

void SMPHandler::smpReaderWork(jau::service_runner& sr) noexcept {
    jau::snsize_t len;
    if( !validateConnected() ) {
        ERR_PRINT("SMPHandler::reader: Invalid IO state -> Stop");
        sr.set_shall_stop();
        return;
    }

    len = l2cap.read(rbuffer.get_wptr(), rbuffer.size());
    if( 0 < len ) {
        std::unique_ptr<const SMPPDUMsg> smpPDU = SMPPDUMsg::getSpecialized(rbuffer.get_ptr(), static_cast<jau::nsize_t>(len));
        const SMPPDUMsg::Opcode opc = smpPDU->getOpcode();

        if( SMPPDUMsg::Opcode::SECURITY_REQUEST == opc ) {
            COND_PRINT(env.DEBUG_DATA, "SMPHandler-IO RECV (SEC_REQ) %s", smpPDU->toString().c_str());
            jau::for_each_fidelity(smpSecurityReqCallbackList, [&](SMPSecurityReqCallback &cb) {
               cb(*smpPDU);
            });
        } else {
            COND_PRINT(env.DEBUG_DATA, "SMPHandler-IO RECV (MSG) %s", smpPDU->toString().c_str());
            if( smpPDURing.isFull() ) {
                const jau::nsize_t dropCount = smpPDURing.capacity()/4;
                smpPDURing.drop(dropCount);
                WARN_PRINT("SMPHandler-IO RECV Drop (%u oldest elements of %u capacity, ring full)", dropCount, smpPDURing.capacity());
            }
            if( !smpPDURing.putBlocking( std::move(smpPDU), 0_s ) ) {
                ERR_PRINT2("smpPDURing put: %s", smpPDURing.toString().c_str());
                sr.set_shall_stop();
                return;
            }
        }
    } else if( len == L2CAPClient::number(L2CAPClient::RWExitCode::INTERRUPTED) ) {
        WORDY_PRINT("SMPHandler::reader: l2cap read: IRQed res %d (%s); %s",
                len, L2CAPClient::getRWExitCodeString(len).c_str(), getStateString().c_str());
        if( !sr.shall_stop() ) {
            // need to stop service_runner if interrupted externally
            sr.set_shall_stop();
        }
    } else if( len != L2CAPClient::number(L2CAPClient::RWExitCode::POLL_TIMEOUT) &&
               len != L2CAPClient::number(L2CAPClient::RWExitCode::READ_TIMEOUT) ) { // expected TIMEOUT if idle
        if( 0 > len ) { // actual error case
            IRQ_PRINT("SMPHandler::reader: l2cap read: Error res %d (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len).c_str(), getStateString().c_str());
            sr.set_shall_stop();
            has_ioerror = true;
        } else { // zero size
            WORDY_PRINT("SMPHandler::reader: l2cap read: Zero res %d (%s); %s",
                    len, L2CAPClient::getRWExitCodeString(len).c_str(), getStateString().c_str());
        }
    }
}

void SMPHandler::smpReaderEndLocked(jau::service_runner& sr) noexcept {
    (void)sr;
    WORDY_PRINT("SMPHandler::reader: Ended. Ring has %u entries flushed", smpPDURing.size());
    smpPDURing.clear();
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

SMPHandler::SMPHandler(const std::shared_ptr<BTDevice> &device) noexcept
: env(SMPEnv::get()),
  wbr_device(device), deviceString(device->getAddressAndType().toString()),
  rbuffer(number(Defaults::SMP_MTU_BUFFER_SZ), jau::lb_endian_t::little),
  l2cap(device->getAdapter().dev_id, device->getAdapter().getAddressAndType(), L2CAP_PSM::UNDEFINED, L2CAP_CID::SMP),
  is_connected(l2cap.open(*device)), has_ioerror(false),
  smp_reader_service("SMPHandler::reader", THREAD_SHUTDOWN_TIMEOUT_MS,
                     jau::bind_member(this, &SMPHandler::smpReaderWork),
                     jau::service_runner::Callback() /* init */,
                     jau::bind_member(this, &SMPHandler::smpReaderEndLocked)),
  smpPDURing(env.SMPPDU_RING_CAPACITY),
  mtu(number(Defaults::MIN_SMP_MTU))
{
    if( !validateConnected() ) {
        ERR_PRINT("SMPHandler.ctor: L2CAP could not connect");
        is_connected = false;
        return;
    }

    l2cap.set_interrupted_query( jau::bind_member(&smp_reader_service, &jau::service_runner::shall_stop2) );
    smp_reader_service.start();

    DBG_PRINT("SMPHandler::ctor: Started: SMPHandler[%s], l2cap[%s]: %s",
                getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());

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

    // Avoid disconnect re-entry -> potential deadlock
    bool expConn = true; // C++11, exp as value since C++20
    if( !is_connected.compare_exchange_strong(expConn, false) ) {
        // not connected
        const bool smp_service_stopped = smp_reader_service.join(); // [data] race: wait until disconnecting thread has stopped service
        l2cap.close();
        DBG_PRINT("SMPHandler::disconnect: Not connected: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s], stopped %d: %s",
                  disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(),
                  smp_service_stopped, deviceString.c_str());
        clearAllCallbacks();
        return false;
    }

    PERF3_TS_TD("SMPHandler::disconnect.1");
    const bool smp_service_stop_res = smp_reader_service.stop();
    l2cap.close();
    PERF3_TS_TD("SMPHandler::disconnect.2");

    // Lock to avoid other threads using instance while disconnecting
    const std::lock_guard<std::recursive_mutex> lock(mtx_command); // RAII-style acquire and relinquish via destructor
    DBG_PRINT("SMPHandler::disconnect: Start: disconnectDevice %d, ioErrorCause %d: GattHandler[%s], l2cap[%s]: %s",
              disconnectDevice, ioErrorCause, getStateString().c_str(), l2cap.getStateString().c_str(), deviceString.c_str());
    clearAllCallbacks();

    DBG_PRINT("SMPHandler::disconnect: End: stopped %d, %s", smp_service_stop_res, deviceString.c_str());

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
    return true;
}

void SMPHandler::send(const SMPPDUMsg & msg) {
    if( !validateConnected() ) {
        throw jau::IllegalStateError("SMPHandler::send: Invalid IO State: req "+msg.toString()+" to "+deviceString, E_FILE_LINE);
    }
    if( msg.pdu.size() > mtu ) {
        throw jau::IllegalStateError("clientMaxMTU "+std::to_string(msg.pdu.size())+" > usedMTU "+std::to_string(mtu)+
                                     " to "+deviceString, E_FILE_LINE);
    }

    // Thread safe l2cap.write(..) operation..
    const jau::snsize_t len = l2cap.write(msg.pdu.get_ptr(), msg.pdu.size());
    if( len != L2CAPClient::number(L2CAPClient::RWExitCode::INTERRUPTED) ) { // expected exits
        if( 0 > len ) {
            ERR_PRINT("l2cap write: Error res %d (%s); %s; %s -> disconnect: %s",
                    len, L2CAPClient::getRWExitCodeString(len).c_str(), getStateString().c_str(),
                    msg.toString().c_str(), deviceString.c_str());
            has_ioerror = true;
            disconnect(true /* disconnect_device */, true /* ioerr_cause */); // state -> Disconnected
            throw BTException("SMPHandler::send: l2cap write: Error: req "+msg.toString()+" -> disconnect: "+deviceString, E_FILE_LINE);
        }
        if( static_cast<size_t>(len) != msg.pdu.size() ) {
            ERR_PRINT("l2cap write: Error: Message size has %d != exp %zu: %s -> disconnect: %s",
                    len, msg.pdu.size(), msg.toString().c_str(), deviceString.c_str());
            has_ioerror = true;
            disconnect(true /* disconnect_device */, true /* ioerr_cause */); // state -> Disconnected
            throw BTException("SMPHandler::send: l2cap write: Error: Message size has "+std::to_string(len)+" != exp "+std::to_string(msg.pdu.size())
                                     +": "+msg.toString()+" -> disconnect: "+deviceString, E_FILE_LINE);
        }
    } else {
        WORDY_PRINT("SMPHandler::reader: l2cap read: IRQed res %d (%s); %s",
                len, L2CAPClient::getRWExitCodeString(len).c_str(), getStateString().c_str());
    }
}

std::unique_ptr<const SMPPDUMsg> SMPHandler::sendWithReply(const SMPPDUMsg & msg, const jau::fraction_i64& timeout) {
    send( msg );

    // Ringbuffer read is thread safe
    std::unique_ptr<const SMPPDUMsg> res;
    if( !smpPDURing.getBlocking(res, timeout) || nullptr == res ) {
        errno = ETIMEDOUT;
        IRQ_PRINT("SMPHandler::sendWithReply: nullptr result (timeout %d): req %s to %s", timeout, msg.toString().c_str(), deviceString.c_str());
        has_ioerror = true;
        disconnect(true /* disconnectDevice */, true /* ioErrorCause */);
        throw BTException("SMPHandler::sendWithReply: nullptr result (timeout "+timeout.to_string()+"): req "+msg.toString()+" to "+deviceString, E_FILE_LINE);
    }
    return res;
}

/**
 * SMPSecurityReqCallback handling
 */

static SMPHandler::SMPSecurityReqCallbackList::equal_comparator _changedSMPSecurityReqCallbackEqComp =
        [](const SMPHandler::SMPSecurityReqCallback& a, const SMPHandler::SMPSecurityReqCallback& b) -> bool { return a == b; };


void SMPHandler::addSMPSecurityReqCallback(const SMPSecurityReqCallback & l) {
    smpSecurityReqCallbackList.push_back(l);
}
SMPHandler::size_type SMPHandler::removeSMPSecurityReqCallback(const SMPSecurityReqCallback & l) {
    return smpSecurityReqCallbackList.erase_matching(l, true /* all_matching */, _changedSMPSecurityReqCallbackEqComp);
}

void SMPHandler::clearAllCallbacks() noexcept {
    smpSecurityReqCallbackList.clear();
}

