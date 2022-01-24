/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

#include "org_direct_bt_DBGattDesc.h"
#include "org_direct_bt_DBGattChar.h"
#include "org_direct_bt_DBGattService.h"
#include "org_direct_bt_DBGattServer.h"
#include "org_direct_bt_DBGattServer_Listener.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/DBGattServer.hpp"

using namespace direct_bt;

/**
 *
 * DBGattValue
 *
 */

// package org.direct_bt;
// public DBGattValue(final byte[] value, final int capacity, final boolean variable_length)
static const std::string _dbGattValueClazzCtorArgs("([BIZ)V");

static jobject _createDBGattValueFromDesc(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, DBGattDesc* valueHolder) {
    const jau::POctets& value = valueHolder->getValue();
    jbyteArray jval = env_->NewByteArray(value.size());
    env_->SetByteArrayRegion(jval, 0, value.size(), (const jbyte*)(value.get_ptr()));
    jau::java_exception_check_and_throw(env_, E_FILE_LINE);

    jobject jDBGattValue = env_->NewObject(clazz, clazz_ctor, jval, (jint)value.capacity(), valueHolder->hasVariableLength());
    jau::java_exception_check_and_throw(env_, E_FILE_LINE);

    env_->DeleteLocalRef(jval);

    return jDBGattValue;
};

static jobject _createDBGattValueFromChar(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, DBGattChar* valueHolder) {
    const jau::POctets& value = valueHolder->getValue();
    jbyteArray jval = env_->NewByteArray(value.size());
    env_->SetByteArrayRegion(jval, 0, value.size(), (const jbyte*)(value.get_ptr()));
    jau::java_exception_check_and_throw(env_, E_FILE_LINE);

    jobject jDBGattValue = env_->NewObject(clazz, clazz_ctor, jval, (jint)value.capacity(), valueHolder->hasVariableLength());
    jau::java_exception_check_and_throw(env_, E_FILE_LINE);

    env_->DeleteLocalRef(jval);

    return jDBGattValue;
};

/**
 *
 * DBGattDesc
 *
 */

