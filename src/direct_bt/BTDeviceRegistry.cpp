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

#include <unordered_set>
#include <unordered_map>

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

        /**
         * {@inheritDoc}
         * <p>
         * Implementation simply returns the BDAddressAndType hash code,
         * `name` is ignored.
         * </p>
         */
        std::size_t hash_code() const noexcept {
            return addressAndType.hash_code();
        }

        std::string toString() const {
            return "["+addressAndType.toString()+", '"+name+"']";
        }
    };
    /**
     * {@inheritDoc}
     * <p>
     * Implementation simply tests the {@link BDAddressAndType} fields for equality,
     * `name` is ignored.
     * </p>
     */
    inline bool operator==(const DeviceID& lhs, const DeviceID& rhs) noexcept {
        if( &lhs == &rhs ) {
            return true;
        }
        return lhs.addressAndType == rhs.addressAndType;
    }
    inline bool operator!=(const DeviceID& lhs, const DeviceID& rhs) noexcept
    { return !(lhs == rhs); }
}  // namespace direct_bt::BTDeviceRegistry

// injecting specialization of std::hash to namespace std of our types above
namespace std
{
    template<> struct hash<direct_bt::BTDeviceRegistry::DeviceID> {
        std::size_t operator()(direct_bt::BTDeviceRegistry::DeviceID const& a) const noexcept {
            return a.hash_code();
        }
    };
}

namespace direct_bt::BTDeviceRegistry {
    static std::unordered_set<DeviceID> devicesInProcessing;
    static std::recursive_mutex mtx_devicesProcessing;

    static std::unordered_set<DeviceID> devicesProcessed;
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
        devicesProcessed.emplace_hint(devicesProcessed.end(), a, n);
    }
    bool isDeviceProcessed(const BDAddressAndType & a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
        return devicesProcessed.end() != devicesProcessed.find( DeviceID(a, "") );
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
    static void printList(FILE *out, const std::string &msg, std::unordered_set<DeviceID> &cont) {
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
        devicesInProcessing.emplace_hint(devicesInProcessing.end(), a, n);
    }
    bool removeFromDevicesProcessing(const BDAddressAndType &a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        auto it = devicesInProcessing.find( DeviceID(a, "") );
        if( devicesInProcessing.end() != it ) {
            devicesInProcessing.erase(it);
            return true;
        }
        return false;
    }
    bool isDeviceProcessing(const BDAddressAndType & a) {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        return devicesInProcessing.end() != devicesInProcessing.find( DeviceID(a, "") );
    }
    size_t getDeviceProcessingCount() {
        const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
        return devicesInProcessing.size();
    }

} // namespace direct_bt::BTDeviceRegistry
