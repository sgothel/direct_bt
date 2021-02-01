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

#include "jau_direct_bt_DBTDevice.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/BTAdapter.hpp"
#include "direct_bt/BTManager.hpp"

using namespace direct_bt;
using namespace jau;

static const std::string _notificationReceivedMethodArgs("(Lorg/direct_bt/BTGattChar;[BJ)V");
static const std::string _indicationReceivedMethodArgs("(Lorg/direct_bt/BTGattChar;[BJZ)V");

class JNIGattCharListener : public BTGattCharListener {
  private:
    /**
        package org.tinyb;

        public abstract class BTGattCharListener {
            long nativeInstance;

            public void notificationReceived(final BTGattChar charDecl,
                                             final byte[] value, final long timestamp) {
            }

            public void indicationReceived(final BTGattChar charDecl,
                                           final byte[] value, final long timestamp,
                                           final boolean confirmationSent) {
            }

        };
    */
    const BTGattChar * associatedCharacteristicRef;
    JNIGlobalRef listenerObj; // keep listener instance alive
    JNIGlobalRef associatedCharacteristicObj; // keeps associated characteristic alive, if not null
    jmethodID  mNotificationReceived = nullptr;
    jmethodID  mIndicationReceived = nullptr;

  public:

    JNIGattCharListener(JNIEnv *env, BTDevice *device, jobject listener, BTGattChar * associatedCharacteristicRef_)
    : associatedCharacteristicRef(associatedCharacteristicRef_),
      listenerObj(listener)
    {
        jclass listenerClazz = search_class(env, listenerObj.getObject());
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == listenerClazz ) {
            throw InternalError("BTGattCharListener not found", E_FILE_LINE);
        }

        if( nullptr != associatedCharacteristicRef_ ) {
            JavaGlobalObj::check(associatedCharacteristicRef_->getJavaObject(), E_FILE_LINE);
            associatedCharacteristicObj = JavaGlobalObj::GetJavaObject(associatedCharacteristicRef_->getJavaObject()); // new global ref
        }

        mNotificationReceived = search_method(env, listenerClazz, "notificationReceived", _notificationReceivedMethodArgs.c_str(), false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mNotificationReceived ) {
            throw InternalError("BTGattCharListener has no notificationReceived"+_notificationReceivedMethodArgs+" method, for "+device->toString(), E_FILE_LINE);
        }
        mIndicationReceived = search_method(env, listenerClazz, "indicationReceived", _indicationReceivedMethodArgs.c_str(), false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mNotificationReceived ) {
            throw InternalError("BTGattCharListener has no indicationReceived"+_indicationReceivedMethodArgs+" method, for "+device->toString(), E_FILE_LINE);
        }
    }

    bool match(const BTGattChar & characteristic) noexcept override {
        if( nullptr == associatedCharacteristicRef ) {
            return true;
        }
        return characteristic == *associatedCharacteristicRef;
    }

    void notificationReceived(BTGattCharRef charDecl,
                              const TROOctets& charValue, const uint64_t timestamp) override {
        std::shared_ptr<jau::JavaAnon> jCharDeclRef = charDecl->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jCharDeclRef) ) {
            return; // java object has been pulled
        }
        JNIEnv *env = *jni_env;
        jobject jCharDecl = jau::JavaGlobalObj::GetObject(jCharDeclRef);

        const size_t value_size = charValue.getSize();
        jbyteArray jval = env->NewByteArray((jsize)value_size);
        env->SetByteArrayRegion(jval, 0, (jsize)value_size, (const jbyte *)charValue.get_ptr());
        java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(listenerObj.getObject(), mNotificationReceived,
                            jCharDecl, jval, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(jval);
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& charValue, const uint64_t timestamp,
                            const bool confirmationSent) override {
        std::shared_ptr<jau::JavaAnon> jCharDeclRef = charDecl->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jCharDeclRef) ) {
            return; // java object has been pulled
        }
        JNIEnv *env = *jni_env;
        jobject jCharDecl = jau::JavaGlobalObj::GetObject(jCharDeclRef);

        const size_t value_size = charValue.getSize();
        jbyteArray jval = env->NewByteArray((jsize)value_size);
        env->SetByteArrayRegion(jval, 0, (jsize)value_size, (const jbyte *)charValue.get_ptr());
        java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(listenerObj.getObject(), mIndicationReceived,
                            jCharDecl, jval, (jlong)timestamp, (jboolean)confirmationSent);
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(jval);
    }
};


