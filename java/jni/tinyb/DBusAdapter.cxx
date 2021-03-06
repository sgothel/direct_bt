/*
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

#include "tinyb/BluetoothAdapter.hpp"
#include "tinyb/BluetoothDevice.hpp"
#include "tinyb/BluetoothObject.hpp"

#include "tinyb_dbus_DBusAdapter.h"

#include "helper_tinyb.hpp"

using namespace tinyb;
using namespace jau;

jobject Java_tinyb_dbus_DBusAdapter_getBluetoothType(JNIEnv *env, jobject obj)
{
    try {
        (void)obj;

        return get_bluetooth_type(env, "ADAPTER");
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jobject Java_tinyb_dbus_DBusAdapter_clone(JNIEnv *env, jobject obj)
{
    try {
        return generic_clone<BluetoothAdapter>(env, obj);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jboolean Java_tinyb_dbus_DBusAdapter_startDiscovery(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->start_discovery() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_tinyb_dbus_DBusAdapter_stopDiscoveryImpl(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->stop_discovery() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jobject Java_tinyb_dbus_DBusAdapter_getDiscoveredDevices(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::vector<std::unique_ptr<BluetoothDevice>> array = obj_adapter->get_devices();
        jobject result = convert_vector_uniqueptr_to_jarraylist<std::vector<std::unique_ptr<BluetoothDevice>>, BluetoothDevice>(
                env, array, "(J)V");

        return result;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jint Java_tinyb_dbus_DBusAdapter_removeDiscoveredDevices(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::vector<std::unique_ptr<tinyb::BluetoothDevice>> array = obj_adapter->get_devices();
        
        for (unsigned int i =0;i<array.size();i++) {
            std::unique_ptr<tinyb::BluetoothDevice> *obj_device = &array.at(i);
            std::string path = obj_device->get()->get_object_path();
            // printf("PATH:%s\n", path.c_str());
            // fflush(stdout);
            obj_adapter->remove_device(path.c_str());
            
        }
        return array.size();
        
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jstring Java_tinyb_dbus_DBusAdapter_getAddressString(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::string address = obj_adapter->get_address();

        return env->NewStringUTF((const char *)address.c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_tinyb_dbus_DBusAdapter_getName(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::string name = obj_adapter->get_name();

        return env->NewStringUTF((const char *)name.c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_tinyb_dbus_DBusAdapter_getAlias(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::string alias = obj_adapter->get_alias();

        return env->NewStringUTF((const char *)alias.c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_tinyb_dbus_DBusAdapter_setAlias(JNIEnv *env, jobject obj, jstring str)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        const std::string string_to_write = from_jstring_to_string(env, str);

        obj_adapter->set_alias(string_to_write);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jlong Java_tinyb_dbus_DBusAdapter_getBluetoothClass(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return (jlong)obj_adapter->get_class();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_tinyb_dbus_DBusAdapter_getPoweredState(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->get_powered() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_tinyb_dbus_DBusAdapter_setPowered(JNIEnv *env, jobject obj, jboolean val)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        bool val_to_write = from_jboolean_to_bool(val);
        obj_adapter->set_powered(val_to_write);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

void Java_tinyb_dbus_DBusAdapter_enablePoweredNotifications(JNIEnv *env, jobject obj, jobject callback)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        std::shared_ptr<JNIGlobalRef> callback_ptr(new JNIGlobalRef(callback));
        obj_adapter->enable_powered_notifications([ callback_ptr ] (bool v)
            {
                jclass notification = search_class(*jni_env, **callback_ptr);
                jmethodID  method = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
                jni_env->DeleteLocalRef(notification);

                jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
                jmethodID constructor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);

                jobject result = jni_env->NewObject(boolean_cls, constructor, v ? JNI_TRUE : JNI_FALSE);
                jni_env->DeleteLocalRef(boolean_cls);

                jni_env->CallVoidMethod(**callback_ptr, method, result);
                jni_env->DeleteLocalRef(result);

            });
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_tinyb_dbus_DBusAdapter_disablePoweredNotifications(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        obj_adapter->disable_powered_notifications();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean Java_tinyb_dbus_DBusAdapter_getDiscoverable(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->get_discoverable() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_tinyb_dbus_DBusAdapter_setDiscoverable(JNIEnv *env, jobject obj, jboolean val)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        bool val_to_write = from_jboolean_to_bool(val);
        obj_adapter->set_discoverable(val_to_write);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

void Java_tinyb_dbus_DBusAdapter_enableDiscoverableNotifications(JNIEnv *env, jobject obj, jobject callback)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        std::shared_ptr<JNIGlobalRef> callback_ptr(new JNIGlobalRef(callback));
        obj_adapter->enable_discoverable_notifications([ callback_ptr ] (bool v)
            {
                jclass notification = search_class(*jni_env, **callback_ptr);
                jmethodID  method = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
                jni_env->DeleteLocalRef(notification);

                jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
                jmethodID constructor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);

                jobject result = jni_env->NewObject(boolean_cls, constructor, v ? JNI_TRUE : JNI_FALSE);
                jni_env->DeleteLocalRef(boolean_cls);

                jni_env->CallVoidMethod(**callback_ptr, method, result);
                jni_env->DeleteLocalRef(result);

            });
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_tinyb_dbus_DBusAdapter_disableDiscoverableNotifications(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        obj_adapter->disable_discoverable_notifications();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jlong Java_tinyb_dbus_DBusAdapter_getDiscoverableTimeout(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return (jlong)obj_adapter->get_discoverable_timeout();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_tinyb_dbus_DBusAdapter_setDiscoverableTimout(JNIEnv *env, jobject obj, jlong timeout)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        if (timeout < 0)
        {
            throw std::invalid_argument("timeout argument is negative\n");
        }
        obj_adapter->set_discoverable_timeout((unsigned int)timeout);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_tinyb_dbus_DBusAdapter_getPairable(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->get_pairable() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

void Java_tinyb_dbus_DBusAdapter_enablePairableNotifications(JNIEnv *env, jobject obj, jobject callback)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        std::shared_ptr<JNIGlobalRef> callback_ptr(new JNIGlobalRef(callback));
        obj_adapter->enable_pairable_notifications([ callback_ptr ] (bool v)
            {
                jclass notification = search_class(*jni_env, **callback_ptr);
                jmethodID  method = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
                jni_env->DeleteLocalRef(notification);

                jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
                jmethodID constructor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);

                jobject result = jni_env->NewObject(boolean_cls, constructor, v ? JNI_TRUE : JNI_FALSE);
                jni_env->DeleteLocalRef(boolean_cls);

                jni_env->CallVoidMethod(**callback_ptr, method, result);
                jni_env->DeleteLocalRef(result);

            });
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_tinyb_dbus_DBusAdapter_disablePairableNotifications(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        obj_adapter->disable_pairable_notifications();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jboolean Java_tinyb_dbus_DBusAdapter_setPairable(JNIEnv *env, jobject obj, jboolean val)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        bool val_to_write = from_jboolean_to_bool(val);
        obj_adapter->set_pairable(val_to_write);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jlong Java_tinyb_dbus_DBusAdapter_getPairableTimeout(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return (jlong)obj_adapter->get_pairable_timeout();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jboolean Java_tinyb_dbus_DBusAdapter_setPairableTimeout(JNIEnv *env, jobject obj, jlong timeout)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        if (timeout < 0)
        {
            throw std::invalid_argument("timeout argument is negative\n");
        }
        obj_adapter->set_pairable_timeout((unsigned int)timeout);
        return JNI_TRUE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

jboolean Java_tinyb_dbus_DBusAdapter_getDiscovering(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        return obj_adapter->get_discovering() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return JNI_FALSE;
}

void Java_tinyb_dbus_DBusAdapter_enableDiscoveringNotifications(JNIEnv *env, jobject obj, jobject callback)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        std::shared_ptr<JNIGlobalRef> callback_ptr(new JNIGlobalRef(callback));
        obj_adapter->enable_discovering_notifications([ callback_ptr ] (bool v)
            {
                jclass notification = search_class(*jni_env, **callback_ptr);
                jmethodID  method = search_method(*jni_env, notification, "run", "(Ljava/lang/Object;)V", false);
                jni_env->DeleteLocalRef(notification);

                jclass boolean_cls = search_class(*jni_env, "java/lang/Boolean");
                jmethodID constructor = search_method(*jni_env, boolean_cls, "<init>", "(Z)V", false);

                jobject result = jni_env->NewObject(boolean_cls, constructor, v ? JNI_TRUE : JNI_FALSE);
                jni_env->DeleteLocalRef(boolean_cls);

                jni_env->CallVoidMethod(**callback_ptr, method, result);
                jni_env->DeleteLocalRef(result);

            });
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_tinyb_dbus_DBusAdapter_disableDiscoveringNotifications(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter =
                                    getInstance<BluetoothAdapter>(env, obj);
        obj_adapter->disable_discovering_notifications();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jobjectArray Java_tinyb_dbus_DBusAdapter_getUUIDs(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::vector<std::string> uuids = obj_adapter->get_uuids();
        unsigned int uuids_size = uuids.size();

        jclass string_class = search_class(env, "Ljava/lang/String;");
        jobjectArray result = env->NewObjectArray(uuids_size, string_class, 0);
        if (!result)
        {
            throw std::bad_alloc();
        }

        for (unsigned int i = 0; i < uuids_size; ++i)
        {
            std::string str_elem = uuids.at(i);
            jobject elem = env->NewStringUTF((const char *)str_elem.c_str());
            env->SetObjectArrayElement(result, i, elem);
        }

        return result;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jstring Java_tinyb_dbus_DBusAdapter_getModalias(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);
        std::unique_ptr<std::string> modalias = obj_adapter->get_modalias();
        if(modalias == nullptr)
            return nullptr;

        return env->NewStringUTF((const char *)modalias->c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

void Java_tinyb_dbus_DBusAdapter_delete(JNIEnv *env, jobject obj)
{
    try {
        BluetoothAdapter *adapter = getInstance<BluetoothAdapter>(env, obj);
        delete adapter;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_tinyb_dbus_DBusAdapter_setDiscoveryFilter(JNIEnv *env, jobject obj, jobject uuids, jint rssi, jint pathloss, jint transportType)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        jclass cList = env->FindClass("java/util/List");

        jmethodID mSize = env->GetMethodID(cList, "size", "()I");
        jmethodID mGet = env->GetMethodID(cList, "get", "(I)Ljava/lang/Object;");

        jint size = env->CallIntMethod(uuids, mSize);
        std::vector<BluetoothUUID> native_uuids;

        for (jint i = 0; i < size; i++) {
            jstring strObj = (jstring) env->CallObjectMethod(uuids, mGet, i);
            const char * chr = env->GetStringUTFChars(strObj, NULL);
            BluetoothUUID uuid(chr);
            native_uuids.push_back(uuid);
            env->ReleaseStringUTFChars(strObj, chr);
        }

        TransportType t_type = from_int_to_transport_type((int) transportType);

        obj_adapter->set_discovery_filter(native_uuids, (int16_t) rssi, (uint16_t) pathloss, t_type);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jobject Java_tinyb_dbus_DBusAdapter_connectDeviceImpl(JNIEnv *env, jobject obj, jstring jaddress, jstring jaddressType)
{
    try {
        BluetoothAdapter *obj_adapter = getInstance<BluetoothAdapter>(env, obj);

        const std::string address = from_jstring_to_string(env, jaddress);
        const std::string addressType = from_jstring_to_string(env, jaddressType);

        fprintf(stderr, "connectDeviceJ.0\n"); fflush(stderr);
        std::unique_ptr<tinyb::BluetoothDevice> b_device = obj_adapter->connect_device(address, addressType);
        fprintf(stderr, "connectDeviceJ.1\n"); fflush(stderr);

        BluetoothDevice *b_device_naked = b_device.release();
        fprintf(stderr, "connectDeviceJ.2\n"); fflush(stderr);
        if (!b_device_naked)
        {
            return nullptr;
        }
        jclass clazz = search_class(env, *b_device_naked);
        jmethodID clazz_ctor = search_method(env, clazz, "<init>", "(J)V", false);

        jobject result = env->NewObject(clazz, clazz_ctor, (jlong)b_device_naked);
        fprintf(stderr, "connectDeviceJ.X\n"); fflush(stderr);
        return result;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return NULL;
}

