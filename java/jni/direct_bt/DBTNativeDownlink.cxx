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

#include "jau_direct_bt_DBTNativeDownlink.h"

#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTTypes1.hpp"

using namespace direct_bt;
using namespace jau::jni;

void Java_jau_direct_1bt_DBTNativeDownlink_initNativeJavaObject(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    try {
        shared_ptr_ref<JavaUplink> javaUplink(nativeInstance); // hold copy until done
        javaUplink.null_check2();
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below
        jclass javaClazz = search_class(env, global_obj.getObject());
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == javaClazz ) {
            throw jau::InternalError("DBTNativeDownlink class not found", E_FILE_LINE);
        }
        jmethodID  mNotifyDeleted = search_method(env, javaClazz, "notifyDeleted", "()V", false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mNotifyDeleted ) {
            throw jau::InternalError("DBTNativeDownlink class has no notifyDeleted() method, for "+javaUplink->toString(), E_FILE_LINE);
        }
        javaUplink->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), mNotifyDeleted ) );
        JavaGlobalObj::check(javaUplink->getJavaObject(), E_FILE_LINE);
        DBG_JNI_PRINT("Java_jau_direct_1bt_DBTNativeDownlink_initNativeJavaObject %p -> %s", javaUplink.shared_ptr().get(), javaUplink->toString().c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTNativeDownlink_deleteNativeJavaObject(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        shared_ptr_ref<JavaUplink> javaUplink(nativeInstance); // hold copy until done
        javaUplink.null_check2();
        DBG_JNI_PRINT("Java_jau_direct_1bt_DBTNativeDownlink_deleteNativeJavaObject %p -> %s", javaUplink.shared_ptr().get(), javaUplink->toString().c_str());
        javaUplink->setJavaObject();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

