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

#include "jau_direct_bt_DBTGattService.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/BTAdapter.hpp"

using namespace direct_bt;
using namespace jau;

jstring Java_jau_direct_1bt_DBTGattService_toStringImpl(JNIEnv *env, jobject obj) {
    try {
        BTGattService *nativePtr = getJavaUplinkObject<BTGattService>(env, obj);
        JavaGlobalObj::check(nativePtr->getJavaObject(), E_FILE_LINE);
        return from_string_to_jstring(env, nativePtr->toString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}


void Java_jau_direct_1bt_DBTGattService_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance) {
    (void)obj;
    try {
        BTGattService *service = castInstance<BTGattService>(nativeInstance);
        (void)service;
        // No delete: Service instance owned by BTDevice
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

static const std::string _characteristicClazzCtorArgs("(JLjau/direct_bt/DBTGattService;S[Ljava/lang/String;ZZLjava/lang/String;SI)V");

jobject Java_jau_direct_1bt_DBTGattService_getCharsImpl(JNIEnv *env, jobject obj) {
    try {
        BTGattService *service = getJavaUplinkObject<BTGattService>(env, obj);
        JavaGlobalObj::check(service->getJavaObject(), E_FILE_LINE);

        jau::darray<std::shared_ptr<BTGattChar>> & characteristics = service->characteristicList;

        // BTGattChar(final long nativeInstance, final BTGattService service,
        //                       final short handle, final String[] properties,
        //                       final boolean hasNotify, final boolean hasIndicate,
        //                       final String value_type_uuid, final short value_handle,
        //                       final int clientCharacteristicsConfigIndex)

        std::function<jobject(JNIEnv*, jclass, jmethodID, BTGattChar *)> ctor_char =
                [](JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, BTGattChar *characteristic)->jobject {
                    // prepare adapter ctor
                    std::shared_ptr<BTGattService> _service = characteristic->getServiceChecked();
                    JavaGlobalObj::check(_service->getJavaObject(), E_FILE_LINE);
                    jobject jservice = JavaGlobalObj::GetObject(_service->getJavaObject());

                    jau::darray<std::unique_ptr<std::string>> props = BTGattChar::getPropertiesStringList(characteristic->properties);
                    size_t props_size = props.size();

                    jobjectArray jproperties;
                    {
                        jclass string_class = search_class(env_, "java/lang/String");
                        jproperties = env_->NewObjectArray((jsize)props_size, string_class, 0);
                        java_exception_check_and_throw(env_, E_FILE_LINE);
                        env_->DeleteLocalRef(string_class);
                    }
                    for (size_t i = 0; i < props_size; ++i) {
                        jobject elem = from_string_to_jstring(env_, *props[i].get());
                        env_->SetObjectArrayElement(jproperties, (jsize)i, elem);
                        env_->DeleteLocalRef(elem);
                    }
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    const bool hasNotify = characteristic->hasProperties(BTGattChar::PropertyBitVal::Notify);
                    const bool hasIndicate = characteristic->hasProperties(BTGattChar::PropertyBitVal::Indicate);

                    const jstring uuid = from_string_to_jstring(env_,
                            directBTJNISettings.getUnifyUUID128Bit() ? characteristic->value_type->toUUID128String() :
                                                                       characteristic->value_type->toString());
                    java_exception_check_and_throw(env_, E_FILE_LINE);

                    jobject jcharVal = env_->NewObject(clazz, clazz_ctor, (jlong)characteristic, jservice,
                            characteristic->handle, jproperties, hasNotify, hasIndicate,
                            uuid, characteristic->value_handle, characteristic->clientCharConfigIndex);
                    java_exception_check_and_throw(env_, E_FILE_LINE);
                    JNIGlobalRef::check(jcharVal, E_FILE_LINE);
                    std::shared_ptr<JavaAnon> jCharRef = characteristic->getJavaObject(); // GlobalRef
                    JavaGlobalObj::check(jCharRef, E_FILE_LINE);
                    env_->DeleteLocalRef(jproperties);
                    env_->DeleteLocalRef(jcharVal);
                    return JavaGlobalObj::GetObject(jCharRef);
                };
        return convert_vector_sharedptr_to_jarraylist<jau::darray<std::shared_ptr<BTGattChar>>, BTGattChar>(
                env, characteristics, _characteristicClazzCtorArgs.c_str(), ctor_char);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

