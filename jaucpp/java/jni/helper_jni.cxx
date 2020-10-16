/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Author: Andrei Vasiliu <andrei.vasiliu@intel.com>
 * Copyright (c) 2016 Intel Corporation.
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

#include <jau/jni/helper_jni.hpp>

using namespace jau;

//
// C++ <-> java exceptions
//

bool jau::java_exception_check(JNIEnv *env, const char* file, int line)
{
    jthrowable e = env->ExceptionOccurred();
    if( nullptr != e ) {
#if 1
        // ExceptionDescribe prints an exception and a backtrace of the stack to a system error-reporting channel, such as stderr.
        // The pending exception is cleared as a side-effect of calling this function. This is a convenience routine provided for debugging.
        env->ExceptionDescribe();
#endif
        env->ExceptionClear(); // just be sure, to have same side-effects

        jclass eClazz = search_class(env, e);
        jmethodID toString = search_method(env, eClazz, "toString", "()Ljava/lang/String;", false);
        jstring jmsg = (jstring) env->CallObjectMethod(e, toString);
        std::string msg = from_jstring_to_string(env, jmsg);
        fprintf(stderr, "Java exception occurred @ %s:%d and forward to Java: %s\n", file, line, msg.c_str()); fflush(stderr);

        env->Throw(e); // re-throw the java exception - java side!
        return true;
    }
    return false;
}

void jau::java_exception_check_and_throw(JNIEnv *env, const char* file, int line)
{
    jthrowable e = env->ExceptionOccurred();
    if( nullptr != e ) {
        // ExceptionDescribe prints an exception and a backtrace of the stack to a system error-reporting channel, such as stderr.
        // The pending exception is cleared as a side-effect of calling this function. This is a convenience routine provided for debugging.
        env->ExceptionDescribe();
        env->ExceptionClear(); // just be sure, to have same side-effects

        jclass eClazz = search_class(env, e);
        jmethodID toString = search_method(env, eClazz, "toString", "()Ljava/lang/String;", false);
        jstring jmsg = (jstring) env->CallObjectMethod(e, toString);
        std::string msg = from_jstring_to_string(env, jmsg);
        fprintf(stderr, "Java exception occurred @ %s:%d and forward to Native: %s\n", file, line, msg.c_str()); fflush(stderr);

        throw jau::RuntimeException("Java exception occurred @ %s : %d: "+msg, file, line);
    }
}

void jau::print_native_caught_exception_fwd2java(const std::exception &e, const char* file, int line) {
    fprintf(stderr, "Native exception caught @ %s:%d and forward to Java: %s\n", file, line, e.what()); fflush(stderr);
}
void jau::print_native_caught_exception_fwd2java(const std::string &msg, const char* file, int line) {
    fprintf(stderr, "Native exception caught @ %s:%d and forward to Java: %s\n", file, line, msg.c_str()); fflush(stderr);
}
void jau::print_native_caught_exception_fwd2java(const char * cmsg, const char* file, int line) {
    fprintf(stderr, "Native exception caught @ %s:%d and forward to Java: %s\n", file, line, cmsg); fflush(stderr);
}

void jau::raise_java_exception(JNIEnv *env, const std::exception &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/Error"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const std::runtime_error &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::RuntimeException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::InternalError &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/InternalError"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::NullPointerException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/NullPointerException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::IllegalArgumentException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const std::invalid_argument &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::IllegalStateException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::UnsupportedOperationException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/UnsupportedOperationException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::IndexOutOfBoundsException &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/IndexOutOfBoundsException"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const std::bad_alloc &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), e.what());
}
void jau::raise_java_exception(JNIEnv *env, const jau::OutOfMemoryError &e, const char* file, int line) {
    print_native_caught_exception_fwd2java(e, file, line);
    env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), e.what());
}

static std::string _unknown_exception_type_msg("Unknown exception type");

