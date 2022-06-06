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

#include "org_direct_bt_BTFactory.h"

#include <direct_bt/version.h>

#include "helper_base.hpp"

jstring Java_org_direct_1bt_BTFactory_getNativeVersion(JNIEnv *env, jclass clazz)
{
    try {
        (void) clazz;

        std::string api_version = std::string(DIRECT_BT_VERSION);
        return env->NewStringUTF(api_version.c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_org_direct_1bt_BTFactory_getNativeAPIVersion(JNIEnv *env, jclass clazz)
{
    try {
        (void) clazz;

        std::string api_version = std::string(DIRECT_BT_VERSION_API);
        return env->NewStringUTF(api_version.c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_org_direct_1bt_BTFactory_setenv(JNIEnv *env, jclass clazz, jstring jname, jstring jval, jboolean overwrite)
{
    try {
        (void) clazz;

        std::string name = jau::jni::from_jstring_to_string(env, jname);
        std::string value = jau::jni::from_jstring_to_string(env, jval);
        if( name.length() > 0 ) {
            if( value.length() > 0 ) {
                setenv(name.c_str(), value.c_str(), overwrite);
            } else {
                setenv(name.c_str(), "true", overwrite);
            }
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
