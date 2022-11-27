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

#include "jau_direct_bt_DBTGattChar.h"

#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/BTAdapter.hpp"

using namespace direct_bt;
using namespace jau::jni;

void Java_jau_direct_1bt_DBTGattChar_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance) {
    (void)obj;
    try {
        shared_ptr_ref<BTGattChar> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            std::shared_ptr<BTGattChar>* sref_ptr = castInstance<BTGattChar>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_jau_direct_1bt_DBTGattChar_toStringImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTGattChar> characteristic(env, obj); // hold until done
        JavaAnonRef characteristic_java = characteristic->getJavaObject(); // hold until done!
        JavaGlobalObj::check(characteristic_java, E_FILE_LINE);
        return from_string_to_jstring(env, characteristic->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

static const std::string _descriptorClazzCtorArgs("(JLjau/direct_bt/DBTGattChar;Ljava/lang/String;S[B)V");

jobject Java_jau_direct_1bt_DBTGattChar_getDescriptorsImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTGattChar> characteristic(env, obj); // hold until done
        JavaAnonRef characteristic_java = characteristic->getJavaObject(); // hold until done!
        JavaGlobalObj::check(characteristic_java, E_FILE_LINE);

        jau::darray<BTGattDescRef> & descriptorList = characteristic->descriptorList;

        // BTGattDesc(final long nativeInstance, final BTGattChar characteristic,
        //                   final String type_uuid, final short handle, final byte[] value)

        // BTGattDesc(final long nativeInstance, final BTGattChar characteristic,
        //                   final String type_uuid, final short handle, final byte[] value)

        std::function<jobject(JNIEnv*, jclass, jmethodID, const BTGattDescRef&)> ctor_desc =
                [](JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, const BTGattDescRef& descriptor)->jobject {
                    // prepare adapter ctor
                    std::shared_ptr<BTGattChar> _characteristic = descriptor->getGattCharUnchecked();
                    if( nullptr == _characteristic ) {
                        throw jau::RuntimeException("Descriptor's characteristic null: "+descriptor->toString(), E_FILE_LINE);
                    }
                    JavaAnonRef _characteristic_java = _characteristic->getJavaObject(); // hold until done!
                    JavaGlobalObj::check(_characteristic_java, E_FILE_LINE);
                    jobject jcharacteristic = JavaGlobalObj::GetObject(_characteristic_java);

                    const jstring juuid = from_string_to_jstring(env_, descriptor->type->toUUID128String());
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    const size_t value_size = descriptor->value.size();
                    jbyteArray jval = env_->NewByteArray((jsize)value_size);
                    env_->SetByteArrayRegion(jval, 0, (jsize)value_size, (const jbyte *)descriptor->value.get_ptr());
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    shared_ptr_ref<BTGattDesc> descriptor_sref(descriptor); // new instance to be released into new jobject
                    jobject jdesc = env_->NewObject(clazz, clazz_ctor, descriptor_sref.release_to_jlong(), jcharacteristic,
                            juuid, (jshort)descriptor->handle, jval);
                    java_exception_check_and_throw(env_, E_FILE_LINE);
                    JNIGlobalRef::check(jdesc, E_FILE_LINE);
                    JavaAnonRef jDescRef = descriptor->getJavaObject(); // GlobalRef
                    JavaGlobalObj::check(jDescRef, E_FILE_LINE);
                    env_->DeleteLocalRef(juuid);
                    env_->DeleteLocalRef(jval);
                    env_->DeleteLocalRef(jdesc);
                    return JavaGlobalObj::GetObject(jDescRef);
                };
        return convert_vector_sharedptr_to_jarraylist<jau::darray<BTGattDescRef>, BTGattDesc>(
                env, descriptorList, _descriptorClazzCtorArgs.c_str(), ctor_desc);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jbyteArray Java_jau_direct_1bt_DBTGattChar_readValueImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTGattChar> characteristic(env, obj); // hold until done
        JavaAnonRef characteristic_java = characteristic->getJavaObject(); // hold until done!
        JavaGlobalObj::check(characteristic_java, E_FILE_LINE);

        jau::POctets res(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0, jau::endian::little);
        if( !characteristic->readValue(res) ) {
            ERR_PRINT("Characteristic readValue failed: %s", characteristic->toString().c_str());
            return env->NewByteArray((jsize)0);
        }

        const size_t value_size = res.size();
        jbyteArray jres = env->NewByteArray((jsize)value_size);
        env->SetByteArrayRegion(jres, 0, (jsize)value_size, (const jbyte *)res.get_ptr());
        java_exception_check_and_throw(env, E_FILE_LINE);
        return jres;

    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_jau_direct_1bt_DBTGattChar_writeValueImpl(JNIEnv *env, jobject obj, jbyteArray jval, jboolean withResponse) {
    try {
        shared_ptr_ref<BTGattChar> characteristic(env, obj); // hold until done
        JavaAnonRef characteristic_java = characteristic->getJavaObject(); // hold until done!
        JavaGlobalObj::check(characteristic_java, E_FILE_LINE);

        if( nullptr == jval ) {
            throw jau::IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const int value_size = env->GetArrayLength(jval);
        if( 0 == value_size ) {
            return JNI_TRUE;
        }

        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * value_ptr = criticalArray.get(jval, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == value_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        jau::TROOctets value(value_ptr, value_size, jau::endian::little);
        bool res;
        if( withResponse ) {
            res = characteristic->writeValue(value);
        } else {
            res = characteristic->writeValueNoResp(value);
        }
        if( !res ) {
            ERR_PRINT("Characteristic writeValue(withResponse %d) failed: %s",
                    withResponse, characteristic->toString().c_str());
            return JNI_FALSE;
        }
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTGattChar_configNotificationIndicationImpl(JNIEnv *env, jobject obj,
                        jboolean enableNotification, jboolean enableIndication, jbooleanArray jEnabledState) {
    try {
        shared_ptr_ref<BTGattChar> characteristic(env, obj, false /* throw_on_nullptr */); // hold until done
        if( characteristic.is_null() ) {
            if( !enableNotification && !enableIndication ) {
                // OK to have native characteristic being shutdown @ disable
                DBG_PRINT("Characteristic's native instance has been deleted");
                return false;
            }
            throw jau::IllegalStateException("Characteristic's native instance deleted", E_FILE_LINE);
        }
        JavaAnonRef characteristic_java = characteristic->getJavaObject(); // hold until done!
        JavaGlobalObj::check(characteristic_java, E_FILE_LINE);

        if( nullptr == jEnabledState ) {
            throw jau::IllegalArgumentException("boolean array null", E_FILE_LINE);
        }
        const int state_size = env->GetArrayLength(jEnabledState);
        if( 2 > state_size ) {
            throw jau::IllegalArgumentException("boolean array smaller than 2, length "+std::to_string(state_size), E_FILE_LINE);
        }
        JNICriticalArray<jboolean, jbooleanArray> criticalArray(env); // RAII - release
        jboolean * state_ptr = criticalArray.get(jEnabledState, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( nullptr == state_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(boolean array) is null", E_FILE_LINE);
        }

        bool cccdEnableResult[2];
        bool res = characteristic->configNotificationIndication(enableNotification, enableIndication, cccdEnableResult);
        DBG_PRINT("BTGattChar::configNotificationIndication Config Notification(%d), Indication(%d): Result %d",
                cccdEnableResult[0], cccdEnableResult[1], res);
        state_ptr[0] = cccdEnableResult[0];
        state_ptr[1] = cccdEnableResult[1];
        return res;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

