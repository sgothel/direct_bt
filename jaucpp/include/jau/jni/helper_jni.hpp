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

#ifndef JAU_HELPER_JNI_HPP_
#define JAU_HELPER_JNI_HPP_

#include <vector>
#include <memory>
#include <functional>
#include <jni.h>

#include <jau/java_uplink.hpp>
#include <jau/basic_types.hpp>

#include <jau/jni/jni_mem.hpp>

namespace jau {

    //
    // C++ <-> java exceptions
    //

    /**
     * Return true if a java exception occurred, otherwise false.
     * <p>
     * In case of an exception, the information might be logged to stderr.
     * </p>
     * <p>
     * In case of an exception, user shall release resourced in their JNI code
     * and leave immediately.
     * </p>
     */
    bool java_exception_check(JNIEnv *env, const char* file, int line);

    /**
     * Throws a C++ exception if a java exception occurred, otherwise do nothing.
     * <p>
     * In case of an exception, the information might be logged to stderr.
     * </p>
     * <p>
     * In case of an exception and hence thrown C++ exception,
     * might want to catch all and handle it via {@link #rethrow_and_raise_java_exception(JNIEnv*)}.
     * </p>
     */
    void java_exception_check_and_throw(JNIEnv *env, const char* file, int line);

    void print_native_caught_exception_fwd2java(const std::exception &e, const char* file, int line);
    void print_native_caught_exception_fwd2java(const std::string &msg, const char* file, int line);
    void print_native_caught_exception_fwd2java(const char * cmsg, const char* file, int line);

    void raise_java_exception(JNIEnv *env, const std::exception &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const std::runtime_error &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::RuntimeException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::InternalError &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::NullPointerException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::IllegalArgumentException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const std::invalid_argument &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::IllegalStateException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::UnsupportedOperationException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::IndexOutOfBoundsException &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const std::bad_alloc &e, const char* file, int line);
    void raise_java_exception(JNIEnv *env, const jau::OutOfMemoryError &e, const char* file, int line);

    /**
     * Re-throw current exception and raise respective java exception
     * using any matching function above.
     */
    void rethrow_and_raise_java_exception_jauimpl(JNIEnv *env, const char* file, int line);

    /**
     * Re-throw current exception and raise respective java exception
     * using any matching function above.
     */
    #define rethrow_and_raise_java_exception_jau(E) rethrow_and_raise_java_exception_jauimpl((E), __FILE__, __LINE__)
    // inline void rethrow_and_raise_java_exception_jau(JNIEnv *env) { rethrow_and_raise_java_exception_jauimpl(env, __FILE__, __LINE__); }

    //
    // Basic
    //

    jfieldID getField(JNIEnv *env, jobject obj, const char* field_name, const char* field_signature);
    inline jfieldID getInstanceField(JNIEnv *env, jobject obj) {
        return getField(env, obj, "nativeInstance", "J");
    }

    jclass search_class(JNIEnv *env, const char *clazz_name);
    jclass search_class(JNIEnv *env, jobject obj);
    jclass search_class(JNIEnv *env, JavaUplink &object);

    jmethodID search_method(JNIEnv *env, jclass clazz, const char *method_name,
                            const char *prototype, bool is_static);
    jfieldID search_field(JNIEnv *env, jclass clazz, const char *field_name,
                            const char *type, bool is_static);

    bool from_jboolean_to_bool(jboolean val);
    std::string from_jstring_to_string(JNIEnv *env, jstring str);
    jstring from_string_to_jstring(JNIEnv *env, const std::string & str);

    jobject get_new_arraylist(JNIEnv *env, unsigned int size, jmethodID *add);

    //
    // C++ JavaAnon implementation
    //

    /**
     * Implementation for JavaAnon,
     * by simply wrapping a JNIGlobalRef instance.
     */
    class JavaGlobalObj : public JavaAnon {
        private:
            JNIGlobalRef javaObjectRef;
            jmethodID  mNotifyDeleted;

        public:
            static inline void check(const std::shared_ptr<JavaAnon> & shref, const char* file, int line) {
                if( nullptr == shref ) {
                    throw RuntimeException("JavaGlobalObj::check: Null shared-JavaAnonObj", file, line);
                }
                const jobject obj = static_cast<const JavaGlobalObj*>(shref.get())->getObject();
                if( nullptr == obj ) {
                    throw RuntimeException("JavaGlobalObj::check: Null object", file, line);
                }
            }
            static bool isValid(const std::shared_ptr<JavaAnon> & shref) noexcept {
                if( nullptr == shref ) {
                    return false;
                }
                const jobject obj = static_cast<const JavaGlobalObj*>(shref.get())->getObject();
                if( nullptr == obj ) {
                    return false;
                }
                return true;
            }
            JavaGlobalObj(jobject obj, jmethodID mNotifyDeleted) noexcept
            : javaObjectRef(obj), mNotifyDeleted(mNotifyDeleted) { }

