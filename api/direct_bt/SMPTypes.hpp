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

#ifndef SMP_TYPES_HPP_
#define SMP_TYPES_HPP_

#include <cstring>
#include <string>
#include <cstdint>
#include <cmath>

#include <algorithm>
#include <mutex>

#include <jau/basic_types.hpp>
#include <jau/octets.hpp>

#include "BTTypes0.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * SMPTypes.hpp Module for SMPPDUMsg Types, SMPAuthReqs etc:
 *
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
 *
 */
namespace direct_bt {

    /** \addtogroup DBTSystemAPI
     *
     *  @{
     */

    class SMPException : public jau::RuntimeException {
        protected:
            SMPException(std::string type, std::string const& m, const char* file, int line) noexcept
            : RuntimeException(std::move(type), m, file, line) {}

        public:
            SMPException(std::string const& m, const char* file, int line) noexcept
            : RuntimeException("HCIException", m, file, line) {}
    };

    class SMPPacketException : public SMPException {
        public:
            SMPPacketException(std::string const& m, const char* file, int line) noexcept
            : SMPException("SMPPacketException", m, file, line) {}
    };
    class SMPOpcodeException : public SMPException {
        public:
            SMPOpcodeException(std::string const& m, const char* file, int line) noexcept
            : SMPException("SMPOpcodeException", m, file, line) {}
    };

    class SMPValueException : public SMPException {
        public:
            SMPValueException(std::string const& m, const char* file, int line) noexcept
            : SMPException("SMPValueException", m, file, line) {}
    };

    enum class SMPConstInt : int32_t {
    };
    constexpr int32_t number(const SMPConstInt rhs) noexcept {
        return static_cast<int>(rhs);
    }

    enum class SMPConstU16 : uint16_t {
        /** SMP Timeout Vol 3, Part H (SM): 3.4 */
        SMP_TIMEOUT_MS        = 30000
    };
    constexpr uint16_t number(const SMPConstU16 rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }


    /**
     * SMP Pairing Process state definition
     * <pre>
     * Vol 3, Part H (SM): APPENDIX C MESSAGE SEQUENCE CHARTS
     * </pre>
     * @see PairingMode
     */
    enum class SMPPairingState : uint8_t {
        /** No pairing in process. Current PairingMode shall be PairingMode::NONE. */
        NONE                        = 0,

        /** Pairing failed. Current PairingMode shall be PairingMode::NONE. */
        FAILED                      = 1,

        /**
         * Phase 0: Pairing requested by responding (slave)  device via SMPSecurityReqMsg.<br>
         * Signals initiating (host) device to start the Pairing Feature Exchange.<br>
         * Current PairingMode shall be PairingMode::NEGOTIATING.
         */
        REQUESTED_BY_RESPONDER      = 2,

        /**
         * Phase 1: Pairing requested by initiating (master) device via SMPPairingMsg.<br>
         * Starts the Pairing Feature Exchange.<br>
         * Current PairingMode shall be PairingMode::NEGOTIATING.
         */
        FEATURE_EXCHANGE_STARTED    = 3,

        /**
         * Phase 1: Pairing responded by responding (slave)  device via SMPPairingMsg.<br>
         * Completes the Pairing Feature Exchange. Optional user input shall be given for Phase 2.<br>
         * Current PairingMode shall be set to a definitive value.
         */
        FEATURE_EXCHANGE_COMPLETED  = 4,

        /** Phase 2: Authentication (MITM) PASSKEY expected now, see PairingMode::PASSKEY_ENTRY_ini */
        PASSKEY_EXPECTED            = 5,
        /** Phase 2: Authentication (MITM) Numeric Comparison Reply expected now, see PairingMode::NUMERIC_COMPARE_ini */
        NUMERIC_COMPARE_EXPECTED    = 6,
        /** Phase 2: Authentication (MITM) OOB data expected now, see PairingMode::OUT_OF_BAND */
        OOB_EXPECTED                = 7,

        /** Phase 3: Key & value distribution started after SMPPairConfirmMsg or SMPPairPubKeyMsg (LE Secure Connection) exchange between initiating (master) and responding (slave) device. */
        KEY_DISTRIBUTION            = 8,

        /**
         * Phase 3: Key & value distribution completed by responding (slave) device sending SMPIdentInfoMsg (#1) , SMPIdentAddrInfoMsg (#2) or SMPSignInfoMsg (#3),<br>
         * depending on the key distribution field SMPKeyDistFormat SMPPairingMsg::getInitKeyDist() and SMPPairingMsg::getRespKeyDist().<br>
         * <p>
         * The link is assumed to be encrypted from here on and AdapterStatusListener::deviceReady() gets called on all listener.
         * </p>
         */
        COMPLETED                   = 9
    };
    std::string to_string(const SMPPairingState state) noexcept;

    /**
     * Returns true if the given SMPPairingState indicates an active pairing process,
     * i.e. none of the following terminal states: ::SMPPairingState::COMPLETED, ::SMPPairingState::FAILED or ::SMPPairingState::NONE
     */
    constexpr bool isSMPPairingActive(const SMPPairingState state) noexcept {
        return SMPPairingState::COMPLETED != state &&
               SMPPairingState::FAILED    != state &&
               SMPPairingState::NONE      != state;
    }

    /**
     * Returns true if the given SMPPairingState indicates a finished pairing process,
     * i.e. one of the following terminal states: ::SMPPairingState::COMPLETED or ::SMPPairingState::FAILED
     */
    constexpr bool hasSMPPairingFinished(const SMPPairingState state) noexcept {
        return SMPPairingState::COMPLETED == state ||
               SMPPairingState::FAILED    == state;
    }

    /**
     * Returns true if the given SMPPairingState indicates expected user interaction,
     * i.e. one of the following states: ::SMPPairingState::PASSKEY_EXPECTED, ::SMPPairingState::NUMERIC_COMPARE_EXPECTED or ::SMPPairingState::OOB_EXPECTED
     */
    constexpr bool isSMPPairingUserInteraction(const SMPPairingState state) noexcept {
        return SMPPairingState::PASSKEY_EXPECTED         == state ||
               SMPPairingState::NUMERIC_COMPARE_EXPECTED == state ||
               SMPPairingState::OOB_EXPECTED             == state;
    }

    /**
     * Returns true if the given SMPPairingState indicates a pairing process waiting for user input,
     * i.e. one of the following terminal states: ::SMPPairingState::FEATURE_EXCHANGE_STARTED,
     * ::SMPPairingState::FEATURE_EXCHANGE_COMPLETED or the given `inputSpec`.
     *
     * @param inputSpec should be one of ::SMPPairingState::PASSKEY_EXPECTED, ::SMPPairingState::NUMERIC_COMPARE_EXPECTED or ::SMPPairingState::OOB_EXPECTED.
     */
    constexpr bool isSMPPairingAllowingInput(const SMPPairingState state, const SMPPairingState inputSpec) noexcept {
        return SMPPairingState::FEATURE_EXCHANGE_STARTED   == state ||
               SMPPairingState::FEATURE_EXCHANGE_COMPLETED == state ||
               inputSpec                                   == state;
    }

    /**
     * Vol 3, Part H, 2.3.2 IO capabilities
     */
    enum class SMPIOCapability : uint8_t {
        /** Display output only, value 0. */
        DISPLAY_ONLY                = 0x00,
        /** Display output and boolean confirmation input keys only, value 1. */
        DISPLAY_YES_NO              = 0x01,
        /** Keyboard input only, value 2. */
        KEYBOARD_ONLY               = 0x02,
        /** No input not output, value 3. */
        NO_INPUT_NO_OUTPUT          = 0x03,
        /** Display output and keyboard input, value 4. */
        KEYBOARD_DISPLAY            = 0x04,
        /** Denoting unset value, i.e. not defined. */
        UNSET                       = 0xFF,
    };
    constexpr SMPIOCapability to_SMPIOCapability(const uint8_t v) noexcept {
        if( v <= 4 ) {
            return static_cast<SMPIOCapability>(v);
        }
        return SMPIOCapability::UNSET;
    }
    constexpr uint8_t number(const SMPIOCapability rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const SMPIOCapability ioc) noexcept;
    constexpr bool hasSMPIOCapabilityBinaryInput(const SMPIOCapability ioc) noexcept {
        return ioc == SMPIOCapability::DISPLAY_YES_NO ||
               ioc == SMPIOCapability::KEYBOARD_ONLY ||
               ioc == SMPIOCapability::KEYBOARD_DISPLAY;
    }
    constexpr bool hasSMPIOCapabilityFullInput(const SMPIOCapability ioc) noexcept {
        return ioc == SMPIOCapability::KEYBOARD_ONLY ||
               ioc == SMPIOCapability::KEYBOARD_DISPLAY;
    }

