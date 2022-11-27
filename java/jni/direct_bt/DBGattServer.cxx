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
using namespace jau::jni;

/**
 *
 * DBGattValue
 *
 */

// package org.direct_bt;
// public DBGattValue(final byte[] value, final int capacity, final boolean variable_length)
static const std::string _dbGattValueClazzName("org/direct_bt/DBGattValue");
static const std::string _dbGattValueClazzCtorArgs("([BIZ)V");

static jobject _createDBGattValueFromDesc(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, const DBGattDescRef& valueHolder) {
    const jau::POctets& value = valueHolder->getValue();
    jbyteArray jval = env_->NewByteArray( (jsize) value.size() );
    env_->SetByteArrayRegion(jval, 0, (jsize) value.size(), (const jbyte*)(value.get_ptr()));
    java_exception_check_and_throw(env_, E_FILE_LINE);

    jobject jDBGattValue = env_->NewObject(clazz, clazz_ctor, jval, (jint)value.capacity(), valueHolder->hasVariableLength());
    java_exception_check_and_throw(env_, E_FILE_LINE);

    env_->DeleteLocalRef(jval);

    return jDBGattValue;
};

static jobject _createDBGattValueFromChar(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, const DBGattCharRef& valueHolder) {
    const jau::POctets& value = valueHolder->getValue();
    jbyteArray jval = env_->NewByteArray( (jsize) value.size() );
    env_->SetByteArrayRegion(jval, 0, (jsize) value.size(), (const jbyte*)(value.get_ptr()));
    java_exception_check_and_throw(env_, E_FILE_LINE);

    jobject jDBGattValue = env_->NewObject(clazz, clazz_ctor, jval, (jint)value.capacity(), valueHolder->hasVariableLength());
    java_exception_check_and_throw(env_, E_FILE_LINE);

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
        shared_ptr_ref<DBGattDesc> ref(env, obj); // hold until done
        jclass clazz = search_class(env, _dbGattValueClazzName.c_str());
        return convert_instance_to_jobject<DBGattDesc>(env, clazz, _dbGattValueClazzCtorArgs.c_str(), _createDBGattValueFromDesc, ref.shared_ptr());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_org_direct_1bt_DBGattDesc_setValue(JNIEnv *env, jobject obj, jbyteArray jsource, jint jsource_pos, jint jsource_len, jint jdest_pos) {
    try {
        shared_ptr_ref<DBGattDesc> ref(env, obj); // hold until done

        if( nullptr == jsource ) {
            return JNI_FALSE;
        }

        const jau::nsize_t source_len0 = env->GetArrayLength(jsource);
        const jau::nsize_t source_len1 = jsource_len;
        const jau::nsize_t source_pos1 = jsource_pos;
        const jau::nsize_t dest_pos = jdest_pos;

        if( 0 < source_len0 && 0 < source_len1 && source_pos1 + source_len1 <= source_len0 ) {
            JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
            const uint8_t * source = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == source ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
            }
            return ref->setValue(source+source_pos1, source_len1, dest_pos) ? JNI_TRUE : JNI_FALSE;
        } else {
            return JNI_FALSE;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
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
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below

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
        std::string stype = from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        shared_ptr_ref<DBGattDesc> ref( new DBGattDesc(type, std::move(value), JNI_TRUE == jvariable_length_) );

        ref->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), nullptr ) );
        JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_DBGattDesc_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        shared_ptr_ref<DBGattDesc> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            if( !sref.is_null() ) {
                JavaAnonRef sref_java = sref->getJavaObject(); // hold until done!
                JavaGlobalObj::check(sref_java, E_FILE_LINE);
                sref->setJavaObject();
            }
            std::shared_ptr<DBGattDesc>* sref_ptr = castInstance<DBGattDesc>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattDesc_getHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattDesc> ref(env, obj); // hold until done
        return (jshort)( ref->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}


