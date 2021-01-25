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

#include <BTAdapter.hpp>
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

int L2CAPComm::l2cap_open_dev(const EUI48 & adapterAddress, const uint16_t psm, const uint16_t cid, const BDAddressType addrType) {
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
    a.l2_bdaddr_type = ::number(addrType);
    if ( ::bind(fd, (struct sockaddr *) &a, sizeof(a)) < 0 ) {
        ERR_PRINT("L2CAPComm::l2cap_open_dev: bind failed");
        goto failed;
    }
    return fd;

failed:
    err = errno;
    ::close(fd);
    errno = err;

    return -1;
}

int L2CAPComm::l2cap_close_dev(int dd)
{
    return ::close(dd);
}


// *************************************************
// *************************************************
// *************************************************

L2CAPComm::L2CAPComm(const EUI48& adapterAddress_, const uint16_t psm_, const uint16_t cid_)
: env(L2CAPEnv::get()),
  adapterAddress(adapterAddress_),
  psm(psm_), cid(cid_),
  deviceAddressAndType(BDAddressAndType::ANY_BREDR_DEVICE),
  socket_descriptor(-1),
  is_open(false), has_ioerror(false), interrupt_flag(false), tid_connect(0), tid_read(0)
{ }


/**
 * Setting BT_SECURITY within open() after bind() and before connect()
 * causes BlueZ/Kernel to immediately process SMP, leading to a potential deadlock.
 *
 * Here we experience, setting security level before connect()
 * will block the thread within connect(), potentially a mutex used in the SMP procedure.
 *
 * Hence we set BT_SECURITY after connect() within open().
 */
#define SET_BT_SECURITY_POST_CONNECT 1

