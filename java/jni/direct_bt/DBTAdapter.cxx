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

#include "org_direct_bt_AdapterStatusListener.h"
#include "jau_direct_bt_DBTAdapter.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTAdapter.hpp"
#include "direct_bt/BTManager.hpp"

using namespace direct_bt;
using namespace jau::jni;

static const std::string _adapterSettingsClassName("org/direct_bt/AdapterSettings");
static const std::string _adapterSettingsClazzCtorArgs("(I)V");
static const std::string _eirDataTypeSetClassName("org/direct_bt/EIRDataTypeSet");
static const std::string _eirDataTypeSetClazzCtorArgs("(I)V");
static const std::string _hciStatusCodeClassName("org/direct_bt/HCIStatusCode");
static const std::string _hciStatusCodeClazzGetArgs("(B)Lorg/direct_bt/HCIStatusCode;");
static const std::string _scanTypeClassName("org/direct_bt/ScanType");
static const std::string _scanTypeClazzGetArgs("(B)Lorg/direct_bt/ScanType;");
static const std::string _discoveryPolicyClassName("org/direct_bt/DiscoveryPolicy");
static const std::string _discoveryPolicyClazzGetArgs("(B)Lorg/direct_bt/DiscoveryPolicy;");
static const std::string _pairingModeClassName("org/direct_bt/PairingMode");
static const std::string _pairingModeClazzGetArgs("(B)Lorg/direct_bt/PairingMode;");
static const std::string _pairingStateClassName("org/direct_bt/SMPPairingState");
static const std::string _pairingStateClazzGetArgs("(B)Lorg/direct_bt/SMPPairingState;");
static const std::string _deviceClazzCtorArgs("(JLjau/direct_bt/DBTAdapter;[BBJLjava/lang/String;)V");

static const std::string _adapterSettingsChangedMethodArgs("(Lorg/direct_bt/BTAdapter;Lorg/direct_bt/AdapterSettings;Lorg/direct_bt/AdapterSettings;Lorg/direct_bt/AdapterSettings;J)V");
static const std::string _discoveringChangedMethodArgs("(Lorg/direct_bt/BTAdapter;Lorg/direct_bt/ScanType;Lorg/direct_bt/ScanType;ZLorg/direct_bt/DiscoveryPolicy;J)V");
static const std::string _deviceFoundMethodArgs("(Lorg/direct_bt/BTDevice;J)Z");
static const std::string _deviceUpdatedMethodArgs("(Lorg/direct_bt/BTDevice;Lorg/direct_bt/EIRDataTypeSet;J)V");
static const std::string _deviceConnectedMethodArgs("(Lorg/direct_bt/BTDevice;ZJ)V");
static const std::string _devicePairingStateMethodArgs("(Lorg/direct_bt/BTDevice;Lorg/direct_bt/SMPPairingState;Lorg/direct_bt/PairingMode;J)V");
static const std::string _deviceReadyMethodArgs("(Lorg/direct_bt/BTDevice;J)V");
static const std::string _deviceDisconnectedMethodArgs("(Lorg/direct_bt/BTDevice;Lorg/direct_bt/HCIStatusCode;SJ)V");

