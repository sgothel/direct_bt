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

#ifndef DBT_DEBUG_HPP_
#define DBT_DEBUG_HPP_

#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdarg>

extern "C" {
    #include <errno.h>
}

#include "DBTEnv.hpp"

// #define PERF_PRINT_ON 1

namespace direct_bt {

    /** Use for environment-variable DBTEnv::DEBUG conditional debug messages, prefix '[elapsed_time] Debug: '. */
    void DBG_PRINT(const char * format, ...) noexcept;

    /**
     * Use for environment-variable DBTEnv::VERBOSE conditional verbose messages, prefix '[elapsed_time] Wordy: '.
     * <p>
     * 'Wordy' is the shorter English form of the Latin word 'verbosus', from which the word 'verbosity' is sourced.
     * </p>
     */
    void WORDY_PRINT(const char * format, ...) noexcept;

    #define PERF_TS_T0_BASE()  const uint64_t _t0 = direct_bt::getCurrentMilliseconds()

    #define PERF_TS_TD_BASE(m)  { const uint64_t _td = direct_bt::getCurrentMilliseconds() - _t0; \
                                  fprintf(stderr, "[%'9" PRIu64 "] PERF %s done in %d ms,\n", direct_bt::DBTEnv::getElapsedMillisecond(), (m), (int)_td); }
    #ifdef PERF_PRINT_ON
        #define PERF_TS_T0() PERF_TS_T0_BASE()
        #define PERF_TS_TD(m) PERF_TS_TD_BASE(m)
    #else
        #define PERF_TS_T0()
        #define PERF_TS_TD(m)
    #endif
    #ifdef PERF2_PRINT_ON
        #define PERF2_TS_T0() PERF_TS_T0_BASE()
        #define PERF2_TS_TD(m) PERF_TS_TD_BASE(m)
    #else
        #define PERF2_TS_T0()
        #define PERF2_TS_TD(m)
    #endif
    #ifdef PERF3_PRINT_ON
        #define PERF3_TS_T0() PERF_TS_T0_BASE()
        #define PERF3_TS_TD(m) PERF_TS_TD_BASE(m)
    #else
        #define PERF3_TS_T0()
        #define PERF3_TS_TD(m)
    #endif

    /** Use for unconditional ::abort() call with given messages, prefix '[elapsed_time] ABORT @ file:line: '. Function also appends last errno and strerror(errno). */
    void ABORT_impl(const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional ::abort() call with given messages, prefix '[elapsed_time] ABORT @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define ABORT(...) { direct_bt::ABORT_impl(__FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional error messages, prefix '[elapsed_time] Error @ file:line: '. Function also appends last errno and strerror(errno). */
    void ERR_PRINTv(const char *file, const int line, const char * format, va_list args) noexcept;

    /** Use for unconditional error messages, prefix '[elapsed_time] 'prefix' @ file:line: '. Function also appends last errno and strerror(errno). */
    void ERR_PRINT_impl(const char *prefix, const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional error messages, prefix '[elapsed_time] Error @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define ERR_PRINT(...) { direct_bt::ERR_PRINT_impl("Error", __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional interruption messages, prefix '[elapsed_time] Interrupted @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define IRQ_PRINT(...) { direct_bt::ERR_PRINT_impl("Interrupted", __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional warning messages, prefix '[elapsed_time] Warning @ file:line: ' */
    void WARN_PRINTv(const char *file, const int line, const char * format, va_list args) noexcept;

    /** Use for unconditional warning messages, prefix '[elapsed_time] Warning @ file:line: ' */
    void WARN_PRINT_impl(const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional warning messages, prefix '[elapsed_time] Warning @ FILE:LINE: ' */
    #define WARN_PRINT(...) { direct_bt::WARN_PRINT_impl(__FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional informal messages, prefix '[elapsed_time] Info: '. */
    void INFO_PRINT(const char * format, ...) noexcept;

    /** Use for unconditional plain messages, prefix '[elapsed_time] '. */
    void PLAIN_PRINT(const char * format, ...) noexcept;

    /** Use for conditional plain messages, prefix '[elapsed_time] '. */
    void COND_PRINT_impl(const char *func, const char *file, const int line, const char * format, ...) noexcept;

    /** Use for conditional plain messages, prefix '[elapsed_time] '. */
    #define COND_PRINT(C, ...) { if(C) { COND_PRINT_impl(__func__, __FILE__, __LINE__, __VA_ARGS__); } }

    template<class ListElemType>
    inline void printSharedPtrList(std::string prefix, std::vector<std::shared_ptr<ListElemType>> & list) noexcept {
        fprintf(stderr, "%s: Start: %zd elements\n", prefix.c_str(), (size_t)list.size());
        int idx = 0;
        for (auto it = list.begin(); it != list.end(); idx++) {
            std::shared_ptr<ListElemType> & e = *it;
            if ( nullptr != e ) {
                fprintf(stderr, "%s[%d]: useCount %zd, mem %p\n", prefix.c_str(), idx, (size_t)e.use_count(), e.get());
            } else {
                fprintf(stderr, "%s[%d]: NULL\n", prefix.c_str(), idx);
            }
            ++it;
        }
    }

} // namespace direct_bt

#endif /* DBT_DEBUG_HPP_ */
