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

#include "org_direct_bt_EInfoReport.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTTypes0.hpp"

using namespace direct_bt;

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    ctorImpl1
 * Signature: ()J
 */
jlong Java_org_direct_1bt_EInfoReport_ctorImpl1(JNIEnv *env, jobject obj) {
    try {
        (void)obj;
        // new instance
        std::shared_ptr<EInfoReport> ref = std::make_shared<EInfoReport>();

        std::shared_ptr<EInfoReport> * ref_ptr = new std::shared_ptr<EInfoReport>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    ctorImpl2
 * Signature: (J)J
 */
jlong Java_org_direct_1bt_EInfoReport_ctorImpl2(JNIEnv *env, jobject obj, jlong nativeInstanceOther) {
    try {
        (void)obj;
        std::shared_ptr<EInfoReport> * ref_ptr_other = reinterpret_cast<std::shared_ptr<EInfoReport> *>(nativeInstanceOther);
        if (ref_ptr_other == nullptr) {
            throw jau::RuntimeException("Given std::shared_ptr<EInfoReport> * nullptr", E_FILE_LINE);
        }
        if (*ref_ptr_other == nullptr) {
            throw jau::RuntimeException("Given std::shared_ptr<EInfoReport> nullptr", E_FILE_LINE);
        }
        // shared instance with other
        std::shared_ptr<EInfoReport> ref( *ref_ptr_other );

        std::shared_ptr<EInfoReport> * ref_ptr = new std::shared_ptr<EInfoReport>(std::move(ref));
        return (jlong)(intptr_t)ref_ptr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_org_direct_1bt_EInfoReport_replace_nativeImpl(JNIEnv *env, jobject obj, jlong nativeInstanceOther) {
    try {
        std::shared_ptr<EInfoReport> * ref_ptr_other = reinterpret_cast<std::shared_ptr<EInfoReport> *>(nativeInstanceOther);
        if (ref_ptr_other == nullptr) {
            throw jau::RuntimeException("Given std::shared_ptr<EInfoReport> * nullptr", E_FILE_LINE);
        }
        if (*ref_ptr_other == nullptr) {
            throw jau::RuntimeException("Given std::shared_ptr<EInfoReport> nullptr", E_FILE_LINE);
        }
        std::shared_ptr<EInfoReport>* eir_ptr_ref = jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);

        // replace the shared managed object
        *eir_ptr_ref = *ref_ptr_other;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    dtorImpl
 * Signature: (J)V
 */
void Java_org_direct_1bt_EInfoReport_dtorImpl(JNIEnv *env, jclass clazz, jlong nativeInstance) {
    (void)clazz;
    try {
        if( 0 != nativeInstance ) {
            std::shared_ptr<EInfoReport> * ref_ptr = reinterpret_cast<std::shared_ptr<EInfoReport> *>(nativeInstance);
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_org_direct_1bt_EInfoReport_clear(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->clear();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jint Java_org_direct_1bt_EInfoReport_setImpl(JNIEnv *env, jobject obj, jobject jeir_other) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        std::shared_ptr<EInfoReport>& eir_other_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, jeir_other);
        return static_cast<jint>( number( eir_ptr->set(*eir_other_ptr) ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setAddressTypeImpl
 * Signature: (B)V
 */
void Java_org_direct_1bt_EInfoReport_setAddressTypeImpl(JNIEnv *env, jobject obj, jbyte jat) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setAddressType(static_cast<BDAddressType>(jat));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setAddressImpl
 * Signature: ([B)V
 */
void Java_org_direct_1bt_EInfoReport_setAddressImpl(JNIEnv *env, jobject obj, jbyteArray jaddress) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);

        if( nullptr == jaddress ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t address_size = env->GetArrayLength(jaddress);
        if( sizeof(EUI48) > address_size ) {
            throw jau::IllegalArgumentException("address byte size "+std::to_string(address_size)+" < "+std::to_string(sizeof(EUI48)), E_FILE_LINE);
        }
        JNICriticalArray<uint8_t, jbyteArray> criticalArray(env); // RAII - release
        uint8_t * address_ptr = criticalArray.get(jaddress, criticalArray.Mode::NO_UPDATE_AND_RELEASE);
        if( NULL == address_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(address byte array) is null", E_FILE_LINE);
        }
        const EUI48& address = *reinterpret_cast<EUI48 *>(address_ptr);

        eir_ptr->setAddress(address);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setRSSI
 * Signature: (B)V
 */
void Java_org_direct_1bt_EInfoReport_setRSSI(JNIEnv *env, jobject obj, jbyte jrssi) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setRSSI(static_cast<int8_t>(jrssi));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setTxPower
 * Signature: (B)V
 */
void Java_org_direct_1bt_EInfoReport_setTxPower(JNIEnv *env, jobject obj, jbyte jtxp) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setTxPower(static_cast<int8_t>(jtxp));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setFlagsImpl
 * Signature: (B)V
 */
void Java_org_direct_1bt_EInfoReport_setFlagsImpl(JNIEnv *env, jobject obj, jbyte jf) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setFlags(static_cast<GAPFlags>(jf));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    addFlagImpl
 * Signature: (B)V
 */
void Java_org_direct_1bt_EInfoReport_addFlagImpl(JNIEnv *env, jobject obj, jbyte jf) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->addFlags(static_cast<GAPFlags>(jf));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setName
 * Signature: (Ljava/lang/String;)V
 */
void Java_org_direct_1bt_EInfoReport_setName(JNIEnv *env, jobject obj, jstring jname) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        std::string name = jau::from_jstring_to_string(env, jname);
        eir_ptr->setName(name);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setShortName
 * Signature: (Ljava/lang/String;)V
 */
void Java_org_direct_1bt_EInfoReport_setShortName(JNIEnv *env, jobject obj, jstring jsname) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        std::string sname = jau::from_jstring_to_string(env, jsname);
        eir_ptr->setShortName(sname);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    addService
 * Signature: (Ljava/lang/String;)V
 */
void Java_org_direct_1bt_EInfoReport_addService(JNIEnv *env, jobject obj, jstring juuid) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        std::string uuid_s = jau::from_jstring_to_string(env, juuid);
        std::shared_ptr<const jau::uuid_t> uuid = jau::uuid_t::create(uuid_s);
        eir_ptr->addService(uuid);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setServicesComplete
 * Signature: (Z)V
 */
void Java_org_direct_1bt_EInfoReport_setServicesComplete(JNIEnv *env, jobject obj, jboolean jv) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setServicesComplete(JNI_TRUE==jv);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setDeviceClass
 * Signature: (I)V
 */
void Java_org_direct_1bt_EInfoReport_setDeviceClass(JNIEnv *env, jobject obj, jint jv) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setDeviceClass(static_cast<uint32_t>(jv));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setDeviceID
 * Signature: (SSSS)V
 */
void Java_org_direct_1bt_EInfoReport_setDeviceID(JNIEnv *env, jobject obj, jshort jsource, jshort jvendor, jshort jproduct, jshort jversion) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setDeviceID(static_cast<uint16_t>(jsource), static_cast<uint16_t>(jvendor), static_cast<uint16_t>(jproduct), static_cast<uint16_t>(jversion));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    setConnInterval
 * Signature: (SS)V
 */
void Java_org_direct_1bt_EInfoReport_setConnInterval(JNIEnv *env, jobject obj, jshort jmin, jshort jmax) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        eir_ptr->setConnInterval(static_cast<uint16_t>(jmin), static_cast<uint16_t>(jmax));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getTimestamp
 * Signature: ()J
 */
jlong Java_org_direct_1bt_EInfoReport_getTimestamp(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jlong>( eir_ptr->getTimestamp() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getEIRDataMaskImpl
 * Signature: ()I
 */
jint Java_org_direct_1bt_EInfoReport_getEIRDataMaskImpl(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jint>( number( eir_ptr->getEIRDataMask() ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jint Java_org_direct_1bt_EInfoReport_getSourceImpl(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jint>( EInfoReport::number( eir_ptr->getSource() ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return static_cast<jint>( EInfoReport::number( EInfoReport::Source::NA ) );
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getFlagsImpl
 * Signature: ()B
 */
jbyte Java_org_direct_1bt_EInfoReport_getFlagsImpl(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jbyte>( number( eir_ptr->getFlags() ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getADAddressType
 * Signature: ()B
 */
jbyte Java_org_direct_1bt_EInfoReport_getADAddressType(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jbyte>( eir_ptr->getADAddressType() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getAddressTypeImpl
 * Signature: ()B
 */
jbyte Java_org_direct_1bt_EInfoReport_getAddressTypeImpl(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jbyte>( number( eir_ptr->getAddressType() ) );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getAddressImpl
 * Signature: ()[B
 */
jbyteArray Java_org_direct_1bt_EInfoReport_getAddressImpl(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        const EUI48 & addr = eir_ptr->getAddress();
        jbyteArray jaddr = env->NewByteArray(sizeof(addr));
        env->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
        return jaddr;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
jstring Java_org_direct_1bt_EInfoReport_getName(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return jau::from_string_to_jstring(env, eir_ptr->getName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getShortName
 * Signature: ()Ljava/lang/String;
 */
jstring Java_org_direct_1bt_EInfoReport_getShortName(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return jau::from_string_to_jstring(env, eir_ptr->getShortName());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getRSSI
 * Signature: ()B
 */
jbyte Java_org_direct_1bt_EInfoReport_getRSSI(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jbyte>( eir_ptr->getRSSI() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getTxPower
 * Signature: ()B
 */
jbyte Java_org_direct_1bt_EInfoReport_getTxPower(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jbyte>( eir_ptr->getTxPower() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

jobject Java_org_direct_1bt_EInfoReport_getManufacturerData(JNIEnv *env, jobject obj)
{
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        std::shared_ptr<ManufactureSpecificData> mdata = eir_ptr->getManufactureSpecificData();

        jclass map_cls = jau::search_class(env, "java/util/HashMap");
        jmethodID map_ctor = jau::search_method(env, map_cls, "<init>", "(I)V", false);
        jmethodID map_put = jau::search_method(env, map_cls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", false);

        jclass short_cls = jau::search_class(env, "java/lang/Short");
        jmethodID short_ctor = jau::search_method(env, short_cls, "<init>", "(S)V", false);
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

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getServices
 * Signature: ()Ljava/util/List;
 */
jobject Java_org_direct_1bt_EInfoReport_getServices(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        jau::darray<std::shared_ptr<const jau::uuid_t>> service_uuids = eir_ptr->getServices();
        std::function<jobject(JNIEnv*, const jau::uuid_t*)> ctor_uuid2string =
                [](JNIEnv *env_, const jau::uuid_t* uuid_ptr)->jobject {
                    return jau::from_string_to_jstring(env_, uuid_ptr->toUUID128String());
                };
        return jau::convert_vector_sharedptr_to_jarraylist<jau::darray<std::shared_ptr<const jau::uuid_t>>, const jau::uuid_t>(env, service_uuids, ctor_uuid2string);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getServicesComplete
 * Signature: ()Z
 */
jboolean Java_org_direct_1bt_EInfoReport_getServicesComplete(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return eir_ptr->getServicesComplete() ? JNI_TRUE : JNI_FALSE;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceClass
 * Signature: ()I
 */
jint Java_org_direct_1bt_EInfoReport_getDeviceClass(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jint>( eir_ptr->getDeviceClass() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceIDSource
 * Signature: ()S
 */
jshort Java_org_direct_1bt_EInfoReport_getDeviceIDSource(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jshort>( eir_ptr->getDeviceIDSource() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceIDVendor
 * Signature: ()S
 */
jshort Java_org_direct_1bt_EInfoReport_getDeviceIDVendor(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jshort>( eir_ptr->getDeviceIDVendor() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceIDProduct
 * Signature: ()S
 */
jshort Java_org_direct_1bt_EInfoReport_getDeviceIDProduct(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jshort>( eir_ptr->getDeviceIDProduct() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceIDVersion
 * Signature: ()S
 */
jshort Java_org_direct_1bt_EInfoReport_getDeviceIDVersion(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return static_cast<jshort>( eir_ptr->getDeviceIDVersion() );
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return 0;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getConnInterval
 * Signature: ([S)V
 */
void Java_org_direct_1bt_EInfoReport_getConnInterval(JNIEnv *env, jobject obj, jshortArray jminmax) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);

        if( nullptr == jminmax ) {
            throw jau::IllegalArgumentException("address null", E_FILE_LINE);
        }
        const size_t array_size = env->GetArrayLength(jminmax);
        if( 2 > array_size ) {
            throw jau::IllegalArgumentException("minmax array size "+std::to_string(array_size)+" < 2", E_FILE_LINE);
        }
        JNICriticalArray<uint16_t, jshortArray> criticalArray(env); // RAII - release
        uint16_t * array_ptr = criticalArray.get(jminmax, criticalArray.Mode::UPDATE_AND_RELEASE);
        if( NULL == array_ptr ) {
            throw jau::InternalError("GetPrimitiveArrayCritical(short array) is null", E_FILE_LINE);
        }
        eir_ptr->getConnInterval(array_ptr[0], array_ptr[1]);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    getDeviceIDModalias
 * Signature: ()Ljava/lang/String;
 */
jstring Java_org_direct_1bt_EInfoReport_getDeviceIDModalias(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return jau::from_string_to_jstring(env, eir_ptr->getDeviceIDModalias());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    eirDataMaskToString
 * Signature: ()Ljava/lang/String;
 */
jstring Java_org_direct_1bt_EInfoReport_eirDataMaskToString(JNIEnv *env, jobject obj) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return jau::from_string_to_jstring(env, eir_ptr->eirDataMaskToString());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

/*
 * Class:     org_direct_bt_EInfoReport
 * Method:    toString
 * Signature: (Z)Ljava/lang/String;
 */
jstring Java_org_direct_1bt_EInfoReport_toString(JNIEnv *env, jobject obj, jboolean includeServices) {
    try {
        std::shared_ptr<EInfoReport>& eir_ptr = *jau::getInstance<std::shared_ptr<EInfoReport>>(env, obj);
        return jau::from_string_to_jstring(env, eir_ptr->toString(JNI_TRUE==includeServices));
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}
