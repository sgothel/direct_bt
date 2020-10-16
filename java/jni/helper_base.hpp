/*
 * Author: Andrei Vasiliu <andrei.vasiliu@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
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

#ifndef HELPER_BASE_HPP_
#define HELPER_BASE_HPP_

#include <vector>
#include <memory>
#include <functional>
#include <jni.h>

#include <jau/jni/helper_jni.hpp>

#include "direct_bt/BTTypes.hpp"
#include "tinyb/BluetoothException.hpp"

jobject get_bluetooth_type(JNIEnv *env, const char *field_name);

void raise_java_exception(JNIEnv *env, const direct_bt::BluetoothException &e, const char* file, int line);
void raise_java_exception(JNIEnv *env, const tinyb::BluetoothException &e, const char* file, int line);

/**
 * Re-throw current exception and raise respective java exception
 * using any matching function above.
 */
void rethrow_and_raise_java_exception_impl(JNIEnv *env, const char* file, int line);

/**
 * Re-throw current exception and raise respective java exception
 * using any matching function above.
 */
#define rethrow_and_raise_java_exception(E) rethrow_and_raise_java_exception_impl((E), __FILE__, __LINE__)
// inline void rethrow_and_raise_java_exception(JNIEnv *env) { rethrow_and_raise_java_exception_impl(env, __FILE__, __LINE__); }

#endif /* HELPER_BASE_HPP_ */