class JNIAdapterStatusListener : public AdapterStatusListener {
  private:
    /**
        public abstract class AdapterStatusListener {
            public void adapterSettingsChanged(final BluetoothAdapter adapter,
                                               final AdapterSettings oldmask, final AdapterSettings newmask,
                                               final AdapterSettings changedmask, final long timestamp) { }
            public void discoveringChanged(final BluetoothAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled,
                                           final DiscoveryPolicy policy, final long timestamp) { }
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

    JNIGlobalRef adapterSettingsClazzRef;
    jmethodID adapterSettingsClazzCtor;
    JNIGlobalRef eirDataTypeSetClazzRef;
    jmethodID eirDataTypeSetClazzCtor;
    JNIGlobalRef hciStatusCodeClazzRef;
    jmethodID hciStatusCodeClazzGet;
    JNIGlobalRef scanTypeClazzRef;
    jmethodID scanTypeClazzGet;
    JNIGlobalRef discoveryPolicyClazzRef;
    jmethodID discoveryPolicyClazzGet;
    JNIGlobalRef pairingModeClazzRef;
    jmethodID pairingModeClazzGet;
    JNIGlobalRef pairingStateClazzRef;
    jmethodID pairingStateClazzGet;

    JNIGlobalRef deviceClazzRef;
    jmethodID deviceClazzCtor;
    jfieldID deviceClazzTSLastDiscoveryField;
    jfieldID deviceClazzTSLastUpdateField;
    jfieldID deviceClazzConnectionHandleField;
    jmethodID  mAdapterSettingsChanged = nullptr;
    jmethodID  mDiscoveringChanged = nullptr;
    jmethodID  mDeviceFound = nullptr;
    jmethodID  mDeviceUpdated = nullptr;
    jmethodID  mDeviceConnected= nullptr;
    jmethodID  mDevicePairingState= nullptr;
    jmethodID  mDeviceReady = nullptr;
    jmethodID  mDeviceDisconnected = nullptr;

  public:

    std::string toString() const noexcept override {
        return "JNIAdapterStatusListener[this "+jau::to_hexstring(this)+", iname "+std::to_string(iname)+"]";
    }

    // listenerObjRef dtor will call notifyDelete and clears the nativeInstance handle
    ~JNIAdapterStatusListener() override = default;

    JNIAdapterStatusListener(JNIEnv *env, jobject statusListenerObj)
    : iname(iname_next.fetch_add(1))
    {
        jclass listenerClazz = search_class(env, statusListenerObj);

        // adapterSettingsClazzRef, adapterSettingsClazzCtor
        {
            jclass adapterSettingsClazz = search_class(env, _adapterSettingsClassName.c_str());
            adapterSettingsClazzRef = JNIGlobalRef(adapterSettingsClazz);
            env->DeleteLocalRef(adapterSettingsClazz);
        }
        adapterSettingsClazzCtor = search_method(env, adapterSettingsClazzRef.getClass(), "<init>", _adapterSettingsClazzCtorArgs.c_str(), false);

        // eirDataTypeSetClazzRef, eirDataTypeSetClazzCtor
        {
            jclass eirDataTypeSetClazz = search_class(env, _eirDataTypeSetClassName.c_str());
            eirDataTypeSetClazzRef = JNIGlobalRef(eirDataTypeSetClazz);
            env->DeleteLocalRef(eirDataTypeSetClazz);
        }
        eirDataTypeSetClazzCtor = search_method(env, eirDataTypeSetClazzRef.getClass(), "<init>", _eirDataTypeSetClazzCtorArgs.c_str(), false);

        // hciStatusCodeClazzRef, hciStatusCodeClazzGet
        {
            jclass hciErrorCodeClazz = search_class(env, _hciStatusCodeClassName.c_str());
            hciStatusCodeClazzRef = JNIGlobalRef(hciErrorCodeClazz);
            env->DeleteLocalRef(hciErrorCodeClazz);
        }
        hciStatusCodeClazzGet = search_method(env, hciStatusCodeClazzRef.getClass(), "get", _hciStatusCodeClazzGetArgs.c_str(), true);

        // scanTypeClazzRef, scanTypeClazzGet
        {
            jclass scanTypeClazz = search_class(env, _scanTypeClassName.c_str());
            scanTypeClazzRef = JNIGlobalRef(scanTypeClazz);
            env->DeleteLocalRef(scanTypeClazz);
        }
        scanTypeClazzGet = search_method(env, scanTypeClazzRef.getClass(), "get", _scanTypeClazzGetArgs.c_str(), true);

        // discoveryPolicyClazzRef, discoveryPolicyClazzGet;
        {
            jclass discoveryPolicyClazz = search_class(env, _discoveryPolicyClassName.c_str());
            discoveryPolicyClazzRef = JNIGlobalRef(discoveryPolicyClazz);
            env->DeleteLocalRef(discoveryPolicyClazz);
        }
        discoveryPolicyClazzGet = search_method(env, discoveryPolicyClazzRef.getClass(), "get", _discoveryPolicyClazzGetArgs.c_str(), true);

        // pairingModeClazzRef, pairingModeClazzGet
        {
            jclass pairingModeClazz = search_class(env, _pairingModeClassName.c_str());
            pairingModeClazzRef = JNIGlobalRef(pairingModeClazz);
            env->DeleteLocalRef(pairingModeClazz);
        }
        pairingModeClazzGet = search_method(env, pairingModeClazzRef.getClass(), "get", _pairingModeClazzGetArgs.c_str(), true);

        // pairingStateClazzRef, pairingStateClazzGet
        {
            jclass pairingStateClazz = search_class(env, _pairingStateClassName.c_str());
            pairingStateClazzRef = JNIGlobalRef(pairingStateClazz);
            env->DeleteLocalRef(pairingStateClazz);
        }
        pairingStateClazzGet = search_method(env, pairingStateClazzRef.getClass(), "get", _pairingStateClazzGetArgs.c_str(), true);

        // deviceClazzRef, deviceClazzCtor
        {
            jclass deviceClazz = search_class(env, BTDevice::java_class().c_str());
            deviceClazzRef = JNIGlobalRef(deviceClazz);
            env->DeleteLocalRef(deviceClazz);
        }
        deviceClazzCtor = search_method(env, deviceClazzRef.getClass(), "<init>", _deviceClazzCtorArgs.c_str(), false);

        deviceClazzTSLastDiscoveryField = search_field(env, deviceClazzRef.getClass(), "ts_last_discovery", "J", false);
        deviceClazzTSLastUpdateField = search_field(env, deviceClazzRef.getClass(), "ts_last_update", "J", false);
        deviceClazzConnectionHandleField = search_field(env, deviceClazzRef.getClass(), "hciConnHandle", "S", false);

        mAdapterSettingsChanged = search_method(env, listenerClazz, "adapterSettingsChanged", _adapterSettingsChangedMethodArgs.c_str(), false);
        mDiscoveringChanged = search_method(env, listenerClazz, "discoveringChanged", _discoveringChangedMethodArgs.c_str(), false);
        mDeviceFound = search_method(env, listenerClazz, "deviceFound", _deviceFoundMethodArgs.c_str(), false);
        mDeviceUpdated = search_method(env, listenerClazz, "deviceUpdated", _deviceUpdatedMethodArgs.c_str(), false);
        mDeviceConnected = search_method(env, listenerClazz, "deviceConnected", _deviceConnectedMethodArgs.c_str(), false);
        mDevicePairingState = search_method(env, listenerClazz, "devicePairingState", _devicePairingStateMethodArgs.c_str(), false);
        mDeviceReady = search_method(env, listenerClazz, "deviceReady", _deviceReadyMethodArgs.c_str(), false);
        mDeviceDisconnected = search_method(env, listenerClazz, "deviceDisconnected", _deviceDisconnectedMethodArgs.c_str(), false);
    }

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        (void)a;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef adapter_java = a.getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        jobject adapterSettingOld = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)oldmask);
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingOld, E_FILE_LINE);

        jobject adapterSettingNew = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)newmask);
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingNew, E_FILE_LINE);

        jobject adapterSettingChanged = env->NewObject(adapterSettingsClazzRef.getClass(), adapterSettingsClazzCtor,  (jint)changedmask);
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(adapterSettingChanged, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mAdapterSettingsChanged,
                            JavaGlobalObj::GetObject(adapter_java), adapterSettingOld, adapterSettingNew, adapterSettingChanged, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(adapterSettingOld);
        env->DeleteLocalRef(adapterSettingNew);
        env->DeleteLocalRef(adapterSettingChanged);
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        (void)a;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef adapter_java = a.getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        jobject jcurrentMeta = env->CallStaticObjectMethod(scanTypeClazzRef.getClass(), scanTypeClazzGet, (jbyte)number(currentMeta));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jcurrentMeta, E_FILE_LINE);

        jobject jchangedType = env->CallStaticObjectMethod(scanTypeClazzRef.getClass(), scanTypeClazzGet, (jbyte)number(changedType));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jchangedType, E_FILE_LINE);

        jobject jdiscoveryPolicy = env->CallStaticObjectMethod(discoveryPolicyClazzRef.getClass(), discoveryPolicyClazzGet, (jbyte)number(policy));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jdiscoveryPolicy, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDiscoveringChanged,
                            JavaGlobalObj::GetObject(adapter_java),
                            jcurrentMeta, jchangedType, (jboolean)changedEnabled, jdiscoveryPolicy, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }

  private:

    jobject newJavaBTDevice(JNIEnv *env, const BTDeviceRef& device, const uint64_t timestamp) {
        // DBTDevice(final long nativeInstance, final DBTAdapter adptr, final byte byteAddress[/*6*/], final byte byteAddressType,
        //           final long ts_creation, final String name)
        JavaAnonRef adapter_java = device->getAdapter().getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        const EUI48 & addr = device->getAddressAndType().address;
        jbyteArray jaddr = env->NewByteArray(sizeof(addr));
        env->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
        java_exception_check_and_throw(env, E_FILE_LINE);
        const jstring name = from_string_to_jstring(env, device->getName());
        java_exception_check_and_throw(env, E_FILE_LINE);
        shared_ptr_ref<BTDevice> device_sref(device); // new instance to be released into new jobject
        jobject tmp_jdevice = env->NewObject(deviceClazzRef.getClass(), deviceClazzCtor,
                device_sref.release_to_jlong(), JavaGlobalObj::GetObject(adapter_java),
                jaddr, device->getAddressAndType().type, (jlong)timestamp, name);
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(tmp_jdevice, E_FILE_LINE);
        JavaAnonRef jDeviceRef1 = device->getJavaObject();
        JavaGlobalObj::check(jDeviceRef1, E_FILE_LINE);
        jobject jdevice = JavaGlobalObj::GetObject(jDeviceRef1);
        env->DeleteLocalRef(jaddr);
        env->DeleteLocalRef(name);
        env->DeleteLocalRef(tmp_jdevice);
        return jdevice;
    }

  public:

    bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        jobject jdevice;
        JavaAnonRef device_java = device->getJavaObject();
        if( JavaGlobalObj::isValid(device_java) ) {
            // Reuse Java instance
            jdevice = JavaGlobalObj::GetObject(device_java);
        } else {
            jdevice = newJavaBTDevice(env, device, timestamp);
        }
        env->SetLongField(jdevice, deviceClazzTSLastDiscoveryField, (jlong)device->getLastDiscoveryTimestamp());
        java_exception_check_and_throw(env, E_FILE_LINE);
        jboolean res = env->CallBooleanMethod(JavaGlobalObj::GetObject(asl_java), mDeviceFound, jdevice, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
        return JNI_TRUE == res;
    }

    void deviceUpdated(const BTDeviceRef& device, const EIRDataType updateMask, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef device_java = device->getJavaObject();
        if( !JavaGlobalObj::isValid(device_java) ) {
            return; // java device has been pulled
        }
        env->SetLongField(JavaGlobalObj::GetObject(device_java), deviceClazzTSLastUpdateField, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);

        jobject eirDataTypeSet = env->NewObject(eirDataTypeSetClazzRef.getClass(), eirDataTypeSetClazzCtor, (jint)updateMask);
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(eirDataTypeSet, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDeviceUpdated, JavaGlobalObj::GetObject(device_java), eirDataTypeSet, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->DeleteLocalRef(eirDataTypeSet);
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        jobject jdevice;
        JavaAnonRef device_java = device->getJavaObject();
        if( JavaGlobalObj::isValid(device_java) ) {
            // Reuse Java instance
            jdevice = JavaGlobalObj::GetObject(device_java);
        } else {
            jdevice = newJavaBTDevice(env, device, timestamp);
        }
        env->SetShortField(jdevice, deviceClazzConnectionHandleField, (jshort)device->getConnectionHandle());
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->SetLongField(jdevice, deviceClazzTSLastDiscoveryField, (jlong)device->getLastDiscoveryTimestamp());
        java_exception_check_and_throw(env, E_FILE_LINE);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDeviceConnected, jdevice, (jboolean)discovered, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef device_java = device->getJavaObject();
        if( !JavaGlobalObj::isValid(device_java) ) {
            return; // java device has been pulled
        }

        jobject jdevice = JavaGlobalObj::GetObject(device_java);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);

        jobject jstate = env->CallStaticObjectMethod(pairingStateClazzRef.getClass(), pairingStateClazzGet, static_cast<uint8_t>(state));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jstate, E_FILE_LINE);

        jobject jmode = env->CallStaticObjectMethod(pairingModeClazzRef.getClass(), pairingModeClazzGet, static_cast<uint8_t>(mode));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(jmode, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDevicePairingState, jdevice, jstate, jmode, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void deviceReady(const BTDeviceRef& device, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef device_java = device->getJavaObject();
        if( !JavaGlobalObj::isValid(device_java) ) {
            return; // java device has been pulled
        }

        jobject jdevice = JavaGlobalObj::GetObject(device_java);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDeviceReady, jdevice, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }
    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        JNIEnv *env = *jni_env;
        JavaAnonRef asl_java = getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        JavaAnonRef device_java = device->getJavaObject();
        if( !JavaGlobalObj::isValid(device_java) ) {
            return; // java device has been pulled
        }

        jobject jdevice = JavaGlobalObj::GetObject(device_java);
        env->SetLongField(jdevice, deviceClazzTSLastUpdateField, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);

        jobject hciErrorCode = env->CallStaticObjectMethod(hciStatusCodeClazzRef.getClass(), hciStatusCodeClazzGet, (jbyte)static_cast<uint8_t>(reason));
        java_exception_check_and_throw(env, E_FILE_LINE);
        JNIGlobalRef::check(hciErrorCode, E_FILE_LINE);

        env->SetShortField(jdevice, deviceClazzConnectionHandleField, (jshort)0); // zero out, disconnected
        java_exception_check_and_throw(env, E_FILE_LINE);

        env->CallVoidMethod(JavaGlobalObj::GetObject(asl_java), mDeviceDisconnected, jdevice, hciErrorCode, (jshort)handle, (jlong)timestamp);
        java_exception_check_and_throw(env, E_FILE_LINE);
    }
};
std::atomic<int> JNIAdapterStatusListener::iname_next(0);

