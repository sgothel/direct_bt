/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

#ifndef DBT_CONST_HPP_
#define DBT_CONST_HPP_

#include <cstddef>

#include <jau/int_types.hpp>

namespace direct_bt {

#ifdef __linux__

    /**
     * Linux/BlueZ prohibits access to the existing SMP implementation via L2CAP (socket).
     */
    inline constexpr const bool SMP_SUPPORTED_BY_OS = false;

    inline constexpr const bool USE_LINUX_BT_SECURITY = true;

    inline constexpr const bool CONSIDER_HCI_CMD_FOR_SMP_STATE = false;

    inline constexpr const bool SCAN_DISABLED_POST_CONNECT = true;

#else

    inline constexpr const bool SMP_SUPPORTED_BY_OS = true;

    inline constexpr const bool USE_LINUX_BT_SECURITY = false;

    inline constexpr const bool CONSIDER_HCI_CMD_FOR_SMP_STATE = true;

    inline constexpr const bool SCAN_DISABLED_POST_CONNECT = false;

#endif

    /**
     * Maximum time in milliseconds to wait for a thread shutdown.
     *
     * Usually used for socket reader threads, like used within HCIHandler.
     */
    inline constexpr const jau::nsize_t THREAD_SHUTDOWN_TIMEOUT_MS = 8000;

    /**
     * Maximum time in milliseconds to wait for the next SMP event.
     */
    inline constexpr const jau::nsize_t SMP_NEXT_EVENT_TIMEOUT_MS = 2000;

    /**
     * Maximum number of enabling discovery in background in case of failure
     */
    inline constexpr const jau::nsize_t MAX_BACKGROUND_DISCOVERY_RETRY = 3;

} // namespace direct_bt

#endif /* DBT_CONST_HPP_ */
