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

#include "BTDeviceRegistry.hpp"

#include <jau/cpp_lang_util.hpp>
#include <jau/basic_algos.hpp>
#include <jau/darray.hpp>

using namespace direct_bt;

namespace direct_bt::BTDeviceRegistry {
    struct DeviceQuery {
        EUI48Sub addressSub;
        std::string nameSub;
        DeviceQuery(const EUI48Sub& as, const std::string& ns) : addressSub(as), nameSub(ns) {}
        DeviceQuery() : addressSub(), nameSub() {}
        std::string toString() const {
            if( addressSub.length>0 ) {
                return addressSub.toString();
            } else {
                return "'"+nameSub+"'";
            }
        }
    };
    static jau::darray<DeviceQuery> waitForDevices;

    struct DeviceID {
        BDAddressAndType addressAndType;
        std::string name;
        DeviceID(const BDAddressAndType& a, const std::string& n) : addressAndType(a), name(n) {}
        DeviceID() : addressAndType(), name() {}
        std::string toString() const {
            return "["+addressAndType.toString()+", '"+name+"']";
        }
    };
    static jau::darray<DeviceID> devicesInProcessing;
    static std::recursive_mutex mtx_devicesProcessing;

    static jau::darray<DeviceID> devicesProcessed;
    static std::recursive_mutex mtx_devicesProcessed;

    void addToWaitForDevices(const std::string& addrOrNameSub) {
        EUI48Sub addr1;
        std::string errmsg;
        if( EUI48Sub::scanEUI48Sub(addrOrNameSub, addr1, errmsg) ) {
            waitForDevices.emplace_back( addr1, "" );
        } else {
            addr1.clear();
            waitForDevices.emplace_back( addr1, addrOrNameSub );
        }
    }
    bool isWaitingForDevice(const BDAddressAndType &mac, const std::string &name) {
        return waitForDevices.cend() != jau::find_if(waitForDevices.cbegin(), waitForDevices.cend(), [&](const DeviceQuery & it)->bool {
           return ( it.addressSub.length>0 && mac.address.contains(it.addressSub) ) ||
                  ( it.nameSub.length()>0 && name.find(it.nameSub) != std::string::npos );
        });
    }
    bool isWaitingForAnyDevice() {
        return !waitForDevices.empty();
    }
    size_t getWaitForDevicesCount() {
        return waitForDevices.size();
    }
    static void printList(FILE *out, const std::string &msg, jau::darray<DeviceQuery> &cont) {
        jau::fprintf_td(out, "%s ", msg.c_str());
        jau::for_each(cont.cbegin(), cont.cend(), [out](const DeviceQuery& q) {
            fprintf(out, "%s, ", q.toString().c_str());
        });
        fprintf(out, "\n");
    }
    void printWaitForDevices(FILE *out, const std::string &msg) {
        printList(out, msg, waitForDevices);
    }

    void addToDevicesProcessed(const BDAddressAndType &a, const std::string& n) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        devicesProcessed.emplace_back(a, n);
    }
    bool isDeviceProcessed(const BDAddressAndType & a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        for (auto it = devicesProcessed.cbegin(); it != devicesProcessed.cend(); ++it) {
            if( it->addressAndType == a ) {
                return true;
            }
        }
        return false;
    }
    size_t getDeviceProcessedCount() {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        return devicesProcessed.size();
    }
    bool allDevicesProcessed() {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        for (auto it1 = waitForDevices.cbegin(); it1 != waitForDevices.cend(); ++it1) {
            const DeviceQuery& q = *it1;
            auto it2 = devicesProcessed.cbegin();
            while ( it2 != devicesProcessed.cend() ) {
                const DeviceID& id = *it2;
                if( ( q.addressSub.length>0 && id.addressAndType.address.contains(q.addressSub) ) ||
                    ( q.nameSub.length()>0 && id.name.find(q.nameSub) != std::string::npos ) ) {
                    break;
                }
                ++it2;
            }
            if( it2 == devicesProcessed.cend() ) {
                return false;
            }
        }
        return true;
    }
    static void printList(FILE *out, const std::string &msg, jau::darray<DeviceID> &cont) {
        jau::fprintf_td(out, "%s ", msg.c_str());
        jau::for_each(cont.cbegin(), cont.cend(), [out](const DeviceID &id) {
            fprintf(out, "%s, ", id.toString().c_str());
        });
        fprintf(out, "\n");
    }
    void printDevicesProcessed(FILE *out, const std::string &msg) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        printList(out, msg, devicesProcessed);
    }

    void addToDevicesProcessing(const BDAddressAndType &a, const std::string& n) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        devicesInProcessing.emplace_back(a, n);
    }
    bool removeFromDevicesProcessing(const BDAddressAndType &a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        for (auto it = devicesInProcessing.begin(); it != devicesInProcessing.end(); ++it) {
            if ( it->addressAndType == a ) {
                devicesInProcessing.erase(it);
                return true;
            }
        }
        return false;
    }
    bool isDeviceProcessing(const BDAddressAndType & a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        for (auto it = devicesInProcessing.cbegin(); it != devicesInProcessing.cend(); ++it) {
            if( it->addressAndType == a ) {
                return true;
            }
        }
        return false;
    }
    size_t getDeviceProcessingCount() {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        return devicesInProcessing.size();
    }

} // namespace direct_bt::BTDeviceRegistry