/*
 * Class:     org_direct_bt_AdapterStatusListener
 * Method:    ctorImpl
 * Signature: ()J
 */
jlong Java_org_direct_1bt_AdapterStatusListener_ctorImpl(JNIEnv *env, jobject obj) {
    try {
        // new instance
        shared_ptr_ref<JNIAdapterStatusListener> ref( new JNIAdapterStatusListener(env, obj) );

        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t) nullptr;
}


/*
 * Class:     org_direct_bt_AdapterStatusListener
 * Method:    deleteImpl
 * Signature: (J)V
 */
void Java_org_direct_1bt_AdapterStatusListener_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance) {
    (void)obj;
    try {
        shared_ptr_ref<JNIAdapterStatusListener> sref(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != sref.pointer() ) {
            std::shared_ptr<JNIAdapterStatusListener>* sref_ptr = castInstance<JNIAdapterStatusListener>(nativeInstance);
            delete sref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTAdapter_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        shared_ptr_ref<BTAdapter> adapter(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != adapter.pointer() ) {
            if( !adapter.is_null() ) {
                DBG_PRINT("Java_jau_direct_1bt_DBTAdapter_deleteImpl (w/ close) %s", adapter->toString().c_str());
                adapter->close();
            } else {
                DBG_PRINT("Java_jau_direct_1bt_DBTAdapter_deleteImpl null reference");
            }
            std::shared_ptr<BTAdapter>* ref_ptr = castInstance<BTAdapter>(nativeInstance);
            delete ref_ptr;
        } else {
            DBG_PRINT("Java_jau_direct_1bt_DBTAdapter_deleteImpl null reference store");
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean Java_jau_direct_1bt_DBTAdapter_addStatusListenerImpl(JNIEnv *env, jobject obj, jobject jdeviceOwnerAndMatch, jobject jstatusListener)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<JNIAdapterStatusListener> asl(env, jstatusListener); // hold until done
        JavaAnonRef asl_java = asl->getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        shared_ptr_ref<BTDevice> deviceOwnerAndMatchRef(env, jdeviceOwnerAndMatch, false /* throw_on_nullptr */);
        if( !deviceOwnerAndMatchRef.is_null() ) {
            JavaGlobalObj::check(deviceOwnerAndMatchRef->getJavaObject(), E_FILE_LINE);
        }

        bool addRes;
        if( !deviceOwnerAndMatchRef.is_null() ) {
            addRes = adapter->addStatusListener( deviceOwnerAndMatchRef.shared_ptr(), asl.shared_ptr() );
        } else {
            addRes = adapter->addStatusListener( asl.shared_ptr() );
        }
        if( addRes ) {
            return JNI_TRUE;
        }
        ERR_PRINT("BTAdapter::addStatusListener: FAILED: %s", asl->toString().c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTAdapter_removeStatusListenerImpl(JNIEnv *env, jobject obj, jobject jstatusListener)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<JNIAdapterStatusListener> asl(env, jstatusListener); // hold until done
        JavaAnonRef asl_java = asl->getJavaObject(); // hold until done!
        JavaGlobalObj::check(asl_java, E_FILE_LINE);

        if( ! adapter->removeStatusListener( asl.shared_ptr() ) ) {
            WARN_PRINT("Failed to remove statusListener with nativeInstance: %p at %s", asl.shared_ptr().get(), adapter->toString().c_str());
            return false;
        }
        return true;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jint Java_jau_direct_1bt_DBTAdapter_removeAllStatusListenerImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);
        return (jint)adapter->removeAllStatusListener();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_jau_direct_1bt_DBTAdapter_isDeviceWhitelisted(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
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
jboolean Java_jau_direct_1bt_DBTAdapter_addDeviceToWhitelistImpl1(JNIEnv *env, jobject obj,
                                                                    jbyteArray jaddress, jbyte jaddressType, int jctype,
                                                                    jshort min_interval, jshort max_interval,
                                                                    jshort latency, jshort timeout) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
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
jboolean Java_jau_direct_1bt_DBTAdapter_addDeviceToWhitelistImpl2(JNIEnv *env, jobject obj,
                                                                    jbyteArray jaddress, jbyte jaddressType, int jctype) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
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
jboolean Java_jau_direct_1bt_DBTAdapter_removeDeviceFromWhitelistImpl(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
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

jstring Java_jau_direct_1bt_DBTAdapter_toStringImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return from_string_to_jstring(env, adapter->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jint Java_jau_direct_1bt_DBTAdapter_getBTMajorVersion(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jint) adapter->getBTMajorVersion();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_jau_direct_1bt_DBTAdapter_isPoweredImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->isPowered();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTAdapter_isSuspendedImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->isSuspended();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_jau_direct_1bt_DBTAdapter_isValidImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->isValid();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jlong Java_jau_direct_1bt_DBTAdapter_getLEFeaturesImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jlong) number( adapter->getLEFeatures() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTAdapter_startDiscoveryImpl(JNIEnv *env, jobject obj, jobject jgattServerData,
                                                        jbyte policy, jboolean le_scan_active,
                                                        jshort le_scan_interval, jshort le_scan_window,
                                                        jbyte filter_policy,
                                                        jboolean filter_dup)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<DBGattServer> gatt_server_ref(env, jgattServerData, false /** throw_on_null */); // hold until done
        gatt_server_ref.null_check1();

        return (jbyte) number( adapter->startDiscovery(gatt_server_ref.shared_ptr(),
                                    static_cast<DiscoveryPolicy>(policy), le_scan_active,
                                    le_scan_interval, le_scan_window, filter_policy, filter_dup==JNI_TRUE) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_stopDiscoveryImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jbyte) number( adapter->stopDiscovery() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_getCurrentDiscoveryPolicyImpl(JNIEnv *env, jobject obj) {
    DiscoveryPolicy current = DiscoveryPolicy::AUTO_OFF;
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        current = adapter->getCurrentDiscoveryPolicy();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte)number(current);
}

jboolean Java_jau_direct_1bt_DBTAdapter_removeDevicePausingDiscovery(JNIEnv *env, jobject obj, jobject jdevice) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<BTDevice> device(env, jdevice); // hold until done
        JavaAnonRef device_java = device->getJavaObject(); // hold until done!
        JavaGlobalObj::check(device_java, E_FILE_LINE);

        return adapter->removeDevicePausingDiscovery(*device) ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTAdapter_getRoleImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jbyte) number( adapter->getRole() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number( BTRole::None );
}

jbyte Java_jau_direct_1bt_DBTAdapter_getBTModeImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jbyte) number( adapter->getBTMode() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number( BTMode::NONE );
}

jobject Java_jau_direct_1bt_DBTAdapter_getDiscoveredDevicesImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        jau::darray<BTDeviceRef> array = adapter->getDiscoveredDevices();
        return convert_vector_sharedptr_to_jarraylist(env, array);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jint Java_jau_direct_1bt_DBTAdapter_removeDiscoveredDevicesImpl1(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return (jint) adapter->removeDiscoveredDevices();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_jau_direct_1bt_DBTAdapter_removeDiscoveredDeviceImpl1(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType)
{
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);
        const BDAddressAndType addressAndType(address, static_cast<BDAddressType>( jaddressType ));

        return adapter->removeDiscoveredDevice(addressAndType);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

//
// misc
//

jboolean Java_jau_direct_1bt_DBTAdapter_setPowered(JNIEnv *env, jobject obj, jboolean power_on) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->setPowered(JNI_TRUE == power_on ? true : false) ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTAdapter_setPrivacyImpl(JNIEnv *env, jobject obj, jboolean enable) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        HCIStatusCode res = adapter->setPrivacy(JNI_TRUE == enable ? true : false);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jboolean Java_jau_direct_1bt_DBTAdapter_getSecureConnectionsEnabled(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->getSecureConnectionsEnabled() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTAdapter_setSecureConnectionsImpl(JNIEnv *env, jobject obj, jboolean enable) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        HCIStatusCode res = adapter->setSecureConnections(JNI_TRUE == enable ? true : false);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_setDefaultConnParamImpl(JNIEnv *env, jobject obj,
                                                             jshort conn_interval_min, jshort conn_interval_max,
                                                             jshort conn_latency, jshort supervision_timeout) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        HCIStatusCode res = adapter->setDefaultConnParam(static_cast<uint16_t>(conn_interval_min),
                                                         static_cast<uint16_t>(conn_interval_max),
                                                         static_cast<uint16_t>(conn_latency),
                                                         static_cast<uint16_t>(supervision_timeout));
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

void Java_jau_direct_1bt_DBTAdapter_setServerConnSecurityImpl(JNIEnv *env, jobject obj, jbyte jsec_level, jbyte jio_cap) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        adapter->setServerConnSecurity( to_BTSecurityLevel( static_cast<uint8_t>(jsec_level) ),
                                        to_SMPIOCapability( static_cast<uint8_t>(jio_cap) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_jau_direct_1bt_DBTAdapter_setSMPKeyPath(JNIEnv *env, jobject obj, jstring jpath) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        std::string path = from_jstring_to_string(env, jpath);
        adapter->setSMPKeyPath(path);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jbyte Java_jau_direct_1bt_DBTAdapter_initializeImpl(JNIEnv *env, jobject obj, jbyte jbtMode, jboolean powerOn) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        const BTMode btMode = static_cast<BTMode>(jbtMode);
        HCIStatusCode res = adapter->initialize(btMode, JNI_TRUE == powerOn);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jboolean Java_jau_direct_1bt_DBTAdapter_isInitialized(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->isInitialized();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jbyte Java_jau_direct_1bt_DBTAdapter_resetImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        HCIStatusCode res = adapter->reset();
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_setDefaultLE_1PHYImpl(JNIEnv *env, jobject obj,
                                                           jbyte jTx, jbyte jRx) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        const LE_PHYs Tx = static_cast<LE_PHYs>(jTx);
        const LE_PHYs Rx = static_cast<LE_PHYs>(jRx);

        return (jbyte)number ( adapter->setDefaultLE_PHY(Tx, Rx) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jstring Java_jau_direct_1bt_DBTAdapter_getNameImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return from_string_to_jstring(env, adapter->getName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_jau_direct_1bt_DBTAdapter_getShortNameImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return from_string_to_jstring(env, adapter->getShortName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jbyte Java_jau_direct_1bt_DBTAdapter_setNameImpl(JNIEnv *env, jobject obj, jstring jname, jstring jshort_name) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        std::string name = from_jstring_to_string(env, jname);
        std::string short_name = from_jstring_to_string(env, jshort_name);
        return (jbyte) number( adapter->setName(name, short_name) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jobject Java_jau_direct_1bt_DBTAdapter_connectDeviceImpl(JNIEnv *env, jobject obj, jbyteArray jaddress, jbyte jaddressType) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentError("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentError("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( nullptr == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        const BDAddressType addressType = static_cast<BDAddressType>( jaddressType );
        BTDeviceRef device = adapter->findSharedDevice(address, addressType);
        if( nullptr == device ) {
            device = adapter->findDiscoveredDevice(address, addressType);
        }
        if( nullptr != device ) {
            direct_bt::HCIHandler & hci = adapter->getHCI();
            if( !hci.isOpen() ) {
                throw BTException("Adapter's HCI closed "+adapter->toString(), E_FILE_LINE);
            }
            JavaAnonRef jDeviceRef = device->getJavaObject();
            JavaGlobalObj::check(jDeviceRef, E_FILE_LINE);

            device->connectDefault();
            return JavaGlobalObj::GetObject(jDeviceRef);
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_jau_direct_1bt_DBTAdapter_printDeviceListsImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        adapter->printDeviceLists();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jbyte Java_jau_direct_1bt_DBTAdapter_startAdvertising1Impl(JNIEnv *env, jobject obj,
                                                           jobject jgattServerData,
                                                           jobject jeir,
                                                           jint jadv_mask,
                                                           jint jscanrsp_mask,
                                                           jshort adv_interval_min, jshort adv_interval_max,
                                                           jbyte jadv_type, jbyte adv_chan_map, jbyte filter_policy) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<DBGattServer> gatt_server_ref(env, jgattServerData, false /** throw_on_null */); // hold until done
        gatt_server_ref.null_check1();
        shared_ptr_ref<EInfoReport> eir_ref(env, jeir); // hold until done

        const EIRDataType adv_mask = static_cast<EIRDataType>(jadv_mask);
        const EIRDataType scanrsp_mask = static_cast<EIRDataType>(jscanrsp_mask);
        const AD_PDU_Type adv_type = static_cast<AD_PDU_Type>(jadv_type);
        HCIStatusCode res = adapter->startAdvertising(gatt_server_ref.shared_ptr(), *eir_ref, adv_mask, scanrsp_mask, adv_interval_min, adv_interval_max, adv_type, adv_chan_map, filter_policy);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_startAdvertising2Impl(JNIEnv *env, jobject obj,
                                                           jobject jgattServerData,
                                                           jshort adv_interval_min, jshort adv_interval_max, jbyte jadv_type, jbyte adv_chan_map,
                                                           jbyte filter_policy) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        shared_ptr_ref<DBGattServer> gatt_server_ref(env, jgattServerData, false /** throw_on_null */); // hold until done
        gatt_server_ref.null_check1();

        const AD_PDU_Type adv_type = static_cast<AD_PDU_Type>(jadv_type);
        HCIStatusCode res = adapter->startAdvertising(gatt_server_ref.shared_ptr(), adv_interval_min, adv_interval_max, adv_type, adv_chan_map, filter_policy);
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jbyte Java_jau_direct_1bt_DBTAdapter_stopAdvertisingImpl(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        HCIStatusCode res = adapter->stopAdvertising();
        return (jbyte) number(res);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jbyte) number(HCIStatusCode::INTERNAL_FAILURE);
}

jboolean Java_jau_direct_1bt_DBTAdapter_isAdvertising(JNIEnv *env, jobject obj) {
    try {
        shared_ptr_ref<BTAdapter> adapter(env, obj); // hold until done
        JavaAnonRef adapter_java = adapter->getJavaObject(); // hold until done!
        JavaGlobalObj::check(adapter_java, E_FILE_LINE);

        return adapter->isAdvertising() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