void Java_org_direct_1bt_DBGattDesc_bzero(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattDesc> ref(env, obj); // hold until done
        ref->bzero();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattDesc_toString(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<DBGattDesc> ref(env, obj); // hold until done
        return from_string_to_jstring(env, ref->toString());
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
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        jclass clazz = search_class(env, _dbGattValueClazzName.c_str());
        return convert_instance_to_jobject<DBGattChar>(env, clazz, _dbGattValueClazzCtorArgs.c_str(), _createDBGattValueFromChar, ref.shared_ptr());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_org_direct_1bt_DBGattChar_setValue(JNIEnv *env, jobject obj, jbyteArray jsource, jint jsource_pos, jint jsource_len, jint jdest_pos) {
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done

        if( nullptr == jsource ) {
            return JNI_FALSE;
        }

        const jau::nsize_t source_len0 = env->GetArrayLength(jsource);
        const jau::nsize_t source_len1 = jsource_len;
        const jau::nsize_t source_pos1 = jsource_pos;
        const jau::nsize_t dest_pos = jdest_pos;

        if( 0 < source_len0 && 0 < source_len1 && source_pos1 + source_len1 <= source_len0 ) {
            JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
            const uint8_t * source = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == source ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
            }
            return ref->setValue(source+source_pos1, source_len1, dest_pos) ? JNI_TRUE : JNI_FALSE;
        } else {
            return JNI_FALSE;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
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
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below

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
        const jau::nsize_t count = descriptors.capacity();
        if( 0 < count ) {
            JNICriticalArray<jlong, jlongArray> criticalArray(env); // RAII - release
            jlong * jlong_desc_ref_array = criticalArray.get(jDescriptors, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == jlong_desc_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattDesc* array) is null", E_FILE_LINE);
            }
            for(jau::nsize_t i=0; i < count; ++i) {
                std::shared_ptr<DBGattDesc> desc_ref = *( (std::shared_ptr<DBGattDesc> *) (intptr_t) jlong_desc_ref_array[i] );
                descriptors.push_back( desc_ref );
            }
        }

        // PropertyBitVal
        const BTGattChar::PropertyBitVal properties = static_cast<BTGattChar::PropertyBitVal>(jproperties);

        // uuid_t type
        std::string stype = from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        shared_ptr_ref<DBGattChar> ref( new DBGattChar(type, properties,
                                             std::move(descriptors),
                                             std::move(value), JNI_TRUE == jvariable_length_) );

        ref->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), nullptr ) );
        JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t) nullptr;
}

void Java_org_direct_1bt_DBGattChar_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        shared_ptr_ref<BTGattChar> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            if( !sref.is_null() ) {
                JavaAnonRef sref_java = sref->getJavaObject(); // hold until done!
                JavaGlobalObj::check(sref_java, E_FILE_LINE);
                sref->setJavaObject();
            }
            std::shared_ptr<BTGattChar>* sref_ptr = castInstance<BTGattChar>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattChar_getHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        return (jshort)( ref->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattChar_getEndHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        return (jshort)( ref->getEndHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattChar_getValueHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        return (jshort)( ref->getValueHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}


void Java_org_direct_1bt_DBGattChar_bzero(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        ref->bzero();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattChar_toString(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<DBGattChar> ref(env, obj); // hold until done
        return from_string_to_jstring(env, ref->toString());
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
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below

        // DBGattCharRef List
        jau::darray<DBGattCharRef> characteristics( env->GetArrayLength(jCharacteristics) /* capacity */ );
        const jau::nsize_t count = characteristics.capacity();
        if( 0 < count ) {
            JNICriticalArray<jlong, jlongArray> criticalArray(env); // RAII - release
            jlong * jlong_char_ref_array = criticalArray.get(jCharacteristics, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == jlong_char_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattChar* array) is null", E_FILE_LINE);
            }
            for(jau::nsize_t i=0; i < count; ++i) {
                std::shared_ptr<DBGattChar> char_ref = *( (std::shared_ptr<DBGattChar> *) (intptr_t) jlong_char_ref_array[i] );
                characteristics.push_back( char_ref );
            }
        }

        // uuid_t type
        std::string stype = from_jstring_to_string(env, jtype_);
        std::shared_ptr<const jau::uuid_t> type( jau::uuid_t::create(stype) );

        // new instance
        shared_ptr_ref<DBGattService> ref( new DBGattService( JNI_TRUE == jprimary, type, std::move(characteristics) ) );

        ref->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), nullptr ) );
        JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t) nullptr;
}

