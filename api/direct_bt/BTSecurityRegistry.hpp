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

#ifndef DBT_SEC_SETTINGS_HPP_
#define DBT_SEC_SETTINGS_HPP_

#include <string>
#include <cstdio>

#include <direct_bt/DirectBT.hpp>

namespace direct_bt {

    /**
     * Application toolkit providing BT security setup and its device association
     * on a pattern matching basis, i.e. EUI48Sub or name-sub.
     */
    namespace BTSecurityRegistry {

        struct Entry {
            static constexpr int NO_PASSKEY = -1;

            EUI48Sub addrSub;
            std::string nameSub;

            BTSecurityLevel sec_level { BTSecurityLevel::UNSET };
            SMPIOCapability io_cap { SMPIOCapability::UNSET };
            SMPIOCapability io_cap_auto { SMPIOCapability::UNSET };
            int passkey = NO_PASSKEY;

            Entry(const EUI48Sub& addrSub_)
            : addrSub(addrSub_), nameSub() {}

            Entry(const std::string& nameSub_)
            : addrSub(EUI48Sub::ALL_DEVICE), nameSub(nameSub_) {}

            bool matches(const EUI48Sub& addressSub) const noexcept {
                return addrSub.length > 0 && addressSub.contains(addrSub);
            }
            bool matches(const EUI48& address) const noexcept {
                return addrSub.length > 0 && address.contains(addrSub);
            }
            bool matches(const std::string& name) const noexcept {
                return nameSub.length() > 0 && name.find(nameSub) != std::string::npos;
            }

            constexpr bool isSecLevelOrIOCapSet() const noexcept {
                return SMPIOCapability::UNSET != io_cap ||  BTSecurityLevel::UNSET != sec_level;
            }
            constexpr const BTSecurityLevel& getSecLevel() const noexcept { return sec_level; }
            constexpr const SMPIOCapability& getIOCap() const noexcept { return io_cap; }

            constexpr bool isSecurityAutoEnabled() const noexcept {
                return SMPIOCapability::UNSET != io_cap_auto;
            }
            constexpr const SMPIOCapability& getSecurityAutoIOCap() const noexcept { return io_cap_auto; }

            constexpr int getPairingPasskey() const noexcept { return passkey; }

            constexpr bool getPairingNumericComparison() const noexcept { return true; }

            std::string toString() const noexcept {
                const std::string id = addrSub == EUI48Sub::ALL_DEVICE ? "'"+nameSub+"'" : addrSub.toString();
                return "BTSecurityDetail["+id+", lvl "+
                        to_string(sec_level)+
                        ", io "+to_string(io_cap)+
                        ", auto-io "+to_string(io_cap_auto)+
                        ", passkey "+std::to_string(passkey)+"]";
            }
        };
        Entry* get(const EUI48& addr);
        Entry* get(const EUI48Sub& addrSub);
        Entry* get(const std::string& nameSub);

        /**
         * Returns the reference of the current list of Entry, not a copy.
         */
        jau::darray<Entry>& getEntries();

        Entry* getOrCreate(const std::string& addrOrNameSub);

        /**
         * Clears internal list
         */
        void clear();

        std::string allToString();

    } // namespace BTSecurityRegistry

} // namespace direct_bt

#endif /* DBT_SEC_SETTINGS_HPP_ */