jobject Java_org_direct_1bt_DBGattDesc_getValue(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattDesc> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattDesc>>(env, obj);
        return jau::convert_instance_to_jobject<DBGattDesc>(env, ref_ptr->get(), _dbGattValueClazzCtorArgs.c_str(), _createDBGattValueFromDesc);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/**
 * private native long ctorImpl(final String type,
 *                              final byte[] value, final int capacity, boolean variable_length);
 */
jlong Java_org_direct_1bt_DBGattDesc_ctorImpl(JNIEnv *env, jobject obj,
                                              jstring jtype_,
                                              jbyteArray jvalue_, jint jcapacity_, jboolean jvariable_length_) {
    try {
        if( nullptr == jvalue_ ) {
            throw jau::IllegalArgumentException("byte array null", E_FILE_LINE);
        }

        // POctets value
        jau::POctets value(jcapacity_, env->GetArrayLength(jvalue_), jau::endian::little);
        if( 0 < value.size() ) {
            JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
            const uint8_t * value_ptr = criticalArray.get(jvalue_, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == value_ptr ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
            }
            value.put_bytes_nc(0, value_ptr, value.size());
        }

        // uuid_t type
        std::string stype = jau::from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        std::shared_ptr<DBGattDesc> ref = std::make_shared<DBGattDesc>(
                                            type, std::move(value), JNI_TRUE == jvariable_length_);

        ref->setJavaObject( std::shared_ptr<jau::JavaAnon>( new jau::JavaGlobalObj(obj, nullptr) ) );
        jau::JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        std::shared_ptr<DBGattDesc> * ref_ptr = new std::shared_ptr<DBGattDesc>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_DBGattDesc_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<DBGattDesc> * ref_ptr = reinterpret_cast<std::shared_ptr<DBGattDesc> *>(nativeInstance);
            (*ref_ptr)->setJavaObject();
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattDesc_getHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattDesc> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattDesc>>(env, obj);
        return (jshort)( (*ref_ptr)->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}


void Java_org_direct_1bt_DBGattDesc_bzero(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattDesc> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattDesc>>(env, obj);
        (*ref_ptr)->bzero();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattDesc_toString(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattDesc> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattDesc>>(env, obj);
        return jau::from_string_to_jstring(env, (*ref_ptr)->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}


/**
 *
 * DBGattChar
 *
 */

jobject Java_org_direct_1bt_DBGattChar_getValue(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        return jau::convert_instance_to_jobject<DBGattChar>(env, ref_ptr->get(), _dbGattValueClazzCtorArgs.c_str(), _createDBGattValueFromChar);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/**
 * private native long ctorImpl(final String type,
 *                              final byte properties, final long[] descriptors,
 *                              final byte[] value, final int capacity, boolean variable_length);
 */
jlong Java_org_direct_1bt_DBGattChar_ctorImpl(JNIEnv *env, jobject obj,
                                              jstring jtype_,
                                              jbyte jproperties, jlongArray jDescriptors,
                                              jbyteArray jvalue_, jint jcapacity_, jboolean jvariable_length_) {
    try {
        if( nullptr == jvalue_ ) {
            throw jau::IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        if( nullptr == jDescriptors ) {
            throw jau::IllegalArgumentException("descriptor array null", E_FILE_LINE);
        }

        // POctets value
        jau::POctets value(jcapacity_, env->GetArrayLength(jvalue_), jau::endian::little);
        if( 0 < value.size() ) {
            JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
            const uint8_t * value_ptr = criticalArray.get(jvalue_, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == value_ptr ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
            }
            value.put_bytes_nc(0, value_ptr, value.size());
        }

        // DBGattDescRef List
        jau::darray<DBGattDescRef> descriptors( env->GetArrayLength(jDescriptors) /* capacity */ );
        if( 0 < descriptors.capacity() ) {
            JNICriticalArray<std::shared_ptr<DBGattDesc>*, jlongArray> criticalArray(env); // RAII - release
            std::shared_ptr<DBGattDesc> ** descr_ref_array = criticalArray.get(jDescriptors, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == descr_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattDesc* array) is null", E_FILE_LINE);
            }
            const jau::nsize_t count = descriptors.capacity();
            for(jau::nsize_t i=0; i < count; ++i) {
                descriptors.push_back( *( descr_ref_array[i] ) );
            }
        }

        // PropertyBitVal
        const BTGattChar::PropertyBitVal properties = static_cast<BTGattChar::PropertyBitVal>(jproperties);

        // uuid_t type
        std::string stype = jau::from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        std::shared_ptr<DBGattChar> ref = std::make_shared<DBGattChar>(
                                            type, properties,
                                            std::move(descriptors),
                                            std::move(value), JNI_TRUE == jvariable_length_);

        ref->setJavaObject( std::shared_ptr<jau::JavaAnon>( new jau::JavaGlobalObj(obj, nullptr) ) );
        jau::JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        std::shared_ptr<DBGattChar> * ref_ptr = new std::shared_ptr<DBGattChar>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_DBGattChar_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<DBGattChar> * ref_ptr = reinterpret_cast<std::shared_ptr<DBGattChar> *>(nativeInstance);
            (*ref_ptr)->setJavaObject();
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattChar_getHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        return (jshort)( (*ref_ptr)->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattChar_getEndHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        return (jshort)( (*ref_ptr)->getEndHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattChar_getValueHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        return (jshort)( (*ref_ptr)->getValueHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}


void Java_org_direct_1bt_DBGattChar_bzero(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        (*ref_ptr)->bzero();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattChar_toString(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattChar> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattChar>>(env, obj);
        return jau::from_string_to_jstring(env, (*ref_ptr)->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/**
 *
 * DBGattService
 *
 */

/**
 * private native long ctorImpl(final boolean primary, final String type,
 *                              final long[] characteristics);
 */
jlong Java_org_direct_1bt_DBGattService_ctorImpl(JNIEnv *env, jobject obj,
                                                 jboolean jprimary, jstring jtype_,
                                                 jlongArray jCharacteristics) {
    try {
        if( nullptr == jCharacteristics ) {
            throw jau::IllegalArgumentException("characteristics array null", E_FILE_LINE);
        }

        // DBGattCharRef List
        jau::darray<DBGattCharRef> characteristics( env->GetArrayLength(jCharacteristics) /* capacity */ );
        if( 0 < characteristics.capacity() ) {
            JNICriticalArray<std::shared_ptr<DBGattChar>*, jlongArray> criticalArray(env); // RAII - release
            std::shared_ptr<DBGattChar> ** char_ref_array = criticalArray.get(jCharacteristics, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == char_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattChar* array) is null", E_FILE_LINE);
            }
            const jau::nsize_t count = characteristics.capacity();
            for(jau::nsize_t i=0; i < count; ++i) {
                characteristics.push_back( *( char_ref_array[i] ) );
            }
        }

        // uuid_t type
        std::string stype = jau::from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        std::shared_ptr<DBGattService> ref = std::make_shared<DBGattService>(
                                            JNI_TRUE == jprimary, type,
                                            std::move(characteristics));

        ref->setJavaObject( std::shared_ptr<jau::JavaAnon>( new jau::JavaGlobalObj(obj, nullptr) ) );
        jau::JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        std::shared_ptr<DBGattService> * ref_ptr = new std::shared_ptr<DBGattService>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_DBGattService_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<DBGattService> * ref_ptr = reinterpret_cast<std::shared_ptr<DBGattService> *>(nativeInstance);
            (*ref_ptr)->setJavaObject();
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattService_getHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattService> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattService>>(env, obj);
        return (jshort)( (*ref_ptr)->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattService_getEndHandle(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattService> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattService>>(env, obj);
        return (jshort)( (*ref_ptr)->getEndHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jstring Java_org_direct_1bt_DBGattService_toString(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattService> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattService>>(env, obj);
        return jau::from_string_to_jstring(env, (*ref_ptr)->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/**
 *
 * DBGattServer
 *
 */

/**
 * private native long ctorImpl(final int max_att_mtu, final long[] services);
 */
jlong Java_org_direct_1bt_DBGattServer_ctorImpl(JNIEnv *env, jobject obj,
                                                jint jmax_att_mtu,
                                                jlongArray jService) {
    try {
        if( nullptr == jService ) {
            throw jau::IllegalArgumentException("characteristics array null", E_FILE_LINE);
        }

        // DBGattCharRef List
        jau::darray<DBGattServiceRef> services( env->GetArrayLength(jService) /* capacity */ );
        if( 0 < services.capacity() ) {
            JNICriticalArray<std::shared_ptr<DBGattService>*, jlongArray> criticalArray(env); // RAII - release
            std::shared_ptr<DBGattService> ** service_ref_array = criticalArray.get(jService, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == service_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattService* array) is null", E_FILE_LINE);
            }
            const jau::nsize_t count = services.capacity();
            for(jau::nsize_t i=0; i < count; ++i) {
                services.push_back( *( service_ref_array[i] ) );
            }
        }

        // new instance
        std::shared_ptr<DBGattServer> ref = std::make_shared<DBGattServer>(
                                            jmax_att_mtu,
                                            std::move(services));

        ref->setJavaObject( std::shared_ptr<jau::JavaAnon>( new jau::JavaGlobalObj(obj, nullptr) ) );
        jau::JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        std::shared_ptr<DBGattServer> * ref_ptr = new std::shared_ptr<DBGattServer>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_DBGattServer_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<DBGattServer> * ref_ptr = reinterpret_cast<std::shared_ptr<DBGattServer> *>(nativeInstance);
            (*ref_ptr)->setJavaObject();
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jint Java_org_direct_1bt_DBGattServer_getMaxAttMTU(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<DBGattServer> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattServer>>(env, obj);
        return (jint)( (*ref_ptr)->getMaxAttMTU() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

void Java_org_direct_1bt_DBGattServer_setMaxAttMTU(JNIEnv *env, jobject obj, jint v)
{
    try {
        std::shared_ptr<DBGattServer> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattServer>>(env, obj);
        (*ref_ptr)->setMaxAttMTU(v);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattServer_toString(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<DBGattServer> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattServer>>(env, obj);
        return jau::from_string_to_jstring(env, (*ref_ptr)->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/**
 *
 * DBGattServer.Listener and related DBGattServer methods
 *
 */


class JNIDBGattServerListener : public DBGattServer::Listener {
    private:
        jau::JavaGlobalObj listenerObjRef;
        jmethodID  mConnected = nullptr;
        jmethodID  mDisconnected = nullptr;
        jmethodID  mMtuChanged = nullptr;
        jmethodID  mReadCharValue = nullptr;
        jmethodID  mReadDescValue = nullptr;
        jmethodID  mWriteCharValue = nullptr;
        jmethodID  mWriteCharValueDone = nullptr;
        jmethodID  mWriteDescValue = nullptr;
        jmethodID  mWriteDescValueDone = nullptr;
        jmethodID  mCCDChanged = nullptr;

    public:
        JNIDBGattServerListener(JNIEnv *env, jclass clazz, jobject obj)
        : listenerObjRef(obj, nullptr)
        {
            mConnected = jau::search_method(env, clazz, "connected", "(Lorg/direct_bt/BTDevice;I)V", false);
            mDisconnected = jau::search_method(env, clazz, "disconnected", "(Lorg/direct_bt/BTDevice;)V", false);
            mMtuChanged = jau::search_method(env, clazz, "mtuChanged", "(Lorg/direct_bt/BTDevice;I)V", false);
            mReadCharValue = jau::search_method(env, clazz, "readCharValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;)Z", false);
            mReadDescValue = jau::search_method(env, clazz, "readDescValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;)Z", false);
            mWriteCharValue = jau::search_method(env, clazz, "writeCharValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;[BI)Z", false);
            mWriteCharValueDone = jau::search_method(env, clazz, "writeCharValueDone", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;)V", false);
            mWriteDescValue = jau::search_method(env, clazz, "writeDescValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;[BI)Z", false);
            mWriteDescValueDone = jau::search_method(env, clazz, "writeDescValueDone", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;)V", false);
            mCCDChanged = jau::search_method(env, clazz, "clientCharConfigChanged", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;ZZ)V", false);
        }

        ~JNIDBGattServerListener() noexcept { }

        void connected(BTDeviceRef device, const uint16_t initialMTU) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mConnected, j_device, (jint)initialMTU);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void disconnected(BTDeviceRef device) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mDisconnected, j_device);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void mtuChanged(BTDeviceRef device, const uint16_t mtu) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mMtuChanged, j_device, (jint)mtu);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }

        bool readCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mReadCharValue, j_device, j_s, j_c);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            return JNI_TRUE == res;
        }

        bool readDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = jau::JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mReadDescValue, j_device, j_s, j_c, j_d);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            return JNI_TRUE == res;
        }

        bool writeCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, const jau::TROOctets & value, const uint16_t value_offset) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            const size_t value_size = value.size();
            jbyteArray j_value = env->NewByteArray((jsize)value_size);
            env->SetByteArrayRegion(j_value, 0, (jsize)value_size, (const jbyte *)value.get_ptr());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mWriteCharValue, j_device, j_s, j_c, j_value, (jint)value_offset);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            env->DeleteLocalRef(j_value);
            return JNI_TRUE == res;
        }
        void writeCharValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mWriteCharValueDone, j_device, j_s, j_c);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }

        bool writeDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const jau::TROOctets & value, const uint16_t value_offset) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = jau::JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            const size_t value_size = value.size();
            jbyteArray j_value = env->NewByteArray((jsize)value_size);
            env->SetByteArrayRegion(j_value, 0, (jsize)value_size, (const jbyte *)value.get_ptr());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mWriteDescValue, j_device, j_s, j_c, j_d, j_value, (jint)value_offset);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            env->DeleteLocalRef(j_value);
            return JNI_TRUE == res;
        }
        void writeDescValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = jau::JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mWriteDescValueDone, j_device, j_s, j_c, j_d);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void clientCharConfigChanged(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const bool notificationEnabled, const bool indicationEnabled) override {
            jobject j_device = jau::JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = jau::JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = jau::JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = jau::JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mCCDChanged, j_device, j_s, j_c, j_d,
                                notificationEnabled? JNI_TRUE : JNI_FALSE, indicationEnabled? JNI_TRUE : JNI_FALSE);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
        }
};


/*
 * Class:     org_direct_bt_DBGattServer
 * Method:    addListenerImpl
 * Signature: (Lorg/direct_bt/DBGattServer/Listener;)Z
 */
jboolean Java_org_direct_1bt_DBGattServer_addListenerImpl(JNIEnv *env, jobject obj, jobject jlistener) {
    try {
        std::shared_ptr<DBGattServer> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattServer>>(env, obj);
        std::shared_ptr<JNIDBGattServerListener> * listener_ref_ptr = jau::getInstance<std::shared_ptr<JNIDBGattServerListener>>(env, jlistener);
        bool res = (*ref_ptr)->addListener(*listener_ref_ptr);
        return res ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

/*
 * Class:     org_direct_bt_DBGattServer
 * Method:    removeListenerImpl
 * Signature: (Lorg/direct_bt/DBGattServer/Listener;)Z
 */
jboolean Java_org_direct_1bt_DBGattServer_removeListenerImpl(JNIEnv *env, jobject obj, jobject jlistener) {
    try {
        std::shared_ptr<DBGattServer> * ref_ptr = jau::getInstance<std::shared_ptr<DBGattServer>>(env, obj);
        std::shared_ptr<JNIDBGattServerListener> * listener_ref_ptr = jau::getInstance<std::shared_ptr<JNIDBGattServerListener>>(env, jlistener);
        bool res = (*ref_ptr)->removeListener(*listener_ref_ptr);
        return res ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}


/*
 * Class:     org_direct_bt_DBGattServer_Listener
 * Method:    ctorImpl
 * Signature: ()J
 */
jlong Java_org_direct_1bt_DBGattServer_00024Listener_ctorImpl(JNIEnv *env, jobject obj) {
    try {
        jclass clazz = jau::search_class(env, obj);
        std::shared_ptr<JNIDBGattServerListener> ref = std::make_shared<JNIDBGattServerListener>(env, clazz, obj);
        env->DeleteLocalRef(clazz);

        std::shared_ptr<JNIDBGattServerListener> * ref_ptr = new std::shared_ptr<JNIDBGattServerListener>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

/*
 * Class:     org_direct_bt_DBGattServer_Listener
 * Method:    dtorImpl
 * Signature: (J)V
 */
void Java_org_direct_1bt_DBGattServer_00024Listener_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<JNIDBGattServerListener> * ref_ptr = reinterpret_cast<std::shared_ptr<JNIDBGattServerListener> *>(nativeInstance);
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