    /**
     * Vol 3, Part H, 2.3.3 OOB authentication data
     *
     */
    enum class SMPOOBDataFlag : uint8_t {
        OOB_AUTH_DATA_NOT_PRESENT    = 0x00,/**< OOB_AUTH_DATA_NOT_PRESENT */
        OOB_AUTH_DATA_REMOTE_PRESENT = 0x01 /**< OOB_AUTH_DATA_REMOTE_PRESENT */
    };
    constexpr uint8_t number(const SMPOOBDataFlag rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const SMPOOBDataFlag v) noexcept;

    /**
     * SMP Authentication Requirements Bits, denotes specific bits or whole protocol uint8_t bit-mask.
     * <pre>
     * BT Core Spec v5.2: Vol 3, Part H (SM): 3.5.1 SMP Pairing Request
     * BT Core Spec v5.2: Vol 3, Part H (SM): 3.5.2 SMP Pairing Response
     * BT Core Spec v5.2: Vol 3, Part H (SM): 3.6.7 SMP Security Request
     *
     * BT Core Spec v5.2: Vol 1, Part A, 5.4 LE SECURITY
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.1 Security Properties
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.2 Authentication stage 1 – Just Works or Numeric Comparison
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.3 Authentication stage 1 – Passkey Entry
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.4 Authentication stage 1 – Out of Band
     *
     * </pre>
     *
     * Layout LSB -> MSB
     * <pre>
     * uint8_t bonding_flags : 2, mitm : 1, sc : 1, keypress : 1, ct2 : 1, rfu : 2;
     * </pre>
     */
    enum class SMPAuthReqs : uint8_t {
        NONE                        =          0,
        /**
         * Indicate bonding being requested by the initiating device.
         */
        BONDING                     = 0b00000001,
        /** Reserved for future use */
        BONDING_RFU                 = 0b00000010,
        /**
         * A device sets the MITM flag to one to request an Authenticated security property
         * for the STK when using LE legacy pairing
         * and the LTK when using LE Secure Connections.
         * <p>
         * MITM protection can be secured by the following authenticated PairingMode
         * <pre>
         * - PairingMode::PASSKEY_ENTRY best
         * - PairingMode::NUMERIC_COMPARISON good
         * - PairingMode::OUT_OF_BAND good, depending on the OOB data
         * </pre>
         * </p>
         * <p>
         * Unauthenticated PairingMode::JUST_WORKS gives no MITM protection.
         * </p>
         * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.1 Security Properties
         */
        MITM                        = 0b00000100,
        /**
         * If LE Secure Connections pairing is supported by
         * the device, then the SC field shall be set to 1, otherwise it shall be set to 0.
         * If both devices support LE Secure Connections pairing,
         * then LE Secure Connections pairing shall be used,
         * otherwise LE Legacy pairing shall be used.
         */
        SECURE_CONNECTIONS          = 0b00001000,
        /**
         * The keypress field is used only in the Passkey Entry
         * protocol and shall be ignored in other protocols.
         * When both sides set that field to one,
         * Keypress notifications shall be generated and sent using SMP
         * Pairing Keypress Notification PDUs.
         */
        KEYPRESS                    = 0b00010000,
        /**
         * The CT2 field shall be set to 1 upon transmission to indicate
         * support for the h7 function.
         * <pre>
         * See sections:
         * - 2.4.2.4 Derivation of BR/EDR link key from LE LTK
         * - 2.4.2.5 Derivation of LE LTK from BR/EDR link key
         * </pre>
         */
        CT2_H7_FUNC_SUPPORT         = 0b00100000,
        /** Reserved for future use */
        RFU_1                       = 0b01000000,
        /** Reserved for future use */
        RFU_2                       = 0b10000000
    };
    constexpr SMPAuthReqs operator ^(const SMPAuthReqs lhs, const SMPAuthReqs rhs) noexcept {
        return static_cast<SMPAuthReqs> ( static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs) );
    }
    constexpr SMPAuthReqs operator |(const SMPAuthReqs lhs, const SMPAuthReqs rhs) noexcept {
        return static_cast<SMPAuthReqs> ( static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs) );
    }
    constexpr SMPAuthReqs operator &(const SMPAuthReqs lhs, const SMPAuthReqs rhs) noexcept {
        return static_cast<SMPAuthReqs> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
    }
    constexpr bool operator ==(const SMPAuthReqs lhs, const SMPAuthReqs rhs) noexcept {
        return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs);
    }
    constexpr bool operator !=(const SMPAuthReqs lhs, const SMPAuthReqs rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr uint8_t number(const SMPAuthReqs rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr bool is_set(const SMPAuthReqs mask, const SMPAuthReqs bit) noexcept {
        return bit == ( mask & bit );
    }
    std::string to_string(const SMPAuthReqs mask) noexcept;

    /**
     * Returns the PairingMode derived from both devices' sets of SMPAuthReqs, SMPIOCapability and SMPOOBDataFlag
     * <pre>
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.6 (STK, le_sc_all_supported==false)
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.7 (LTK, le_sc_all_supported==true)
     *
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.1 Security Properties
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.2 Authentication stage 1 – Just Works or Numeric Comparison
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.3 Authentication stage 1 – Passkey Entry
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.6.4 Authentication stage 1 – Out of Band
     * </pre>
     *
     * @param use_sc true if both devices support Secure Connections pairing, otherwise false for legacy pairing.
     * @param authReqs_ini SMPAuthReqs of initiator
     * @param ioCap_ini SMPIOCapability of initiator
     * @param oobFlag_ini SMPOOBDataFlag of initiator
     * @param authReqs_res SMPAuthReqs of responder
     * @param ioCap_res SMPIOCapability of responder
     * @param oobFlag_res SMPOOBDataFlag of responder
     * @return resulting PairingMode
     */
    PairingMode getPairingMode(const bool use_sc,
                               const SMPAuthReqs authReqs_ini, const SMPIOCapability ioCap_ini, const SMPOOBDataFlag oobFlag_ini,
                               const SMPAuthReqs authReqs_res, const SMPIOCapability ioCap_res, const SMPOOBDataFlag oobFlag_res) noexcept;

    /**
     * Returns the PairingMode derived from both devices' SMPIOCapability
     * <pre>
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.8
     * </pre>
     *
     * @param use_sc true if both devices support Secure Connections pairing, otherwise false for legacy pairing.
     * @param ioCap_ini SMPIOCapability of initiator
     * @param ioCap_res SMPIOCapability of responder
     * @return resulting PairingMode
     */
    PairingMode getPairingMode(const bool use_sc,
                               const SMPIOCapability ioCap_ini, const SMPIOCapability ioCap_res) noexcept;

    /**
     * SMP Key Type for Distribution, indicates keys distributed in the Transport Specific Key Distribution phase.
     * <pre>
     * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
     * See also Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
     * </pre>
     * </p>
     * Layout LSB -> MSB
     * <pre>
     * uint8_t EncKey : 1, IdKey : 1, SignKey : 1, LinkKey : 1, RFU : 4;
     * </pre>
     */
    enum class SMPKeyType : uint8_t {
        NONE                        =          0,
        /**
         * LE legacy pairing: Indicates device shall distribute LTK using the Encryption Information command,
         * followed by EDIV and Rand using the Master Identification command.
         * <p>
         * LE Secure Connections pairing (SMP on LE transport): Ignored,
         * EDIV and Rand shall be zero and shall not be distributed.
         * </p>
         * <p>
         * SMP on BR/EDR transport: Indicates device likes to derive LTK from BR/EDR Link Key.<br>
         * When EncKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
         * the procedures for calculating the LTK from the BR/EDR Link Key shall be used.
         * </p>
         */
        ENC_KEY                     = 0b00000001,
        /**
         * Indicates that the device shall distribute IRK using the Identity Information command
         * followed by its public device or status random address using Identity Address Information.
         */
        ID_KEY                      = 0b00000010,
        /**
         * Indicates that the device shall distribute CSRK using the Signing Information command.
         */
        SIGN_KEY                    = 0b00000100,
        /**
         * SMP on the LE transport: Indicate that the device would like to derive the Link Key from the LTK.<br>
         * When LinkKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
         * the procedures for calculating the BR/EDR link key from the LTK shall be used.<br>
         * Devices not supporting LE Secure Connections shall set this bit to zero and ignore it on reception.
         * <p>
         * SMP on BR/EDR transport: Reserved for future use.
         * </p>
         */
        LINK_KEY                    = 0b00001000,
        /** Reserved for future use */
        RFU_1                       = 0b00010000,
        /** Reserved for future use */
        RFU_2                       = 0b00100000,
        /** Reserved for future use */
        RFU_3                       = 0b01000000,
        /** Reserved for future use */
        RFU_4                       = 0b10000000
    };
    constexpr SMPKeyType operator ^(const SMPKeyType lhs, const SMPKeyType rhs) noexcept {
        return static_cast<SMPKeyType> ( static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs) );
    }
    constexpr SMPKeyType& operator ^=(SMPKeyType& store, const SMPKeyType& rhs) noexcept {
        store = static_cast<SMPKeyType> ( static_cast<uint8_t>(store) ^ static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPKeyType operator |(const SMPKeyType lhs, const SMPKeyType rhs) noexcept {
        return static_cast<SMPKeyType> ( static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs) );
    }
    constexpr SMPKeyType& operator |=(SMPKeyType& store, const SMPKeyType& rhs) noexcept {
        store = static_cast<SMPKeyType> ( static_cast<uint8_t>(store) | static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPKeyType operator &(const SMPKeyType lhs, const SMPKeyType rhs) noexcept {
        return static_cast<SMPKeyType> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
    }
    constexpr SMPKeyType& operator &=(SMPKeyType& store, const SMPKeyType& rhs) noexcept {
        store = static_cast<SMPKeyType> ( static_cast<uint8_t>(store) & static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr bool operator ==(const SMPKeyType lhs, const SMPKeyType rhs) noexcept {
        return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs);
    }
    constexpr bool operator !=(const SMPKeyType lhs, const SMPKeyType rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr uint8_t number(const SMPKeyType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr bool is_set(const SMPKeyType mask, const SMPKeyType bit) noexcept {
        return bit == ( mask & bit );
    }
    std::string to_string(const SMPKeyType mask) noexcept;

    /**
     * SMP Long Term Key, used for platform agnostic persistence.
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     * <p>
     * Byte layout must be synchronized with java org.tinyb.SMPLongTermKey
     * </p>
     */
    __pack( struct SMPLongTermKey {
        /**
         * SMPLongTermKey Property Bits
         */
        enum class Property : uint8_t {
            /** No specific property */
            NONE = 0x00,
            /** Responder Key (LL slave). Absence indicates Initiator Key (LL master). */
            RESPONDER = 0x01,
            /** Authentication used. */
            AUTH = 0x02,
            /** Secure Connection used. */
            SC   = 0x04
        };
        static constexpr uint8_t number(const Property rhs) noexcept {
            return static_cast<uint8_t>(rhs);
        }
        static std::string getPropertyString(const Property mask) noexcept;

        /** SMPLongTermKey::Property bit mask. */
        Property properties;
        /** Encryption Size, zero if key is invalid. */
        uint8_t enc_size;
        /** Encryption Diversifier */
        uint16_t ediv;
        /** Random Number */
        uint64_t rand;
        /** Long Term Key (LTK) */
        jau::uint128_t ltk;

        // 28 bytes

        constexpr bool isValid() const noexcept { return 0 != enc_size; }

        bool isResponder() const noexcept;

        void clear() noexcept {
            bzero(reinterpret_cast<void *>(this), sizeof(SMPLongTermKey));
        }

        std::string toString() const noexcept { // hex-fmt aligned with btmon
            return "LTK[props "+getPropertyString(properties)+", enc_size "+std::to_string(enc_size)+
                   ", ediv "+jau::bytesHexString(reinterpret_cast<const uint8_t *>(&ediv), 0, sizeof(ediv), true /* lsbFirst */)+
                   ", rand "+jau::bytesHexString(reinterpret_cast<const uint8_t *>(&rand), 0, sizeof(rand), true /* lsbFirst */)+
                   ", ltk "+jau::bytesHexString(ltk.data, 0, sizeof(ltk), true /* lsbFirst */)+
                   ", valid "+std::to_string(isValid())+
                   "]";
        }
    } );
    constexpr SMPLongTermKey::Property operator ~(const SMPLongTermKey::Property rhs) noexcept {
        return static_cast<SMPLongTermKey::Property> ( ~static_cast<uint8_t>(rhs) );
    }
    constexpr SMPLongTermKey::Property operator ^(const SMPLongTermKey::Property lhs, const SMPLongTermKey::Property rhs) noexcept {
        return static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs) );
    }
    constexpr SMPLongTermKey::Property& operator ^=(SMPLongTermKey::Property& store, const SMPLongTermKey::Property& rhs) noexcept {
        store = static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(store) ^ static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPLongTermKey::Property operator |(const SMPLongTermKey::Property lhs, const SMPLongTermKey::Property rhs) noexcept {
        return static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs) );
    }
    constexpr SMPLongTermKey::Property& operator |=(SMPLongTermKey::Property& store, const SMPLongTermKey::Property& rhs) noexcept {
        store = static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(store) | static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPLongTermKey::Property operator &(const SMPLongTermKey::Property lhs, const SMPLongTermKey::Property rhs) noexcept {
        return static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
    }
    constexpr SMPLongTermKey::Property& operator &=(SMPLongTermKey::Property& store, const SMPLongTermKey::Property& rhs) noexcept {
        store = static_cast<SMPLongTermKey::Property> ( static_cast<uint8_t>(store) & static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr bool operator ==(const SMPLongTermKey::Property lhs, const SMPLongTermKey::Property rhs) noexcept {
        return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs);
    }
    constexpr bool operator !=(const SMPLongTermKey::Property lhs, const SMPLongTermKey::Property rhs) noexcept {
        return !( lhs == rhs );
    }
    inline std::string to_String(const SMPLongTermKey& ltk) noexcept { return ltk.toString(); }

    /**
     * SMP Identity Resolving Key, used for platform agnostic persistence.
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     * <p>
     * Byte layout must be synchronized with java org.tinyb.SMPIdentityResolvingKey
     * </p>
     */
    __pack( struct SMPIdentityResolvingKey {
        /**
         * SMPIdentityResolvingKey Property Bits
         */
        enum class Property : uint8_t {
            /** No specific property */
            NONE = 0x00,
            /** Responder Key (LL slave). Absence indicates Initiator Key (LL master). */
            RESPONDER = 0x01,
            /** Authentication used. */
            AUTH = 0x02
        };
        static constexpr uint8_t number(const Property rhs) noexcept {
            return static_cast<uint8_t>(rhs);
        }
        static std::string getPropertyString(const Property mask) noexcept;

        /** SMPIdentityResolvingKey::Property bit mask. */
        Property properties;
        /** Identity Resolving Key (IRK) */
        jau::uint128_t irk;

        bool isResponder() const noexcept;

        void clear() noexcept {
            bzero(reinterpret_cast<void *>(this), sizeof(SMPIdentityResolvingKey));
        }

        std::string toString() const noexcept { // hex-fmt aligned with btmon
            return "IRK[props "+getPropertyString(properties)+
                   ", irk "+jau::bytesHexString(irk.data, 0, sizeof(irk), true /* lsbFirst */)+
                   "]";
        }
    } );
    constexpr SMPIdentityResolvingKey::Property operator ^(const SMPIdentityResolvingKey::Property lhs, const SMPIdentityResolvingKey::Property rhs) noexcept {
        return static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs) );
    }
    constexpr SMPIdentityResolvingKey::Property& operator ^=(SMPIdentityResolvingKey::Property& store, const SMPIdentityResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(store) ^ static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPIdentityResolvingKey::Property operator |(const SMPIdentityResolvingKey::Property lhs, const SMPIdentityResolvingKey::Property rhs) noexcept {
        return static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs) );
    }
    constexpr SMPIdentityResolvingKey::Property& operator |=(SMPIdentityResolvingKey::Property& store, const SMPIdentityResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(store) | static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPIdentityResolvingKey::Property operator &(const SMPIdentityResolvingKey::Property lhs, const SMPIdentityResolvingKey::Property rhs) noexcept {
        return static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
    }
    constexpr SMPIdentityResolvingKey::Property& operator &=(SMPIdentityResolvingKey::Property& store, const SMPIdentityResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPIdentityResolvingKey::Property> ( static_cast<uint8_t>(store) & static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr bool operator ==(const SMPIdentityResolvingKey::Property lhs, const SMPIdentityResolvingKey::Property rhs) noexcept {
        return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs);
    }
    constexpr bool operator !=(const SMPIdentityResolvingKey::Property lhs, const SMPIdentityResolvingKey::Property rhs) noexcept {
        return !( lhs == rhs );
    }
    inline std::string to_String(const SMPIdentityResolvingKey& csrk) noexcept { return csrk.toString(); }

    /**
     * SMP Signature Resolving Key, used for platform agnostic persistence.
     * <p>
     * One way for ATT Signed Write.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     * <p>
     * Byte layout must be synchronized with java org.tinyb.SMPSignatureResolvingKey
     * </p>
     */
    __pack( struct SMPSignatureResolvingKey {
        /**
         * SMPSignatureResolvingKey Property Bits
         */
        enum class Property : uint8_t {
            /** No specific property */
            NONE = 0x00,
            /** Responder Key (LL slave). Absence indicates Initiator Key (LL master). */
            RESPONDER = 0x01,
            /** Authentication used. */
            AUTH = 0x02
        };
        static constexpr uint8_t number(const Property rhs) noexcept {
            return static_cast<uint8_t>(rhs);
        }
        static std::string getPropertyString(const Property mask) noexcept;

        /** SMPSignatureResolvingKey::Property bit mask. */
        Property properties;
        /** Connection Signature Resolving Key (CSRK) */
        jau::uint128_t csrk;

        bool isResponder() const noexcept;

        void clear() noexcept {
            bzero(reinterpret_cast<void *>(this), sizeof(SMPSignatureResolvingKey));
        }

        std::string toString() const noexcept { // hex-fmt aligned with btmon
            return "CSRK[props "+getPropertyString(properties)+
                   ", csrk "+jau::bytesHexString(csrk.data, 0, sizeof(csrk), true /* lsbFirst */)+
                   "]";
        }
    } );
    constexpr SMPSignatureResolvingKey::Property operator ^(const SMPSignatureResolvingKey::Property lhs, const SMPSignatureResolvingKey::Property rhs) noexcept {
        return static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(lhs) ^ static_cast<uint8_t>(rhs) );
    }
    constexpr SMPSignatureResolvingKey::Property& operator ^=(SMPSignatureResolvingKey::Property& store, const SMPSignatureResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(store) ^ static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPSignatureResolvingKey::Property operator |(const SMPSignatureResolvingKey::Property lhs, const SMPSignatureResolvingKey::Property rhs) noexcept {
        return static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs) );
    }
    constexpr SMPSignatureResolvingKey::Property& operator |=(SMPSignatureResolvingKey::Property& store, const SMPSignatureResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(store) | static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr SMPSignatureResolvingKey::Property operator &(const SMPSignatureResolvingKey::Property lhs, const SMPSignatureResolvingKey::Property rhs) noexcept {
        return static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
    }
    constexpr SMPSignatureResolvingKey::Property& operator &=(SMPSignatureResolvingKey::Property& store, const SMPSignatureResolvingKey::Property& rhs) noexcept {
        store = static_cast<SMPSignatureResolvingKey::Property> ( static_cast<uint8_t>(store) & static_cast<uint8_t>(rhs) );
        return store;
    }
    constexpr bool operator ==(const SMPSignatureResolvingKey::Property lhs, const SMPSignatureResolvingKey::Property rhs) noexcept {
        return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs);
    }
    constexpr bool operator !=(const SMPSignatureResolvingKey::Property lhs, const SMPSignatureResolvingKey::Property rhs) noexcept {
        return !( lhs == rhs );
    }
    inline std::string to_String(const SMPSignatureResolvingKey& csrk) noexcept { return csrk.toString(); }

    /**
     * Local SMP Link Key, used for platform agnostic persistence,
     * mapping to platform specific MgmtLoadLinkKeyCmd and MgmtEvtNewLinkKey.
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     * <p>
     * Byte layout must be synchronized with java org.tinyb.SMPLinkKey
     * </p>
     */
    __pack( struct SMPLinkKey {
        /**
         * Link Key type compatible with Mgmt's MgmtLinkKeyType and hence MgmtLinkKeyInfo
         */
        enum class KeyType : uint8_t {
            /** Combination key */
            COMBI             = 0x00,
            /** Local Unit key */
            LOCAL_UNIT        = 0x01,
            /** Remote Unit key */
            REMOTE_UNIT       = 0x02,
            /** Debug Combination key */
            DBG_COMBI         = 0x03,
            /** Unauthenticated Combination key from P-192 */
            UNAUTH_COMBI_P192 = 0x04,
            /** Authenticated Combination key from P-192 */
            AUTH_COMBI_P192   = 0x05,
            /** Changed Combination key */
            CHANGED_COMBI     = 0x06,
            /** Unauthenticated Combination key from P-256 */
            UNAUTH_COMBI_P256 = 0x07,
            /** Authenticated Combination key from P-256 */
            AUTH_COMBI_P256   = 0x08,
            /** Denoting no or invalid link key type */
            NONE              = 0xff
        };
        static constexpr uint8_t number(const KeyType rhs) noexcept {
            return static_cast<uint8_t>(rhs);
        }
        static std::string getTypeString(const KeyType type) noexcept;

        bool responder;
        KeyType type;
        jau::uint128_t key;
        uint8_t pin_length;

        constexpr bool isValid() const noexcept { return KeyType::NONE != type; }

        /**
         * Returns true if the type is a combination key,
         * i.e. used for BTRole::Master and BTRole::Slave.
         *
         * This is usally being true when using Secure Connections (SC).
         */
        constexpr bool isCombiKey() const noexcept {
            switch(type) {
                case KeyType::COMBI:
                case KeyType::DBG_COMBI:
                case KeyType::UNAUTH_COMBI_P192:
                case KeyType::AUTH_COMBI_P192:
                case KeyType::CHANGED_COMBI:
                case KeyType::UNAUTH_COMBI_P256:
                case KeyType::AUTH_COMBI_P256:
                    return true;
                default:
                    return false;
            }
        }

        bool isResponder() const noexcept { return responder; }

        void clear() noexcept {
            bzero(reinterpret_cast<void *>(this), sizeof(SMPLinkKey));
        }

        std::string toString() const noexcept { // hex-fmt aligned with btmon
            return "LK[resp "+std::to_string(responder)+", type "+getTypeString(type)+
                   ", key "+jau::bytesHexString(key.data, 0, sizeof(key), true /* lsbFirst */)+
                   ", plen "+std::to_string(pin_length)+
                   "]";
        }
    } );

    /**
     * Handles the Security Manager Protocol (SMP) using Protocol Data Unit (PDU)
     * encoded messages over L2CAP channel.
     * <p>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.3 Command Format
     * </p>
     * <p>
     * Message format between both devices, negotiating security details.
     * </p>
     * <p>
     * Vol 3 (Host), Part H Security Manager Specification (SM): 1.2.1 Bit and byte ordering conventions<br>
     * Little-Endian: Multiple-octet fields shall be transmitted with the least significant octet first.
     * </p>
     * @see SMPAuthReqs
     */
    class SMPPDUMsg
    {
        public:
            /** SMP Command Codes Vol 3, Part H (SM): 3.3 */
            enum class Opcode : uint8_t {
                UNDEFINED                       = 0x00, // our own pseudo opcode, indicating no ATT PDU message

                PAIRING_REQUEST                 = 0x01,
                PAIRING_RESPONSE                = 0x02,
                PAIRING_CONFIRM                 = 0x03,
                PAIRING_RANDOM                  = 0x04,
                PAIRING_FAILED                  = 0x05,

                ENCRYPTION_INFORMATION          = 0x06,
                MASTER_IDENTIFICATION           = 0x07,
                IDENTITY_INFORMATION            = 0x08,
                IDENTITY_ADDRESS_INFORMATION    = 0x09,
                SIGNING_INFORMATION             = 0x0A,
                SECURITY_REQUEST                = 0x0B,

                PAIRING_PUBLIC_KEY              = 0x0C,
                PAIRING_DHKEY_CHECK             = 0x0D,
                PAIRING_KEYPRESS_NOTIFICATION   = 0x0E
            };
            static constexpr uint8_t number(const Opcode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getOpcodeString(const Opcode opc) noexcept;

        protected:
            friend class SMPHandler;

            void check_range() {
                pdu.check_range(0, getDataOffset()+getDataSize(), E_FILE_LINE);
            }

            void checkOpcode(const Opcode expected) const
            {
                const Opcode has = getOpcode();
                if( expected != has ) {
                    throw SMPOpcodeException("Has opcode "+jau::to_hexstring(number(has))+" "+getOpcodeString(has)+
                                     ", but expected "+jau::to_hexstring(number(expected))+" "+getOpcodeString(expected), E_FILE_LINE);
                }
            }
            void checkOpcode(const Opcode exp1, const Opcode exp2) const
            {
                const Opcode has = getOpcode();
                if( exp1 != has && exp2 != has ) {
                    throw SMPOpcodeException("Has opcode "+jau::to_hexstring(number(has))+" "+getOpcodeString(has)+
                                     ", but expected either "+jau::to_hexstring(number(exp1))+" "+getOpcodeString(exp1)+
                                     " or  "+jau::to_hexstring(number(exp1))+" "+getOpcodeString(exp1), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept {
                return "opcode="+jau::to_hexstring(number(getOpcode()))+" "+getOpcodeString(getOpcode())+
                        ", size[total="+std::to_string(pdu.size())+", param "+std::to_string(getPDUParamSize())+"]";
            }
            virtual std::string valueString() const noexcept {
                return "size "+std::to_string(getDataSize())+", data "
                        +jau::bytesHexString(pdu.get_ptr(), getDataOffset(), getDataSize(), true /* lsbFirst */);
            }

            /** actual received PDU */
            jau::POctets pdu;

            /** creation timestamp in milliseconds */
            uint64_t ts_creation;

        public:
            /**
             * Return a newly created specialized instance pointer to base class.
             * <p>
             * Returned memory reference is managed by caller (delete etc)
             * </p>
             */
            static std::unique_ptr<const SMPPDUMsg> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /** Persistent memory, w/ ownership ..*/
            SMPPDUMsg(const uint8_t* source, const jau::nsize_t size)
                : pdu(source, std::max<jau::nsize_t>(1, size), jau::endian::little),
                  ts_creation(jau::getCurrentMilliseconds())
            { }

            /** Persistent memory, w/ ownership ..*/
            SMPPDUMsg(const uint8_t* source, const jau::nsize_t size, const jau::nsize_t min_size)
                : pdu(source, std::max<jau::nsize_t>(1, size), jau::endian::little),
                  ts_creation(jau::getCurrentMilliseconds())
            { 
                pdu.check_range(0, std::max<jau::nsize_t>(1, min_size), E_FILE_LINE);
            }

            /** Persistent memory, w/ ownership ..*/
            SMPPDUMsg(const Opcode opc, const jau::nsize_t size)
                : pdu(std::max<jau::nsize_t>(1, size), jau::endian::little),
                  ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.put_uint8_nc(0, number(opc));
            }

            virtual ~SMPPDUMsg() noexcept = default;

            /**
             * Clone template for convenience, based on derived class's copy-constructor.
             * <pre>
             * const SMPPDUMsg & smpPDU;
             * const SMPSignInfoMsg * signInfo = static_cast<const SMPSignInfoMsg *>(&smpPDU);
             * SMPPDUMsg* b1 = SMPPDUMsg::clone(*signInfo);
             * SMPSignInfoMsg* b2 = SMPPDUMsg::clone(*signInfo);
             * </pre>
             * @tparam T The derived definite class type, deducible by source argument
             * @param source the source to be copied
             * @return a new instance.
             */
            template<class T>
            static T* clone(const T& source) noexcept { return new T(source); }

            constexpr uint64_t getTimestamp() const noexcept { return ts_creation; }

            /** SMP Command Codes Vol 3, Part H (SM): 3.3 */
            constexpr Opcode getOpcode() const noexcept {
                return static_cast<Opcode>(pdu.get_uint8_nc(0));
            }

            /**
             * Returns the actual PDU size less one octet for the opcode,
             * which should result in 0-22 octets or 64 octets.
             * <p>
             * Note that the PDU parameter include the data value below.
             * </p>
             * <p>
             * Use getDataSize() for the actual required data size
             * according to the specific packet.
             * </p>
             *
             * @see SMPPDUMsg::getDataSize()
             */
            constexpr jau::nsize_t getPDUParamSize() const noexcept {
                return pdu.size() - 1 /* opcode */;
            }

            /**
             * Returns the required data size according to the specified packet,
             * which should be within 0-22 or 64 octets.
             *
             * @see SMPPDUMsg::getPDUParamSize()
             */
            constexpr_cxx20 virtual jau::nsize_t getDataSize() const noexcept {
                return getPDUParamSize();
            }


            /**
             * Returns the octet offset to the data segment in this PDU
             * including the mandatory opcode,
             * i.e. the number of octets until the first value octet.
             */
            constexpr jau::nsize_t getDataOffset() const noexcept { return 1; /* default: opcode */ }

            virtual std::string getName() const noexcept {
                return "SMPPDUMsg";
            }

            virtual std::string toString() const noexcept{
                return getName()+"["+baseString()+", value["+valueString()+"]]";
            }
    };
    inline std::string to_String(const SMPPDUMsg& m) noexcept { return m.toString(); }

    /**
     * Tag type to group all SMP messages covering encryption keys,
     * treated as byte stream (all of them).
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPEncKeyByteStream : public SMPPDUMsg
    {
        public:
            /** Persistent memory, w/ ownership ..*/
            SMPEncKeyByteStream(const uint8_t* source, const jau::nsize_t size)
            : SMPPDUMsg(source, size) { }

            /** Persistent memory, w/ ownership ..*/
            SMPEncKeyByteStream(const Opcode opc, const jau::nsize_t size)
            : SMPPDUMsg(opc, size) { }

            ~SMPEncKeyByteStream() noexcept override = default;
    };

    /**
     * Vol 3, Part H: 3.5.1 Pairing Request message.<br>
     * Vol 3, Part H: 3.5.2 Pairing Response message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.3 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_REQUEST or Opcode::PAIRING_RESPONSE
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t io_capability
     * uint8_t oob_data_flag
     * uint8_t auth_req_mask
     * uint8_t max_encryption_key_size
     * uint8_t initiator_key_distribution
     * uint8_t responder_key_distribution
     * </pre>
     *
     * <br>
     * SMP Pairing Request Vol 3, Part H (SM): 3.5.1
     * <p>
     * Initiator starts the Pairing Feature Exchange
     * by sending a Pairing Request command to the responding device.
     * </p>
     * <p>
     * The rules for handing a collision between a pairing procedure
     * on the LE transport and a pairing procedure on the BR/EDR transport
     * are defined in Vol 3, Part C (GAP): 14.2 BRD/EDR/LE security aspects - Collision Handling.
     * </p>
     *
     * <br>
     * SMP Pairing Response Vol 3, Part H (SM): 3.5.2
     * <p>
     * Command is used by the responding device to complete the Pairing Feature Exchange
     * after it has received a Pairing Request command from the initiating device,
     * if the responding device allows pairing.
     * </p>
     * <p>
     * If a Pairing Request is received over the BR/EDR transport
     * when either cross-transport key derivation/generation is not supported
     * or the BR/EDR transport is not encrypted using a Link Key generated using P256,
     * a Pairing Failed shall be sent with the error code
     * SMPPairFailedMsg::ReasonCode::CROSSXPORT_KEY_DERIGEN_NOT_ALLOWED (Cross-Transport Key Derivation/Generation Not Allowed).
     * </p>
     * <p>
     * The rules for handing a collision between a pairing procedure
     * on the LE transport and a pairing procedure on the BR/EDR transport
     * are defined in Vol 3, Part C (GAP): 14.2 BRD/EDR/LE security aspects - Collision Handling.
     * </p>
     */
    class SMPPairingMsg final : public SMPPDUMsg
    {
        private:
            const bool request;
            const SMPAuthReqs authReqMask;
            const SMPKeyType initiator_key_dist;
            const SMPKeyType responder_key_dist;

        public:
            SMPPairingMsg(const bool request_, const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length, 7),
              request(request_),
              authReqMask(static_cast<SMPAuthReqs>( pdu.get_uint8_nc(3) )),
              initiator_key_dist(static_cast<SMPKeyType>(pdu.get_uint8_nc(5))),
              responder_key_dist(static_cast<SMPKeyType>(pdu.get_uint8_nc(6)))
            {
                checkOpcode(request? Opcode::PAIRING_REQUEST : Opcode::PAIRING_RESPONSE);
                check_range();
            }

            SMPPairingMsg(const bool request_,
                          const SMPIOCapability ioc, const SMPOOBDataFlag odf,
                          const SMPAuthReqs auth_req_mask, const uint8_t maxEncKeySize,
                          const SMPKeyType initiator_key_dist_,
                          const SMPKeyType responder_key_dist_)
            : SMPPDUMsg(request_? Opcode::PAIRING_REQUEST : Opcode::PAIRING_RESPONSE, 1+6),
              request(request_),
              authReqMask(auth_req_mask), initiator_key_dist(initiator_key_dist_), responder_key_dist(responder_key_dist_)
            {
                pdu.put_uint8(1, direct_bt::number(ioc));
                pdu.put_uint8(2, direct_bt::number(odf));
                pdu.put_uint8(3, direct_bt::number(authReqMask));
                pdu.put_uint8(4, maxEncKeySize);
                pdu.put_uint8(5, direct_bt::number(initiator_key_dist));
                pdu.put_uint8(6, direct_bt::number(responder_key_dist));
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 6;
            }

            /**
             * Returns the IO capability bit field.
             * <pre>
             * Vol 3, Part H, 2.3.2 IO capabilities
             * </pre>
             * @see IOCapability
             */
            constexpr SMPIOCapability getIOCapability() const noexcept {
                return static_cast<SMPIOCapability>(pdu.get_uint8_nc(1));
            }

            /**
             * Returns the OBB authenticate data flag
             * <pre>
             * Vol 3, Part H, 2.3.3 OOB authentication data
             * </pre>
             * @see OOBDataFlag
             */
            constexpr SMPOOBDataFlag getOOBDataFlag() const noexcept {
                return static_cast<SMPOOBDataFlag>(pdu.get_uint8_nc(2));
            }

            /**
             * Returns the Authentication Requirements mask.
             * <pre>
             * SMP Pairing Request Vol 3, Part H (SM): 3.5.1
             * SMP Pairing Response Vol 3, Part H (SM): 3.5.2
             * </pre>
             * @see AuthRequirements
             */
            constexpr SMPAuthReqs getAuthReqMask() const noexcept {
                return authReqMask;
            }
            constexpr bool isAuthRequirementBitSet(const SMPAuthReqs bit) const noexcept {
                return is_set(authReqMask, bit);
            }

            /**
             * This value defines the maximum encryption key size in octets that the device
             * can support. The maximum key size shall be in the range 7 to 16 octets.
             */
            constexpr uint8_t getMaxEncryptionKeySize() const noexcept {
                return pdu.get_uint8_nc(4);
            }
            /**
             * Returns the Initiator Key Distribution field,
             * which defines which keys the initiator shall
             * distribute and use during the Transport Specific Key Distribution phase.
             * <pre>
             * See Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
             * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
             * </pre>
             * @see SMPKeyDistFormat
             */
            constexpr SMPKeyType getInitKeyDist() const noexcept {
                return initiator_key_dist;
            }
            /**
             * Return the Responder Key Distribution field,
             * which defines which keys the responder shall
             * distribute and use during the Transport Specific Key Distribution phase.
             * <p>
             * See Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
             * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
             * </p>
             * @see SMPKeyDistFormat
             */
            constexpr SMPKeyType getRespKeyDist() const noexcept {
                return responder_key_dist;
            }

            std::string getName() const noexcept override {
                return "SMPPairingMsg";
            }

        protected:
            std::string valueString() const noexcept override {
                return "iocap "+to_string(getIOCapability())+
                       ", oob "+to_string(getOOBDataFlag())+
                       ", auth_req "+to_string(getAuthReqMask())+
                       ", max_keysz "+std::to_string(getMaxEncryptionKeySize())+
                       ", key_dist[init "+to_string(getInitKeyDist())+
                       ", resp "+to_string(getRespKeyDist())+
                       "]";
            }
    };

    /**
     * Vol 3, Part H: 3.5.3 Pairing Confirm message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_CONFIRM
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t confirm_value
     * </pre>
     *
     * Used following a successful Pairing Feature Exchange to start
     * STK Generation for LE legacy pairing and LTK Generation for LE Secure Connections pairing.
     * <p>
     * Command is used by both devices to send the confirm value to the peer device,<br>
     * see Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2 and<br>
     * and Vol 3, Part H, 2.3.5.6 SM - Pairing algo - LE Secure Connections pairing phase 2.
     * </p>
     * <p>
     * The initiating device starts key generation by sending the Pairing Confirm command to the responding device.<br>
     * If the initiating device wants to abort pairing it can transmit a Pairing Failed command instead.
     * </p>
     * <p>
     * The responding device sends the Pairing Confirm command
     * after it has received a Pairing Confirm command from the initiating device.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPPairConfirmMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPPairConfirmMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::PAIRING_CONFIRM);
            }

            SMPPairConfirmMsg(const jau::uint128_t & confirm_value)
            : SMPEncKeyByteStream(Opcode::PAIRING_CONFIRM, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, confirm_value);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Confirm value (16 octets)
             * <p>
             * In LE legacy pairing, the initiating device sends Mconfirm and the
             * responding device sends Sconfirm as defined in
             * Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2
             * </p>
             * <p>
             * In LE Secure Connections, Ca and Cb are defined in
             * Vol 3, Part H, Section 2.2.6 SM - Crypto Toolbox - LE Secure Connections confirm value generation function f4.<br>
             * See Vol 3, Part H, 2.3.5.6 SM - Pairing algo - LE Secure Connections pairing phase 2.
             * </p>
             */
            constexpr jau::uint128_t getConfirmValue() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPPairConfirm";
            }

        protected:
            std::string valueString() const noexcept override { // hex-fmt aligned with btmon
                return "size "+std::to_string(getDataSize())+", value "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.5.4 Pairing Random message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_RANDOM
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t random_value
     * </pre>
     *
     * Used by the initiating and responding device to send the
     * random number used to calculate the Confirm value sent in the Pairing Confirm command.
     * <p>
     * The initiating device sends a Pairing Random command
     * after it has received a Pairing Confirm command from the responding device.
     * </p>
     * <p>
     * LE legacy pairing: Responding device sends a Pairing Random command
     * after receiving a Pairing Random command from the initiating device
     * if the Confirm value calculated on the responding device matches the
     * Confirm value received from the initiating device.<br>
     * If the calculated Confirm value does not match
     * then the responding device shall respond with the Pairing Failed command.
     * </p>
     * <p>
     * LE Secure Connections: Responding device sends a Pairing Random command
     * after it has received a Pairing Random command from the initiating device
     * <i>if the Confirm value calculated on the responding device matches the
     * Confirm value received from the initiating device(?)</i>.<br>
     * If the calculated Confirm value does not match then the responding device
     * shall respond with the Pairing Failed command.
     * </p>
     * <p>
     * The initiating device shall encrypt the link using
     * the generated key (STK in LE legacy pairing or LTK in LE Secure Connections)
     * if the Confirm value calculated on the initiating device matches
     * the Confirm value received from the responding device.<br>
     * The successful encryption or re-encryption of the link is the signal
     * to the responding device that key generation has completed successfully.<br>
     * If the calculated Confirm value does not match then
     * the initiating device shall respond with the Pairing Failed command.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPPairRandMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPPairRandMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::PAIRING_RANDOM);
            }

            SMPPairRandMsg(const jau::uint128_t & random_value)
            : SMPEncKeyByteStream(Opcode::PAIRING_RANDOM, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, random_value);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Random value (16 octets)
             * <p>
             * In LE legacy pairing, the initiating device sends Mrand and the
             * responding device sends Srand as defined in
             * Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2
             * </p>
             * <p>
             * In LE Secure Connections,
             * the initiating device sends Na and the responding device sends Nb.
             * </p>
             */
            constexpr jau::uint128_t getRand() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPPairRand";
            }

        protected:
            std::string valueString() const noexcept override { // hex-fmt aligned with btmon
                return "size "+std::to_string(getDataSize())+", rand "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.5.5 Pairing Failed message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_FAILED
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t reason_code
     * </pre>
     */
    class SMPPairFailedMsg final : public SMPPDUMsg
    {
        public:
            enum class ReasonCode : uint8_t {
                UNDEFINED                   = 0x00,
                PASSKEY_ENTRY_FAILED        = 0x01,
                OOB_NOT_AVAILABLE           = 0x02,
                AUTHENTICATION_REQUIREMENTS = 0x03,
                CONFIRM_VALUE_FAILED        = 0x04,
                PAIRING_NOT_SUPPORTED       = 0x05,
                ENCRYPTION_KEY_SIZE         = 0x06,
                COMMON_NOT_SUPPORTED        = 0x07,
                UNSPECIFIED_REASON          = 0x08,
                REPEATED_ATTEMPTS           = 0x09,
                INVALID_PARAMTERS           = 0x0A,
                DHKEY_CHECK_FAILED          = 0x0B,
                NUMERIC_COMPARISON_FAILED   = 0x0C,
                BREDR_PAIRING_IN_PROGRESS   = 0x0D,
                CROSSXPORT_KEY_DERIGEN_NOT_ALLOWED = 0x0E
            };
            static constexpr uint8_t number(const ReasonCode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getReasonCodeString(const ReasonCode reasonCode) noexcept;

            SMPPairFailedMsg(const uint8_t* source, const jau::nsize_t length) 
            : SMPPDUMsg(source, length) 
            {
                check_range();
                checkOpcode(Opcode::PAIRING_FAILED);
            }

            SMPPairFailedMsg(const ReasonCode rc)
            : SMPPDUMsg(Opcode::PAIRING_FAILED, 1+1)
            {
                pdu.put_uint8(1, number(rc));
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            constexpr ReasonCode getReasonCode() const noexcept {
                return static_cast<ReasonCode>(pdu.get_uint8_nc(1));
            }

            std::string getName() const noexcept override {
                return "SMPPairFailed";
            }

        protected:
            std::string valueString() const noexcept override {
                const ReasonCode ec = getReasonCode();
                return jau::to_hexstring(number(ec)) + ": " + getReasonCodeString(ec);
            }
    };


    /**
     * Vol 3, Part H: 3.5.6 Pairing Public Key message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5.6 Pairing Public Key
     * </pre>
     *
     * Opcode::PAIRING_PUBLIC_KEY
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint256_t public_key_x_value
     * jau::uint256_t public_key_y_value
     * </pre>
     *
     * Message is used to transfer the device’s local public key (X and Y coordinates) to the remote device.<br>
     * This message is used by both the initiator and responder.<br>
     * This PDU is only used for LE Secure Connections.
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPPairPubKeyMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPPairPubKeyMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::PAIRING_PUBLIC_KEY);
            }

            SMPPairPubKeyMsg(const jau::uint256_t & pub_key_x, const jau::uint256_t & pub_key_y)
            : SMPEncKeyByteStream(Opcode::PAIRING_PUBLIC_KEY, 1+32+32)
            {
                jau::put_uint256(pdu.get_wptr(), 1,    pub_key_x);
                jau::put_uint256(pdu.get_wptr(), 1+32, pub_key_y);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 32+32;
            }

            /**
             * Returns the 256-bit Public Key X value (32 octets)
             */
            constexpr jau::uint256_t getPubKeyX() const noexcept { return jau::get_uint256(pdu.get_ptr(), 1); }

            /**
             * Returns the 256-bit Public Key Y value (32 octets)
             */
            constexpr jau::uint256_t getPubKeyY() const noexcept { return jau::get_uint256(pdu.get_ptr(), 1+32); }

            std::string getName() const noexcept override {
                return "SMPPairPubKey";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", pk_x "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, 32, true /* lsbFirst */)+
                        ", pk_y "+
                        jau::bytesHexString(pdu.get_ptr_nc(1+32), 0, 32, true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.5.7 Pairing DHKey Check message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_DHKEY_CHECK
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t dhkey_check_values
     * </pre>
     *
     * Message is used to transmit the 128-bit DHKey Check values (Ea/Eb) generated using f6.<br>
     * This message is used by both initiator and responder.<br>
     * This PDU is only used for LE Secure Connections.
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPPairDHKeyCheckMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPPairDHKeyCheckMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::PAIRING_DHKEY_CHECK);
            }

            SMPPairDHKeyCheckMsg(const jau::uint128_t & dhkey_check_values)
            : SMPEncKeyByteStream(Opcode::PAIRING_DHKEY_CHECK, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, dhkey_check_values);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit DHKey Check value (16 octets)
             */
            constexpr jau::uint128_t getDHKeyCheck() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPPairDHKeyCheck";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", dhkey_chk "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.5.8 Passkey Entry: Keypress notification messages.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_KEYPRESS_NOTIFICATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t notification_type
     * </pre>
     *
     * Message is used during the Passkey Entry protocol by a device with KeyboardOnly IO capabilities
     * to inform the remote device when keys have been entered or erased.
     */
    class SMPPasskeyNotification final : public SMPPDUMsg
    {
        public:
            enum class TypeCode : uint8_t {
                PASSKEY_ENTRY_STARTED       = 0x00,
                PASSKEY_DIGIT_ENTERED       = 0x01,
                PASSKEY_DIGIT_ERASED        = 0x02,
                PASSKEY_CLEARED             = 0x03,
                PASSKEY_ENTRY_COMPLETED     = 0x04
            };
            static constexpr uint8_t number(const TypeCode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getTypeCodeString(const TypeCode tc) noexcept;

            SMPPasskeyNotification(const uint8_t* source, const jau::nsize_t length) 
            : SMPPDUMsg(source, length) 
            {
                check_range();
                checkOpcode(Opcode::PAIRING_KEYPRESS_NOTIFICATION);
            }

            SMPPasskeyNotification(const TypeCode tc)
            : SMPPDUMsg(Opcode::PAIRING_KEYPRESS_NOTIFICATION, 1+1)
            {
                pdu.put_uint8_nc(1, number(tc));
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            constexpr TypeCode getTypeCode() const noexcept {
                return static_cast<TypeCode>(pdu.get_uint8_nc(1));
            }

            std::string getName() const noexcept override {
                return "SMPPasskeyNotify";
            }

        protected:
            std::string valueString() const noexcept override {
                const TypeCode ec = getTypeCode();
                return jau::to_hexstring(number(ec)) + ": " + getTypeCodeString(ec);
            }
    };

    /**
     * Vol 3, Part H: 3.6.2 Encryption Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6.1 Key distribution and generation
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.1 Definition of keys and values
     * </pre>
     *
     * Opcode::ENCRYPTION_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t long_term_key
     * </pre>
     *
     * Message is used in the LE legacy pairing Transport Specific Key Distribution
     * to distribute Long Term Key (LTK) that is used when encrypting future connections.
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated LTK.
     * </p>
     * <p>
     * Legacy: #1 in distribution, first value.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPEncInfoMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPEncInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::ENCRYPTION_INFORMATION);
            }

            SMPEncInfoMsg(const jau::uint128_t & long_term_key)
            : SMPEncKeyByteStream(Opcode::ENCRYPTION_INFORMATION, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, long_term_key);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Long Term Key (16 octets)
             * <p>
             * The generated LTK value being distributed,
             * see Vol 3, Part H, 2.4.2.3 SM - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr jau::uint128_t getLTK() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPEncInfo";
            }

        protected:
            std::string valueString() const noexcept override { // hex-fmt aligned with btmon
                return "size "+std::to_string(getDataSize())+", ltk "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.6.3 Master Identification message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6.1 Key distribution and generation
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.1 Definition of keys and values
     * </pre>
     *
     * Opcode::MASTER_IDENTIFICATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint16_t ediv
     * uint64_t rand
     * </pre>
     *
     * Message is used in the LE legacy pairing Transport Specific Key Distribution phase
     * to distribute Encrypted Diversifier (EDIV) and Random Number (Rand) which are used when encrypting future connections.
     *
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated LTK.
     * </p>
     * <p>
     * Legacy: #2 in distribution
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPMasterIdentMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPMasterIdentMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::MASTER_IDENTIFICATION);
            }

            SMPMasterIdentMsg(const uint16_t ediv, const uint64_t & rand)
            : SMPEncKeyByteStream(Opcode::MASTER_IDENTIFICATION, 1+2+8)
            {
                jau::put_uint16(pdu.get_wptr(), 1, ediv);
                jau::put_uint64(pdu.get_wptr(), 1+2, rand);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 10;
            }

            /**
             * Returns the 16-bit EDIV value (2 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint16_t getEDIV() const noexcept { return jau::get_uint16(pdu.get_ptr(), 1); }

            /**
             * Returns the 64-bit Rand value (8 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint64_t getRand() const noexcept { return jau::get_uint64(pdu.get_ptr(), 1+2); }

            std::string getName() const noexcept override {
                return "SMPMasterIdent";
            }

        protected:
            std::string valueString() const noexcept override { // hex-fmt aligned with btmon
                return "size "+std::to_string(getDataSize())+", ediv "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, 2, false /* lsbFirst */)+
                        ", rand "+
                        jau::bytesHexString(pdu.get_ptr_nc(1+2), 0, 8, false /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.6.4 Identify Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6.1 Key distribution and generation
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.1 Definition of keys and values
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.2.1 Generation of IRK
     * </pre>
     *
     * Opcode::IDENTITY_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t identity_resolving_key
     * </pre>
     *
     * Message is used in the Transport Specific Key Distribution phase to distribute Identity Resolving Key (IRK).
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     * <p>
     * Legacy: #3 in distribution<br>
     * Secure Connection: #1 in distribution, first value.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPIdentInfoMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPIdentInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::IDENTITY_INFORMATION);
            }

            SMPIdentInfoMsg(const jau::uint128_t & identity_resolving_key)
            : SMPEncKeyByteStream(Opcode::IDENTITY_INFORMATION, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, identity_resolving_key);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Identity Resolving Key (IRK, 16 octets)
             * <p>
             * The 128-bit IRK value being distributed,
             * see Vol 3, Part H, 2.4.2.1 SM - Definition of keys and values - Generation of IRK.
             * </p>
             */
            constexpr jau::uint128_t getIRK() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPIdentInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", irk "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };


    /**
     * Vol 3, Part H: 3.6.5 Identity Address Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6.1 Key distribution and generation
     * </pre>
     *
     * Opcode::IDENTITY_ADDRESS_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t address_type (0x01 static random, 0x00 public)
     * EUI48   address
     * </pre>
     *
     * Message is used in the Transport Specific Key Distribution phase
     * to distribute its public device address or static random address.
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     * <p>
     * Legacy: #4 in distribution<br>
     * Secure Connection: #2 in distribution
     * </p>
     */
    class SMPIdentAddrInfoMsg final : public SMPPDUMsg
    {
        public:
            SMPIdentAddrInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                check_range();
                checkOpcode(Opcode::IDENTITY_ADDRESS_INFORMATION);
            }

            SMPIdentAddrInfoMsg(const bool addrIsStaticRandom, const EUI48 & addr)
            : SMPPDUMsg(Opcode::IDENTITY_ADDRESS_INFORMATION, 1+1+6)
            {
                pdu.put_uint8(1, addrIsStaticRandom ? 0x01 : 0x00);
                pdu.put_eui48(1+1, addr);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 1+6;
            }

            /**
             * Returns whether the device address is static random (true) or public (false).
             */
            constexpr bool isStaticRandomAddress() const noexcept { return pdu.get_uint16_nc(1) == 0x01; }

            /**
             * Returns the device address
             */
            inline EUI48 getAddress() const noexcept { return pdu.get_eui48_nc(1+1); }

            std::string getName() const noexcept override {
                return "SMPIdentAddrInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                const std::string ats = isStaticRandomAddress() ? "static-random" : "public";
                return "address["+getAddress().toString()+", "+ats+"]";
            }
    };

    /**
     * Vol 3, Part H: 3.6.6 Signing Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6.1 Key distribution and generation
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.1 Definition of keys and values
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.4.2.2 Generation of CSRK
     * </pre>
     *
     * Opcode::SIGNING_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t signature_key
     * </pre>
     *
     * Message is used in the Transport Specific Key Distribution
     * to distribute the Connection Signature Resolving Key (CSRK), which a device uses to sign data (ATT Signed Write).
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     * <p>
     * Legacy: #5 in distribution, last value.<br>
     * Secure Connection: #3 in distribution, last value.
     * </p>
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as little-endian or as a byte stream.
     * </p>
     */
    class SMPSignInfoMsg final : public SMPEncKeyByteStream
    {
        public:
            SMPSignInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPEncKeyByteStream(source, length)
            {
                check_range();
                checkOpcode(Opcode::SIGNING_INFORMATION);
            }

            SMPSignInfoMsg(const jau::uint128_t & signature_key)
            : SMPEncKeyByteStream(Opcode::SIGNING_INFORMATION, 1+16)
            {
                jau::put_uint128(pdu.get_wptr(), 1, signature_key);
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Connection Signature Resolving Key (CSRK, 16 octets)
             * <p>
             * The 128-bit CSRK value being distributed,
             * see Vol 3, Part H, 2.4.2.2 SM - Definition of keys and values - Generation of CSRK.
             * </p>
             */
            constexpr jau::uint128_t getCSRK() const noexcept { return jau::get_uint128(pdu.get_ptr(), 1); }

            std::string getName() const noexcept override {
                return "SMPSignInfo";
            }

        protected:
            std::string valueString() const noexcept override { // hex-fmt aligned with btmon
                return "size "+std::to_string(getDataSize())+", csrk "+
                        jau::bytesHexString(pdu.get_ptr_nc(1), 0, getDataSize(), true /* lsbFirst */);
            }
    };

    /**
     * Vol 3, Part H: 3.6.7 Security Request message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::SECURITY_REQUEST
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t auth_req_mask
     * </pre>
     *
     * Message is used by the slave to request that the master initiates security with the requested security properties,<br>
     * see Vol 3 (Host), Part H (SM): 2 (SM), 2.4 SECURITY IN BLUETOOTH LOW ENERGY, 2.4.6 Slave Security Request
     */
    class SMPSecurityReqMsg final : public SMPPDUMsg
    {
        private:
            SMPAuthReqs authReqMask;
        public:
            SMPSecurityReqMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length, 2),
              authReqMask(static_cast<SMPAuthReqs>( pdu.get_uint8_nc(1) ))
            {
                check_range();
                checkOpcode(Opcode::SECURITY_REQUEST);
            }

            SMPSecurityReqMsg(const SMPAuthReqs auth_req_mask)
            : SMPPDUMsg(Opcode::SECURITY_REQUEST, 1+1), authReqMask(auth_req_mask)
            {
                pdu.put_uint8(1, direct_bt::number(authReqMask));
                check_range();
            }

            constexpr_cxx20 jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            /**
             * Returns the SMPPairingMsg::AuthRequirements (1 octet)
             * <p>
             * The AuthReq field is a bit field that indicates the requested security properties,<br>
             * see Vol 3 (Host), Part H (SM): 2 (SM), 2.3 Pairing Methods, 2.3.1 Security Properties,<br>
             * for the STK or LTK and GAP bonding information,<br>
             * see Vol 3 (Host), Part C (GAP): 9.4 Bonding Modes and Procedures<br>
             * </p>
             */
            constexpr SMPAuthReqs getAuthReqMask() const noexcept {
                return authReqMask;
            }
            constexpr bool isAuthRequirementBitSet(const SMPAuthReqs bit) const noexcept {
                return is_set(authReqMask, bit);
            }

            std::string getName() const noexcept override {
                return "SMPSecurityReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "auth_req "+to_string(getAuthReqMask());
            }
    };

    /**@}*/

} // namespace direct_bt

#endif /* SMP_TYPES_HPP_ */
