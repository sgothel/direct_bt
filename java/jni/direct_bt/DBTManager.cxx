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

#include "jau_direct_bt_DBTManager.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/BTDevice.hpp"
#include "direct_bt/BTAdapter.hpp"
#include "direct_bt/BTManager.hpp"

using namespace direct_bt;
using namespace jau::jni;

static const std::string _removeAdapterCBMethodName("removeAdapterCB");
static const std::string _removeAdapterCBMethodArgs("(II)V");
static const std::string _updatedAdapterCBMethodName("updatedAdapterCB");
static const std::string _updatedAdapterCBMethodArgs("(II)V");

struct BooleanMgmtCBContext {
    MgmtEvent::Opcode opc;
    JNIGlobalRef jmgmtRef;
    jmethodID  mid;

    BooleanMgmtCBContext(MgmtEvent::Opcode opc_, JNIGlobalRef jmgmtRef_, jmethodID mid_)
    : opc(opc_), jmgmtRef(jmgmtRef_), mid(mid_) { }

    bool operator==(const BooleanMgmtCBContext& rhs) const
    {
        if( &rhs == this ) {
            return true;
        }
        return rhs.opc == opc &&
               rhs.jmgmtRef == jmgmtRef;
    }

    bool operator!=(const BooleanMgmtCBContext& rhs) const
    { return !( *this == rhs ); }

};
typedef std::shared_ptr<BooleanMgmtCBContext> BooleanMgmtCBContextRef;

static void _addMgmtCBOnce(JNIEnv *env, BTManager & mgmt, JNIGlobalRef jmgmtRef, MgmtEvent::Opcode opc,
                           const std::string &jmethodName, const std::string &jmethodArgs)
{
    try {
        bool(*nativeCallback)(BooleanMgmtCBContextRef&, const MgmtEvent&) =
                [](BooleanMgmtCBContextRef& ctx_ref, const MgmtEvent& e)->bool {
            JNIEnv *env_ = *jni_env;
            const int dev_id = e.getDevID();

            if( MgmtEvent::Opcode::INDEX_REMOVED == ctx_ref->opc ) {
                env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // remove
            } else if( MgmtEvent::Opcode::INDEX_ADDED == ctx_ref->opc ) {
                env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // updated
            } else if( MgmtEvent::Opcode::NEW_SETTINGS == ctx_ref->opc ) {
                const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
                if( isAdapterSettingBitSet(event.getSettings(), AdapterSetting::POWERED) ) {
                    // probably newly POWERED on
                    env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // updated
                }
            }
            return true;
        };


        jclass mgmtClazz = search_class(env, jmgmtRef.getObject());
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mgmtClazz ) {
            throw jau::InternalError("BTManager not found", E_FILE_LINE);
        }
        jmethodID mid = search_method(env, mgmtClazz, jmethodName.c_str(), jmethodArgs.c_str(), false);
        java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mid ) {
            throw jau::InternalError("BTManager has no "+jmethodName+"."+jmethodArgs+" method, for "+mgmt.toString(), E_FILE_LINE);
        }

        // move BooleanDeviceCBContextRef into CaptureValueInvocationFunc and operator== includes javaCallback comparison
        jau::FunctionDef<bool, const MgmtEvent&> funcDef = jau::bindCaptureValueFunc(std::make_shared<BooleanMgmtCBContext>(opc, jmgmtRef, mid), nativeCallback);
        mgmt.addMgmtEventCallback(-1, opc, funcDef);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

jlong Java_jau_direct_1bt_DBTManager_ctorImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTManager> ref( BTManager::get() );
        JNIGlobalRef global_obj(obj); // lock instance first (global reference), inserted below
        java_exception_check_and_throw(env, E_FILE_LINE);
        ref->setJavaObject( std::make_shared<JavaGlobalObj>( std::move(global_obj), nullptr ) );
        JavaGlobalObj::check(ref->getJavaObject(), E_FILE_LINE);

        JNIGlobalRef jmgmtRef = JavaGlobalObj::GetJavaObject(ref->getJavaObject());
        _addMgmtCBOnce(env, *ref, jmgmtRef, MgmtEvent::Opcode::INDEX_REMOVED, _removeAdapterCBMethodName, _removeAdapterCBMethodArgs);
        _addMgmtCBOnce(env, *ref, jmgmtRef, MgmtEvent::Opcode::INDEX_ADDED, _updatedAdapterCBMethodName, _updatedAdapterCBMethodArgs);
        _addMgmtCBOnce(env, *ref, jmgmtRef, MgmtEvent::Opcode::NEW_SETTINGS, _updatedAdapterCBMethodName, _updatedAdapterCBMethodArgs);
        DBG_PRINT("Java_jau_direct_1bt_DBTManager_init: Manager %s", ref->toString().c_str());
        return ref.release_to_jlong();
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return (jlong) (intptr_t)nullptr;
}

