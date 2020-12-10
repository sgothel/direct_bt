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

#include "direct_bt_tinyb_DBTManager.h"

// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "helper_base.hpp"
#include "helper_dbt.hpp"

#include "direct_bt/DBTManager.hpp"
#include "direct_bt/DBTDevice.hpp"
#include "direct_bt/DBTAdapter.hpp"

using namespace direct_bt;
using namespace jau;

static const std::string _removeAdapterCBMethodName("removeAdapterCB");
static const std::string _removeAdapterCBMethodArgs("(II)V");
static const std::string _updatedAdapterCBMethodName("updatedAdapterCB");
static const std::string _updatedAdapterCBMethodArgs("(II)V");

struct BooleanMgmtCBContext {
    MgmtEvent::Opcode opc;
    JNIGlobalRef jmgmtRef;
    jmethodID  mid;

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

static void _addMgmtCBOnce(JNIEnv *env, DBTManager & mgmt, JNIGlobalRef jmgmtRef, MgmtEvent::Opcode opc,
                           const std::string &jmethodName, const std::string &jmethodArgs)
{
    try {
        bool(*nativeCallback)(BooleanMgmtCBContextRef&, std::shared_ptr<MgmtEvent>) =
                [](BooleanMgmtCBContextRef& ctx_ref, std::shared_ptr<MgmtEvent> e)->bool {
            JNIEnv *env_ = *jni_env;
            const int dev_id = e->getDevID();

            if( MgmtEvent::Opcode::INDEX_REMOVED == ctx_ref->opc ) {
                env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // remove
            } else if( MgmtEvent::Opcode::INDEX_ADDED == ctx_ref->opc ) {
                env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // updated
            } else if( MgmtEvent::Opcode::NEW_SETTINGS == ctx_ref->opc ) {
                const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(e.get());
                if( isAdapterSettingBitSet(event.getSettings(), AdapterSetting::POWERED) ) {
                    // probably newly POWERED on
                    env_->CallVoidMethod(*(ctx_ref->jmgmtRef), ctx_ref->mid, dev_id, ctx_ref->opc); // updated
                }
            }
            return true;
        };


        jclass mgmtClazz = jau::search_class(env, jmgmtRef.getObject());
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mgmtClazz ) {
            throw jau::InternalError("DBTManager not found", E_FILE_LINE);
        }
        jmethodID mid = jau::search_method(env, mgmtClazz, jmethodName.c_str(), jmethodArgs.c_str(), false);
        jau::java_exception_check_and_throw(env, E_FILE_LINE);
        if( nullptr == mid ) {
            throw jau::InternalError("DBTManager has no "+jmethodName+"."+jmethodArgs+" method, for "+mgmt.toString(), E_FILE_LINE);
        }
        BooleanMgmtCBContext * ctx = new BooleanMgmtCBContext{opc, jmgmtRef, mid };

        // move BooleanDeviceCBContextRef into CaptureInvocationFunc and operator== includes javaCallback comparison
        FunctionDef<bool, std::shared_ptr<MgmtEvent>> funcDef = bindCaptureFunc(BooleanMgmtCBContextRef(ctx), nativeCallback);
        mgmt.addMgmtEventCallback(-1, opc, funcDef);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_direct_1bt_tinyb_DBTManager_initImpl(JNIEnv *env, jobject obj, jboolean unifyUUID128Bit, jint jbtMode)
{
    directBTJNISettings.setUnifyUUID128Bit(unifyUUID128Bit);
    try {
        BTMode btMode = static_cast<BTMode>(jbtMode);
        DBTManager *manager = &DBTManager::get(btMode); // special: static singleton
        setInstance<DBTManager>(env, obj, manager);
        java_exception_check_and_throw(env, E_FILE_LINE);
        manager->setJavaObject( std::shared_ptr<JavaAnon>( new JavaGlobalObj(obj, nullptr) ) );
        JavaGlobalObj::check(manager->getJavaObject(), E_FILE_LINE);

        JNIGlobalRef jmgmtRef = JavaGlobalObj::GetJavaObject(manager->getJavaObject());
        _addMgmtCBOnce(env, *manager, jmgmtRef, MgmtEvent::Opcode::INDEX_REMOVED, _removeAdapterCBMethodName, _removeAdapterCBMethodArgs);
        _addMgmtCBOnce(env, *manager, jmgmtRef, MgmtEvent::Opcode::INDEX_ADDED, _updatedAdapterCBMethodName, _updatedAdapterCBMethodArgs);
        _addMgmtCBOnce(env, *manager, jmgmtRef, MgmtEvent::Opcode::NEW_SETTINGS, _updatedAdapterCBMethodName, _updatedAdapterCBMethodArgs);
        DBG_PRINT("Java_direct_1bt_tinyb_DBTManager_init: Manager %s", manager->toString().c_str());
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

void Java_direct_1bt_tinyb_DBTManager_deleteImpl(JNIEnv *env, jobject obj, jlong nativeInstance)
{
    (void)obj;
    try {
        DBTManager *manager = castInstance<DBTManager>(nativeInstance); // special: static singleton
        manager->close();
        manager->setJavaObject();
        (void) manager;
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
}

static const std::string _adapterClazzCtorArgs("(J[BLjava/lang/String;I)V");
static jobject _createJavaAdapter(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, DBTAdapter* adapter) {
    // prepare adapter ctor
    const EUI48 addr = adapter->getAddress();
    jbyteArray jaddr = env_->NewByteArray(sizeof(addr));
    env_->SetByteArrayRegion(jaddr, 0, sizeof(addr), (const jbyte*)(addr.b));
    jau::java_exception_check_and_throw(env_, E_FILE_LINE);
    const jstring name = from_string_to_jstring(env_, adapter->getName());
    java_exception_check_and_throw(env_, E_FILE_LINE);
    jobject jAdapter = env_->NewObject(clazz, clazz_ctor, (jlong)adapter, jaddr, name, adapter->dev_id);
    java_exception_check_and_throw(env_, E_FILE_LINE);
    JNIGlobalRef::check(jAdapter, E_FILE_LINE);
    std::shared_ptr<JavaAnon> jAdapterRef = adapter->getJavaObject(); // GlobalRef
    JavaGlobalObj::check(jAdapterRef, E_FILE_LINE);
    env_->DeleteLocalRef(jaddr);
    env_->DeleteLocalRef(name);
    env_->DeleteLocalRef(jAdapter);

    DBG_PRINT("Java_direct_1bt_tinyb_DBTManager_createJavaAdapter: New Adapter %p %s", adapter, adapter->toString().c_str());
    return JavaGlobalObj::GetObject(jAdapterRef);
};

jobject Java_direct_1bt_tinyb_DBTManager_getAdapterListImpl(JNIEnv *env, jobject obj)
{
    try {
        DBTManager *manager = getInstance<DBTManager>(env, obj);
        DBG_PRINT("Java_direct_1bt_tinyb_DBTManager_getAdapterListImpl: Manager %s", manager->toString().c_str());

        std::vector<std::unique_ptr<DBTAdapter>> adapters;
        const int adapterCount = manager->getAdapterCount();
        for(int dev_id = 0; dev_id < adapterCount; dev_id++) {
            if( nullptr == manager->getAdapterInfo(dev_id) ) {
                ERR_PRINT("DBTManager::getAdapterListImpl: Adapter dev_id %d: Not found", dev_id);
                continue;
            }
            std::unique_ptr<DBTAdapter> adapter(new DBTAdapter( dev_id ) );
            if( !adapter->isValid() ) {
                throw BluetoothException("Invalid adapter @ dev_id "+std::to_string( dev_id ), E_FILE_LINE);
            }
            if( !adapter->hasDevId() ) {
                throw BluetoothException("Invalid adapter dev-id @ dev_id "+std::to_string( dev_id ), E_FILE_LINE);
            }
            if( dev_id != adapter->dev_id ) { // just make sure idx == dev_id
                throw BluetoothException("Invalid adapter dev-id "+std::to_string( adapter->dev_id )+" != dev_id "+std::to_string( dev_id ), E_FILE_LINE);
            }
            adapters.push_back(std::move(adapter));
        }
        return convert_vector_uniqueptr_to_jarraylist<DBTAdapter>(env, adapters, _adapterClazzCtorArgs.c_str(), _createJavaAdapter);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}

jobject Java_direct_1bt_tinyb_DBTManager_getAdapterImpl(JNIEnv *env, jobject obj, jint dev_id)
{
    try {
        DBTManager *manager = getInstance<DBTManager>(env, obj);

        if( nullptr == manager->getAdapterInfo(dev_id) ) {
            ERR_PRINT("DBTManager::getAdapterImpl: Adapter dev_id %d: Not found", dev_id);
            return nullptr;
        }
        DBTAdapter * adapter = new DBTAdapter( dev_id );
        DBG_PRINT("DBTManager::getAdapterImpl: Adapter dev_id %d: %s", dev_id, adapter->toString().c_str());

        if( !adapter->isValid() ) {
            throw BluetoothException("Invalid adapter @ dev_id "+std::to_string( dev_id ), E_FILE_LINE);
        }
        if( !adapter->hasDevId() ) {
            throw BluetoothException("Invalid adapter dev-id @ dev_id "+std::to_string( dev_id ), E_FILE_LINE);
        }
        if( dev_id != adapter->dev_id ) { // just make sure idx == dev_id
            throw BluetoothException("Invalid adapter dev-id "+std::to_string( adapter->dev_id )+" != dev_id "+std::to_string( dev_id ), E_FILE_LINE);
        }
        return jau::convert_instance_to_jobject<DBTAdapter>(env, adapter,  _adapterClazzCtorArgs.c_str(), _createJavaAdapter);
    } catch(...) {
        rethrow_and_raise_java_exception(env);
    }
    return nullptr;
}