void jau::rethrow_and_raise_java_exception_jauimpl(JNIEnv *env, const char* file, int line) {
    // std::exception_ptr e = std::current_exception();
    try {
        // std::rethrow_exception(e);
        throw; // re-throw current exception
    } catch (const std::bad_alloc &e) {
        jau::raise_java_exception(env, e, file, line);
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
    } catch (const jau::RuntimeException &e) {
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

//
// Basic
//

jfieldID jau::getField(JNIEnv *env, jobject obj, const char* field_name, const char* field_signature) {
    jclass clazz = env->GetObjectClass(obj);
    java_exception_check_and_throw(env, E_FILE_LINE);
    // J == long
    jfieldID res = env->GetFieldID(clazz, field_name, field_signature);
    java_exception_check_and_throw(env, E_FILE_LINE);
    return res;
}

jclass jau::search_class(JNIEnv *env, const char *clazz_name)
{
    jclass clazz = env->FindClass(clazz_name);
    java_exception_check_and_throw(env, E_FILE_LINE);
    if (!clazz)
    {
        throw jau::InternalError(std::string("no class found: ")+clazz_name, E_FILE_LINE);
    }
    return clazz;
}

jclass jau::search_class(JNIEnv *env, jobject obj)
{
    jclass clazz = env->GetObjectClass(obj);
    java_exception_check_and_throw(env, E_FILE_LINE);
    if (!clazz)
    {
        throw jau::InternalError("no class found", E_FILE_LINE);
    }
    return clazz;
}

jclass jau::search_class(JNIEnv *env, JavaUplink &object)
{
    return search_class(env, object.get_java_class().c_str());
}

jmethodID jau::search_method(JNIEnv *env, jclass clazz, const char *method_name,
                             const char *prototype, bool is_static)
{
    jmethodID method;
    if (is_static)
    {
        method = env->GetStaticMethodID(clazz, method_name, prototype);
    }
    else
    {
        method = env->GetMethodID(clazz, method_name, prototype);
    }
    java_exception_check_and_throw(env, E_FILE_LINE);

    if (!method)
    {
        throw jau::InternalError(std::string("no method found: ")+method_name, E_FILE_LINE);
    }

    return method;
}

jfieldID jau::search_field(JNIEnv *env, jclass clazz, const char *field_name,
                           const char *type, bool is_static)
{
    jfieldID field;
    if (is_static)
    {
        field = env->GetStaticFieldID(clazz, field_name, type);
    }
    else
    {
        field = env->GetFieldID(clazz, field_name, type);
    }
    java_exception_check_and_throw(env, E_FILE_LINE);

    if (!field)
    {
        jau::InternalError(std::string("no field found: ")+field_name, E_FILE_LINE);
    }

    return field;
}

bool jau::from_jboolean_to_bool(jboolean val)
{
    bool result;

    if (val == JNI_TRUE)
    {
        result = true;
    }
    else
    {
        if (val == JNI_FALSE)
        {
            result = false;
        }
        else
        {
            throw jau::InternalError("the jboolean value is not true/false", E_FILE_LINE);
        }
    }

    return result;
}

std::string jau::from_jstring_to_string(JNIEnv *env, jstring str)
{
    jboolean is_copy = JNI_TRUE;
    if (!str) {
        throw std::invalid_argument("String should not be null");
    }
    const char *str_chars = (char *)env->GetStringUTFChars(str, &is_copy);
    if (!str_chars) {
            throw std::bad_alloc();
    }
    const std::string string_to_write = std::string(str_chars);

    env->ReleaseStringUTFChars(str, str_chars);

    return string_to_write;
}

jstring jau::from_string_to_jstring(JNIEnv *env, const std::string & str)
{
    return env->NewStringUTF(str.c_str());
}

jobject jau::get_new_arraylist(JNIEnv *env, unsigned int size, jmethodID *add)
{
    jclass arraylist_class = search_class(env, "java/util/ArrayList");
    jmethodID arraylist_ctor = search_method(env, arraylist_class, "<init>", "(I)V", false);

    jobject result = env->NewObject(arraylist_class, arraylist_ctor, size);
    if (!result)
    {
        throw jau::InternalError("Cannot create instance of class ArrayList", E_FILE_LINE);
    }

    *add = search_method(env, arraylist_class, "add", "(Ljava/lang/Object;)Z", false);

    env->DeleteLocalRef(arraylist_class);
    return result;
}


//
// C++ java_anon implementation
//

JavaGlobalObj::~JavaGlobalObj() noexcept {
    jobject obj = javaObjectRef.getObject();
    if( nullptr == obj || nullptr == mNotifyDeleted ) {
        return;
    }
    JNIEnv *env = *jni_env;
    env->CallVoidMethod(obj, mNotifyDeleted);
    java_exception_check_and_throw(env, E_FILE_LINE); // would abort() if thrown
}

//
// C++ java_anon <-> java access, assuming field "long nativeInstance" and native method 'void checkValid()'
//

//
// C++ java_anon <-> java access, all generic
//

