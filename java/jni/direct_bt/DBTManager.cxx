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

static const std::string _adapterClazzCtorArgs("(JLjava/lang/String;Ljava/lang/String;I)V");
static jobject _createJavaAdapter(JNIEnv *env_, jclass clazz, jmethodID clazz_ctor, DBTAdapter* adapter) {
    // prepare adapter ctor
    const jstring addr = from_string_to_jstring(env_, adapter->getAddressString());
    const jstring name = from_string_to_jstring(env_, adapter->getName());
    java_exception_check_and_throw(env_, E_FILE_LINE);
    jobject jAdapter = env_->NewObject(clazz, clazz_ctor, (jlong)adapter, addr, name, adapter->dev_id);
    java_exception_check_and_throw(env_, E_FILE_LINE);
    JNIGlobalRef::check(jAdapter, E_FILE_LINE);
    std::shared_ptr<JavaAnon> jAdapterRef = adapter->getJavaObject(); // GlobalRef
    JavaGlobalObj::check(jAdapterRef, E_FILE_LINE);
    env_->DeleteLocalRef(addr);
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

