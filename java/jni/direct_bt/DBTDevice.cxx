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

        const size_t value_size = charValue.size();
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

        const size_t value_size = charValue.size();
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

jbyte Java_jau_direct_1bt_DBTDevice_getRoleImpl(JNIEnv *env, jobject obj)
{
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        return (jbyte) number( device->getRole() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number( BTRole::None );
}

jbyte Java_jau_direct_1bt_DBTDevice_getConnectedLE_1PHYImpl(JNIEnv *env, jobject obj,
                                                            jbyteArray jresTx, jbyteArray jresRx) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jresTx ) {
            throw IllegalArgumentException("resTx byte array null", E_FILE_LINE);
        }
        if( nullptr == jresRx ) {
            throw IllegalArgumentException("resRx byte array null", E_FILE_LINE);
        }

        const size_t resTx_size = env->GetArrayLength(jresTx);
        if( 1 > resTx_size ) {
            throw IllegalArgumentException("resTx byte array "+std::to_string(resTx_size)+" < 1", E_FILE_LINE);
        }
        const size_t resRx_size = env->GetArrayLength(jresRx);
        if( 1 > resRx_size ) {
            throw IllegalArgumentException("resRx byte array "+std::to_string(resRx_size)+" < 1", E_FILE_LINE);
        }

        JNICriticalArray<uint8_t, jbyteArray> criticalArrayTx(env); // RAII - release
        uint8_t * resTx_ptr = criticalArrayTx.get(jresTx, criticalArrayTx.Mode::UPDATE_AND_RELEASE);
        if( NULL == resTx_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(resTx byte array) is null", E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArrayRx(env); // RAII - release
        uint8_t * resRx_ptr = criticalArrayRx.get(jresRx, criticalArrayTx.Mode::UPDATE_AND_RELEASE);
        if( NULL == resRx_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(resRx byte array) is null", E_FILE_LINE);
        }

        LE_PHYs& resTx = *reinterpret_cast<LE_PHYs *>(resTx_ptr);
        LE_PHYs& resRx = *reinterpret_cast<LE_PHYs *>(resRx_ptr);

        return number( device->getConnectedLE_PHY(resTx, resRx) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_setConnectedLE_1PHYImpl(JNIEnv *env, jobject obj,
                                                            jbyte jTx, jbyte jRx) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        const LE_PHYs Tx = static_cast<LE_PHYs>(jTx);
        const LE_PHYs Rx = static_cast<LE_PHYs>(jRx);

        return number ( device->setConnectedLE_PHY(Tx, Rx) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_getTxPhysImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getTxPhys() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTDevice_getRxPhysImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->getRxPhys() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
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

        return number( device->getAvailableSMPKeys(JNI_TRUE == responder) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jbyte Java_jau_direct_1bt_DBTDevice_uploadKeysImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->uploadKeys() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

void Java_jau_direct_1bt_DBTDevice_getLongTermKeyImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPLongTermKey) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPLongTermKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPLongTermKey& ltk_sink = *reinterpret_cast<SMPLongTermKey *>(sink_ptr);
        ltk_sink = device->getLongTermKey(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_setLongTermKeyImpl(JNIEnv *env, jobject obj, jbyteArray jsource) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsource ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t source_size = env->GetArrayLength(jsource);
        if( sizeof(SMPLongTermKey) > source_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(source_size)+" < "+std::to_string(sizeof(SMPLongTermKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * source_ptr = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == source_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const SMPLongTermKey& ltk = *reinterpret_cast<SMPLongTermKey *>(source_ptr);

        device->setLongTermKey(ltk);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_getIdentityResolvingKeyImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPIdentityResolvingKey) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPIdentityResolvingKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPIdentityResolvingKey& irk_sink = *reinterpret_cast<SMPIdentityResolvingKey *>(sink_ptr);
        irk_sink = device->getIdentityResolvingKey(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_setIdentityResolvingKeyImpl(JNIEnv *env, jobject obj, jbyteArray jsource) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsource ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t source_size = env->GetArrayLength(jsource);
        if( sizeof(SMPIdentityResolvingKey) > source_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(source_size)+" < "+std::to_string(sizeof(SMPIdentityResolvingKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * source_ptr = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == source_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const SMPIdentityResolvingKey& irk = *reinterpret_cast<SMPIdentityResolvingKey *>(source_ptr);

        device->setIdentityResolvingKey(irk);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_getSignatureResolvingKeyImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPSignatureResolvingKey) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPSignatureResolvingKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPSignatureResolvingKey& irk_sink = *reinterpret_cast<SMPSignatureResolvingKey *>(sink_ptr);
        irk_sink = device->getSignatureResolvingKey(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_setSignatureResolvingKeyImpl(JNIEnv *env, jobject obj, jbyteArray jsource) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsource ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t source_size = env->GetArrayLength(jsource);
        if( sizeof(SMPSignatureResolvingKey) > source_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(source_size)+" < "+std::to_string(sizeof(SMPSignatureResolvingKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * source_ptr = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == source_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const SMPSignatureResolvingKey& irk = *reinterpret_cast<SMPSignatureResolvingKey *>(source_ptr);

        device->setSignatureResolvingKey(irk);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_getLinkKeyImpl(JNIEnv *env, jobject obj, jboolean responder, jbyteArray jsink) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsink ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t sink_size = env->GetArrayLength(jsink);
        if( sizeof(SMPLinkKey) > sink_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(sink_size)+" < "+std::to_string(sizeof(SMPLinkKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * sink_ptr = criticalArray.get(jsink, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == sink_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        SMPLinkKey& lk_sink = *reinterpret_cast<SMPLinkKey *>(sink_ptr);
        lk_sink = device->getLinkKey(JNI_TRUE == responder); // assign data of new key copy to JNI critical-array
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTDevice_setLinkKeyImpl(JNIEnv *env, jobject obj, jbyteArray jsource) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jsource ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t source_size = env->GetArrayLength(jsource);
        if( sizeof(SMPLinkKey) > source_size ) {
            throw IllegalArgumentException("byte array "+std::to_string(source_size)+" < "+std::to_string(sizeof(SMPLinkKey)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * source_ptr = criticalArray.get(jsource, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == source_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const SMPLinkKey& lk = *reinterpret_cast<SMPLinkKey *>(source_ptr);

        device->setLinkKey(lk);
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

jboolean Java_jau_direct_1bt_DBTDevice_setConnSecurityImpl(JNIEnv *env, jobject obj, jbyte jsec_level, jbyte jio_cap) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnSecurity( to_BTSecurityLevel( static_cast<uint8_t>(jsec_level) ),
                                        to_SMPIOCapability( static_cast<uint8_t>(jio_cap) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTDevice_setConnSecurityAutoImpl(JNIEnv *env, jobject obj, jbyte jio_cap) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return device->setConnSecurityAuto( to_SMPIOCapability( static_cast<uint8_t>(jio_cap) ) );
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

jbyte Java_jau_direct_1bt_DBTDevice_setPairingPasskeyImpl(JNIEnv *env, jobject obj, jint jpasskey) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->setPairingPasskey( static_cast<uint32_t>(jpasskey) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return static_cast<uint8_t>( HCIStatusCode::INTERNAL_FAILURE );
}

jbyte Java_jau_direct_1bt_DBTDevice_setPairingPasskeyNegativeImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        return number( device->setPairingPasskeyNegative() );
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

jobject Java_jau_direct_1bt_DBTDevice_getGattServicesImpl(JNIEnv *env, jobject obj) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        jau::darray<BTGattServiceRef> services = device->getGattServices(); // implicit GATT connect and discovery if required incl GenericAccess retrieval
        if( services.size() > 0 ) {
            std::shared_ptr<GattGenericAccessSvc> ga = device->getGattGenericAccess();
            if( nullptr != ga ) {
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
                    const jboolean isPrimary = service->primary;
                    const jstring juuid = from_string_to_jstring(env_, service->type->toUUID128String());
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    jobject jservice = env_->NewObject(clazz, clazz_ctor, (jlong)service, jdevice, isPrimary,
                            juuid, service->handle, service->end_handle);
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

jboolean Java_jau_direct_1bt_DBTDevice_sendNotification(JNIEnv *env, jobject obj, jshort char_value_handle, jbyteArray jval) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jval ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t value_size = env->GetArrayLength(jval);
        if( 0 >= value_size ) {
            return JNI_TRUE; // no data is OK
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * value_ptr = criticalArray.get(jval, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == value_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const jau::TROOctets value(value_ptr, value_size, endian::little);
        device->sendNotification(char_value_handle, value);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTDevice_sendIndication(JNIEnv *env, jobject obj, jshort char_value_handle, jbyteArray jval) {
    try {
        BTDevice *device = getJavaUplinkObject<BTDevice>(env, obj);
        JavaGlobalObj::check(device->getJavaObject(), E_FILE_LINE);

        if( nullptr == jval ) {
            throw IllegalArgumentException("byte array null", E_FILE_LINE);
        }
        const size_t value_size = env->GetArrayLength(jval);
        if( 0 >= value_size ) {
            return JNI_TRUE; // no data is OK
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * value_ptr = criticalArray.get(jval, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == value_ptr ) {
            throw InternalError("GetPrimitiveArrayCritical(byte array) is null", E_FILE_LINE);
        }
        const jau::TROOctets value(value_ptr, value_size, endian::little);
        device->sendIndication(char_value_handle, value);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
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
            jbyteArray arr = env->NewByteArray(mdata->getData().size());
            env->SetByteArrayRegion(arr, 0, mdata->getData().size(), (const jbyte *)mdata->getData().get_ptr());
            jobject key = env->NewObject(short_cls, short_ctor, mdata->getCompany());
            env->CallObjectMethod(result, map_put, key, arr);

            env->DeleteLocalRef(arr);
            env->DeleteLocalRef(key);
        } else {
            result = env->NewObject(map_cls, map_ctor, 0);
        }
        if (nullptr == result) {
            throw jau::OutOfMemoryError("new HashMap() returned null", E_FILE_LINE);
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

#if 0
//
// Leave below code 'in' disabled, as an example of how to bind Java callback functions to C++ callback functions ad-hoc.

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
#endif
