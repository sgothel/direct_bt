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

#include "BTTypes0.hpp"

namespace direct_bt {

    /** \addtogroup DBTUserAPI
     *
     *  @{
     */

    /**
     * Application toolkit providing BT device registration of processed and awaited devices.
     * The latter on a pattern matching basis, i.e. EUI48Sub or name-sub.
     */
    namespace BTDeviceRegistry {
        /**
         * Specifies devices queries to act upon.
         */
        struct DeviceQuery {
            /**
             * DeviceQuery type, i.e. EUI48Sub or a std::string  name.
             */
            enum class Type : int {
                /** DeviceQuery type, using a sensor device EUI48Sub. */
                EUI48SUB,
                /** DeviceQuery type, using a sensor device std::string name. */
                NAME
            };

            Type type;
            EUI48Sub addressSub;
            std::string nameSub;

            DeviceQuery(EUI48Sub as) : type(Type::EUI48SUB), addressSub(std::move(as)), nameSub() {}

            DeviceQuery(std::string ns) : type(Type::NAME), addressSub(EUI48Sub::ANY_DEVICE), nameSub(std::move(ns)) {}

            bool isEUI48Sub() const noexcept { return Type::EUI48SUB == type; }

            std::string toString() const {
                if( Type::EUI48SUB == type ) {
                    return "[a: "+addressSub.toString()+"]";
                } else {
                    return "[n: '"+nameSub+"']";
                }
            }
        };

        void addToWaitForDevices(const std::string& addrOrNameSub) noexcept;
        bool isWaitingForAnyDevice() noexcept;
        size_t getWaitForDevicesCount() noexcept;
        std::string getWaitForDevicesString() noexcept;
        /**
         * Returns the reference of the current list of DeviceQuery, not a copy.
         */
        jau::darray<DeviceQuery>& getWaitForDevices() noexcept;
        /**
         * Clears internal list
         */
        void clearWaitForDevices() noexcept;

        /**
         * Specifies unique device identities,
         * using {@link BDAddressAndType} as key.
         */
        struct DeviceID {
            BDAddressAndType addressAndType;
            std::string name;

            DeviceID(BDAddressAndType  a, std::string  n) : addressAndType(std::move(a)), name(std::move(n)) {}
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

        void addToProcessedDevices(const BDAddressAndType &a, const std::string& n) noexcept;
        bool isDeviceProcessed(const BDAddressAndType & a) noexcept;
        size_t getProcessedDeviceCount() noexcept;

        std::string getProcessedDevicesString() noexcept;

        /**
         * Returns a copy of the current collection of processed DeviceID.
         */
        jau::darray<DeviceID> getProcessedDevices() noexcept;
        /**
         * Clears internal list
         */
        void clearProcessedDevices() noexcept;

        /**
         * Function for user defined BTDeviceRegistry::DeviceQuery matching criteria and algorithm.
         * <p>
         * Return {@code true} if the given {@code address} and/or {@code name} matches
         * with the BTDeviceRegistry::DeviceQuery::addressSub and/or
         * BTDeviceRegistry::DeviceQuery::nameSub.
         * </p>
         * <p>
         * Example (lambda):
         * <pre>
         *    [](const EUI48& a, const std::string& n, const DeviceQuery& q)->bool {
         *       return q.isEUI48Sub() ? a.contains(q.addressSub) : n.find(q.nameSub) != std::string::npos;
         *    });
         * </pre>
         * </p>
         */
        typedef bool (*DeviceQueryMatchFunc)(const EUI48& address, const std::string& name, const DeviceQuery& q);

        /**
         * Returns {@code true} if the given {@code address} and/or {@code name}
         * matches any of the BTDeviceRegistry::addToWaitForDevices() awaited devices.
         * <p>
         * Matching criteria and algorithm is defined by the given BTDeviceRegistry::DeviceQueryMatchFunc.
         * </p>
         * @see BTDeviceRegistry::isWaitingForDevice()
         */
        bool isWaitingForDevice(const EUI48 &address, const std::string &name, DeviceQueryMatchFunc m) noexcept;

        /**
         * Returns {@code true} if the given {@code address} and/or {@code name}
         * matches any of the BTDeviceRegistry::addToWaitForDevices() awaited devices.
         * <p>
         * Matching criteria is either the awaited device's BTDeviceRegistry::DeviceQuery::addressSub
         * or BTDeviceRegistry::DeviceQuery::nameSub, whichever is set.
         * </p>
         * <p>
         * Matching algorithm is a simple {@code contains} pattern match,
         * i.e. the given {@code address} or {@code name} contains the corresponding BTDeviceRegistry::DeviceQuery element.
         * </p>
         * @see BTDeviceRegistry::isWaitingForDevice()
         */
        inline bool isWaitingForDevice(const EUI48 &address, const std::string &name) noexcept {
            return isWaitingForDevice(address, name, [](const EUI48& a, const std::string& n, const DeviceQuery& q)->bool {
                return q.isEUI48Sub() ? a.contains(q.addressSub) : n.find(q.nameSub) != std::string::npos;
            });
        }

        /**
         * Returns {@code true} if all addToWaitForDevices() awaited devices
         * have been addToProcessedDevices() processed.
         * <p>
         * Matching criteria and algorithm is defined by the given BTDeviceRegistry::DeviceQueryMatchFunc.
         * </p>
         * @see BTDeviceRegistry::areAllDevicesProcessed()
         */
        bool areAllDevicesProcessed(DeviceQueryMatchFunc m) noexcept;

        /**
         * Returns {@code true} if all addToWaitForDevices() awaited devices
         * have been addToProcessedDevices() processed.
         * <p>
         * Matching criteria is either the awaited device's BTDeviceRegistry::DeviceQuery::addressSub
         * or BTDeviceRegistry::DeviceQuery::nameSub, whichever is set.
         * </p>
         * <p>
         * Matching algorithm is a simple {@code contains} pattern match,
         * i.e. the processed BTDeviceRegistry::DeviceID contains one element of BTDeviceRegistry::DeviceQuery.
         * </p>
         * @see BTDeviceRegistry::areAllDevicesProcessed()
         */
        inline bool areAllDevicesProcessed() noexcept {
            return areAllDevicesProcessed( [](const EUI48& a, const std::string& n, const DeviceQuery& q)->bool {
                                            return q.isEUI48Sub() ? a.contains(q.addressSub) : n.find(q.nameSub) != std::string::npos;
                                         });
        }
    }

    /**@}*/

} // namespace direct_bt

// injecting specialization of std::hash to namespace std of our types above
namespace std
{
    /** \addtogroup DBTUserAPI
     *
     *  @{
     */
    template<> struct hash<direct_bt::BTDeviceRegistry::DeviceID> {
        std::size_t operator()(direct_bt::BTDeviceRegistry::DeviceID const& a) const noexcept {
            return a.hash_code();
        }
    };

    /**@}*/
}

#endif /* DBT_DEV_ACCOUNTING_HPP_ */