void Java_org_direct_1bt_DBGattService_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        shared_ptr_ref<DBGattService> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            if( !sref.is_null() ) {
                JavaAnonRef sref_java = sref->getJavaObject(); // hold until done!
                JavaGlobalObj::check(sref_java, E_FILE_LINE);
                sref->setJavaObject();
            }
            std::shared_ptr<DBGattService>* sref_ptr = castInstance<DBGattService>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jshort Java_org_direct_1bt_DBGattService_getHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattService> ref(env, obj); // hold until done
        return (jshort)( ref->getHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jshort Java_org_direct_1bt_DBGattService_getEndHandle(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattService> ref(env, obj); // hold until done
        return (jshort)( ref->getEndHandle() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jstring Java_org_direct_1bt_DBGattService_toString(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<DBGattService> ref(env, obj); // hold until done
        return from_string_to_jstring(env, ref->toString());
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
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below

        // DBGattServiceRef List
        jau::darray<DBGattServiceRef> services( env->GetArrayLength(jService) /* capacity */ );
        const jau::nsize_t count = services.capacity();
        if( 0 < count ) {
            JNICriticalArray<jlong, jlongArray> criticalArray(env); // RAII - release
            jlong * jlong_service_ref_array = criticalArray.get(jService, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
            if( nullptr == jlong_service_ref_array ) {
                throw jau::InternalError("GetPrimitiveArrayCritical(DBGattService* array) is null", E_FILE_LINE);
            }
            for(jau::nsize_t i=0; i < count; ++i) {
                std::shared_ptr<DBGattService> service_ref = *( (std::shared_ptr<DBGattService> *) (intptr_t) jlong_service_ref_array[i] );
                services.push_back( service_ref );
            }
        }

        // new instance
        shared_ptr_ref<DBGattServer> ref( new DBGattServer( jmax_att_mtu, std::move(services) ) );

        ref->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), nullptr ) );
        JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t) nullptr;
}

