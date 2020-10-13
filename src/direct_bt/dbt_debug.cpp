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

#include "direct_bt/dbt_debug.hpp"

#include <cstdarg>

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <cxxabi.h>

using namespace direct_bt;

std::string direct_bt::get_backtrace(int skip_frames) noexcept {
    // symbol:
    //  1: _ZN9direct_bt10DBTAdapter14startDiscoveryEbNS_19HCILEOwnAddressTypeEtt + 0x58d @ ip 0x7faa959d6daf, sp 0x7ffe38f301e0
    // de-mangled:
    //  1: direct_bt::DBTAdapter::startDiscovery(bool, direct_bt::HCILEOwnAddressType, unsigned short, unsigned short) + 0x58d @ ip 0x7f687b459071, sp 0x7fff2bf795d0
    std::string out;
    int depth, res;
    char symbol[201];
    char cstr[256];
    unw_context_t uc;
    unw_word_t ip, sp;
    unw_cursor_t cursor;
    unw_word_t offset;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    for(depth=0; unw_step(&cursor) > 0; depth++) {
        if( skip_frames > depth ) {
            continue;
        }
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        if( 0 == ( res = unw_get_proc_name(&cursor, symbol, sizeof(symbol), &offset) ) ) {
            int status;
            char *real_name;
            symbol[sizeof(symbol) -1] = 0; // fail safe
            if ( (real_name = abi::__cxa_demangle(symbol, nullptr, nullptr, &status)) == nullptr ) {
                real_name = symbol; // didn't work, use symbol
            }
            snprintf(cstr, sizeof(cstr), "%3d: %s + 0x%lx @ ip 0x%lx, sp 0x%lx\n", depth, real_name, (uint64_t)offset, (uint64_t)ip, (uint64_t)sp);
            if( real_name != symbol ) {
                free( real_name );
            }
        } else {
            snprintf(cstr, sizeof(cstr), "%3d: ip 0x%lx, sp 0x%lx, get_proc_name error 0x%x\n", depth, (uint64_t)ip, (uint64_t)sp, res);
        }
        out.append(cstr);
    }
    return out;
}

void direct_bt::print_backtrace(int skip_frames) noexcept {
    fprintf(stderr, get_backtrace(skip_frames).c_str());
    fflush(stderr);
}

void direct_bt::DBG_PRINT_impl(const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Debug: ", DBTEnv::getElapsedMillisecond());
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::WORDY_PRINT_impl(const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Wordy: ", DBTEnv::getElapsedMillisecond());
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::ABORT_impl(const char *func, const char *file, const int line, const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] ABORT @ %s:%d %s: ", DBTEnv::getElapsedMillisecond(), file, line, func);
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "; last errno %d %s\n", errno, strerror(errno));
    fflush(stderr);
    direct_bt::print_backtrace(2);
    abort();
}

void direct_bt::ERR_PRINTv(const char *func, const char *file, const int line, const char * format, va_list args) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Error @ %s:%d %s: ", DBTEnv::getElapsedMillisecond(), file, line, func);
    vfprintf(stderr, format, args);
    fprintf(stderr, "; last errno %d %s\n", errno, strerror(errno));
    fflush(stderr);
    direct_bt::print_backtrace(2);
}

void direct_bt::ERR_PRINT_impl(const char *prefix, const char *func, const char *file, const int line, const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] %s @ %s:%d %s: ", DBTEnv::getElapsedMillisecond(), prefix, file, line, func);
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "; last errno %d %s\n", errno, strerror(errno));
    fflush(stderr);
    direct_bt::print_backtrace(2);
}

void direct_bt::WARN_PRINTv(const char *func, const char *file, const int line, const char * format, va_list args) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Warning @ %s:%d %s: ", DBTEnv::getElapsedMillisecond(), file, line, func);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::WARN_PRINT_impl(const char *func, const char *file, const int line, const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Warning @ %s:%d %s: ", DBTEnv::getElapsedMillisecond(), file, line, func);
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::INFO_PRINT(const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] Info: ", DBTEnv::getElapsedMillisecond());
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::PLAIN_PRINT(const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] ", DBTEnv::getElapsedMillisecond());
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void direct_bt::COND_PRINT_impl(const char * format, ...) noexcept {
    fprintf(stderr, "[%'9" PRIu64 "] ", DBTEnv::getElapsedMillisecond());
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    fflush(stderr);
}
