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

#include "jau_direct_bt_DBTGattDesc.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/BTAdapter.hpp"

using namespace direct_bt;
using namespace jau;

void Java_jau_direct_1bt_DBTGattDesc_deleteImpl(JNIEnv *env, jobject obj) {
    try {
        BTGattDesc *descriptor = getJavaUplinkObject<BTGattDesc>(env, obj);
        (void)descriptor;
        // No delete: Service instance owned by GATTService -> BTDevice
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_jau_direct_1bt_DBTGattDesc_toStringImpl(JNIEnv *env, jobject obj) {
    (void)obj;
    try {
        BTGattDesc *descriptor = getJavaUplinkObject<BTGattDesc>(env, obj);
        JavaGlobalObj::check(descriptor->getJavaObject(), E_FILE_LINE);
        return from_string_to_jstring(env, descriptor->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jbyteArray Java_jau_direct_1bt_DBTGattDesc_readValueImpl(JNIEnv *env, jobject obj) {
    try {
        BTGattDesc *descriptor = getJavaUplinkObject<BTGattDesc>(env, obj);
        JavaGlobalObj::check(descriptor->getJavaObject(), E_FILE_LINE);

        if( !descriptor->readValue() ) {
            ERR_PRINT("Characteristic readValue failed: %s", descriptor->toString().c_str());
            return env->NewByteArray((jsize)0);
        }
        const size_t value_size = descriptor->value.getSize();
        jbyteArray jres = env->NewByteArray((jsize)value_size);
        env->SetByteArrayRegion(jres, 0, (jsize)value_size, (const jbyte *)descriptor->value.get_ptr());
        java_exception_check_and_throw(env, E_FILE_LINE);
        return jres;

    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_jau_direct_1bt_DBTGattDesc_writeValueImpl(JNIEnv *env, jobject obj, jbyteArray jval) {
    try {
        if( nullptr == jval ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t value_size = (size_t)env->GetArrayLength(jval);
        if( 0 == value_size ) {
            return JNI_TRUE;
        }
        BTGattDesc *descriptor = getJavaUplinkObject<BTGattDesc>(env, obj);
        JavaGlobalObj::check(descriptor->getJavaObject(), E_FILE_LINE);

        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * value_ptr = criticalArray.get(jval, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == value_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        TROOctets value(value_ptr, value_size);
        descriptor->value = value; // copy data

        if( !descriptor->writeValue() ) {
            ERR_PRINT("Descriptor writeValue failed: %s", descriptor->toString().c_str());
            return JNI_FALSE;
        }
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}


