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

// #define VERBOSE_ON 1
#include <dbt_debug.hpp>

#include "HCIComm.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <sys/param.h>
    #include <sys/uio.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <poll.h>
}

namespace direct_bt {

int HCIComm::hci_open_dev(const uint16_t dev_id, const uint16_t channel) noexcept
{
	sockaddr_hci a;
	int fd, err;

	/**
	 * dev_id is unsigned and hence always >= 0
	if ( 0 > dev_id ) {
		errno = ENODEV;
		ERR_PRINT("hci_open_dev: invalid dev_id");
		return -1;
	} */

	// Create a loose HCI socket
	fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (0 > fd ) {
        ERR_PRINT("HCIComm::hci_open_dev: socket failed");
		return fd;
	}

	// Bind socket to the HCI device
	bzero((void *)&a, sizeof(a));
	a.hci_family = AF_BLUETOOTH;
	a.hci_dev = dev_id;
	a.hci_channel = channel;
	if (bind(fd, (struct sockaddr *) &a, sizeof(a)) < 0) {
	    ERR_PRINT("hci_open_dev: bind failed");
		goto failed;
	}

	return fd;

failed:
	err = errno;
	::close(fd);
	errno = err;

	return -1;
}

int HCIComm::hci_close_dev(int dd) noexcept
{
	return ::close(dd);
}

void HCIComm::close() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor
    if( 0 > socket_descriptor ) {
        return;
    }
    hci_close_dev(socket_descriptor);
    socket_descriptor = -1;
}

int HCIComm::read(uint8_t* buffer, const int capacity, const int32_t timeoutMS) noexcept {
    int len = 0;
    if( 0 > socket_descriptor || 0 > capacity ) {
        goto errout;
    }
    if( 0 == capacity ) {
        goto done;
    }

    if( timeoutMS ) {
        struct pollfd p;
        int n;

        p.fd = socket_descriptor; p.events = POLLIN;
#if 0
        sigset_t sigmask;
        sigemptyset(&sigmask);
        // sigaddset(&sigmask, SIGALRM);
        struct timespec timeout_ts;
        timeout_ts.tv_sec=0;
        timeout_ts.tv_nsec=(long)timeoutMS*1000000L;
        while ((n = ppoll(&p, 1, &timeout_ts, &sigmask)) < 0) {
#else
        while ((n = poll(&p, 1, timeoutMS)) < 0) {
#endif
            if (errno == EAGAIN || errno == EINTR ) {
                // cont temp unavail or interruption
                continue;
            }
            goto errout;
        }
        if (!n) {
            errno = ETIMEDOUT;
            goto errout;
        }
    }

    while ((len = ::read(socket_descriptor, buffer, capacity)) < 0) {
        if (errno == EAGAIN || errno == EINTR ) {
            // cont temp unavail or interruption
            continue;
        }
        goto errout;
    }

done:
    return len;

errout:
    return -1;
}

int HCIComm::write(const uint8_t* buffer, const int size) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_write); // RAII-style acquire and relinquish via destructor
    int len = 0;
    if( 0 > socket_descriptor || 0 > size ) {
        goto errout;
    }
    if( 0 == size ) {
        goto done;
    }

    while( ( len = ::write(socket_descriptor, buffer, size) ) < 0 ) {
        if( EAGAIN == errno || EINTR == errno ) {
            continue;
        }
        goto errout;
    }

done:
    return len;

errout:
    return -1;
}

} /* namespace direct_bt */