void Java_jau_direct_1bt_DBTDevice_initImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jstring Java_jau_direct_1bt_DBTDevice_getNameImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *nativePtr = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(nativePtr->getJavaObject(), E_FILE_LINE);
        return from_string_to_jstring(env, nativePtr->getName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_jau_direct_1bt_DBTDevice_toStringImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *nativePtr = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(nativePtr->getJavaObject(), E_FILE_LINE);
        return from_string_to_jstring(env, nativePtr->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_jau_direct_1bt_DBTDevice_addCharListener(JNIEnv *env, jobject obj, jobject listener, jobject jAssociatedCharacteristic) {
    try {
        if( nullptr == listener ) {
            throw IllegalArgumentException("BTGattCharListener argument is null", E_FILE_LINE);
        }
        {
            JNIGattCharListener * pre =
                    getObjectRef<JNIGattCharListener>(env, listener, "nativeInstance");
            if( nullptr != pre ) {
                throw IllegalStateException("BTGattCharListener's nativeInstance not null, already in use", E_FILE_LINE);
                return false;
            }
        }
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
        if( nullptr == gatt ) {
            throw IllegalStateException("BTGattChar's device GATTHandle not connected: "+ device->toString(), E_FILE_LINE);
        }

        BTGattChar * associatedCharacteristicRef = nullptr;
        if( nullptr != jAssociatedCharacteristic ) {
            associatedCharacteristicRef = getJavaUplinkObject<BTGattChar>(env, jAssociatedCharacteristic);
        }

        std::shared_ptr<BTGattCharListener> l =
                std::shared_ptr<BTGattCharListener>( new JNIGattCharListener(env, device, listener, associatedCharacteristicRef) );

        if( gatt->addCharListener(l) ) {
            setInstance(env, listener, l.get());
            return JNI_TRUE;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTDevice_removeCharListener(JNIEnv *env, jobject obj, jobject jlistener) {
    try {
        if( nullptr == jlistener ) {
            throw IllegalArgumentException("BTGattCharListener argument is null", E_FILE_LINE);
        }
        JNIGattCharListener * pre =
                getObjectRef<JNIGattCharListener>(env, jlistener, "nativeInstance");
        if( nullptr == pre ) {
            WARN_PRINT("BTGattCharListener's nativeInstance is null, not in use");
            return false;
        }
        setObjectRef<JNIGattCharListener>(env, jlistener, nullptr, "nativeInstance");

        BTDevice *device = getJavaUplinkObjectUnchecked<BTDevice>(env, obj);
        if( nullptr == device ) {
            // OK to have device being deleted already @ shutdown
            return 0;
        }
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
        if( nullptr == gatt ) {
            // OK to have BTGattHandler being shutdown @ disable
            DBG_PRINT("BTGattChar's device GATTHandle not connected: %s", device->toString().c_str());
            return false;
        }

        if( ! gatt->removeCharListener(pre) ) {
            WARN_PRINT("Failed to remove BTGattCharListener with nativeInstance: %p at %s", pre, device->toString().c_str());
            return false;
        }
        return true;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jint Java_jau_direct_1bt_DBTDevice_removeAllAssociatedCharListener(JNIEnv *env, jobject obj, jobject jAssociatedCharacteristic) {
    try {
        if( nullptr == jAssociatedCharacteristic ) {
            throw IllegalArgumentException("associatedCharacteristic argument is null", E_FILE_LINE);
        }
        BTDevice *device = getJavaUplinkObjectUnchecked<BTDevice>(env, obj);
        if( nullptr == device ) {
            // OK to have device being deleted already @ shutdown
            return 0;
        }
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
        if( nullptr == gatt ) {
            // OK to have BTGattHandler being shutdown @ disable
            DBG_PRINT("BTGattChar's device GATTHandle not connected: %s", device->toString().c_str());
            return 0;
        }

        BTGattChar * associatedCharacteristicRef = getJavaUplinkObject<BTGattChar>(env, jAssociatedCharacteristic);
        JavaGlobalObj::check(associatedCharacteristicRef->getJavaObject(), E_FILE_LINE);

        return gatt->removeAllAssociatedCharListener(associatedCharacteristicRef);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jint Java_jau_direct_1bt_DBTDevice_removeAllCharListener(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObjectUnchecked<BTDevice>(env, obj);
        if( nullptr == device ) {
            // OK to have device being deleted already @ shutdown
            return 0;
        }
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
        if( nullptr == gatt ) {
            // OK to have BTGattHandler being shutdown @ disable
            DBG_PRINT("BTGattChar's device GATTHandle not connected: %s", device->toString().c_str());
            return 0;
        }
        return gatt->removeAllCharListener();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

void Java_jau_direct_1bt_DBTDevice_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        BTDevice *device = castInstance<BTDevice>(nativeInstance);
        DBG_PRINT("Java_jau_direct_1bt_DBTDevice_deleteImpl (remove only) %s", device->toString().c_str());
        device->remove();
        // No delete: BTDevice instance owned by BTAdapter
        // However, device->remove() might issue destruction
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jbyte Java_jau_direct_1bt_DBTDevice_disconnectImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return (jint) number( device->disconnect() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jboolean Java_jau_direct_1bt_DBTDevice_removeImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        device->remove();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_TRUE;
}

jbyte Java_jau_direct_1bt_DBTDevice_connectDefaultImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return (jbyte) number( device->connectDefault() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_connectLEImpl0(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        HCIStatusCode res = device->connectLE();
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_connectLEImpl1(JNIEnv *env, jobject obj,
                                                     jshort interval, jshort window,
                                                     jshort min_interval, jshort max_interval,
                                                     jshort latency, jshort timeout)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        HCIStatusCode res = device->connectLE(interval, window, min_interval, max_interval, latency, timeout);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_getAvailableSMPKeysImpl(JNIEnv *env, jobject obj, jboolean responder) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getAvailableSMPKeys(JNI_TRUE == responder) ); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

void Java_jau_direct_1bt_DBTDevice_getLongTermKeyInfoImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPLongTermKeyInfo) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPLongTermKeyInfo& ltk_sink = *reinterpret_cast<SMPLongTermKeyInfo *>(sink_ptr);
        ltk_sink = device->getLongTermKeyInfo(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jbyte Java_jau_direct_1bt_DBTDevice_setLongTermKeyInfoImpl(JNIEnv *env, jobject obj, jbyteArray jsource) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsource ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t source_size = env->GetArrayLength(jsource);
        if( sizeof(SMPLongTermKeyInfo) > source_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(source_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * source_ptr = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == source_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const SMPLongTermKeyInfo& ltk = *reinterpret_cast<SMPLongTermKeyInfo *>(source_ptr);

        const HCIStatusCode res = device->setLongTermKeyInfo(ltk);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

void Java_jau_direct_1bt_DBTDevice_getSignatureResolvingKeyInfoImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPSignatureResolvingKeyInfo) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPSignatureResolvingKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPSignatureResolvingKeyInfo& csrk_sink = *reinterpret_cast<SMPSignatureResolvingKeyInfo *>(sink_ptr);
        csrk_sink = device->getSignatureResolvingKeyInfo(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jbyte Java_jau_direct_1bt_DBTDevice_unpairImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        HCIStatusCode res = device->unpair();
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jboolean Java_jau_direct_1bt_DBTDevice_setConnSecurityLevelImpl(JNIEnv *env, jobject obj, jbyte jsec_level) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnSecurityLevel( getBTSecurityLevel( static_cast<uint8_t>(jsec_level) ));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTDevice_getConnSecurityLevelImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getConnSecurityLevel() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return number( BTSecurityLevel::UNSET );
}

jboolean Java_jau_direct_1bt_DBTDevice_setConnIOCapabilityImpl(JNIEnv *env, jobject obj, jbyte jio_cap) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnIOCapability( getSMPIOCapability( static_cast<uint8_t>(jio_cap) ));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTDevice_setConnSecurityImpl(JNIEnv *env, jobject obj, jbyte jsec_level, jbyte jio_cap) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnSecurity( getBTSecurityLevel( static_cast<uint8_t>(jsec_level) ),
                                        getSMPIOCapability( static_cast<uint8_t>(jio_cap) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTDevice_getConnIOCapabilityImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getConnIOCapability() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return number( SMPIOCapability::UNSET );
}

jbyte Java_jau_direct_1bt_DBTDevice_getPairingModeImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getPairingMode() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return number( PairingMode::NONE );
}

jbyte Java_jau_direct_1bt_DBTDevice_getPairingStateImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return static_cast<uint8_t>( device->getPairingState() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return static_cast<uint8_t>( SMPPairingState::NONE );
}

jboolean Java_jau_direct_1bt_DBTDevice_setConnSecurityAutoImpl(JNIEnv *env, jobject obj, jbyte jio_cap) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnSecurityAuto( getSMPIOCapability( static_cast<uint8_t>(jio_cap) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTDevice_isConnSecurityAutoEnabled(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->isConnSecurityAutoEnabled();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTDevice_setPairingPasskeyImpl(JNIEnv *env, jobject obj, jint jpasskey) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        // const std::string passkey = nullptr != jpasskey ? from_jstring_to_string(env, jpasskey) : std::string();
        return number( device->setPairingPasskey( static_cast<uint32_t>(jpasskey) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return static_cast<uint8_t>( HCIStatusCode::INTERNAL_FAILURE );
}

jbyte Java_jau_direct_1bt_DBTDevice_setPairingNumericComparisonImpl(JNIEnv *env, jobject obj, jboolean jequal) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->setPairingNumericComparison( JNI_TRUE == jequal ? true : false ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return static_cast<uint8_t>( HCIStatusCode::INTERNAL_FAILURE );
}

//
// getter
//

static const std::string _serviceClazzCtorArgs("(JLjau/direct_bt/DBTDevice;ZLjava/lang/String;SS)V");

jobject Java_jau_direct_1bt_DBTDevice_getServicesImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        jau::darray<BTGattServiceRef> services = device->getGattServices(); // implicit GATT connect and discovery if required incl GenericAccess retrieval
        if( services.size() > 0 ) {
            std::shared_ptr<GattGenericAccessSvc> ga = device->getGattGenericAccess();
            if( nullptr != ga ) {
                env->SetShortField(obj, getField(env, obj, "appearance", "S"), static_cast<jshort>(ga->appearance));
                java_exception_check_and_throw(env, E_FILE_LINE);
                DBG_PRINT("BTDevice.getServices(): GenericAccess: %s", ga->toString().c_str());
            }
        } else {
            return nullptr;
        }

        // BTGattService(final long nativeInstance, final BTDevice device, final boolean isPrimary,
        //                final String type_uuid, final short handleStart, final short handleEnd)

        std::function<jobject(JNIEnv*, jclass, jmethodID, BTGattService*)> ctor_service =
                [](JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, BTGattService *service)->jobject {
                    // prepare adapter ctor
                    std::shared_ptr<BTDevice> _device = service->getDeviceChecked();
                    JavaGlobalObj::check(_device->getJavaObject(), E_FILE_LINE);
                    jobject jdevice = JavaGlobalObj::GetObject(_device->getJavaObject());
                    const jboolean isPrimary = service->isPrimary;
                    const jstring juuid = from_string_to_jstring(env_,
                            directBTJNISettings.getUnifyUUID128Bit() ? service->type->toUUID128String() :
                                                                       service->type->toString());
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    jobject jservice = env_->NewObject(clazz, clazz_ctor, (jlong)service, jdevice, isPrimary,
                            juuid, service->startHandle, service->endHandle);
                    java_exception_check_and_throw(env_, E_FILE_LINE);
                    JNIGlobalRef::check(jservice, E_FILE_LINE);
                    std::shared_ptr<JavaAnon> jServiceRef = service->getJavaObject(); // GlobalRef
                    JavaGlobalObj::check(jServiceRef, E_FILE_LINE);
                    env_->DeleteLocalRef(juuid);
                    env_->DeleteLocalRef(jservice);
                    return JavaGlobalObj::GetObject(jServiceRef);
                };
        return convert_vector_sharedptr_to_jarraylist<jau::darray<BTGattServiceRef>, BTGattService>(
                env, services, _serviceClazzCtorArgs.c_str(), ctor_service);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_jau_direct_1bt_DBTDevice_pingGATTImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->pingGATT() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jstring Java_jau_direct_1bt_DBTDevice_getIcon(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return nullptr; // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr; // FIXME;
}

void Java_jau_direct_1bt_DBTDevice_setTrustedImpl(JNIEnv *env, jobject obj, jboolean value)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        (void)value;
        // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_setBlockedImpl(JNIEnv *env, jobject obj, jboolean value)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        (void)value;
        // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean JNICALL Java_jau_direct_1bt_DBTDevice_getLegacyPairing(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return JNI_FALSE; // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jshort Java_jau_direct_1bt_DBTDevice_getRSSI(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return (jshort) device->getRSSI();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}


jobjectArray Java_jau_direct_1bt_DBTDevice_getUUIDs(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return nullptr; // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_jau_direct_1bt_DBTDevice_getModalias(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return nullptr; // FIXME
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jobject Java_jau_direct_1bt_DBTDevice_getManufacturerData(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        std::shared_ptr<ManufactureSpecificData> mdata = device->getManufactureSpecificData();

        jclass map_cls = search_class(env, "java/util/HashMap");
        jmethodID map_ctor = search_method(env, map_cls, "<init>", "(I)V", false);
        jmethodID map_put = search_method(env, map_cls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", false);

        jclass short_cls = search_class(env, "java/lang/Short");
        jmethodID short_ctor = search_method(env, short_cls, "<init>", "(S)V", false);
        jobject result = nullptr;

        if( nullptr != mdata ) {
            result = env->NewObject(map_cls, map_ctor, 1);
            jbyteArray arr = env->NewByteArray(mdata->data.getSize());
            env->SetByteArrayRegion(arr, 0, mdata->data.getSize(), (const jbyte *)mdata->data.get_ptr());
            jobject key = env->NewObject(short_cls, short_ctor, mdata->company);
            env->CallObjectMethod(result, map_put, key, arr);

            env->DeleteLocalRef(arr);
            env->DeleteLocalRef(key);
        } else {
            result = env->NewObject(map_cls, map_ctor, 0);
        }
        if (nullptr == result) {
            throw std::bad_alloc();
        }
        return result;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jshort Java_jau_direct_1bt_DBTDevice_getTxPower(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        return (jshort) device->getTxPower();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

//
// BooleanDeviceCBContext
//

struct BooleanDeviceCBContext {
    BDAddressAndType addressAndType;
    JNIGlobalRef javaCallback_ref;
    jmethodID  mRun;
    JNIGlobalRef boolean_cls_ref;
    jmethodID boolean_ctor;

    BooleanDeviceCBContext(
            BDAddressAndType addressAndType_,
            JNIGlobalRef javaCallback_ref_,
            jmethodID  mRun_,
            JNIGlobalRef boolean_cls_ref_,
            jmethodID boolean_ctor_)
    : addressAndType(addressAndType_), javaCallback_ref(javaCallback_ref_),
      mRun(mRun_), boolean_cls_ref(boolean_cls_ref_), boolean_ctor(boolean_ctor_)
    { }


    bool operator==(const BooleanDeviceCBContext& rhs) const
    {
        if( &rhs == this ) {
            return true;
        }
        return rhs.addressAndType == addressAndType &&
               rhs.javaCallback_ref == javaCallback_ref;
    }

    bool operator!=(const BooleanDeviceCBContext& rhs) const
    { return !( *this == rhs ); }

};
typedef std::shared_ptr<BooleanDeviceCBContext> BooleanDeviceCBContextRef;


//
// Blocked
//

static void disableBlockedNotifications(JNIEnv *env, jobject obj, BTManager &mgmt)
{
    InvocationFunc<bool, const MgmtEvent&> * funcptr =
            getObjectRef<InvocationFunc<bool, const MgmtEvent&>>(env, obj, "blockedNotificationRef");
    if( nullptr != funcptr ) {
        FunctionDef<bool, const MgmtEvent&> funcDef( funcptr );
        funcptr = nullptr;
        setObjectRef(env, obj, funcptr, "blockedNotificationRef"); // clear java ref
        int count;
        if( 1 != ( count = mgmt.removeMgmtEventCallback(MgmtEvent::Opcode::DEVICE_BLOCKED, funcDef) ) ) {
            throw InternalError(std::string("removeMgmtEventCallback of ")+funcDef.toString()+" not 1 but "+std::to_string(count), E_FILE_LINE);
        }
        if( 1 != ( count = mgmt.removeMgmtEventCallback(MgmtEvent::Opcode::DEVICE_UNBLOCKED, funcDef) ) ) {
            throw InternalError(std::string("removeMgmtEventCallback of ")+funcDef.toString()+" not 1 but "+std::to_string(count), E_FILE_LINE);
        }
    }
}
void Java_jau_direct_1bt_DBTDevice_disableBlockedNotificationsImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        BTManager & mgmt = device->getAdapter().getManager();

        disableBlockedNotifications(env, obj, mgmt);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
void Java_jau_direct_1bt_DBTDevice_enableBlockedNotificationsImpl(JNIEnv *env, jobject obj, jobject javaCallback)
{
    try {
        BTDevice *device= getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        BTAdapter & adapter = device->getAdapter();
        BTManager & mgmt = adapter.getManager();

        disableBlockedNotifications(env, obj, mgmt);

        bool(*nativeCallback)(BooleanDeviceCBContextRef&, const MgmtEvent&) =
                [](BooleanDeviceCBContextRef& ctx_ref, const MgmtEvent& e)->bool {
            bool isBlocked = false;
            if( MgmtEvent::Opcode::DEVICE_BLOCKED == e.getOpcode() ) {
                const MgmtEvtDeviceBlocked &event = *static_cast<const MgmtEvtDeviceBlocked *>(&e);
                if( event.getAddress() != ctx_ref->addressAndType.address || event.getAddressType() != ctx_ref->addressAndType.type ) {
                    return false; // not this device
                }
                isBlocked = true;
            } else if( MgmtEvent::Opcode::DEVICE_UNBLOCKED == e.getOpcode() ) {
                const MgmtEvtDeviceUnblocked &event = *static_cast<const MgmtEvtDeviceUnblocked *>(&e);
                if( event.getAddress() != ctx_ref->addressAndType.address || event.getAddressType() != ctx_ref->addressAndType.type ) {
                    return false; // not this device
                }
                isBlocked = false;
            } else {
                return false; // oops
            }

            jobject result = jni_env->NewObject(ctx_ref->boolean_cls_ref.getClass(), ctx_ref->boolean_ctor, isBlocked ? JNI_TRUE : JNI_FALSE);
            jni_env->CallVoidMethod(*(ctx_ref->javaCallback_ref), ctx_ref->mRun, result);
            jni_env->DeleteLocalRef(result);
            return true;
        };
        jclass notification = search_class(*jni_env, javaCallback);
        jmethodID  mRun = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        jni_env->DeleteLocalRef(notification);

        jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
        jmethodID boolean_ctor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);
        java_exception_check_and_throw(env, E_FILE_LINE);

        BooleanDeviceCBContextRef ctx_ref = std::make_shared<BooleanDeviceCBContext>(
                device->getAddressAndType(), JNIGlobalRef(javaCallback), mRun, JNIGlobalRef(boolean_cls), boolean_ctor );
        jni_env->DeleteLocalRef(boolean_cls);

        // move BooleanDeviceCBContextRef into CaptureInvocationFunc and operator== includes javaCallback comparison
        FunctionDef<bool, const MgmtEvent&> funcDef = bindCaptureFunc(ctx_ref, nativeCallback);
        setObjectRef(env, obj, funcDef.cloneFunction(), "blockedNotificationRef"); // set java ref
        mgmt.addMgmtEventCallback(adapter.dev_id, MgmtEvent::Opcode::DEVICE_BLOCKED, funcDef);
        mgmt.addMgmtEventCallback(adapter.dev_id, MgmtEvent::Opcode::DEVICE_UNBLOCKED, funcDef);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

//
// Paired
//

static void disablePairedNotifications(JNIEnv *env, jobject obj, BTManager &mgmt)
{
    InvocationFunc<bool, const MgmtEvent&> * funcptr =
            getObjectRef<InvocationFunc<bool, const MgmtEvent&>>(env, obj, "pairedNotificationRef");
    if( nullptr != funcptr ) {
        FunctionDef<bool, const MgmtEvent&> funcDef( funcptr );
        funcptr = nullptr;
        setObjectRef(env, obj, funcptr, "pairedNotificationRef"); // clear java ref
        int count;
        if( 1 != ( count = mgmt.removeMgmtEventCallback(MgmtEvent::Opcode::DEVICE_UNPAIRED, funcDef) ) ) {
            throw InternalError(std::string("removeMgmtEventCallback of ")+funcDef.toString()+" not 1 but "+std::to_string(count), E_FILE_LINE);
        }
    }
}
void Java_jau_direct_1bt_DBTDevice_disablePairedNotificationsImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        BTManager & mgmt = device->getAdapter().getManager();

        disablePairedNotifications(env, obj, mgmt);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
void Java_jau_direct_1bt_DBTDevice_enablePairedNotificationsImpl(JNIEnv *env, jobject obj, jobject javaCallback)
{
    try {
        BTDevice *device= getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);
        BTAdapter & adapter = device->getAdapter();
        BTManager & mgmt = adapter.getManager();

        disablePairedNotifications(env, obj, mgmt);

        bool(*nativeCallback)(BooleanDeviceCBContextRef&, const MgmtEvent&) =
                [](BooleanDeviceCBContextRef& ctx_ref, const MgmtEvent& e)->bool {
            const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(&e);
            if( event.getAddress() != ctx_ref->addressAndType.address || event.getAddressType() != ctx_ref->addressAndType.type ) {
                return false; // not this device
            }
            jobject result = jni_env->NewObject(ctx_ref->boolean_cls_ref.getClass(), ctx_ref->boolean_ctor, JNI_FALSE);
            jni_env->CallVoidMethod(*(ctx_ref->javaCallback_ref), ctx_ref->mRun, result);
            jni_env->DeleteLocalRef(result);
            return true;
        };
        jclass notification = search_class(*jni_env, javaCallback);
        jmethodID  mRun = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        jni_env->DeleteLocalRef(notification);

        jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
        jmethodID boolean_ctor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);
        java_exception_check_and_throw(env, E_FILE_LINE);

        BooleanDeviceCBContext * ctx = new BooleanDeviceCBContext{
            device->getAddressAndType(), JNIGlobalRef(javaCallback), mRun, JNIGlobalRef(boolean_cls), boolean_ctor };
        jni_env->DeleteLocalRef(boolean_cls);

        // move BooleanDeviceCBContextRef into CaptureInvocationFunc and operator== includes javaCallback comparison
        FunctionDef<bool, const MgmtEvent&> funcDef = bindCaptureFunc(BooleanDeviceCBContextRef(ctx), nativeCallback);
        setObjectRef(env, obj, funcDef.cloneFunction(), "pairedNotificationRef"); // set java ref
        // Note that this is only called natively for unpaired, i.e. paired:=false. Using deviceConnected for paired:=true on Java side
        mgmt.addMgmtEventCallback(adapter.dev_id, MgmtEvent::Opcode::DEVICE_UNPAIRED, funcDef);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}
