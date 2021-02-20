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

#include "org_direct_bt_BTUtils.h"

#include <cstdint>
#include <cinttypes>

#include <time.h>

#include <jau/dfa_utf8_decode.hpp>
#include <jau/environment.hpp>

#include "helper_base.hpp"

static const uint64_t NanoPerMilli = 1000000UL;
static const uint64_t MilliPerOne = 1000UL;

/**
 * See <http://man7.org/linux/man-pages/man2/clock_gettime.2.html>
 * <p>
 * Regarding avoiding kernel via VDSO,
 * see <http://man7.org/linux/man-pages/man7/vdso.7.html>,
 * clock_gettime seems to be well supported at least on kernel >= 4.4.
 * Only bfin and sh are missing, while ia64 seems to be complicated.
 */
jlong Java_org_direct_1bt_BTUtils_currentTimeMillis(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;

    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t res = static_cast<uint64_t>( t.tv_sec ) * MilliPerOne +
                   static_cast<uint64_t>( t.tv_nsec ) / NanoPerMilli;
    return (jlong)res;
}

jlong Java_org_direct_1bt_BTUtils_wallClockSeconds(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;

    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (jlong)( static_cast<uint64_t>( t.tv_sec ) );
}


jlong Java_org_direct_1bt_BTUtils_startupTimeMillisImpl(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;

    return jau::environment::startupTimeMilliseconds;
}

jstring Java_org_direct_1bt_BTUtils_decodeUTF8String(JNIEnv *env, jclass clazz, jbyteArray jbuffer, jint offset, jint size) {
    (void)clazz;

    const int buffer_size = env->GetArrayLength(jbuffer);
    if( 0 == buffer_size ) {
        return env->NewStringUTF("");
    }
    if( buffer_size < offset+size ) {
        std::string msg("buffer.length "+std::to_string(buffer_size)+
                        " < offset "+std::to_string(offset)+
                        " + size "+std::to_string(size));
        throw std::invalid_argument(msg.c_str());
    }

    std::string sres;
    {
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * buffer_ptr = criticalArray.get(jbuffer, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == buffer_ptr ) {
            throw std::invalid_argument("GetPrimitiveArrayCritical(byte array) is null");
        }
        sres = jau::dfa_utf8_decode(buffer_ptr+offset, static_cast<size_t>(size));
    }
    return jau::from_string_to_jstring(env, sres);
}
