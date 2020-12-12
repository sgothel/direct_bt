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

#include "direct_bt_tinyb_DBTAdapter.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/DBTAdapter.hpp"

using namespace direct_bt;

static const std::string _adapterSettingsClassName("org/tinyb/AdapterSettings");
static const std::string _adapterSettingsClazzCtorArgs("(I)V");
static const std::string _eirDataTypeSetClassName("org/tinyb/EIRDataTypeSet");
static const std::string _eirDataTypeSetClazzCtorArgs("(I)V");
static const std::string _hciStatusCodeClassName("org/tinyb/HCIStatusCode");
static const std::string _hciStatusCodeClazzGetArgs("(B)Lorg/tinyb/HCIStatusCode;");
static const std::string _scanTypeClassName("org/tinyb/ScanType");
static const std::string _scanTypeClazzGetArgs("(B)Lorg/tinyb/ScanType;");
static const std::string _pairingModeClassName("org/tinyb/PairingMode");
static const std::string _pairingModeClazzGetArgs("(B)Lorg/tinyb/PairingMode;");
static const std::string _pairingStateClassName("org/tinyb/SMPPairingState");
static const std::string _pairingStateClazzGetArgs("(B)Lorg/tinyb/SMPPairingState;");
static const std::string _deviceClazzCtorArgs("(JLdirect_bt/tinyb/DBTAdapter;[BBLjava/lang/String;J)V");

static const std::string _adapterSettingsChangedMethodArgs("(Lorg/tinyb/BluetoothAdapter;Lorg/tinyb/AdapterSettings;Lorg/tinyb/AdapterSettings;Lorg/tinyb/AdapterSettings;J)V");
static const std::string _discoveringChangedMethodArgs("(Lorg/tinyb/BluetoothAdapter;Lorg/tinyb/ScanType;Lorg/tinyb/ScanType;ZZJ)V");
static const std::string _deviceFoundMethodArgs("(Lorg/tinyb/BluetoothDevice;J)V");
static const std::string _deviceUpdatedMethodArgs("(Lorg/tinyb/BluetoothDevice;Lorg/tinyb/EIRDataTypeSet;J)V");
static const std::string _deviceConnectedMethodArgs("(Lorg/tinyb/BluetoothDevice;SJ)V");
static const std::string _devicePairingStateMethodArgs("(Lorg/tinyb/BluetoothDevice;Lorg/tinyb/SMPPairingState;Lorg/tinyb/PairingMode;J)V");
static const std::string _deviceReadyMethodArgs("(Lorg/tinyb/BluetoothDevice;J)V");
static const std::string _deviceDisconnectedMethodArgs("(Lorg/tinyb/BluetoothDevice;Lorg/tinyb/HCIStatusCode;SJ)V");

class JNIAdapterStatusListener : public AdapterStatusListener {
  private:
    /**
        package org.tinyb;

        public abstract class AdapterStatusListener {
            private long nativeInstance;

            public void adapterSettingsChanged(final BluetoothAdapter adapter,
                                               final AdapterSettings oldmask, final AdapterSettings newmask,
                                               final AdapterSettings changedmask, final long timestamp) { }
            public void discoveringChanged(final BluetoothAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled,
                                           final boolean keepAlive, final long timestamp) { }
            public void deviceFound(final BluetoothDevice device, final long timestamp) { }
            public void deviceUpdated(final BluetoothDevice device, final EIRDataTypeSet updateMask, final long timestamp) { }
            public void deviceConnected(final BluetoothDevice device, final short handle, final long timestamp) { }
            public void devicePairingState(final BluetoothDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {}
            public void deviceReady(final BluetoothDevice device, final long timestamp) {}
            public void deviceDisconnected(final BluetoothDevice device, final HCIStatusCode reason, final short handle, final long timestamp) { }

        };
    */
    static std::atomic<int> iname_next;
    int const iname;
    DBTDevice const * const deviceMatchRef;
    std::shared_ptr<jau::JavaAnon> adapterObjRef;
    JNIGlobalRef adapterSettingsClazzRef;
    jmethodID adapterSettingsClazzCtor;
    JNIGlobalRef eirDataTypeSetClazzRef;
    jmethodID eirDataTypeSetClazzCtor;
    JNIGlobalRef hciStatusCodeClazzRef;
    jmethodID hciStatusCodeClazzGet;
    JNIGlobalRef scanTypeClazzRef;
    jmethodID scanTypeClazzGet;
    JNIGlobalRef pairingModeClazzRef;
    jmethodID pairingModeClazzGet;
    JNIGlobalRef pairingStateClazzRef;
    jmethodID pairingStateClazzGet;