            JavaGlobalObj(const JavaGlobalObj &o) noexcept = default;
            JavaGlobalObj(JavaGlobalObj &&o) noexcept = default;
            JavaGlobalObj& operator=(const JavaGlobalObj &o) noexcept = default;
            JavaGlobalObj& operator=(JavaGlobalObj &&o) noexcept = default;

            virtual ~JavaGlobalObj() noexcept;

            std::string toString() const noexcept override {
                const uint64_t ref = (uint64_t)(void*)javaObjectRef.getObject();
                return "JavaGlobalObj["+uint64HexString(ref, true)+"]";
            }

            /** Clears the java reference, i.e. nulling it, without deleting the global reference via JNI. */
            void clear() noexcept override { javaObjectRef.clear(); }

            JNIGlobalRef & getJavaObject() noexcept { return javaObjectRef; }

            /* Provides access to the stored GlobalRef as an jobject. */
            jobject getObject() const noexcept { return javaObjectRef.getObject(); }
            /* Provides access to the stored GlobalRef as a jclass. */
            jclass getClass() const noexcept { return javaObjectRef.getClass(); }

            /* Provides access to the stored GlobalRef as an getJavaObject. */
            static JNIGlobalRef GetJavaObject(const std::shared_ptr<JavaAnon> & shref) noexcept {
                return static_cast<JavaGlobalObj*>(shref.get())->getJavaObject();
            }
            /* Provides access to the stored GlobalRef as an jobject. */
            static jobject GetObject(const std::shared_ptr<JavaAnon> & shref) noexcept {
                return static_cast<JavaGlobalObj*>(shref.get())->getObject();
            }

            /* Provides access to the stored GlobalRef as a jclass. */
            static jclass GetClass(const std::shared_ptr<JavaAnon> & shref) noexcept {
                return static_cast<JavaGlobalObj*>(shref.get())->getClass();
            }
    };

    //
    // C++ JavaUplink <-> java access, assuming it implementats JavaUplink: field "long nativeInstance" and native method 'void checkValid()' etc
    //

    template <typename T>
    T *getJavaUplinkObject(JNIEnv *env, jobject obj)
    {
        jlong instance = env->GetLongField(obj, getInstanceField(env, obj));
        T *t = reinterpret_cast<T *>(instance);
        if (t == nullptr) {
            throw std::runtime_error("Trying to acquire null NativeObject");
        }
        t->checkValid();
        return t;
    }

    template <typename T>
    T *getJavaUplinkObjectUnchecked(JNIEnv *env, jobject obj)
    {
        jlong instance = env->GetLongField(obj, getInstanceField(env, obj));
        return reinterpret_cast<T *>(instance);
    }

    template <typename T>
    void setJavaUplinkObject(JNIEnv *env, jobject obj, T *t)
    {
        if (t == nullptr) {
            throw std::runtime_error("Trying to create null NativeObject");
        }
        jlong instance = reinterpret_cast<jlong>(t);
        env->SetLongField(obj, getInstanceField(env, obj), instance);
    }

    //
    // C++ JavaAnon <-> java access, all generic
    //

    template <typename T>
    T *castInstance(jlong instance)
    {
        T *t = reinterpret_cast<T *>(instance);
        if (t == nullptr) {
            throw std::runtime_error("Trying to cast null object");
        }
        return t;
    }

    template <typename T>
    T *getObjectRef(JNIEnv *env, jobject obj, const char* field_name)
    {
        jlong jobj = env->GetLongField(obj, getField(env, obj, field_name, "J"));
        java_exception_check_and_throw(env, E_FILE_LINE);
        return reinterpret_cast<T *>(jobj);
    }

    template <typename T>
    void setObjectRef(JNIEnv *env, jobject obj, T *t, const char* field_name)
    {
        jlong jobj = reinterpret_cast<jlong>(t);
        env->SetLongField(obj, getField(env, obj, field_name, "J"), jobj);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }

    template <typename T>
    T *getInstance(JNIEnv *env, jobject obj)
    {
        jlong instance = env->GetLongField(obj, getInstanceField(env, obj));
        T *t = reinterpret_cast<T *>(instance);
        if (t == nullptr) {
            throw std::runtime_error("Trying to acquire null object");
        }
        return t;
    }

    template <typename T>
    T *getInstanceUnchecked(JNIEnv *env, jobject obj)
    {
        jlong instance = env->GetLongField(obj, getInstanceField(env, obj));
        return reinterpret_cast<T *>(instance);
    }

    template <typename T>
    void setInstance(JNIEnv *env, jobject obj, T *t)
    {
        if (t == nullptr) {
            throw std::runtime_error("Trying to create null object");
        }
        jlong instance = reinterpret_cast<jlong>(t);
        env->SetLongField(obj, getInstanceField(env, obj), instance);
    }