void Java_jau_direct_1bt_DBTManager_dtorImpl(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        shared_ptr_ref<BTManager> manager(nativeInstance, false /* throw_on_nullptr */); // hold copy until done
        if( nullptr != manager.pointer() ) {
            if( !manager.is_null() ) {
                JavaAnonRef manager_java = manager->getJavaObject(); // hold until done!
                JavaGlobalObj::check(manager_java, E_FILE_LINE);
                manager->setJavaObject();
                manager->close();
            }
            std::shared_ptr<BTManager>* ref_ptr = castInstance<BTManager>(nativeInstance);
            delete ref_ptr;
        }
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

static const std::string _adapterClazzCtorArgs("(J[BBLjava/lang/String;I)V");
static jobject _createJavaAdapter(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, const std::shared_ptr<BTAdapter>& adapter) {
    // prepare adapter ctor
    const EUI48 addr = adapter->getAddressAndType().address;
    jbyteArray jaddr = env_->NewByteArray(sizeof(addr));
    env_->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
    java_exception_check_and_throw(env_, E_FILE_LINE);
    const jstring name = from_string_to_jstring(env_, adapter->getName());
    java_exception_check_and_throw(env_, E_FILE_LINE);
    shared_ptr_ref<BTAdapter> adapter_ref( adapter );
    jobject jAdapter = env_->NewObject(clazz, clazz_ctor, adapter_ref.release_to_jlong(), jaddr, adapter->getAddressAndType().type, name, adapter->dev_id);
    java_exception_check_and_throw(env_, E_FILE_LINE);
    JNIGlobalRef::check(jAdapter, E_FILE_LINE);
    JavaAnonRef jAdapterRef = adapter->getJavaObject(); // GlobalRef
    JavaGlobalObj::check(jAdapterRef, E_FILE_LINE);
    env_->DeleteLocalRef(jaddr);
    env_->DeleteLocalRef(name);
    env_->DeleteLocalRef(jAdapter);

    DBG_PRINT("Java_jau_direct_1bt_DBTManager_createJavaAdapter: New Adapter %p %s", adapter.get(), adapter->toString().c_str());
    return JavaGlobalObj::GetObject(jAdapterRef);
};

jobject Java_jau_direct_1bt_DBTManager_getAdapterListImpl(JNIEnv *env, jobject obj)
{
    try {
        shared_ptr_ref<BTManager> ref(env, obj); // hold until done
        DBG_PRINT("Java_jau_direct_1bt_DBTManager_getAdapterListImpl: Manager %s", ref->toString().c_str());

        jau::darray<std::shared_ptr<BTAdapter>> adapters = ref->getAdapters();
        return convert_vector_sharedptr_to_jarraylist<jau::darray<std::shared_ptr<BTAdapter>>, BTAdapter>(
                env, adapters, _adapterClazzCtorArgs.c_str(), _createJavaAdapter);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jobject Java_jau_direct_1bt_DBTManager_getAdapterImpl(JNIEnv *env, jobject obj, jint dev_id)
{
    try {
        shared_ptr_ref<BTManager> ref(env, obj); // hold until done

        std::shared_ptr<BTAdapter> adapter = ref->getAdapter(dev_id);
        if( nullptr == adapter ) {
            ERR_PRINT("BTManager::getAdapterImpl: Adapter dev_id %d: Not found", dev_id);
            return nullptr;
        }
        DBG_PRINT("BTManager::getAdapterImpl: Adapter dev_id %d: %s", dev_id, adapter->toString().c_str());

        return convert_instance_to_jobject<BTAdapter>(env, adapter,  _adapterClazzCtorArgs.c_str(), _createJavaAdapter);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}
