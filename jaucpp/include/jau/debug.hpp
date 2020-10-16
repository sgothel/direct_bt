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

#ifndef JAU_DEBUG_HPP_
#define JAU_DEBUG_HPP_

#include <memory>
#include <vector>

#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdarg>

extern "C" {
    #include <errno.h>
}

#include <jau/environment.hpp>

// #define PERF_PRINT_ON 1

namespace jau {

    /**
     * Returns a de-mangled backtrace string separated by newline excluding this function.
     * @param skip_frames number of stack frames to skip, default is one frame for this function.
     * @return the de-mangled backtrace, separated by newline
     */
    std::string get_backtrace(int skip_frames=1) noexcept;

    /**
     * Prints the de-mangled backtrace string separated by newline excluding this function to stderr, using get_backtrace().
     * @param skip_frames number of stack frames to skip, default is two frames for this function and for get_backtrace().
     */
    void print_backtrace(int skip_frames=2) noexcept;

    void DBG_PRINT_impl(const char * format, ...) noexcept;

    /** Use for environment-variable environment::DEBUG conditional debug messages, prefix '[elapsed_time] Debug: '. */
    #define DBG_PRINT(...) { if( jau::environment::get().DEBUG ) { jau::DBG_PRINT_impl(__VA_ARGS__); } }

    /** Use for environment-variable environment::DEBUG_JNI conditional debug messages, prefix '[elapsed_time] Debug: '. */
    #define DBG_JNI_PRINT(...) { if( jau::environment::get().DEBUG_JNI ) { jau::DBG_PRINT_impl(__VA_ARGS__); } }

    void WORDY_PRINT_impl(const char * format, ...) noexcept;

    /**
     * Use for environment-variable environment::VERBOSE conditional verbose messages, prefix '[elapsed_time] Wordy: '.
     * <p>
     * 'Wordy' is the shorter English form of the Latin word 'verbosus', from which the word 'verbosity' is sourced.
     * </p>
     */
    #define WORDY_PRINT(...) { if( jau::environment::get().VERBOSE ) { jau::WORDY_PRINT_impl(__VA_ARGS__); } }


    #define PERF_TS_T0_BASE()  const uint64_t _t0 = jau::getCurrentMilliseconds()

    #define PERF_TS_TD_BASE(m)  { const uint64_t _td = jau::getCurrentMilliseconds() - _t0; \
                                  fprintf(stderr, "[%'9" PRIu64 "] PERF %s done in %d ms,\n", jau::environment::getElapsedMillisecond(), (m), (int)_td); }
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
    void ABORT_impl(const char *func, const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional ::abort() call with given messages, prefix '[elapsed_time] ABORT @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define ABORT(...) { jau::ABORT_impl(__func__, __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional error messages, prefix '[elapsed_time] Error @ file:line: '. Function also appends last errno and strerror(errno). */
    void ERR_PRINTv(const char *func, const char *file, const int line, const char * format, va_list args) noexcept;

    void ERR_PRINT_impl(const char *prefix, const bool backtrace, const char *func, const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional error messages, prefix '[elapsed_time] Error @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define ERR_PRINT(...) { jau::ERR_PRINT_impl("Error", true /* backtrace */, __func__, __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional interruption messages, prefix '[elapsed_time] Interrupted @ FILE:LINE: '. Function also appends last errno and strerror(errno). */
    #define IRQ_PRINT(...) { jau::ERR_PRINT_impl("Interrupted", false /* backtrace */, __func__, __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional warning messages, prefix '[elapsed_time] Warning @ file:line: ' */
    void WARN_PRINTv(const char *func, const char *file, const int line, const char * format, va_list args) noexcept;

    void WARN_PRINT_impl(const char *func, const char *file, const int line, const char * format, ...) noexcept;

    /** Use for unconditional warning messages, prefix '[elapsed_time] Warning @ FILE:LINE: ' */
    #define WARN_PRINT(...) { jau::WARN_PRINT_impl(__func__, __FILE__, __LINE__, __VA_ARGS__); }

    /** Use for unconditional informal messages, prefix '[elapsed_time] Info: '. */
    void INFO_PRINT(const char * format, ...) noexcept;

    /** Use for unconditional plain messages, prefix '[elapsed_time] '. */
    void PLAIN_PRINT(const char * format, ...) noexcept;


    void COND_PRINT_impl(const char * format, ...) noexcept;

    /** Use for conditional plain messages, prefix '[elapsed_time] '. */
    #define COND_PRINT(C, ...) { if( C ) { jau::COND_PRINT_impl(__VA_ARGS__); } }


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

} // namespace jau

#endif /* JAU_DEBUG_HPP_ */