    JNIGlobalRef deviceClazzRef;
    jmethodID deviceClazzCtor;
    jfieldID deviceClazzTSLastDiscoveryField;
    jfieldID deviceClazzTSLastUpdateField;
    jfieldID deviceClazzConnectionHandleField;
    jau::JavaGlobalObj listenerObjRef;
    jmethodID  mAdapterSettingsChanged = nullptr;
    jmethodID  mDiscoveringChanged = nullptr;
    jmethodID  mDeviceFound = nullptr;
    jmethodID  mDeviceUpdated = nullptr;
    jmethodID  mDeviceConnected= nullptr;
    jmethodID  mDevicePairingState= nullptr;
    jmethodID  mDeviceReady = nullptr;
    jmethodID  mDeviceDisconnected = nullptr;

  public:

    std::string toString() const override {
        const std::string devMatchAddr = nullptr != deviceMatchRef ? deviceMatchRef->getAddressAndType().toString() : "nil";
        return "JNIAdapterStatusListener[this "+jau::aptrHexString(this)+", iname "+std::to_string(iname)+", devMatchAddr "+devMatchAddr+"]";
    }

    ~JNIAdapterStatusListener() override {
        // listenerObjRef dtor will call notifyDelete and clears the nativeInstance handle
    }

    JNIAdapterStatusListener(JNIEnv *env, DBTAdapter *adapter,
                             jclass listenerClazz, jobject statusListenerObj, jmethodID  statusListenerNotifyDeleted,
                             const DBTDevice * _deviceMatchRef)
    : iname(iname_next.fetch_add(1)), deviceMatchRef(_deviceMatchRef), listenerObjRef(statusListenerObj, statusListenerNotifyDeleted)
    {
        adapterObjRef = adapter->getJavaObject();
        jau::JavaGlobalObj::check(adapterObjRef, E_FILE_LINE);

        // adapterSettingsClazzRef, adapterSettingsClazzCtor
        {
            jclass adapterSettingsClazz = jau::search_class(env, _adapterSettingsClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == adapterSettingsClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_adapterSettingsClassName, E_FILE_LINE);
            }
            adapterSettingsClazzRef = JNIGlobalRef(adapterSettingsClazz);
            env->DeleteLocalRef(adapterSettingsClazz);
        }
        adapterSettingsClazzCtor = jau::search_method(env, adapterSettingsClazzRef.getClass(), "<init>", _adapterSettingsClazzCtorArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == adapterSettingsClazzCtor ) {
            throw jau::InternalError("AdapterSettings ctor not found: "+_adapterSettingsClassName+".<init>"+_adapterSettingsClazzCtorArgs, E_FILE_LINE);
        }