bool L2CAPComm::open(const BTDevice& device, const BTSecurityLevel sec_level) {

    bool expOpen = false; // C++11, exp as value since C++20
    if( !is_open.compare_exchange_strong(expOpen, true) ) {
        DBG_PRINT("L2CAPComm::open: Already open: %s, dd %d, %s, psm %u, cid %u",
                  getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        return false;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor

    /**
     * bt_io_connect ( create_io ) with source address
     * - fd = socket(.._)
     * - bind(fd, ..)
     * - l2cap_set
     * -- set imtu, omtu, mode
     * -- l2cap_set_master
     * -- l2cap_set_flushable
     * -- set_priority
     * -- set_sec_level
     * --- setsockopt(.. BT_SECURITY ..)
     *
     * - l2cap_connect with destination address
     * -- connect(fd, ..)
     */
    deviceAddressAndType = device.getAddressAndType();

    /** BT Core Spec v5.2: Vol 3, Part A: L2CAP_CONNECTION_REQ */
    sockaddr_l2 req;
    int res;
    int to_retry_count=0; // ETIMEDOUT retry count

    DBG_PRINT("L2CAPComm::open: Start Connect: %s, dd %d, %s, psm %u, cid %u",
              getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);

    socket_descriptor = l2cap_open_dev(adapterAddress, psm, cid, BDAddressType::BDADDR_LE_PUBLIC);

    if( 0 > socket_descriptor ) {
        goto failure; // open failed
    }

#if !SET_BT_SECURITY_POST_CONNECT
    #if USE_LINUX_BT_SECURITY
        if( BTSecurityLevel::UNSET < sec_level ) {
            if( !setBTSecurityLevelImpl(sec_level) ) {
                goto failure; // sec_level failed
            }
        }
    #endif
#endif

    tid_connect = pthread_self(); // temporary safe tid to allow interruption

    // actual request to connect to remote device
    bzero((void *)&req, sizeof(req));
    req.l2_family = AF_BLUETOOTH;
    req.l2_psm = jau::cpu_to_le(psm);
    req.l2_bdaddr = deviceAddressAndType.address;
    req.l2_cid = jau::cpu_to_le(cid);
    req.l2_bdaddr_type = ::number(deviceAddressAndType.type);

    while( !interrupt_flag ) {
        // blocking
        res = ::connect(socket_descriptor, (struct sockaddr*)&req, sizeof(req));

        DBG_PRINT("L2CAPComm::open: Connect Result %d, errno 0%X %s, %s", res, errno, strerror(errno), deviceAddressAndType.toString().c_str());

        if( !res )
        {
            break; // done

        } else if( ETIMEDOUT == errno ) {
            to_retry_count++;
            if( to_retry_count < number(Defaults::L2CAP_CONNECT_MAX_RETRY) ) {
                WORDY_PRINT("L2CAPComm::open: Connect timeout, retry %d", to_retry_count);
                continue;
            } else {
                ERR_PRINT("L2CAPComm::open: Connect timeout, retried %d", to_retry_count);
                goto failure; // exit
            }

        } else  {
            // EALREADY == errno || ENETUNREACH == errno || EHOSTUNREACH == errno || ..
            ERR_PRINT("L2CAPComm::open: Connect failed");
            goto failure; // exit
        }
    }
    // success
    tid_connect = 0;

#if SET_BT_SECURITY_POST_CONNECT
    #if USE_LINUX_BT_SECURITY
        if( BTSecurityLevel::UNSET < sec_level ) {
            if( !setBTSecurityLevelImpl(sec_level) ) {
                goto failure; // sec_level failed
            }
        }
    #endif
#endif

    return true;

failure:
    const int err = errno;
    close();
    errno = err;
    return false;
}

bool L2CAPComm::close() noexcept {
    bool expOpen = true; // C++11, exp as value since C++20
    if( !is_open.compare_exchange_strong(expOpen, false) ) {
        DBG_PRINT("L2CAPComm::close: Not connected: %s, dd %d, %s, psm %u, cid %u",
                  getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        return true;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor

    has_ioerror = false;
    DBG_PRINT("L2CAPComm::close: Start: %s, dd %d, %s, psm %u, cid %u",
              getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
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
                ERR_PRINT("L2CAPComm::close: pthread_kill read %p FAILED: %d", (void*)_tid_read, kerr);
            }
        }
        // interrupt connect(..) and , avoiding prolonged hang
        interrupt_flag = true;
        if( 0 != _tid_connect && _tid_read != _tid_connect && tid_self != _tid_connect ) {
            int kerr;
            if( 0 != ( kerr = pthread_kill(_tid_connect, SIGALRM) ) ) {
                ERR_PRINT("L2CAPComm::close: pthread_kill connect %p FAILED: %d", (void*)_tid_connect, kerr);
            }
        }
    }

    l2cap_close_dev(socket_descriptor);
    socket_descriptor = -1;
    interrupt_flag = false;
    PERF_TS_TD("L2CAPComm::close");
    DBG_PRINT("L2CAPComm::close: End: dd %d", socket_descriptor.load());
    return true;
}

bool L2CAPComm::setBTSecurityLevel(const BTSecurityLevel sec_level) {
    if( !is_open ) {
        DBG_PRINT("L2CAPComm::setBTSecurityLevel: Not connected: %s, dd %d, %s, psm %u, cid %u",
                  getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        return false;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor

    return setBTSecurityLevelImpl(sec_level);
}

bool L2CAPComm::setBTSecurityLevelImpl(const BTSecurityLevel sec_level) {
    if( BTSecurityLevel::NONE > sec_level ) {
        DBG_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s, not set", getBTSecurityLevelString(sec_level).c_str());
        return false;
    }

#if USE_LINUX_BT_SECURITY
    struct bt_security bt_sec;
    int result;

    BTSecurityLevel old_sec_level = getBTSecurityLevelImpl();
    if( old_sec_level != sec_level ) {
        bzero(&bt_sec, sizeof(bt_sec));
        bt_sec.level = direct_bt::number(sec_level);
        result = setsockopt(socket_descriptor, SOL_BLUETOOTH, BT_SECURITY, &bt_sec, sizeof(bt_sec));
        if ( 0 == result ) {
            DBG_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s -> %s, success",
                    getBTSecurityLevelString(old_sec_level).c_str(), getBTSecurityLevelString(sec_level).c_str());
            return true;
        } else {
            ERR_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s -> %s, failed",
                    getBTSecurityLevelString(old_sec_level).c_str(), getBTSecurityLevelString(sec_level).c_str());
            return false;
        }
    } else {
        DBG_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s == %s, success (ignored)",
                getBTSecurityLevelString(old_sec_level).c_str(), getBTSecurityLevelString(sec_level).c_str());
        return true;
    }
#else
    DBG_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s, not implemented", getBTSecurityLevelString(sec_level).c_str());
    return false;
#endif
}

BTSecurityLevel L2CAPComm::getBTSecurityLevel() {
    if( !is_open ) {
        DBG_PRINT("L2CAPComm::getBTSecurityLevel: Not connected: %s, dd %d, %s, psm %u, cid %u",
                  getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        return BTSecurityLevel::UNSET;
    }
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor

    return getBTSecurityLevelImpl();
}

BTSecurityLevel L2CAPComm::getBTSecurityLevelImpl() {
    BTSecurityLevel sec_level = BTSecurityLevel::UNSET;
#if USE_LINUX_BT_SECURITY
    struct bt_security bt_sec;
    socklen_t optlen = sizeof(bt_sec);
    int result;

    bzero(&bt_sec, sizeof(bt_sec));
    result = getsockopt(socket_descriptor, SOL_BLUETOOTH, BT_SECURITY, &bt_sec, &optlen);
    if ( 0 == result ) {
        if( optlen == sizeof(bt_sec) ) {
            sec_level = static_cast<BTSecurityLevel>(bt_sec.level);
            DBG_PRINT("L2CAPComm::getBTSecurityLevel: sec_level %s, success", getBTSecurityLevelString(sec_level).c_str());
        } else {
            ERR_PRINT("L2CAPComm::getBTSecurityLevel: sec_level %s, failed. Returned size %zd != %zd ",
                    getBTSecurityLevelString(sec_level).c_str(), optlen, sizeof(bt_sec));
        }
    } else {
        ERR_PRINT("L2CAPComm::getBTSecurityLevel: sec_level %s, failed. Result %d", getBTSecurityLevelString(sec_level).c_str(), result);
    }
#else
    DBG_PRINT("L2CAPComm::setBTSecurityLevel: sec_level %s, not implemented", getBTSecurityLevelString(sec_level).c_str());
#endif
    return sec_level;
}

jau::snsize_t L2CAPComm::read(uint8_t* buffer, const jau::nsize_t capacity) {
    const int32_t timeoutMS = env.L2CAP_READER_POLL_TIMEOUT;
    jau::snsize_t len = 0;
    jau::snsize_t err_res = 0;

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
        if( is_open ) {
            if( env.L2CAP_RESTART_COUNT_ON_ERROR < 0 ) {
                ABORT("L2CAPComm::read: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                      err_res, getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
            } else {
                IRQ_PRINT("L2CAPComm::read: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                      err_res, getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
            }
        }
    }
    return err_res;
}

jau::snsize_t L2CAPComm::write(const uint8_t * buffer, const jau::nsize_t length) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor
    jau::snsize_t len = 0;
    jau::snsize_t err_res = 0;

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
    if( is_open ) {
        if( env.L2CAP_RESTART_COUNT_ON_ERROR < 0 ) {
            ABORT("L2CAPComm::write: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                  err_res, getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        } else {
            IRQ_PRINT("L2CAPComm::write: Error res %d; %s, dd %d, %s, psm %u, cid %u",
                  err_res, getStateString().c_str(), socket_descriptor.load(), deviceAddressAndType.toString().c_str(), psm, cid);
        }
    }
    return err_res;
}

