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

#ifndef BT_TYPES0_HPP_
#define BT_TYPES0_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <jau/java_uplink.hpp>
#include <jau/basic_types.hpp>
#include <jau/darray.hpp>
#include <jau/octets.hpp>
#include <jau/uuid.hpp>

#include "BTAddress.hpp"

#define JAVA_DBT_PACKAGE "jau/direct_bt/"
#define JAVA_MAIN_PACKAGE "org/direct_bt/"

namespace direct_bt {

    /** \addtogroup DBTUserAPI
     *
     *  @{
     */

    class BTException : public jau::RuntimeException {
        public:
        BTException(std::string const m, const char* file, int line) noexcept
        : RuntimeException("BTException", m, file, line) {}

        BTException(const char *m, const char* file, int line) noexcept
        : RuntimeException("BTException", m, file, line) {}
    };


    /**
     * Bluetooth roles from the perspective of the link layer (connection initiator).
     * @see [BTAdapter roles](@ref BTAdapterRoles).
     * @see [BTDevice roles](@ref BTDeviceRoles).
     * @since 2.4.0
     */
    enum class BTRole : uint8_t {
        /** Undefined role. */
        None       = 0,
        /** Master or *central* role, discovering remote devices and initiating connection. This is a ::GATTRole::Client. */
        Master     = 1,
        /** Slave or *peripheral* role, advertising and waiting for connections to accept. This is a ::GATTRole::Server. */
        Slave      = 2
    };
    constexpr uint8_t number(const BTRole rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr BTRole operator !(const BTRole rhs) noexcept {
        switch(rhs) {
            case BTRole::Master: return BTRole::Slave;
            case BTRole::Slave: return BTRole::Master;
            default: return BTRole::None;
        }
    }
    std::string to_string(const BTRole v) noexcept;

    /**
     * Bluetooth GATT roles
     * @see [BTGattHandler roles](@ref BTGattHandlerRoles).
     * @see [BTAdapter roles](@ref BTAdapterRoles).
     * @see [BTDevice roles](@ref BTDeviceRoles).
     * @since 2.4.0
     */
    enum class GATTRole : uint8_t {
        /** Undefined role. */
        None       = 0,
        /** Local GATT server role to a remote BTDevice ::BTRole::Master running a ::GATTRole::Client. Local BTAdapter is in ::BTRole::Slave role. */
        Server     = 1,
        /** Local GATT client role to a remote BTDevice ::BTRole::Slave running a ::GATTRole::Server. Local BTAdapter is in ::BTRole::Master role. */
        Client     = 2
    };
    constexpr uint8_t number(const GATTRole rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const GATTRole v) noexcept;

    /**
     * Bluetooth adapter operating mode
     */
    enum class BTMode : uint8_t {
        /** Zero mode, neither DUAL, BREDR nor LE. Usually an error. */
        NONE        = 0,
        /** Dual Bluetooth mode, i.e. BREDR + LE. */
        DUAL        = 1,
        /** BREDR only Bluetooth mode */
        BREDR       = 2,
        /** LE only Bluetooth mode */
        LE          = 3
    };
    constexpr uint8_t number(const BTMode rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const BTMode v) noexcept;

    /**
     * Maps the specified name to a constant of BTMode.
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link BTMode#NONE} if not supported.
     */
    BTMode to_BTMode(const std::string & value) noexcept;

    /**
     * HCI Supported Commands
     * <pre>
     * BT Core Spec v5.2: Vol 4, Part E, 6.27 (HCI) Supported Commands
     * BT Core Spec v5.2: Vol 4, Part E, 7.4.2 Read Local Supported Commands command
     * </pre>
     *
     * Unable to encode via enum and build-in integer: 64 octets = 512 bit wide!
     *
    enum class HCISupportedCommands : jau::uint512_t {
        NONE                    = 0,
        HCI_Create_Connection   = ...
    };
     */

    /**
     * LE Link Layer Feature Set
     * <pre>
     * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
     *
     * BT Core Spec v5.2: Vol 4, Part E, 7.8.3 LE Read Local Supported Features command
     *
     * BT Core Spec v5.2: Vol 4, Part E, 7.8.21 LE Read Remote Features command
     * BT Core Spec v5.2: Vol 4, Part E, 7.7.65.4 LE Read Remote Features Complete event
     *
     * BT Core Spec v5.2: Vol 6, Part B, 7.8.115 LE Set Host Feature Command
     * </pre>
     */
    enum class LE_Features : uint64_t {
        NONE                    =                                                                  0,/**< NONE */
        LE_Encryption           = 0b0000000000000000000000000000000000000000000000000000000000000001,/**< LE_Encryption */
        Conn_Param_Req_Proc     = 0b0000000000000000000000000000000000000000000000000000000000000010,/**< Conn_Param_Request_Proc */
        Ext_Rej_Ind             = 0b0000000000000000000000000000000000000000000000000000000000000100,
        SlaveInit_Feat_Exchg    = 0b0000000000000000000000000000000000000000000000000000000000001000,
        LE_Ping                 = 0b0000000000000000000000000000000000000000000000000000000000010000,
        LE_Data_Pkt_Len_Ext     = 0b0000000000000000000000000000000000000000000000000000000000100000,
        LL_Privacy              = 0b0000000000000000000000000000000000000000000000000000000001000000,
        Ext_Scan_Filter_Pol     = 0b0000000000000000000000000000000000000000000000000000000010000000,
        LE_2M_PHY               = 0b0000000000000000000000000000000000000000000000000000000100000000,
        Stable_Mod_Idx_Tx       = 0b0000000000000000000000000000000000000000000000000000001000000000,
        Stable_Mod_Idx_Rx       = 0b0000000000000000000000000000000000000000000000000000010000000000,
        LE_Coded_PHY            = 0b0000000000000000000000000000000000000000000000000000100000000000,
        LE_Ext_Adv              = 0b0000000000000000000000000000000000000000000000000001000000000000,
        LE_Per_Adv              = 0b0000000000000000000000000000000000000000000000000010000000000000,
        Chan_Sel_Algo_2         = 0b0000000000000000000000000000000000000000000000000100000000000000,
        LE_Pwr_Cls_1            = 0b0000000000000000000000000000000000000000000000001000000000000000,
        Min_Num_Used_Chan_Proc  = 0b0000000000000000000000000000000000000000000000010000000000000000,
        Conn_CTE_Req            = 0b0000000000000000000000000000000000000000000000100000000000000000,
        Conn_CTE_Res            = 0b0000000000000000000000000000000000000000000001000000000000000000,
        ConnLess_CTE_Tx         = 0b0000000000000000000000000000000000000000000010000000000000000000,
        ConnLess_CTE_Rx         = 0b0000000000000000000000000000000000000000000100000000000000000000,
        AoD                     = 0b0000000000000000000000000000000000000000001000000000000000000000,
        AoA                     = 0b0000000000000000000000000000000000000000010000000000000000000000,
        Rx_Const_Tone_Ext       = 0b0000000000000000000000000000000000000000100000000000000000000000, // bit #23
        Per_Adv_Sync_Tx_Sender  = 0b0000000000000000000000000000000000000001000000000000000000000000,
        Per_Adv_Sync_Tx_Rec     = 0b0000000000000000000000000000000000000010000000000000000000000000,
        Zzz_Clk_Acc_Upd         = 0b0000000000000000000000000000000000000100000000000000000000000000,
        Rem_Pub_Key_Val         = 0b0000000000000000000000000000000000001000000000000000000000000000,
        Conn_Iso_Stream_Master  = 0b0000000000000000000000000000000000010000000000000000000000000000,
        Conn_Iso_Stream_Slave   = 0b0000000000000000000000000000000000100000000000000000000000000000,
        Iso_Brdcst              = 0b0000000000000000000000000000000001000000000000000000000000000000,
        Sync_Rx                 = 0b0000000000000000000000000000000010000000000000000000000000000000,
        Iso_Chan                = 0b0000000000000000000000000000000100000000000000000000000000000000,
        LE_Pwr_Ctrl_Req         = 0b0000000000000000000000000000001000000000000000000000000000000000,
        LE_Pwr_Chg_Ind          = 0b0000000000000000000000000000010000000000000000000000000000000000,
        LE_Path_Loss_Mon        = 0b0000000000000000000000000000100000000000000000000000000000000000  // bit #35
    };
    constexpr uint64_t number(const LE_Features rhs) noexcept {
        return static_cast<uint64_t>(rhs);
    }
    constexpr LE_Features operator ^(const LE_Features lhs, const LE_Features rhs) noexcept {
        return static_cast<LE_Features> ( number(lhs) ^ number(rhs) );
    }
    constexpr LE_Features operator |(const LE_Features lhs, const LE_Features rhs) noexcept {
        return static_cast<LE_Features> ( number(lhs) | number(rhs) );
    }
    constexpr LE_Features operator &(const LE_Features lhs, const LE_Features rhs) noexcept {
        return static_cast<LE_Features> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const LE_Features lhs, const LE_Features rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const LE_Features lhs, const LE_Features rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr bool is_set(const LE_Features mask, const LE_Features bit) noexcept {
        return bit == ( mask & bit );
    }
    std::string to_string(const LE_Features mask) noexcept;

    /**
     * LE Transport PHY bit values
     * <pre>
     * BT Core Spec v5.2: Vol 4, Part E, 7.8.47 LE Read PHY command (we transfer the sequential value to this bitmask for unification)
     * BT Core Spec v5.2: Vol 4, Part E, 7.8.48 LE Set Default PHY command
     * </pre>
     */
    enum class LE_PHYs : uint8_t {
        NONE        = 0,
        LE_1M       = 0b00000001,
        LE_2M       = 0b00000010,
        LE_CODED    = 0b00000100
    };
    constexpr uint8_t number(const LE_PHYs rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr LE_PHYs operator ^(const LE_PHYs lhs, const LE_PHYs rhs) noexcept {
        return static_cast<LE_PHYs> ( number(lhs) ^ number(rhs) );
    }
    constexpr LE_PHYs operator |(const LE_PHYs lhs, const LE_PHYs rhs) noexcept {
        return static_cast<LE_PHYs> ( number(lhs) | number(rhs) );
    }
    constexpr LE_PHYs operator &(const LE_PHYs lhs, const LE_PHYs rhs) noexcept {
        return static_cast<LE_PHYs> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const LE_PHYs lhs, const LE_PHYs rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const LE_PHYs lhs, const LE_PHYs rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr bool is_set(const LE_PHYs mask, const LE_PHYs bit) noexcept {
        return bit == ( mask & bit );
    }
    std::string to_string(const LE_PHYs mask) noexcept;

    /**
     * Bluetooth Security Level.
     * <p>
     * This BTSecurityLevel is natively compatible
     * with Linux/BlueZ's BT_SECURITY values 1-4.
     * </p>
     */
    enum class BTSecurityLevel : uint8_t {
        /** Security Level not set, value 0. */
        UNSET         = 0,
        /** No encryption and no authentication. Also known as BT_SECURITY_LOW, value 1. */
        NONE          = 1,
        /** Encryption and no authentication (no MITM). Also known as BT_SECURITY_MEDIUM, value 2. */
        ENC_ONLY      = 2,
        /** Encryption and authentication (MITM). Also known as BT_SECURITY_HIGH, value 3. */
        ENC_AUTH      = 3,
        /** Authenticated Secure Connections. Also known as BT_SECURITY_FIPS, value 4. */
        ENC_AUTH_FIPS = 4
    };
    constexpr uint8_t number(const BTSecurityLevel rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr bool operator ==(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr bool operator <(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return number(lhs) < number(rhs);
    }
    constexpr bool operator <=(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return number(lhs) <= number(rhs);
    }
    constexpr bool operator >(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return number(lhs) > number(rhs);
    }
    constexpr bool operator >=(const BTSecurityLevel lhs, const BTSecurityLevel rhs) noexcept {
        return number(lhs) >= number(rhs);
    }
    constexpr BTSecurityLevel to_BTSecurityLevel(const uint8_t v) noexcept {
        if( 1 <= v && v <= 4 ) {
            return static_cast<BTSecurityLevel>(v);
        }
        return BTSecurityLevel::UNSET;
    }
    std::string to_string(const BTSecurityLevel v) noexcept;

    /**
     * Bluetooth secure pairing mode
     * <pre>
     * BT Core Spec v5.2: Vol 1, Part A, 5 Security Overview
     * BT Core Spec v5.2: Vol 1, Part A, 5.4 LE SECURITY
     * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.8
     * </pre>
     * @see SMPPairingState
     */
    enum class PairingMode : uint8_t {
        /** No pairing mode, implying no secure connections, no encryption and no MITM protection. */
        NONE                = 0,
        /** Pairing mode in negotiating, i.e. Pairing Feature Exchange in progress. */
        NEGOTIATING         = 1,
        /** Just Works. Random key exchange with encryption but no MITM protection. */
        JUST_WORKS          = 2,
        /** Passkey Entry input by initiator. Responder produces and display artifact. A known digit sequence (PIN) must be given as a secret to be validated on the device. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
        PASSKEY_ENTRY_ini   = 3,
        /** Passkey Entry input by responder. Initiator produces and display artifact. A known digit sequence (PIN) must be given as a secret to be validated on the device. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
        PASSKEY_ENTRY_res   = 4,
        /** Visual comparison of digit sequence (PIN) input by initiator, shown on both devices. Responder produces and display artifact. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
        NUMERIC_COMPARE_ini = 5,
        /** Visual comparison of digit sequence (PIN) input by responder, shown on both devices. Initiator produces and displays artifact. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
        NUMERIC_COMPARE_res = 6,
        /** Utilizing a second factor secret to be used as a secret, e.g. NFC field. Random key exchange with additional secret (2FA) and encryption and potential MITM protection. */
        OUT_OF_BAND         = 7,
        /** Reusing encryption keys from previous pairing. */
        PRE_PAIRED          = 8
    };
    constexpr uint8_t number(const PairingMode rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const PairingMode v) noexcept;

    /**
     * Meta ScanType as derived from BTMode,
     * with defined value mask consisting of BDAddressType bits.
     * <p>
     * This ScanType is natively compatible with DBTManager's implementation
     * for start and stop discovery.
     * </p>
     */
    enum class ScanType : uint8_t {
        NONE  = 0,
        BREDR = 1 << number(BDAddressType::BDADDR_BREDR),
        LE    = ( 1 << number(BDAddressType::BDADDR_LE_PUBLIC) ) | ( 1 << number(BDAddressType::BDADDR_LE_RANDOM) ),
        DUAL  = BREDR | LE
    };
    constexpr uint8_t number(const ScanType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr ScanType operator ~(const ScanType val) noexcept {
        return static_cast<ScanType> ( ~number(val) );
    }
    constexpr ScanType operator ^(const ScanType lhs, const ScanType rhs) noexcept {
        return static_cast<ScanType> ( number(lhs) ^ number(rhs) );
    }
    constexpr ScanType operator |(const ScanType lhs, const ScanType rhs) noexcept {
        return static_cast<ScanType> ( number(lhs) | number(rhs) );
    }
    constexpr ScanType operator &(const ScanType lhs, const ScanType rhs) noexcept {
        return static_cast<ScanType> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const ScanType lhs, const ScanType rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const ScanType lhs, const ScanType rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr ScanType changeScanType(const ScanType current, const ScanType changeType, const bool changeEnable) noexcept {
        return changeEnable ? ( current | changeType ) : ( current & ~changeType );
    }
    constexpr bool is_set(const ScanType current, const ScanType testType) noexcept {
        return testType == ( current & testType );
    }

    std::string to_string(const ScanType v) noexcept;

    ScanType to_ScanType(BTMode btMode);

    /**
     * LE Advertising (AD) Protocol Data Unit (PDU) Types
     * <p>
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.7.65.2 LE Advertising Report event
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.7.65.13 LE Extended Advertising Report event
     * BT Core Spec v5.2: Vol 6 LE Controller, Part B Link Layer: 2.3 Advertising physical channel PDU
     * BT Core Spec v5.2: Vol 6 LE Controller, Part B Link Layer: 2.3.1 Advertising PDUs
     * </p>
     */
    enum class AD_PDU_Type : uint8_t {
        /**
         * Advertising Indications (ADV_IND),
         * where a peripheral device requests connection to any central device
         * (i.e., not directed at a particular central device). */
        ADV_IND                     = 0x00,
        /** Similar to ADV_IND, yet the connection request is directed at a specific central device. */
        ADV_DIRECT_IND              = 0x01,
        /** Similar to ADV_IND, w/o connection requests and with the option additional information via scan responses. */
        ADV_SCAN_IND                = 0x02,
        /** Non connectable devices, advertising information to any listening device. */
        ADV_NONCONN_IND             = 0x03,
        /** Scan response PDU type. */
        SCAN_RSP                    = 0x04,

        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: ADV_IND variant */
        ADV_IND2                    = 0b0010011,
        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: ADV_DIRECT_IND variant */
        DIRECT_IND2                 = 0b0010101,
        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: ADV_SCAN_IND variant */
        SCAN_IND2                   = 0b0010010,
        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: ADV_NONCONN_IND variant */
        NONCONN_IND2                = 0b0010000,
        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: SCAN_RSP variant to an ADV_IND */
        SCAN_RSP_to_ADV_IND         = 0b0011011,
        /** EAD_Event_Type with EAD_Event_Type::LEGACY_PDU: SCAN_RSP variant to an ADV_SCAN_IND */
        SCAN_RSP_to_ADV_SCAN_IND    = 0b0011010,

        UNDEFINED                   = 0xff
    };
    constexpr uint8_t number(const AD_PDU_Type rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const AD_PDU_Type v) noexcept;


    /**
     * LE Extended Advertising (EAD) Event Types
     * <p>
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.7.65.13 LE Extended Advertising Report event
     * </p>
     */
    enum class EAD_Event_Type : uint16_t {
        NONE        = 0,
        CONN_ADV    = 0b00000001,
        SCAN_ADV    = 0b00000010,
        DIR_ADV     = 0b00000100,
        SCAN_RSP    = 0b00001000,
        LEGACY_PDU  = 0b00010000,
        DATA_B0     = 0b00100000,
        DATA_B1     = 0b01000000,
    };
    constexpr uint16_t number(const EAD_Event_Type rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }
    constexpr EAD_Event_Type operator |(const EAD_Event_Type lhs, const EAD_Event_Type rhs) noexcept {
        return static_cast<EAD_Event_Type> ( number(lhs) | number(rhs) );
    }
    constexpr EAD_Event_Type operator &(const EAD_Event_Type lhs, const EAD_Event_Type rhs) noexcept {
        return static_cast<EAD_Event_Type> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const EAD_Event_Type lhs, const EAD_Event_Type rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const EAD_Event_Type lhs, const EAD_Event_Type rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr bool is_set(const EAD_Event_Type mask, const EAD_Event_Type bit) noexcept { return bit == ( mask & bit ); }
    constexpr void set(EAD_Event_Type &mask, const EAD_Event_Type bit) noexcept { mask = mask | bit; }
    std::string to_string(const EAD_Event_Type v) noexcept;

    /**
     * HCI Whitelist connection type.
     */
    enum class HCIWhitelistConnectType : uint8_t {
        /** Report Connection: Only supported for LE on Linux .. */
        HCI_AUTO_CONN_REPORT = 0x00,
        /** Incoming Connections: Only supported type for BDADDR_BREDR (!LE) on Linux .. */
        HCI_AUTO_CONN_DIRECT = 0x01,
        /** Auto Connect: Only supported for LE on Linux .. */
        HCI_AUTO_CONN_ALWAYS = 0x02
    };
    constexpr uint8_t number(const HCIWhitelistConnectType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }

    enum class AD_Type_Const : uint8_t {
        AD_FLAGS_LIMITED_MODE_BIT = 0x01,
        AD_FLAGS_GENERAL_MODE_BIT = 0x02
    };

    enum class L2CAP_CID : uint16_t {
        UNDEFINED     = 0x0000,
        SIGNALING     = 0x0001,
        CONN_LESS     = 0x0002,
        A2MP          = 0x0003,

        /* BT Core Spec v5.2:  Vol 3, Part G GATT: 5.2.2 LE channel requirements */
        ATT           = 0x0004,

        LE_SIGNALING  = 0x0005,
        SMP           = 0x0006,
        SMP_BREDR     = 0x0007,
        DYN_START     = 0x0040,
        DYN_END       = 0xffff,
        LE_DYN_END    = 0x007f
    };
    constexpr uint16_t number(const L2CAP_CID rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }
    constexpr L2CAP_CID to_L2CAP_CID(const uint16_t v) noexcept { return static_cast<L2CAP_CID>(v); }
    std::string to_string(const L2CAP_CID v) noexcept;

    /**
     * Protocol Service Multiplexers (PSM) Assigned numbers
     * <https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control/>
     */
    enum class L2CAP_PSM : uint16_t {
        UNDEFINED         = 0x0000,
        SDP               = 0x0001,
        RFCOMM            = 0x0003,
        TCSBIN            = 0x0005,
        TCSBIN_CORDLESS   = 0x0007,
        BNEP              = 0x000F,
        HID_CONTROL       = 0x0011,
        HID_INTERRUPT     = 0x0013,
        UPNP              = 0x0015,
        AVCTP             = 0x0017,
        AVDTP             = 0x0019,
        AVCTP_BROWSING    = 0x001B,
        UDI_C_PLANE       = 0x001D,
        ATT               = 0x001F,
        LE_DYN_START      = 0x0080,
        LE_DYN_END        = 0x00FF,
        DYN_START         = 0x1001,
        DYN_END           = 0xffff,
        AUTO_END          = 0x10ff
    };
    constexpr uint16_t number(const L2CAP_PSM rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }
    constexpr L2CAP_PSM to_L2CAP_PSM(const uint16_t v) noexcept { return static_cast<L2CAP_PSM>(v); }
    std::string to_string(const L2CAP_PSM v) noexcept;

    /**
     * BT Core Spec v5.2:  Vol 3, Part A L2CAP Spec: 6 State Machine
     */
    enum class L2CAP_States : uint8_t {
        CLOSED,
        WAIT_CONNECTED,
        WAIT_CONNECTED_RSP,
        CONFIG,
        OPEN,
        WAIT_DISCONNECTED,
        WAIT_CREATE,
        WAIT_CONNECT,
        WAIT_CREATE_RSP,
        WAIT_MOVE,
        WAIT_MOVE_RSP,
        WAIT_MOVE_CONFIRM,
        WAIT_CONFIRM_RSP
    };

    /**
     * ​​Assigned numbers are used in Generic Access Profile (GAP) for inquiry response,
     * EIR data type values, manufacturer-specific data, advertising data,
     * low energy UUIDs and appearance characteristics, and class of device.
     * <p>
     * Type identifier values as defined in "Assigned Numbers - Generic Access Profile"
     * <https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/>
     * </p>
     * <p>
     * Also see Bluetooth Core Specification Supplement V9, Part A: 1, p 9 pp
     * for data format definitions.
     * </p>
     * <p>
     * For data segment layout see Bluetooth Core Specification V5.2 [Vol. 3, Part C, 11, p 1392]
     * </p>
     * <p>
     * https://www.bluetooth.com/specifications/archived-specifications/
     * </p>
     */
    enum class GAP_T : uint8_t {
        // Last sync 2020-02-17 with <https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/>
        NONE                    = 0x00,

        /** Flags */
        FLAGS                   = 0x01,
        /** Incomplete List of 16-bit Service Class UUID. (Supplement, Part A, section 1.1)*/
        UUID16_INCOMPLETE       = 0x02,
        /** Complete List of 16-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID16_COMPLETE         = 0x03,
        /** Incomplete List of 32-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID32_INCOMPLETE       = 0x04,
        /** Complete List of 32-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID32_COMPLETE         = 0x05,
        /** Incomplete List of 128-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID128_INCOMPLETE      = 0x06,
        /** Complete List of 128-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID128_COMPLETE        = 0x07,
        /** Shortened local name (Supplement, Part A, section 1.2) */
        NAME_LOCAL_SHORT        = 0x08,
        /** Complete local name (Supplement, Part A, section 1.2) */
        NAME_LOCAL_COMPLETE     = 0x09,
        /** Transmit power level (Supplement, Part A, section 1.5) */
        TX_POWER_LEVEL          = 0x0A,

        /**
         * SSP: Secure Simple Pairing Out of Band: Supplement, Part A, section 1.6
         * Supplement, Part A, Section 1.6: SSP OOB Data Block w/ SSP_OOB_LEN ([Vol 3] Part C, Section 5.2.2.7.)
         * <p>
         * SSP Class of device (Supplement, Part A, section 1.6).
         * </p>
         */
        SSP_CLASS_OF_DEVICE     = 0x0D,
        /** SSP: Simple Pairing Hash C and Simple Pairing Hash C-192 (Supplement, Part A 1.6) */
        SSP_HASH_C192           = 0x0E,
        /** SSP: Simple Pairing Randomizer R-192 (Supplement, Part A, section 1.6) */
        SSP_RANDOMIZER_R192     = 0x0F,

        /** Device ID Profile v 1.3 or later */
        DEVICE_ID               = 0x10,

        /** Security Manager TK Value (Supplement, Part A, section 1.8) */
        SEC_MGR_TK_VALUE        = 0x10,

        /** Security Manager Out of Band Flags (Supplement, Part A, section 1.7) */
        SEC_MGR_OOB_FLAGS       = 0x11,

        /** Slave Connection Interval Range (Supplement, Part A, section 1.9) */
        SLAVE_CONN_IVAL_RANGE   = 0x12,

        /** List of 16-bit Service Solicitation UUIDs (Supplement, Part A, section 1.10) */
        SOLICIT_UUID16          = 0x14,

        /** List of 128-bit Service Solicitation UUIDs (Supplement, Part A, section 1.10) */
        SOLICIT_UUID128         = 0x15,

        /** Service Data - 16-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID16         = 0x16,

        /* Public Target Address (Supplement, Part A, section 1.13) */
        PUB_TRGT_ADDR           = 0x17,
        /* Random Target Address (Supplement, Part A, section 1.14) */
        RND_TRGT_ADDR           = 0x18,

        /** (GAP) Appearance (Supplement, Part A, section 1.12) */
        GAP_APPEARANCE          = 0x19,

        /** Advertising Interval (Supplement, Part A, section 1.15) */
        ADV_INTERVAL            = 0x1A,
        /** LE Bluetooth Device Address */
        LE_BT_DEV_ADDRESS       = 0x1B,
        /** LE ROLE */
        LE_ROLE                 = 0x1C,

        /** SSP: Simple Pairing Hash C-256 (Supplement, Part A 1.6) */
        SSP_HASH_C256           = 0x1D,
        /** SSP: Simple Pairing Randomizer R-256 (Supplement, Part A, section 1.6) */
        SSP_RANDOMIZER_R256     = 0x1E,

        /** List of 32-bit Service Solicitation UUID (Supplement, Part A, section 1.10) */
        SOLICIT_UUID32          = 0x1F,

        /** Service data, 32-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID32         = 0x20,
        /** Service data, 128-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID128        = 0x21,

        /** SSP: LE Secure Connections Confirmation Value (Supplement Part A, Section 1.6) */
        SSP_LE_SEC_CONN_ACK_VALUE   = 0x22,
        /** SSP: LE Secure Connections Random Value (Supplement Part A, Section 1.6) */
        SSP_LE_SEC_CONN_RND_VALUE   = 0x23,

        /* URI (Supplement, Part A, section 1.18) */
        URI                     = 0x24,

        /* Indoor Positioning - Indoor Positioning Service v1.0 or later */
        INDOOR_POSITIONING      = 0x25,

        /* Transport Discovery Data - Transport Discovery Service v1.0 or later */
        TX_DISCOVERY_DATA       = 0x26,

        /** LE Supported Features (Supplement, Part A, Section 1.19) */
        LE_SUPP_FEATURES        = 0x27,

        CH_MAP_UPDATE_IND       = 0x28,
        PB_ADV                  = 0x29,
        MESH_MESSAGE            = 0x2A,
        MESH_BEACON             = 0x2B,
        BIG_INFO                = 0x2C,
        BROADCAST_CODE          = 0x2D,
        INFO_DATA_3D            = 0x3D,

        /** Manufacturer id code and specific opaque data */
        MANUFACTURE_SPECIFIC    = 0xFF
    };
    constexpr uint8_t number(const GAP_T rhs) noexcept { return static_cast<uint8_t>(rhs); }

    enum class AppearanceCat : uint16_t {
        UNKNOWN = 0,
        GENERIC_PHONE = 64,
        GENERIC_COMPUTER = 128,
        GENERIC_WATCH = 192,
        SPORTS_WATCH = 193,
        GENERIC_CLOCK = 256,
        GENERIC_DISPLAY = 320,
        GENERIC_REMOTE_CLOCK = 384,
        GENERIC_EYE_GLASSES = 448,
        GENERIC_TAG = 512,
        GENERIC_KEYRING = 576,
        GENERIC_MEDIA_PLAYER = 640,
        GENERIC_BARCODE_SCANNER = 704,
        GENERIC_THERMOMETER = 768,
        GENERIC_THERMOMETER_EAR = 769,
        GENERIC_HEART_RATE_SENSOR = 832,
        HEART_RATE_SENSOR_BELT = 833,
        GENERIC_BLOD_PRESSURE = 896,
        BLOD_PRESSURE_ARM = 897,
        BLOD_PRESSURE_WRIST = 898,
        HID = 960,
        HID_KEYBOARD = 961,
        HID_MOUSE = 962,
        HID_JOYSTICK = 963,
        HID_GAMEPAD = 964,
        HID_DIGITIZER_TABLET = 965,
        HID_CARD_READER = 966,
        HID_DIGITAL_PEN = 967,
        HID_BARCODE_SCANNER = 968,
        GENERIC_GLUCOSE_METER = 1024,
        GENERIC_RUNNING_WALKING_SENSOR = 1088,
        RUNNING_WALKING_SENSOR_IN_SHOE = 1089,
        RUNNING_WALKING_SENSOR_ON_SHOE = 1090,
        RUNNING_WALKING_SENSOR_HIP = 1091,
        GENERIC_CYCLING = 1152,
        CYCLING_COMPUTER = 1153,
        CYCLING_SPEED_SENSOR = 1154,
        CYCLING_CADENCE_SENSOR = 1155,
        CYCLING_POWER_SENSOR = 1156,
        CYCLING_SPEED_AND_CADENCE_SENSOR = 1157,
        GENERIC_PULSE_OXIMETER = 3136,
        PULSE_OXIMETER_FINGERTIP = 3137,
        PULSE_OXIMETER_WRIST = 3138,
        GENERIC_WEIGHT_SCALE = 3200,
        GENERIC_PERSONAL_MOBILITY_DEVICE = 3264,
        PERSONAL_MOBILITY_DEVICE_WHEELCHAIR = 3265,
        PERSONAL_MOBILITY_DEVICE_SCOOTER = 3266,
        GENERIC_CONTINUOUS_GLUCOSE_MONITOR = 3328,
        GENERIC_INSULIN_PUMP = 3392,
        INSULIN_PUMP_DURABLE = 3393,
        INSULIN_PUMP_PATCH = 3396,
        INSULIN_PUMP_PEN = 3400,
        GENERIC_MEDICATION_DELIVERY = 3456,
        GENERIC_OUTDOOR_SPORTS_ACTIVITY = 5184,
        OUTDOOR_SPORTS_ACTIVITY_LOCATION_DISPLAY_DEVICE = 5185,
        OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_DISPLAY_DEVICE = 5186,
        OUTDOOR_SPORTS_ACTIVITY_LOCATION_POD = 5187,
        OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_POD = 5188
    };
    constexpr uint16_t number(const AppearanceCat rhs) noexcept { return static_cast<uint16_t>(rhs); }
    std::string to_string(const AppearanceCat v) noexcept;

    // *************************************************
    // *************************************************
    // *************************************************

    class ManufactureSpecificData
    {
        private:
            uint16_t company;
            std::string companyName;
            jau::POctets data;

        public:
            ManufactureSpecificData(uint16_t const company);
            ManufactureSpecificData(uint16_t const company, uint8_t const * const data, jau::nsize_t const data_len);

            ManufactureSpecificData(const ManufactureSpecificData &o) = default;
            ManufactureSpecificData(ManufactureSpecificData &&o) noexcept = default;
            ManufactureSpecificData& operator=(const ManufactureSpecificData &o) = default;
            ManufactureSpecificData& operator=(ManufactureSpecificData &&o) noexcept = default;

            constexpr uint16_t getCompany() const noexcept { return company; }
            const std::string& getCompanyName() const noexcept { return companyName; }

            const jau::TROOctets& getData() const noexcept { return data; }

            std::string toString() const noexcept;
    };

    constexpr bool operator==(const ManufactureSpecificData& lhs, const ManufactureSpecificData& rhs) noexcept
    { return lhs.getCompany() == rhs.getCompany() && lhs.getData() == rhs.getData(); }

    constexpr bool operator!=(const ManufactureSpecificData& lhs, const ManufactureSpecificData& rhs) noexcept
    { return !(lhs == rhs); }

    // *************************************************
    // *************************************************
    // *************************************************

    /**
     * GAP Flags values, see Bluetooth Core Specification Supplement V9, Part A: 1.3, p 12 pp
     *
     * @see EInfoReport
     */
    enum class GAPFlags : uint8_t {
        NONE                   = 0,
        LE_Ltd_Disc            = (1 << 0),
        LE_Gen_Disc            = (1 << 1),
        BREDR_UNSUP            = (1 << 2),
        Dual_SameCtrl          = (1 << 3),
        Dual_SameHost          = (1 << 4),
        RESERVED1              = (1 << 5),
        RESERVED2              = (1 << 6),
        RESERVED3              = (1 << 7)
    };
    constexpr uint8_t number(const GAPFlags rhs) noexcept { return static_cast<uint8_t>(rhs); }
    constexpr GAPFlags operator |(const GAPFlags lhs, const GAPFlags rhs) noexcept {
        return static_cast<GAPFlags> ( number(lhs) | number(rhs) );
    }
    constexpr GAPFlags operator &(const GAPFlags lhs, const GAPFlags rhs) noexcept {
        return static_cast<GAPFlags> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const GAPFlags lhs, const GAPFlags rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const GAPFlags lhs, const GAPFlags rhs) noexcept {
        return !( lhs == rhs );
    }
    std::string to_string(const GAPFlags v) noexcept;

    // *************************************************
    // *************************************************
    // *************************************************

    /**
     * Bit mask of 'Extended Inquiry Response' (EIR) data fields,
     * indicating a set of related data.
     *
     * @see EInfoReport
     */
    enum class EIRDataType : uint32_t {
        NONE         = 0,
        EVT_TYPE     = (1 << 0),
        EXT_EVT_TYPE = (1 << 1),
        BDADDR_TYPE  = (1 << 2),
        BDADDR       = (1 << 3),
        FLAGS        = (1 << 4),
        NAME         = (1 << 5),
        NAME_SHORT   = (1 << 6),
        RSSI         = (1 << 7),
        TX_POWER     = (1 << 8),
        MANUF_DATA   = (1 << 9),
        DEVICE_CLASS = (1 << 10),
        APPEARANCE   = (1 << 11),
        HASH         = (1 << 12),
        RANDOMIZER   = (1 << 13),
        DEVICE_ID    = (1 << 14),
        CONN_IVAL    = (1 << 15),
        SERVICE_UUID = (1 << 30),
        ALL          = 0xffffffff
    };
    constexpr uint32_t number(const EIRDataType rhs) noexcept { return static_cast<uint32_t>(rhs); }

    constexpr EIRDataType operator |(const EIRDataType lhs, const EIRDataType rhs) noexcept {
        return static_cast<EIRDataType> ( number(lhs) | number(rhs) );
    }
    constexpr EIRDataType operator &(const EIRDataType lhs, const EIRDataType rhs) noexcept {
        return static_cast<EIRDataType> ( number(lhs) & number(rhs) );
    }
    constexpr EIRDataType operator ~(const EIRDataType rhs) noexcept {
        return static_cast<EIRDataType> ( ~number(rhs) );
    }
    constexpr bool operator ==(const EIRDataType lhs, const EIRDataType rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const EIRDataType lhs, const EIRDataType rhs) noexcept {
        return !( lhs == rhs );
    }
    constexpr bool is_set(const EIRDataType mask, const EIRDataType bit) noexcept { return bit == ( mask & bit ); }
    constexpr void set(EIRDataType &mask, const EIRDataType bit) noexcept { mask = mask | bit; }
    std::string to_string(const EIRDataType mask) noexcept;

    /** Explicit mask to erase all implicit set EIRDataType fields: EVT_TYPE, EXT_EVT_TYPE, BDADDR_TYPE, BDADDR and RSSI. */
    inline constexpr const EIRDataType EIR_DATA_TYPE_MASK = ~( EIRDataType::EVT_TYPE | EIRDataType::EXT_EVT_TYPE |
                                                               EIRDataType::BDADDR_TYPE | EIRDataType::BDADDR | EIRDataType::RSSI );

    /**
     * Collection of 'Extended Advertising Data' (EAD), 'Advertising Data' (AD)
     * or 'Extended Inquiry Response' (EIR) information.
     *
     * References:
     *
     * - BT Core Spec v5.2: Vol 4, Part E, 7.7.65.2 LE Advertising Report event
     * - BT Core Spec v5.2: Vol 4, Part E, 7.7.65.13 LE Extended Advertising Report event
     * - BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
     * - BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
     * - BT Core Spec Supplement v9, Part A: Section 1 + 2 Examples, p25..
     * - [Assigned Numbers - Generic Access Profile](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/)
     *
     */
    class EInfoReport {
        public:
            enum class Source : int {
                /** Not Available */
                NA          = 0,
                /** (Extended) Advertising Data (AD or EAD) Indication Variant, i.e. initial passive scan data. */
                AD_IND      = 1,
                /** (Extended) Advertising Data (AD or EAD) Scan Response, i.e. optional active scanning data after AD_IND. */
                AD_SCAN_RSP = 2,
                /** Extended Inquiry Response (EIR) */
                EIR         = 3,
                /** Extended Inquiry Response (EIR) from Kernel Mgmt */
                EIR_MGMT    = 4
            };
            static constexpr int number(const Source rhs) noexcept { return static_cast<int>(rhs); }
            static Source toSource(const AD_PDU_Type type);
            static Source toSource(const EAD_Event_Type type);

        private:
            /** Source */
            Source source = Source::NA;
            /** Flag whether source originated from an extended BT5 data set, i.e. EAD */
            bool source_ext = false;
            uint64_t timestamp = 0;
            EIRDataType eir_data_mask = EIRDataType::NONE;

            AD_PDU_Type evt_type = AD_PDU_Type::UNDEFINED;
            EAD_Event_Type ead_type = EAD_Event_Type::NONE;
            uint8_t ad_address_type = 0;
            BDAddressType addressType = BDAddressType::BDADDR_UNDEFINED;
            jau::EUI48 address;

            GAPFlags flags = GAPFlags::NONE;
            std::string name;
            std::string name_short;
            int8_t rssi = 127; // The core spec defines 127 as the "not available" value
            int8_t tx_power = 127; // The core spec defines 127 as the "not available" value
            std::shared_ptr<ManufactureSpecificData> msd = nullptr;
            jau::darray<std::shared_ptr<const jau::uuid_t>> services;
            bool services_complete = false;
            uint32_t device_class = 0;
            AppearanceCat appearance = AppearanceCat::UNKNOWN;
            jau::POctets hash;
            jau::POctets randomizer;
            uint16_t did_source = 0;
            uint16_t did_vendor = 0;
            uint16_t did_product = 0;
            uint16_t did_version = 0;

            /** conn_interval_min in units of 1.25ms, default value 10 for 12.5ms; Value range [6 .. 3200] for [7.5ms .. 4000ms] */
            uint16_t conn_interval_min = 10;
            /** conn_interval_max in units of 1.25ms, default value 24 for 30.0ms; Value range [6 .. 3200] for [7.5ms .. 4000ms] */
            uint16_t conn_interval_max = 24;

            void set(EIRDataType bit) noexcept { eir_data_mask = eir_data_mask | bit; }
            void setADAddressType(uint8_t adAddressType) noexcept;
            void setName(const uint8_t *buffer, int buffer_len) noexcept;
            void setShortName(const uint8_t *buffer, int buffer_len) noexcept;
            void setManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len);

            int next_data_elem(uint8_t *eir_elem_len, uint8_t *eir_elem_type, uint8_t const **eir_elem_data,
                               uint8_t const * data, int offset, int const size) noexcept;

        public:
            EInfoReport() noexcept : hash(16, 0, jau::endian::little), randomizer(16, 0, jau::endian::little) {}

            EInfoReport(const EInfoReport&) = default;
            EInfoReport& operator=(const EInfoReport &o) = default;

            /**
             * Reset all data fields.
             */
            void clear() noexcept;

            /**
             * Merge all fields from given EInfoReport if set and different.
             * @param eir
             * @return The changed fields, i.e. EIRDataType bit field
             */
            EIRDataType set(const EInfoReport& eir) noexcept;

            void setSource(Source s, bool ext) noexcept { source = s; source_ext = ext; }
            void setTimestamp(uint64_t ts) noexcept { timestamp = ts; }
            void setEvtType(AD_PDU_Type et) noexcept { evt_type = et; set(EIRDataType::EVT_TYPE); }
            void setExtEvtType(EAD_Event_Type eadt) noexcept { ead_type = eadt; set(EIRDataType::EXT_EVT_TYPE); }
            void setAddressType(BDAddressType at) noexcept;
            void setAddress(jau::EUI48 const &a) noexcept { address = a; set(EIRDataType::BDADDR); }
            void setRSSI(int8_t v) noexcept { rssi = v; set(EIRDataType::RSSI); }
            void setTxPower(int8_t v) noexcept { tx_power = v; set(EIRDataType::TX_POWER); }

            void setFlags(GAPFlags f) noexcept { flags = f; set(EIRDataType::FLAGS); }
            void addFlags(GAPFlags f) noexcept { flags = flags | f; set(EIRDataType::FLAGS); }
            void setName(const std::string& name_) noexcept;
            void setShortName(const std::string& name_short_) noexcept;

            void setManufactureSpecificData(const ManufactureSpecificData& msd_);
            bool addService(const std::shared_ptr<const jau::uuid_t>& uuid) noexcept;
            bool addService(const jau::uuid_t& uuid) noexcept;
            void setServicesComplete(const bool v) noexcept { services_complete = v; }
            void setDeviceClass(uint32_t c) noexcept { device_class= c; set(EIRDataType::DEVICE_CLASS); }
            void setAppearance(AppearanceCat a) noexcept { appearance= a; set(EIRDataType::APPEARANCE); }
            void setHash(const uint8_t * h) noexcept { hash.resize(16); memcpy(hash.get_wptr(), h, 16); set(EIRDataType::HASH); }
            void setRandomizer(const uint8_t * r) noexcept { randomizer.resize(16); memcpy(randomizer.get_wptr(), r, 16); set(EIRDataType::RANDOMIZER); }
            void setDeviceID(const uint16_t source, const uint16_t vendor, const uint16_t product, const uint16_t version) noexcept;

            /**
             * Set slave connection interval range.
             *
             * Bluetooth Supplement, Part A, section 1.9
             *
             * @param min conn_interval_min in units of 1.25ms, default value 10 for 12.5ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param max conn_interval_max in units of 1.25ms, default value 24 for 30.0ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             */
            void setConnInterval(const uint16_t min, const uint16_t max) noexcept { conn_interval_min=min; conn_interval_max=max; set(EIRDataType::CONN_IVAL); }

            /**
             * Reads a complete Advertising Data (AD) Report
             * and returns the number of AD reports in form of a sharable list of EInfoReport;
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 7.7.65.2 LE Advertising Report event
             * BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
             * BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
             * <pre>
             * https://www.bluetooth.com/specifications/archived-specifications/
             * </p>
             */
            static jau::darray<std::unique_ptr<EInfoReport>> read_ad_reports(uint8_t const * data, jau::nsize_t const data_length) noexcept;

            /**
             * Reads a complete Extended Advertising Data (AD) Report
             * and returns the number of AD reports in form of a sharable list of EInfoReport;
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 7.7.65.13 LE Extended Advertising Report event
             * BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
             * BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
             * </pre>
             * <p>
             * https://www.bluetooth.com/specifications/archived-specifications/
             * </p>
             */
            static jau::darray<std::unique_ptr<EInfoReport>> read_ext_ad_reports(uint8_t const * data, jau::nsize_t const data_length) noexcept;

            /**
             * Reads the Extended Inquiry Response (EIR) or (Extended) Advertising Data (EAD or AD) segments
             * and returns the number of parsed data segments;
             * <p>
             * AD as well as EIR information is passed in little endian order
             * in the same fashion data block:
             * <pre>
             * a -> {
             *             uint8_t len
             *             uint8_t type
             *             uint8_t data[len-1];
             *         }
             * b -> next block = a + 1 + len;
             * </pre>
             * </p>
             * <p>
             *
             * References:
             *
             * - BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
             * - BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
             * - BT Core Spec Supplement v9, Part A: Section 1 + 2 Examples, p25..
             * - [Assigned Numbers - Generic Access Profile](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/)
             *
             * </p>
             * <p>
             * https://www.bluetooth.com/specifications/archived-specifications/
             * </p>
             */
            int read_data(uint8_t const * data, uint8_t const data_length) noexcept;

            /**
             * Writes the Extended Inquiry Response (EIR) or (Extended) Advertising Data (EAD or AD) segments
             * of existing EIRDataType of this instance into the given `data` up to `data_length`.
             * <p>
             * Only fields in the given EIRDataType `write_mask` and getEIRDataMask() will be written,
             * hence `write_mask` allows to select the fields to be written/advertised.
             * </p>
             * <p>
             * Usually only up to 31 bytes are supported to be advertised, i.e. `data_length` may be lower or equal 31.
             * </p>
             * <p>
             * Elements exceeding the `data_length` will be cut off.
             * </p>
             * <p>
             *
             * References:
             *
             * - BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
             * - BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
             * - BT Core Spec Supplement v9, Part A: Section 1 + 2 Examples, p25..
             * - [Assigned Numbers - Generic Access Profile](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/)
             *
             * </p>
             *
             * @param write_mask EIRDataType selection mask
             * @param data destination
             * @param data_length destination length
             * @return number of bytes written
             * @see read_data()
             */
            jau::nsize_t write_data(EIRDataType write_mask, uint8_t * data, jau::nsize_t const data_length) const noexcept;

            Source getSource() const noexcept { return source; }
            bool getSourceExt() const noexcept { return source_ext; }

            uint64_t getTimestamp() const noexcept { return timestamp; }
            bool isSet(EIRDataType bit) const noexcept { return EIRDataType::NONE != (eir_data_mask & bit); }
            EIRDataType getEIRDataMask() const noexcept { return eir_data_mask; }

            AD_PDU_Type getEvtType() const noexcept { return evt_type; }
            EAD_Event_Type getExtEvtType() const noexcept { return ead_type; }
            GAPFlags getFlags() const noexcept { return flags; }
            uint8_t getADAddressType() const noexcept { return ad_address_type; }
            BDAddressType getAddressType() const noexcept { return addressType; }
            jau::EUI48 const & getAddress() const noexcept { return address; }
            std::string const & getName() const noexcept { return name; }
            std::string const & getShortName() const noexcept{ return name_short; }
            int8_t getRSSI() const noexcept { return rssi; }
            int8_t getTxPower() const noexcept { return tx_power; }

            std::shared_ptr<ManufactureSpecificData> getManufactureSpecificData() const noexcept { return msd; }

            const jau::darray<std::shared_ptr<const jau::uuid_t>>& getServices() const noexcept { return services; }
            bool getServicesComplete() const noexcept { return services_complete; }
            int findService(const jau::uuid_t& uuid) const noexcept;

            uint32_t getDeviceClass() const noexcept { return device_class; }
            AppearanceCat getAppearance() const noexcept { return appearance; }
            const jau::TROOctets & getHash() const noexcept { return hash; }
            const jau::TROOctets & getRandomizer() const noexcept { return randomizer; }
            void getDeviceID(uint16_t& source_, uint16_t& vendor_, uint16_t& product_, uint16_t& version_) const noexcept {
                source_ = did_source; vendor_ = did_vendor; product_ = did_product; version_ = did_version;
            }
            uint16_t getDeviceIDSource() const noexcept { return did_source; }
            uint16_t getDeviceIDVendor() const noexcept { return did_vendor; }
            uint16_t getDeviceIDProduct() const noexcept { return did_product; }
            uint16_t getDeviceIDVersion() const noexcept { return did_version; }

            /**
             * Get slave connection interval range.
             *
             * Bluetooth Supplement, Part A, section 1.9
             *
             * @param min conn_interval_min in units of 1.25ms, default value 10 for 12.5ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param max conn_interval_max in units of 1.25ms, default value 24 for 30.0ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             */
            void getConnInterval(uint16_t& min, uint16_t& max) const noexcept { min=conn_interval_min; max=conn_interval_max; }

            std::string getDeviceIDModalias() const noexcept;
            std::string eirDataMaskToString() const noexcept;
            std::string toString(const bool includeServices=true) const noexcept;

            bool operator==(const EInfoReport& o) const noexcept;
            bool operator!=(const EInfoReport& o) const noexcept {
                return !( *this == o );
            }
    };
    std::string to_string(EInfoReport::Source source) noexcept;
    inline std::string to_string(const EInfoReport& eir, const bool includeServices=true) noexcept { return eir.toString(includeServices); }

    typedef std::shared_ptr<EInfoReport> EInfoReportRef;

    // *************************************************
    // *************************************************
    // *************************************************

    /**@}*/

} // namespace direct_bt

#endif /* BT_TYPES0_HPP_ */
