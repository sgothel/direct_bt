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

// #define PERF_PRINT_ON 1
#include <jau/debug.hpp>

#include "BTIoctl.hpp"
#include "HCIIoctl.hpp"
#include "L2CAPIoctl.hpp"

#include "HCIComm.hpp"
#include "L2CAPComm.hpp"
#include "DBTAdapter.hpp"

extern "C" {
    #include <unistd.h>
    #include <sys/socket.h>
    #include <poll.h>
    #include <signal.h>
}

using namespace direct_bt;

L2CAPEnv::L2CAPEnv() noexcept
: exploding( jau::environment::getExplodingProperties("direct_bt.l2cap") ),
  L2CAP_READER_POLL_TIMEOUT( jau::environment::getInt32Property("direct_bt.l2cap.reader.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  L2CAP_RESTART_COUNT_ON_ERROR( jau::environment::getInt32Property("direct_bt.l2cap.restart.count", 5, INT32_MIN /* min */, INT32_MAX /* max */) ), // FIXME: Move to L2CAPComm
  DEBUG_DATA( jau::environment::getBooleanProperty("direct_bt.debug.l2cap.data", false) )
{
}

int L2CAPComm::l2cap_open_dev(const EUI48 & adapterAddress, const uint16_t psm, const uint16_t cid, const bool pubaddrAdapter) {
    sockaddr_l2 a;
    int fd, err;

    // Create a loose L2CAP socket
    fd = ::socket(AF_BLUETOOTH, // AF_BLUETOOTH == PF_BLUETOOTH
                  SOCK_SEQPACKET, BTPROTO_L2CAP);

    if( 0 > fd ) {
        ERR_PRINT("L2CAPComm::l2cap_open_dev: socket failed");
        return fd;
    }

    // Bind socket to the L2CAP adapter
    // BT Core Spec v5.2: Vol 3, Part A: L2CAP_CONNECTION_REQ
    bzero((void *)&a, sizeof(a));
    a.l2_family=AF_BLUETOOTH;
    a.l2_psm = jau::cpu_to_le(psm);
    a.l2_bdaddr = adapterAddress;
    a.l2_cid = jau::cpu_to_le(cid);
    a.l2_bdaddr_type = pubaddrAdapter ? BDADDR_LE_PUBLIC : BDADDR_LE_RANDOM;
    if ( bind(fd, (struct sockaddr *) &a, sizeof(a)) < 0 ) {
        ERR_PRINT("L2CAPComm::l2cap_open_dev: bind failed");
        goto failed;
    }
    return fd;

failed:
    err = errno;
    close(fd);
    errno = err;

    return -1;
}

int L2CAPComm::l2cap_close_dev(int dd)
{
    return close(dd);
}


// *************************************************
// *************************************************
// *************************************************

L2CAPComm::L2CAPComm(std::shared_ptr<DBTDevice> device_, const uint16_t psm_, const uint16_t cid_)
: env(L2CAPEnv::get()),
  device(device_), deviceString(device_->getAddressString()), psm(psm_), cid(cid_),
  socket_descriptor( l2cap_open_dev(device_->getAdapter().getAddress(), psm_, cid_, true /* pubaddrAdptr */) ),
  is_connected(true), has_ioerror(false), interrupt_flag(false), tid_connect(0), tid_read(0)
{
    /** BT Core Spec v5.2: Vol 3, Part A: L2CAP_CONNECTION_REQ */
    sockaddr_l2 req;
    int res;
    int to_retry_count=0; // ETIMEDOUT retry count

    DBG_PRINT("L2CAPComm::ctor: Start Connect: %s, dd %d, %s, psm %u, cid %u, pubDevice %d",
              getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm_, cid_, true);

    if( 0 > socket_descriptor ) {
        goto failure; // open failed
    }
    tid_connect = pthread_self(); // temporary safe tid to allow interruption

    // actual request to connect to remote device
    bzero((void *)&req, sizeof(req));
    req.l2_family = AF_BLUETOOTH;
    req.l2_psm = jau::cpu_to_le(psm_);
    req.l2_bdaddr = device_->getAddress();
    req.l2_cid = jau::cpu_to_le(cid_);
    req.l2_bdaddr_type = device_->getAddressType();

    while( !interrupt_flag ) {
        // blocking
        res = ::connect(socket_descriptor, (struct sockaddr*)&req, sizeof(req));

        DBG_PRINT("L2CAPComm::ctor: Connect Result %d, errno 0%X %s, %s", res, errno, strerror(errno), deviceString.c_str());

        if( !res )
        {
            break; // done

        } else if( ETIMEDOUT == errno ) {
            to_retry_count++;
            if( to_retry_count < number(Defaults::L2CAP_CONNECT_MAX_RETRY) ) {
                WORDY_PRINT("L2CAPComm::ctor: Connect timeout, retry %d", to_retry_count);
                continue;
            } else {
                ERR_PRINT("L2CAPComm::ctor: Connect timeout, retried %d", to_retry_count);
                goto failure; // exit
            }

        } else  {
            // EALREADY == errno || ENETUNREACH == errno || EHOSTUNREACH == errno || ..
            ERR_PRINT("L2CAPComm::ctor: Connect failed");
            goto failure; // exit
        }
    }
    // success
    tid_connect = 0;
    return;

failure:
    const int err = errno;
    disconnect();
    errno = err;
}

bool L2CAPComm::disconnect() noexcept {
    bool expConn = true; // C++11, exp as value since C++20
    if( !is_connected.compare_exchange_strong(expConn, false) ) {
        DBG_PRINT("L2CAPComm::disconnect: Not connected: %s, dd %d, %s, psm %u, cid %u, pubDevice %d",
                  getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid, true);
        return false;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor

    has_ioerror = false;
    DBG_PRINT("L2CAPComm::disconnect: Start: %s, dd %d, %s, psm %u, cid %u, pubDevice %d",
              getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid, true);
    PERF_TS_T0();

    interrupt_flag = true;
    {
        pthread_t tid_self = pthread_self();
        pthread_t _tid_connect = tid_connect;
        pthread_t _tid_read = tid_read;
        tid_read = 0;
        tid_connect = 0;

        // interrupt read(..) and , avoiding prolonged hang
        if( 0 != _tid_read && tid_self != _tid_read ) {
            int kerr;
            if( 0 != ( kerr = pthread_kill(_tid_read, SIGALRM) ) ) {
                ERR_PRINT("L2CAPComm::disconnect: pthread_kill read %p FAILED: %d", (void*)_tid_read, kerr);
            }
        }
        // interrupt connect(..) and , avoiding prolonged hang
        interrupt_flag = true;
        if( 0 != _tid_connect && _tid_read != _tid_connect && tid_self != _tid_connect ) {
            int kerr;
            if( 0 != ( kerr = pthread_kill(_tid_connect, SIGALRM) ) ) {
                ERR_PRINT("L2CAPComm::disconnect: pthread_kill connect %p FAILED: %d", (void*)_tid_connect, kerr);
            }
        }
    }

    l2cap_close_dev(socket_descriptor);
    socket_descriptor = -1;
    interrupt_flag = false;
    PERF_TS_TD("L2CAPComm::disconnect");
    DBG_PRINT("L2CAPComm::disconnect: End: dd %d", socket_descriptor.load());
    return true;
}

ssize_t L2CAPComm::read(uint8_t* buffer, const size_t capacity) {
    const int32_t timeoutMS = env.L2CAP_READER_POLL_TIMEOUT;
    ssize_t len = 0;
    ssize_t err_res = 0;

    tid_read = pthread_self(); // temporary safe tid to allow interruption

    if( 0 > socket_descriptor ) {
        err_res = -1; // invalid socket_descriptor or capacity
        goto errout;
    }
    if( 0 == capacity ) {
        goto done;
    }

    if( timeoutMS ) {
        struct pollfd p;
        int n;

        p.fd = socket_descriptor; p.events = POLLIN;
        while ( !interrupt_flag && (n = poll(&p, 1, timeoutMS)) < 0 ) {
            if ( !interrupt_flag && ( errno == EAGAIN || errno == EINTR ) ) {
                // cont temp unavail or interruption
                continue;
            }
            err_res = -10; // poll error !(ETIMEDOUT || EAGAIN || EINTR)
            goto errout;
        }
        if (!n) {
            err_res = -11; // poll error ETIMEDOUT
            errno = ETIMEDOUT;
            goto errout;
        }
    }

    while ((len = ::read(socket_descriptor, buffer, capacity)) < 0) {
        if (errno == EAGAIN || errno == EINTR ) {
            // cont temp unavail or interruption
            continue;
        }
        err_res = -20; // read error
        goto errout;
    }

done:
    tid_read = 0;
    return len;

errout:
    tid_read = 0;
    if( errno != ETIMEDOUT ) {
        has_ioerror = true;
        if( is_connected ) {
            if( env.L2CAP_RESTART_COUNT_ON_ERROR < 0 ) {
                ABORT("L2CAPComm::read: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                      err_res, getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid);
            } else {
                IRQ_PRINT("L2CAPComm::read: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                      err_res, getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid);
            }
        }
    }
    return err_res;
}

ssize_t L2CAPComm::write(const uint8_t * buffer, const size_t length) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor
    ssize_t len = 0;
    ssize_t err_res = 0;

    if( 0 > socket_descriptor ) {
        err_res = -1; // invalid socket_descriptor or capacity
        goto errout;
    }
    if( 0 == length ) {
        goto done;
    }

    while( ( len = ::write(socket_descriptor, buffer, length) ) < 0 ) {
        if( EAGAIN == errno || EINTR == errno ) {
            continue;
        }
        err_res = -10; // write error !(EAGAIN || EINTR)
        goto errout;
    }

done:
    return len;

errout:
    has_ioerror = true;
    if( is_connected ) {
        if( env.L2CAP_RESTART_COUNT_ON_ERROR < 0 ) {
            ABORT("L2CAPComm::write: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                  err_res, getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid);
        } else {
            IRQ_PRINT("L2CAPComm::write: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                  err_res, getStateString().c_str(), socket_descriptor.load(), deviceString.c_str(), psm, cid);
        }
    }
    return err_res;
}