        // eirDataTypeSetClazzRef, eirDataTypeSetClazzCtor
        {
            jclass eirDataTypeSetClazz = jau::search_class(env, _eirDataTypeSetClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == eirDataTypeSetClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_eirDataTypeSetClassName, E_FILE_LINE);
            }
            eirDataTypeSetClazzRef = JNIGlobalRef(eirDataTypeSetClazz);
            env->DeleteLocalRef(eirDataTypeSetClazz);
        }
        eirDataTypeSetClazzCtor = jau::search_method(env, eirDataTypeSetClazzRef.getClass(), "<init>", _eirDataTypeSetClazzCtorArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == eirDataTypeSetClazzCtor ) {
            throw jau::InternalError("EIRDataType ctor not found: "+_eirDataTypeSetClassName+".<init>"+_eirDataTypeSetClazzCtorArgs, E_FILE_LINE);
        }

        // hciStatusCodeClazzRef, hciStatusCodeClazzGet
        {
            jclass hciErrorCodeClazz = jau::search_class(env, _hciStatusCodeClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == hciErrorCodeClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_hciStatusCodeClassName, E_FILE_LINE);
            }
            hciStatusCodeClazzRef = JNIGlobalRef(hciErrorCodeClazz);
            env->DeleteLocalRef(hciErrorCodeClazz);
        }
        hciStatusCodeClazzGet = jau::search_method(env, hciStatusCodeClazzRef.getClass(), "get", _hciStatusCodeClazzGetArgs.c_str(), true);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == hciStatusCodeClazzGet ) {
            throw jau::InternalError("Static method not found: "+_hciStatusCodeClassName+".get"+_hciStatusCodeClazzGetArgs, E_FILE_LINE);
        }

        // scanTypeClazzRef, scanTypeClazzGet
        {
            jclass scanTypeClazz = jau::search_class(env, _scanTypeClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == scanTypeClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_scanTypeClassName, E_FILE_LINE);
            }
            scanTypeClazzRef = JNIGlobalRef(scanTypeClazz);
            env->DeleteLocalRef(scanTypeClazz);
        }
        scanTypeClazzGet = jau::search_method(env, scanTypeClazzRef.getClass(), "get", _scanTypeClazzGetArgs.c_str(), true);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == scanTypeClazzGet ) {
            throw jau::InternalError("Static method not found: "+_scanTypeClassName+".get"+_scanTypeClazzGetArgs, E_FILE_LINE);
        }

        // pairingModeClazzRef, pairingModeClazzGet
        {
            jclass pairingModeClazz = jau::search_class(env, _pairingModeClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == pairingModeClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_pairingModeClassName, E_FILE_LINE);
            }
            pairingModeClazzRef = JNIGlobalRef(pairingModeClazz);
            env->DeleteLocalRef(pairingModeClazz);
        }
        pairingModeClazzGet = jau::search_method(env, pairingModeClazzRef.getClass(), "get", _pairingModeClazzGetArgs.c_str(), true);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == pairingModeClazzGet ) {
            throw jau::InternalError("Static method not found: "+_pairingModeClassName+".get"+_pairingModeClazzGetArgs, E_FILE_LINE);
        }

        // pairingStateClazzRef, pairingStateClazzGet
        {
            jclass pairingStateClazz = jau::search_class(env, _pairingStateClassName.c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == pairingStateClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+_pairingStateClassName, E_FILE_LINE);
            }
            pairingStateClazzRef = JNIGlobalRef(pairingStateClazz);
            env->DeleteLocalRef(pairingStateClazz);
        }
        pairingStateClazzGet = jau::search_method(env, pairingStateClazzRef.getClass(), "get", _pairingStateClazzGetArgs.c_str(), true);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == pairingStateClazzGet ) {
            throw jau::InternalError("Static method not found: "+_pairingStateClassName+".get"+_pairingStateClazzGetArgs, E_FILE_LINE);
        }

        // deviceClazzRef, deviceClazzCtor
        {
            jclass deviceClazz = jau::search_class(env, DBTDevice::java_class().c_str());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            if( nullptr == deviceClazz ) {
                throw jau::InternalError("DBTDevice::java_class not found: "+DBTDevice::java_class(), E_FILE_LINE);
            }
            deviceClazzRef = JNIGlobalRef(deviceClazz);
            env->DeleteLocalRef(deviceClazz);
        }
        deviceClazzCtor = jau::search_method(env, deviceClazzRef.getClass(), "<init>", _deviceClazzCtorArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == deviceClazzCtor ) {
            throw jau::InternalError("DBTDevice::java_class ctor not found: "+DBTDevice::java_class()+".<init>"+_deviceClazzCtorArgs, E_FILE_LINE);
        }
        deviceClazzTSLastDiscoveryField = env->GetFieldID(deviceClazzRef.getClass(), "ts_last_discovery", "J");
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == deviceClazzTSLastDiscoveryField ) {
            throw jau::InternalError("DBTDevice::java_class field not found: "+DBTDevice::java_class()+".ts_last_discovery", E_FILE_LINE);
        }
        deviceClazzTSLastUpdateField = env->GetFieldID(deviceClazzRef.getClass(), "ts_last_update", "J");
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == deviceClazzTSLastUpdateField ) {
            throw jau::InternalError("DBTDevice::java_class field not found: "+DBTDevice::java_class()+".ts_last_update", E_FILE_LINE);
        }
        deviceClazzConnectionHandleField = env->GetFieldID(deviceClazzRef.getClass(), "hciConnHandle", "S");
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == deviceClazzConnectionHandleField ) {
            throw jau::InternalError("DBTDevice::java_class field not found: "+DBTDevice::java_class()+".hciConnHandle", E_FILE_LINE);
        }

        mAdapterSettingsChanged = jau::search_method(env, listenerClazz, "adapterSettingsChanged", _adapterSettingsChangedMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mAdapterSettingsChanged ) {
            throw jau::InternalError("AdapterStatusListener has no adapterSettingsChanged"+_adapterSettingsChangedMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDiscoveringChanged = jau::search_method(env, listenerClazz, "discoveringChanged", _discoveringChangedMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDiscoveringChanged ) {
            throw jau::InternalError("AdapterStatusListener has no discoveringChanged"+_discoveringChangedMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDeviceFound = jau::search_method(env, listenerClazz, "deviceFound", _deviceFoundMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDeviceFound ) {
            throw jau::InternalError("AdapterStatusListener has no deviceFound"+_deviceFoundMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDeviceUpdated = jau::search_method(env, listenerClazz, "deviceUpdated", _deviceUpdatedMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDeviceUpdated ) {
            throw jau::InternalError("AdapterStatusListener has no deviceUpdated"+_deviceUpdatedMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDeviceConnected = jau::search_method(env, listenerClazz, "deviceConnected", _deviceConnectedMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDeviceConnected ) {
            throw jau::InternalError("AdapterStatusListener has no deviceConnected"+_deviceConnectedMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDevicePairingState = jau::search_method(env, listenerClazz, "devicePairingState", _devicePairingStateMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDevicePairingState ) {
            throw jau::InternalError("AdapterStatusListener has no devicePairingState"+_devicePairingStateMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDeviceReady = jau::search_method(env, listenerClazz, "deviceReady", _deviceReadyMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDeviceReady ) {
            throw jau::InternalError("AdapterStatusListener has no deviceReady"+_deviceReadyMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
        mDeviceDisconnected = jau::search_method(env, listenerClazz, "deviceDisconnected", _deviceDisconnectedMethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mDeviceDisconnected ) {
            throw jau::InternalError("AdapterStatusListener has no deviceDisconnected"+_deviceDisconnectedMethodArgs+" method, for "+adapter->toString(), E_FILE_LINE);
        }
    }

    bool matchDevice(const DBTDevice & device) override {
        if( nullptr == deviceMatchRef ) {
            return true;
        }
        return device == *deviceMatchRef;
    }

    void adapterSettingsChanged(DBTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        (void)a;
        jobject adapterSettingOld = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)oldmask);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingOld, E_FILE_LINE);

        jobject adapterSettingNew = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)newmask);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingNew, E_FILE_LINE);

        jobject adapterSettingChanged = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)changedmask);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingChanged, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mAdapterSettingsChanged,
                jau::JavaGlobalObj::GetObject(adapterObjRef), adapterSettingOld, adapterSettingNew, adapterSettingChanged, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(adapterSettingOld);
        env->DeleteLocalRef(adapterSettingNew);
        env->DeleteLocalRef(adapterSettingChanged);
    }

    void discoveringChanged(DBTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        (void)a;

        jobject jcurrentMeta = env->CallStaticObjectMethod(scanTypeClazzRef.getClass(), scanTypeClazzGet, (jbyte)number(currentMeta));
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jcurrentMeta, E_FILE_LINE);

        jobject jchangedType = env->CallStaticObjectMethod(scanTypeClazzRef.getClass(), scanTypeClazzGet, (jbyte)number(changedType));
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jchangedType, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDiscoveringChanged, jau::JavaGlobalObj::GetObject(adapterObjRef),
                            jcurrentMeta, jchangedType, (jboolean)changedEnabled, (jboolean)keepAlive, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }

    void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        jobject jdevice;
        std::shared_ptr<jau::JavaAnon> jDeviceRef0 = device->getJavaObject();
        if( jau::JavaGlobalObj::isValid(jDeviceRef0) ) {
            // Reuse Java instance
            jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef0);
        } else {
            // New Java instance
            // Device(final long nativeInstance, final Adapter adptr, final String address, final int intAddressType, final String name)
            const EUI48 addr = device->getAddressAndType().address;
            jbyteArray jaddr = env->NewByteArray(sizeof(addr));
            env->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            const jstring name = jau::from_string_to_jstring(env, device->getName());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            jobject tmp_jdevice = env->NewObject(deviceClazzRef.getClass(), deviceClazzCtor,
                    (jlong)device.get(), jau::JavaGlobalObj::GetObject(adapterObjRef),
                    jaddr, device->getAddressAndType().type, name, (jlong)timestamp);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            JNIGlobalRef::check(tmp_jdevice, E_FILE_LINE);
            std::shared_ptr<jau::JavaAnon> jDeviceRef1 = device->getJavaObject();
            jau::JavaGlobalObj::check(jDeviceRef1, E_FILE_LINE);
            jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef1);
            env->DeleteLocalRef(jaddr);
            env->DeleteLocalRef(name);
            env->DeleteLocalRef(tmp_jdevice);
        }
        env->SetLongField(jdevice, deviceClazzTSLastDiscoveryField, (jlong)device->getLastDiscoveryTimestamp());
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        env->CallVoidMethod(listenerObjRef.getObject(), mDeviceFound, jdevice, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }

    void deviceUpdated(std::shared_ptr<DBTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        std::shared_ptr<jau::JavaAnon> jDeviceRef = device->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jDeviceRef) ) {
            return; // java device has been pulled
        }
        JNIEnv *env = *jni_env;
        env->SetLongField(jau::JavaGlobalObj::GetObject(jDeviceRef), deviceClazzTSLastUpdateField, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        jobject eirDataTypeSet = env->NewObject(eirDataTypeSetClazzRef.getClass(), eirDataTypeSetClazzCtor, (jint)updateMask);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(eirDataTypeSet, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDeviceUpdated, jau::JavaGlobalObj::GetObject(jDeviceRef), eirDataTypeSet, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(eirDataTypeSet);
    }

    void deviceConnected(std::shared_ptr<DBTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;

        jobject jdevice;
        std::shared_ptr<jau::JavaAnon> jDeviceRef0 = device->getJavaObject();
        if( jau::JavaGlobalObj::isValid(jDeviceRef0) ) {
            // Reuse Java instance
            jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef0);
        } else {
            // New Java instance
            // Device(final long nativeInstance, final Adapter adptr, final String address, final int intAddressType, final String name)
            const EUI48 addr = device->getAddressAndType().address;
            jbyteArray jaddr = env->NewByteArray(sizeof(addr));
            env->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            const jstring name = jau::from_string_to_jstring(env, device->getName());
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            jobject tmp_jdevice = env->NewObject(deviceClazzRef.getClass(), deviceClazzCtor,
                    (jlong)device.get(), jau::JavaGlobalObj::GetObject(adapterObjRef),
                    jaddr, device->getAddressAndType().type, name, (jlong)timestamp);
            jau::java_exception_check_and_throw(env, E_FILE_LINE);
            JNIGlobalRef::check(tmp_jdevice, E_FILE_LINE);
            std::shared_ptr<jau::JavaAnon> jDeviceRef1 = device->getJavaObject();
            jau::JavaGlobalObj::check(jDeviceRef1, E_FILE_LINE);
            jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef1);
            env->DeleteLocalRef(jaddr);
            env->DeleteLocalRef(name);
            env->DeleteLocalRef(tmp_jdevice);
        }
        env->SetShortField(jdevice, deviceClazzConnectionHandleField, (jshort)handle);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        env->SetLongField(jdevice, deviceClazzTSLastDiscoveryField, (jlong)device->getLastDiscoveryTimestamp());
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDeviceConnected, jdevice, (jshort)handle, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void devicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        std::shared_ptr<jau::JavaAnon> jDeviceRef = device->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jDeviceRef) ) {
            return; // java device has been pulled
        }
        JNIEnv *env = *jni_env;

        jobject jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        jobject jstate = env->CallStaticObjectMethod(pairingStateClazzRef.getClass(), pairingStateClazzGet, static_cast<uint8_t>(state));
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jstate, E_FILE_LINE);

        jobject jmode = env->CallStaticObjectMethod(pairingModeClazzRef.getClass(), pairingModeClazzGet, static_cast<uint8_t>(mode));
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jmode, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDevicePairingState, jdevice, jstate, jmode, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void deviceReady(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        std::shared_ptr<jau::JavaAnon> jDeviceRef = device->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jDeviceRef) ) {
            return; // java device has been pulled
        }
        JNIEnv *env = *jni_env;

        jobject jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDeviceReady, jdevice, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void deviceDisconnected(std::shared_ptr<DBTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        std::shared_ptr<jau::JavaAnon> jDeviceRef = device->getJavaObject();
        if( !jau::JavaGlobalObj::isValid(jDeviceRef) ) {
            return; // java device has been pulled
        }
        JNIEnv *env = *jni_env;

        jobject jdevice = jau::JavaGlobalObj::GetObject(jDeviceRef);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        jobject hciErrorCode = env->CallStaticObjectMethod(hciStatusCodeClazzRef.getClass(), hciStatusCodeClazzGet, (jbyte)static_cast<uint8_t>(reason));
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(hciErrorCode, E_FILE_LINE);

        env->SetShortField(jdevice, deviceClazzConnectionHandleField, (jshort)0); // zero out, disconnected
        jau::java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(listenerObjRef.getObject(), mDeviceDisconnected, jdevice, hciErrorCode, (jshort)handle, (jlong)timestamp);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
    }
};
std::atomic<int> JNIAdapterStatusListener::iname_next(0);

