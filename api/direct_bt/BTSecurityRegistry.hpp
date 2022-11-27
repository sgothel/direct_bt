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

    /** \addtogroup DBTUserAPI
     *
     *  @{
     */

    /**
     * Application toolkit providing BT security setup and its device association
     * on a pattern matching basis, i.e. EUI48Sub or name-sub.
     */
    namespace BTSecurityRegistry {

        /** \addtogroup DBTUserAPI
         *
         *  @{
         */

        struct Entry {
            static constexpr int NO_PASSKEY = -1;

            EUI48Sub addrSub;
            std::string nameSub;

            BTSecurityLevel sec_level { BTSecurityLevel::UNSET };
            SMPIOCapability io_cap { SMPIOCapability::UNSET };
            SMPIOCapability io_cap_auto { SMPIOCapability::UNSET };
            int passkey = NO_PASSKEY;

            Entry(EUI48Sub addrSub_)
            : addrSub(std::move(addrSub_)), nameSub() {}

            Entry(std::string  nameSub_)
            : addrSub(EUI48Sub::ALL_DEVICE), nameSub(std::move(nameSub_)) {}

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

        /**
         * Function for user defined EUI48 address and name BTSecurityRegistry::Entry matching criteria and algorithm.
         * <p>
         * Return {@code true} if the given {@code address} or {@code name} matches
         * with the BTSecurityRegistry::Entry.
         * </p>
         *
         * @param address EUI48 address
         * @param name optional name, maybe empty
         * @param e Entry entry
         */
        typedef bool (*AddressNameEntryMatchFunc)(const EUI48& address, const std::string& name, const Entry& e);

        /**
         * Function for user defined EUI48Sub addressSub and name BTSecurityRegistry::Entry matching criteria and algorithm.
         * <p>
         * Return {@code true} if the given {@code addressSub} or {@code name} matches
         * with the BTSecurityRegistry::Entry.
         * </p>
         *
         * @param addressSub EUI48Sub address
         * @param name optional name, maybe empty
         * @param e Entry entry
         */
        typedef bool (*AddressSubNameEntryMatchFunc)(const EUI48Sub& addressSub, const std::string& name, const Entry& e);

        /**
         * Function for user defined std::string name BTSecurityRegistry::Entry matching criteria and algorithm.
         * <p>
         * Return {@code true} if the given {@code name} matches
         * with the BTSecurityRegistry::Entry.
         * </p>
         *
         * @param name
         * @param e Entry entry
         */
        typedef bool (*NameEntryMatchFunc)(const std::string& name, const Entry& e);

        /**
         * Returns a matching BTSecurityRegistry::Entry with the given {@code addr} and/or {@code name}.
         * <p>
         * Matching criteria and algorithm is defined by the given AddressNameEntryMatchFunc.
         * </p>
         */
        Entry* get(const EUI48& addr, const std::string& name, AddressNameEntryMatchFunc m) noexcept;

        /**
         * Returns a matching BTSecurityRegistry::Entry with the given {@code addrSub} and/or {@code name}.
         * <p>
         * Matching criteria and algorithm is defined by the given AddressSubNameEntryMatchFunc.
         * </p>
         */
        Entry* get(const EUI48Sub& addrSub, const std::string& name, AddressSubNameEntryMatchFunc m) noexcept;

        /**
         * Returns a matching BTSecurityRegistry::Entry with the given {@code name}.
         * <p>
         * Matching criteria and algorithm is defined by the given NameEntryMatchFunc.
         * </p>
         */
        Entry* get(const std::string& name, NameEntryMatchFunc m) noexcept;

        /**
         * Returns a matching Entry,
         * - which Entry::addrSub is set and the given {@code addr} starts with Entry::addrSub, or
         * - which Entry::nameSub is set and the given {@code name} starts with Entry::nameSub.
         *
         * Otherwise {@code null} is returned.
         */
        inline Entry* getStartOf(const EUI48& addr, const std::string& name) noexcept {
            return get(addr, name, [](const EUI48& a, const std::string& n, const Entry& e)->bool {
               return ( e.addrSub.length > 0 && 0 == a.indexOf(e.addrSub, jau::endian::big) ) ||
                      ( e.nameSub.length() > 0 && 0 == n.find(e.nameSub) );
            });
        }
        /**
         * Returns a matching Entry,
         * - which Entry::addrSub is set and the given {@code addrSub} starts with Entry::addrSub, or
         * - which Entry::nameSub is set and the given {@code name} starts with Entry::nameSub.
         *
         * Otherwise {@code null} is returned.
         */
        inline Entry* getStartOf(const EUI48Sub& addrSub, const std::string& name) noexcept {
            return get(addrSub, name, [](const EUI48Sub& as, const std::string& n, const Entry& e)->bool {
               return ( e.addrSub.length > 0 && 0 == as.indexOf(e.addrSub, jau::endian::big) ) ||
                      ( e.nameSub.length() > 0 && 0 == n.find(e.nameSub) );
            });
        }
        /**
         * Returns a matching Entry,
         * which Entry::nameSub is set and the given {@code name} starts with Entry::nameSub.
         *
         * Otherwise {@code null} is returned.
         */
        inline Entry* getStartOf(const std::string& name) noexcept {
            return get(name, [](const std::string& n, const Entry& e)->bool {
               return e.nameSub.length() > 0 && 0 == n.find(e.nameSub);
            });
        }

        /**
         * Returns a matching Entry,
         * - which Entry::addrSub is set and the given {@code addrSub} starts with Entry::addrSub, or
         * - which Entry::nameSub is set and the given {@code name} starts with Entry::nameSub.
         *
         * Otherwise {@code null} is returned.
         */
        inline Entry* getEqual(const EUI48Sub& addrSub, const std::string& name) noexcept {
            return get(addrSub, name, [](const EUI48Sub& as, const std::string& n, const Entry& e)->bool {
               return ( e.addrSub.length > 0 && as == e.addrSub ) ||
                      ( e.nameSub.length() > 0 && n == e.nameSub );
            });
        }
        /**
         * Returns a matching Entry,
         * which Entry::nameSub is set and the given {@code name} starts with Entry::nameSub.
         *
         * Otherwise {@code null} is returned.
         */
        inline Entry* getEqual(const std::string& name) noexcept {
            return get(name, [](const std::string& n, const Entry& e)->bool {
               return e.nameSub.length() > 0 && n == e.nameSub;
            });
        }

        /**
         * Returns the reference of the current list of Entry, not a copy.
         */
        jau::darray<Entry>& getEntries() noexcept;

        /**
         * Determines whether the given {@code addrOrNameSub} is a EUI48Sub or just a {@code name}
         * and retrieves an entry. If no entry exists, creates a new entry.
         * <p>
         * Implementation uses getEqual() to find a pre-existing entry.
         * </p>
         * @param addrOrNameSub either a EUI48Sub or just a name
         * @return new or existing instance
         */
        Entry* getOrCreate(const std::string& addrOrNameSub) noexcept;

        /**
         * Clears internal list
         */
        void clear() noexcept;

        std::string allToString() noexcept;

        /**@}*/

    } // namespace BTSecurityRegistry

    /**@}*/

} // namespace direct_bt

#endif /* DBT_SEC_SETTINGS_HPP_ */