    inline void clearInstance(JNIEnv *env, jobject obj) {
        env->SetLongField(obj, getInstanceField(env, obj), 0);
    }

    template <typename T>
    jobject generic_clone(JNIEnv *env, jobject obj)
    {
        T *obj_generic = getInstance<T>(env, obj);
        T *copy_generic = obj_generic->clone();

        jclass generic_class = search_class(env, *copy_generic);
        jmethodID generic_ctor = search_method(env, generic_class, "<init>", "(J)V", false);

        jobject result = env->NewObject(generic_class, generic_ctor, (jlong)copy_generic);
        if (!result)
        {
            throw std::runtime_error("cannot create instance of class");
        }

        return result;
    }

    //
    // C++ <-> java type mapping
    //

    template <typename T>
    jobject convert_vector_sharedptr_to_jarraylist(JNIEnv *env, std::vector<std::shared_ptr<T>>& array)
    {
        unsigned int array_size = array.size();

        jmethodID arraylist_add;
        jobject result = get_new_arraylist(env, array_size, &arraylist_add);

        if (0 == array_size) {
            return result;
        }

        for (unsigned int i = 0; i < array_size; ++i) {
            std::shared_ptr<T> elem = array[i];
            std::shared_ptr<JavaAnon> objref = elem->getJavaObject();
            if ( nullptr == objref ) {
                throw InternalError("JavaUplink element of array has no valid java-object: "+elem->toString(), E_FILE_LINE);
            }
            env->CallBooleanMethod(result, arraylist_add, JavaGlobalObj::GetObject(objref));
        }
        return result;
    }

    template <typename T>
    jobject convert_vector_uniqueptr_to_jarraylist(JNIEnv *env, std::vector<std::unique_ptr<T>>& array,
            const char *ctor_prototype)
    {
        unsigned int array_size = array.size();

        jmethodID arraylist_add;
        jobject result = get_new_arraylist(env, array_size, &arraylist_add);

        if (array_size == 0)
        {
            return result;
        }

        jclass clazz = search_class(env, T::java_class().c_str());
        jmethodID clazz_ctor = search_method(env, clazz, "<init>", ctor_prototype, false);

        for (unsigned int i = 0; i < array_size; ++i)
        {
            T *elem = array[i].release();
            jobject object = env->NewObject(clazz, clazz_ctor, (jlong)elem);
            if (!object)
            {
                throw jau::InternalError("cannot create instance of class", E_FILE_LINE);
            }
            env->CallBooleanMethod(result, arraylist_add, object);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }
        return result;
    }

    template <typename T>
    jobject convert_vector_uniqueptr_to_jarraylist(JNIEnv *env, std::vector<std::unique_ptr<T>>& array,
            const char *ctor_prototype, std::function<jobject(JNIEnv*, jclass, jmethodID, T*)> ctor)
    {
        unsigned int array_size = array.size();

        jmethodID arraylist_add;
        jobject result = get_new_arraylist(env, array_size, &arraylist_add);

        if (array_size == 0)
        {
            return result;
        }

        jclass clazz = search_class(env, T::java_class().c_str());
        jmethodID clazz_ctor = search_method(env, clazz, "<init>", ctor_prototype, false);

        for (unsigned int i = 0; i < array_size; ++i)
        {
            T *elem = array[i].release();
            jobject object = ctor(env, clazz, clazz_ctor, elem);
            if (!object)
            {
                throw std::runtime_error("cannot create instance of class\n");
            }
            env->CallBooleanMethod(result, arraylist_add, object);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }
        return result;
    }

    template <typename T>
    jobject convert_vector_sharedptr_to_jarraylist(JNIEnv *env, std::vector<std::shared_ptr<T>>& array,
            const char *ctor_prototype, std::function<jobject(JNIEnv*, jclass, jmethodID, T*)> ctor)
    {
        unsigned int array_size = array.size();

        jmethodID arraylist_add;
        jobject result = get_new_arraylist(env, array_size, &arraylist_add);

        if (array_size == 0)
        {
            return result;
        }

        jclass clazz = search_class(env, T::java_class().c_str());
        jmethodID clazz_ctor = search_method(env, clazz, "<init>", ctor_prototype, false);

        for (unsigned int i = 0; i < array_size; ++i)
        {
            T *elem = array[i].get();
            jobject object = ctor(env, clazz, clazz_ctor, elem);
            if (!object)
            {
                throw std::runtime_error("cannot create instance of class\n");
            }
            env->CallBooleanMethod(result, arraylist_add, object);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }
        return result;
    }

} // namespace direct_bt

#endif /* JAU_HELPER_JNI_HPP_ */
