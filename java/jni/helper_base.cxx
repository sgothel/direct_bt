/**
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

#include <jni.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "helper_base.hpp"

void raise_java_exception(JNIEnv *env, const direct_bt::BTException &e, const char* file, int line) {
    jau::print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("org/direct_bt/BTException"), e.what());
}

static std::string _unknown_exception_type_msg("Unknown exception type");

void rethrow_and_raise_java_exception_impl(JNIEnv *env, const char* file, int line) {
    // std::exception_ptr e = std::current_exception();
    try {
        // std::rethrow_exception(e);
        throw; // re-throw current exception
    } catch (const jau::OutOfMemoryError &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::InternalError &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::NullPointerException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::IllegalArgumentException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::IllegalStateException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::UnsupportedOperationException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const jau::IndexOutOfBoundsException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const direct_bt::BTException &e) {
        raise_java_exception(env, e, file, line);
    } catch (const jau::RuntimeException &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const std::bad_alloc &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const std::runtime_error &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const std::invalid_argument &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const std::exception &e) {
        jau::raise_java_exception(env, e, file, line);
    } catch (const std::string &msg) {
        jau::print_native_caught_exception_fwd2java(msg, file, line);
        env->ThrowNew(env->FindClass("java/lang/Error"), msg.c_str());
    } catch (const char *msg) {
        jau::print_native_caught_exception_fwd2java(msg, file, line);
        env->ThrowNew(env->FindClass("java/lang/Error"), msg);
    } catch (...) {
        jau::print_native_caught_exception_fwd2java(_unknown_exception_type_msg, file, line);
        env->ThrowNew(env->FindClass("java/lang/Error"), _unknown_exception_type_msg.c_str());
    }
}