jboolean Java_direct_1bt_tinyb_DBTAdapter_addStatusListener(JNIEnv *env, jobject obj, jobject statusListener, jobject jdeviceMatch)
{
    try {
        if( nullptr == statusListener ) {
            throw jau::IllegalArgumentException("JNIAdapterStatusListener::addStatusListener: statusListener is null", E_FILE_LINE);
        }
        {
            JNIAdapterStatusListener * pre = jau::getInstanceUnchecked<JNIAdapterStatusListener>(env, statusListener);
            if( nullptr != pre ) {
                WARN_PRINT("JNIAdapterStatusListener::addStatusListener: statusListener's nativeInstance not null, already in use");
                return false;
            }
        }
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        DBTDevice * deviceMatchRef = nullptr;
        if( nullptr != jdeviceMatch ) {
            deviceMatchRef = jau::getJavaUplinkObject<DBTDevice>(env, jdeviceMatch);
            jau::JavaGlobalObj::check(deviceMatchRef->getJavaObject(), E_FILE_LINE);
        }

        jclass listenerClazz = jau::search_class(env, statusListener);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == listenerClazz ) {
            throw jau::InternalError("AdapterStatusListener not found", E_FILE_LINE);
        }
        jmethodID  mStatusListenerNotifyDeleted = jau::search_method(env, listenerClazz, "notifyDeleted", "()V", false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mStatusListenerNotifyDeleted ) {
            throw jau::InternalError("AdapterStatusListener class has no notifyDeleted() method", E_FILE_LINE);
        }

        std::shared_ptr<AdapterStatusListener> l =
                std::shared_ptr<AdapterStatusListener>( new JNIAdapterStatusListener(env, adapter,
                        listenerClazz, statusListener, mStatusListenerNotifyDeleted, deviceMatchRef) );

        env->DeleteLocalRef(listenerClazz);

        jau::setInstance(env, statusListener, l.get());
        if( adapter->addStatusListener( l ) ) {
            return JNI_TRUE;
        }
        jau::clearInstance(env, statusListener);
        ERR_PRINT("JNIAdapterStatusListener::addStatusListener: FAILED: %s", l->toString().c_str());
    } catch(...) {
        jau::clearInstance(env, statusListener);
        rethrow_and_raise_java_exception(env);
    }
    ERR_PRINT("JNIAdapterStatusListener::addStatusListener: FAILED XX");
    return JNI_FALSE;
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_removeStatusListenerImpl(JNIEnv *env, jobject obj, jobject statusListener)
{
    try {
        if( nullptr == statusListener ) {
            throw jau::IllegalArgumentException("statusListener is null", E_FILE_LINE);
        }
        JNIAdapterStatusListener * pre = jau::getInstanceUnchecked<JNIAdapterStatusListener>(env, statusListener);
        if( nullptr == pre ) {
            DBG_PRINT("JNIAdapterStatusListener::removeStatusListener: statusListener's nativeInstance is null, not in use");
            return false;
        }
        jau::clearInstance(env, statusListener);

        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( ! adapter->removeStatusListener( pre ) ) {
            WARN_PRINT("Failed to remove statusListener with nativeInstance: %p at %s", pre, adapter->toString().c_str());
            return false;
        }
        {
            JNIAdapterStatusListener * post = jau::getInstanceUnchecked<JNIAdapterStatusListener>(env, statusListener);
            if( nullptr != post ) {
                ERR_PRINT("JNIAdapterStatusListener::removeStatusListener: statusListener's nativeInstance not null post native removal");
                return false;
            }
        }
        return true;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jint Java_direct_1bt_tinyb_DBTAdapter_removeAllStatusListener(JNIEnv *env, jobject obj) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        return adapter->removeAllStatusListener();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_isDeviceWhitelisted(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);
        const BDAddressAndType addressAndType(address, static_cast<BDAddressType>( jaddressType ));

        return adapter->isDeviceWhitelisted(addressAndType);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}
jboolean Java_direct_1bt_tinyb_DBTAdapter_addDeviceToWhitelistImpl1(JNIEnv *env, jobject obj,
                                                                    jbyteArray jaddress, jbyte jaddressType, int jctype,
                                                                    jshort min_interval, jshort max_interval,
                                                                    jshort latency, jshort timeout) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        const BDAddressAndType addressAndType(address, static_cast<BDAddressType>( jaddressType ));
        const HCIWhitelistConnectType ctype = static_cast<HCIWhitelistConnectType>( jctype );
        return adapter->addDeviceToWhitelist(addressAndType, ctype, (uint16_t)min_interval, (uint16_t)max_interval, (uint16_t)latency, (uint16_t)timeout);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}
