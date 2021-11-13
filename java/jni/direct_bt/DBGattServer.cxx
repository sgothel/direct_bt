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
 * private static native long ctorImpl(final String type,
 *                                     final byte[] value, final int capacity, boolean variable_length);
 */
jlong Java_org_direct_1bt_DBGattDesc_ctorImpl(JNIEnv *env, jclass clazz,
                                              jstring jtype_,
                                              jbyteArray jvalue_, jint jcapacity_, jboolean jvariable_length_) {
    (void)clazz;
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
        std::shared_ptr<DBGattDesc> * ref_ptr = new std::shared_ptr<DBGattDesc>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
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
 * private static native long ctorImpl(final String type,
 *                                     final byte properties, final long[] descriptors,
 *                                     final byte[] value, final int capacity, boolean variable_length);
 */
jlong Java_org_direct_1bt_DBGattChar_ctorImpl(JNIEnv *env, jclass clazz,
                                              jstring jtype_,
                                              jbyte jproperties, jlongArray jDescriptors,
                                              jbyteArray jvalue_, jint jcapacity_, jboolean jvariable_length_) {
    (void)clazz;
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
        std::shared_ptr<DBGattChar> * ref_ptr = new std::shared_ptr<DBGattChar>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
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
 * private static native long ctorImpl(final boolean primary, final String type,
 *                                     final long[] characteristics);
 */
jlong Java_org_direct_1bt_DBGattService_ctorImpl(JNIEnv *env, jclass clazz,
                                                 jboolean jprimary, jstring jtype_,
                                                 jlongArray jCharacteristics) {
    (void)clazz;
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
        std::shared_ptr<DBGattService> * ref_ptr = new std::shared_ptr<DBGattService>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
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
 * private static native long ctorImpl(final int max_att_mtu, final long[] services);
 */
jlong Java_org_direct_1bt_DBGattServer_ctorImpl(JNIEnv *env, jclass clazz,
                                                jint jmax_att_mtu,
                                                jlongArray jService) {
    (void)clazz;
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
        std::shared_ptr<DBGattServer> * ref_ptr = new std::shared_ptr<DBGattServer>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
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
