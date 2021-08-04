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

#ifndef DBT_DEV_ACCOUNTING_HPP_
#define DBT_DEV_ACCOUNTING_HPP_

#include <string>
#include <cstdio>

#include <direct_bt/DirectBT.hpp>

namespace direct_bt {

    /**
     * Application toolkit providing BT device registration of processed and awaited devices.
     * The latter on a pattern matching basis, i.e. EUI48Sub or name-sub.
     */
    namespace BTDeviceRegistry {
        /**
         * Specifies devices queries to act upon.
         */
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

        void addToWaitForDevices(const std::string& addrOrNameSub);
        bool isWaitingForDevice(const BDAddressAndType &mac, const std::string &name);
        bool isWaitingForAnyDevice();
        size_t getWaitForDevicesCount();
        void printWaitForDevices(FILE *out, const std::string &msg);
        /**
         * Returns the reference of the current list of DeviceQuery, not a copy.
         */
        jau::darray<DeviceQuery>& getWaitForDevices();
        /**
         * Clears internal list
         */
        void clearWaitForDevices();

        /**
         * Specifies unique device identities,
         * using {@link BDAddressAndType} as key.
         */
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

        void addToDevicesProcessed(const BDAddressAndType &a, const std::string& n);
        bool isDeviceProcessed(const BDAddressAndType & a);
        size_t getDeviceProcessedCount();
        bool allDevicesProcessed();
        void printDevicesProcessed(FILE *out, const std::string &msg);
        /**
         * Returns a copy of the current collection of processed DeviceID.
         */
        jau::darray<DeviceID> getProcessedDevices();
        /**
         * Clears internal list
         */
        void clearProcessedDevices();

        void addToDevicesProcessing(const BDAddressAndType &a, const std::string& n);
        bool removeFromDevicesProcessing(const BDAddressAndType &a);
        bool isDeviceProcessing(const BDAddressAndType & a);
        size_t getDeviceProcessingCount();
        /**
         * Returns a copy of the current collection of processing DeviceID.
         */
        jau::darray<DeviceID> getProcessingDevices();
        /**
         * Clears internal list
         */
        void clearProcessingDevices();
    }

} // namespace direct_bt

// injecting specialization of std::hash to namespace std of our types above
namespace std
{
    template<> struct hash<direct_bt::BTDeviceRegistry::DeviceID> {
        std::size_t operator()(direct_bt::BTDeviceRegistry::DeviceID const& a) const noexcept {
            return a.hash_code();
        }
    };
}

#endif /* DBT_DEV_ACCOUNTING_HPP_ */