void Java_org_direct_1bt_DBGattServer_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        shared_ptr_ref<DBGattServer> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            if( !sref.is_null() ) {
                JavaAnonRef sref_java = sref->getJavaObject(); // hold until done!
                JavaGlobalObj::check(sref_java, E_FILE_LINE);
                sref->setJavaObject();
            }
            std::shared_ptr<DBGattServer>* sref_ptr = castInstance<DBGattServer>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jint Java_org_direct_1bt_DBGattServer_getMaxAttMTU(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<DBGattServer> ref(env, obj); // hold until done
        return (jint)( ref->getMaxAttMTU() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

void Java_org_direct_1bt_DBGattServer_setMaxAttMTU(JNIEnv *env, jobject obj, jint v)
{
    try {
        shared_ptr_ref<DBGattServer> ref(env, obj); // hold until done
        ref->setMaxAttMTU(v);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_org_direct_1bt_DBGattServer_toString(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<DBGattServer> ref(env, obj); // hold until done
        return from_string_to_jstring(env, ref->toString());
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
        JavaGlobalObj listenerObjRef;
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
            mConnected = search_method(env, clazz, "connected", "(Lorg/direct_bt/BTDevice;I)V", false);
            mDisconnected = search_method(env, clazz, "disconnected", "(Lorg/direct_bt/BTDevice;)V", false);
            mMtuChanged = search_method(env, clazz, "mtuChanged", "(Lorg/direct_bt/BTDevice;I)V", false);
            mReadCharValue = search_method(env, clazz, "readCharValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;)Z", false);
            mReadDescValue = search_method(env, clazz, "readDescValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;)Z", false);
            mWriteCharValue = search_method(env, clazz, "writeCharValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;[BI)Z", false);
            mWriteCharValueDone = search_method(env, clazz, "writeCharValueDone", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;)V", false);
            mWriteDescValue = search_method(env, clazz, "writeDescValue", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;[BI)Z", false);
            mWriteDescValueDone = search_method(env, clazz, "writeDescValueDone", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;)V", false);
            mCCDChanged = search_method(env, clazz, "clientCharConfigChanged", "(Lorg/direct_bt/BTDevice;Lorg/direct_bt/DBGattService;Lorg/direct_bt/DBGattChar;Lorg/direct_bt/DBGattDesc;ZZ)V", false);
        }

        ~JNIDBGattServerListener() noexcept override = default;

        void connected(BTDeviceRef device, const uint16_t initialMTU) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mConnected, j_device, (jint)initialMTU);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void disconnected(BTDeviceRef device) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mDisconnected, j_device);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void mtuChanged(BTDeviceRef device, const uint16_t mtu) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mMtuChanged, j_device, (jint)mtu);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }

        bool readCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mReadCharValue, j_device, j_s, j_c);
            java_exception_check_and_throw(env, E_FILE_LINE);
            return JNI_TRUE == res;
        }

        bool readDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mReadDescValue, j_device, j_s, j_c, j_d);
            java_exception_check_and_throw(env, E_FILE_LINE);
            return JNI_TRUE == res;
        }

        bool writeCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, const jau::TROOctets & value, const uint16_t value_offset) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            const size_t value_size = value.size();
            jbyteArray j_value = env->NewByteArray((jsize)value_size);
            env->SetByteArrayRegion(j_value, 0, (jsize)value_size, (const jbyte *)value.get_ptr());
            java_exception_check_and_throw(env, E_FILE_LINE);

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mWriteCharValue, j_device, j_s, j_c, j_value, (jint)value_offset);
            java_exception_check_and_throw(env, E_FILE_LINE);
            env->DeleteLocalRef(j_value);
            return JNI_TRUE == res;
        }
        void writeCharValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mWriteCharValueDone, j_device, j_s, j_c);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }

        bool writeDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const jau::TROOctets & value, const uint16_t value_offset) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            const size_t value_size = value.size();
            jbyteArray j_value = env->NewByteArray((jsize)value_size);
            env->SetByteArrayRegion(j_value, 0, (jsize)value_size, (const jbyte *)value.get_ptr());
            java_exception_check_and_throw(env, E_FILE_LINE);

            jboolean res = env->CallBooleanMethod(listenerObjRef.getObject(), mWriteDescValue, j_device, j_s, j_c, j_d, j_value, (jint)value_offset);
            java_exception_check_and_throw(env, E_FILE_LINE);
            env->DeleteLocalRef(j_value);
            return JNI_TRUE == res;
        }
        void writeDescValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mWriteDescValueDone, j_device, j_s, j_c, j_d);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }

        void clientCharConfigChanged(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const bool notificationEnabled, const bool indicationEnabled) override {
            jobject j_device = JavaGlobalObj::checkAndGetObject(device->getJavaObject(), E_FILE_LINE);
            jobject j_s = JavaGlobalObj::checkAndGetObject(s->getJavaObject(), E_FILE_LINE);
            jobject j_c = JavaGlobalObj::checkAndGetObject(c->getJavaObject(), E_FILE_LINE);
            jobject j_d = JavaGlobalObj::checkAndGetObject(d->getJavaObject(), E_FILE_LINE);
            JNIEnv *env = *jni_env;

            env->CallVoidMethod(listenerObjRef.getObject(), mCCDChanged, j_device, j_s, j_c, j_d,
                                notificationEnabled? JNI_TRUE : JNI_FALSE, indicationEnabled? JNI_TRUE : JNI_FALSE);
            java_exception_check_and_throw(env, E_FILE_LINE);
        }
};


/*
 * Class:     org_direct_bt_DBGattServer
 * Method:    addListenerImpl
 * Signature: (Lorg/direct_bt/DBGattServer/Listener;)Z
 */
jboolean Java_org_direct_1bt_DBGattServer_addListenerImpl(JNIEnv *env, jobject obj, jobject jlistener) {
    try {
        shared_ptr_ref<DBGattServer> ref(env, obj); // hold until done
        shared_ptr_ref<JNIDBGattServerListener> listener_ref(env, jlistener); // hold until done
        bool res = ref->addListener(listener_ref.shared_ptr());
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
        shared_ptr_ref<DBGattServer> ref(env, obj); // hold until done
        shared_ptr_ref<JNIDBGattServerListener> listener_ref(env, jlistener); // hold until done
        bool res = ref->removeListener(listener_ref.shared_ptr());
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
        jclass clazz = search_class(env, obj);
        shared_ptr_ref<JNIDBGattServerListener> ref( new JNIDBGattServerListener(env, clazz, obj) );
        env->DeleteLocalRef(clazz);

        return ref.release_to_jlong();
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
        shared_ptr_ref<JNIDBGattServerListener> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            std::shared_ptr<JNIDBGattServerListener>* sref_ptr = castInstance<JNIDBGattServerListener>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