jboolean Java_direct_1bt_tinyb_DBTAdapter_addDeviceToWhitelistImpl2(JNIEnv *env, jobject obj,
                                                                    jbyteArray jaddress, jbyte jaddressType, int jctype) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        const BDAddressAndType addressAndType(address, static_cast<BDAddressType>( jaddressType ));
        const HCIWhitelistConnectType ctype = static_cast<HCIWhitelistConnectType>( jctype );
        return adapter->addDeviceToWhitelist(addressAndType, ctype);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}
jboolean Java_direct_1bt_tinyb_DBTAdapter_removeDeviceFromWhitelistImpl(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        const BDAddressAndType addressAndType(address, static_cast<BDAddressType>( jaddressType ));
        return adapter->removeDeviceFromWhitelist(addressAndType);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jstring Java_direct_1bt_tinyb_DBTAdapter_toStringImpl(JNIEnv *env, jobject obj) {
    try {
        DBTAdapter *nativePtr = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(nativePtr->getJavaObject(), E_FILE_LINE);
        return jau::from_string_to_jstring(env, nativePtr->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_direct_1bt_tinyb_DBTAdapter_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        DBTAdapter *adapter = jau::castInstance<DBTAdapter>(nativeInstance);
        DBG_PRINT("Java_direct_1bt_tinyb_DBTAdapter_deleteImpl %s", adapter->toString().c_str());
        delete adapter;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_isPoweredImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return adapter->isPowered();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_isSuspendedImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return adapter->isSuspended();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_isValidImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return adapter->isValid();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_direct_1bt_tinyb_DBTAdapter_startDiscoveryImpl(JNIEnv *env, jobject obj, jboolean keepAlive)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return (jbyte) number( adapter->startDiscovery(keepAlive) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_direct_1bt_tinyb_DBTAdapter_stopDiscoveryImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return (jbyte) number( adapter->stopDiscovery() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jobject Java_direct_1bt_tinyb_DBTAdapter_getDiscoveredDevicesImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        std::vector<std::shared_ptr<DBTDevice>> array = adapter->getDiscoveredDevices();
        return convert_vector_sharedptr_to_jarraylist(env, array);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jint Java_direct_1bt_tinyb_DBTAdapter_removeDevicesImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        return adapter->removeDiscoveredDevices();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

//
// misc
//

jboolean Java_direct_1bt_tinyb_DBTAdapter_setPowered(JNIEnv *env, jobject obj, jboolean value) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        return adapter->setPowered(JNI_TRUE == value ? true : false) ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_direct_1bt_tinyb_DBTAdapter_resetImpl(JNIEnv *env, jobject obj) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        HCIStatusCode res = adapter->reset();
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jstring Java_direct_1bt_tinyb_DBTAdapter_getAlias(JNIEnv *env, jobject obj) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        return jau::from_string_to_jstring(env, adapter->getLocalName().getName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_direct_1bt_tinyb_DBTAdapter_setAlias(JNIEnv *env, jobject obj, jstring jnewalias) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        std::string newalias = jau::from_jstring_to_string(env, jnewalias);
        adapter->setLocalName(newalias, std::string());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_setDiscoverable(JNIEnv *env, jobject obj, jboolean value) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        return adapter->setDiscoverable(JNI_TRUE == value ? true : false) ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jobject Java_direct_1bt_tinyb_DBTAdapter_connectDeviceImpl(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(SMPLongTermKeyInfo)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        const BDAddressType addressType = static_cast<BDAddressType>( jaddressType );
        std::shared_ptr<DBTDevice> device = adapter->findDiscoveredDevice(address, addressType);
        if( nullptr != device ) {
            direct_bt::HCIHandler & hci = adapter->getHCI();
            if( !hci.isOpen() ) {
                throw BluetoothException("Adapter's HCI closed "+adapter->toString(), E_FILE_LINE);
            }
            std::shared_ptr<jau::JavaAnon> jDeviceRef = device->getJavaObject();
            jau::JavaGlobalObj::check(jDeviceRef, E_FILE_LINE);

            device->connectDefault();
            return jau::JavaGlobalObj::GetObject(jDeviceRef);
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_direct_1bt_tinyb_DBTAdapter_setPairable(JNIEnv *env, jobject obj, jboolean value) {
    try {
        DBTAdapter *adapter = jau::getJavaUplinkObject<DBTAdapter>(env, obj);
        jau::JavaGlobalObj::check(adapter->getJavaObject(), E_FILE_LINE);
        return adapter->setBondable(JNI_TRUE == value ? true : false) ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

void Java_direct_1bt_tinyb_DBTAdapter_setDiscoveryFilter(JNIEnv *env, jobject obj, jobject juuids, jint rssi, jint pathloss, jint transportType) {
    // List<String> uuids
    (void)env;
    (void)obj;
    (void)juuids;
    (void)rssi;
    (void)pathloss;
    (void)transportType;
}

