/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021-2026 Gothel Software e.K.
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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include  <algorithm>
#include "jau/string_cfmt.hpp"
#include "jau/string_util.hpp"

#include <jau/basic_types.hpp>
#include <jau/darray.hpp>
#include <jau/debug.hpp>

#include "BTTypes0.hpp"

using namespace direct_bt;

template<typename T>
static void append_bitstr(std::string& out, T mask, T bit, const std::string& bitstr, bool& comma) {
    if( bit == ( mask & bit ) ) {
        if( comma ) { out.append(", "); }
        out.append(bitstr); comma = true;
    }
}
#define APPEND_BITSTR(U,V,M) append_bitstr(out, M, U::V, #V, comma);

#define CASE_TO_STRING(V) case V: return #V;
#define CASE2_TO_STRING(U,V) case U::V: return #V;

BDAddressType direct_bt::to_BDAddressType(const HCILEPeerAddressType hciPeerAddrType) noexcept {
    switch(hciPeerAddrType) {
        case HCILEPeerAddressType::PUBLIC:
            return BDAddressType::BDADDR_LE_PUBLIC;
        case HCILEPeerAddressType::RANDOM:
            [[fallthrough]];
        case HCILEPeerAddressType::PUBLIC_IDENTITY:
            [[fallthrough]];
        case HCILEPeerAddressType::RANDOM_STATIC_IDENTITY:
            return BDAddressType::BDADDR_LE_RANDOM;
        default:
            return BDAddressType::BDADDR_UNDEFINED;
    }
}

BDAddressType direct_bt::to_BDAddressType(const HCILEOwnAddressType hciOwnAddrType) noexcept {
    switch(hciOwnAddrType) {
        case HCILEOwnAddressType::PUBLIC:
            return BDAddressType::BDADDR_LE_PUBLIC;
        case HCILEOwnAddressType::RANDOM:
            [[fallthrough]];
        case HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC:
            [[fallthrough]];
        case HCILEOwnAddressType::RESOLVABLE_OR_RANDOM:
            return BDAddressType::BDADDR_LE_RANDOM;
        default:
            return BDAddressType::BDADDR_UNDEFINED;
    }
}

HCILEOwnAddressType direct_bt::to_HCILEOwnAddressType(const BDAddressType addrType, bool resolvable) noexcept {
    switch(addrType) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            return HCILEOwnAddressType::PUBLIC;

        case BDAddressType::BDADDR_LE_RANDOM:
            /** FIXME: Sufficient mapping for adapter put in random address mode? */
            return resolvable ? HCILEOwnAddressType::RESOLVABLE_OR_RANDOM : HCILEOwnAddressType::RANDOM;

        case BDAddressType::BDADDR_BREDR:
            [[fallthrough]];
        case BDAddressType::BDADDR_UNDEFINED:
            [[fallthrough]];
        default:
            return HCILEOwnAddressType::UNDEFINED;
    }
}

namespace direct_bt { // from BTAddress.hpp
    JAU_MAKE_ENUM_STRING_CODE(BDAddressType, BDADDR_BREDR, BDADDR_LE_PUBLIC, BDADDR_LE_RANDOM, BDADDR_UNDEFINED);
    JAU_MAKE_ENUM_STRING_CODE(BLERandomAddressType, UNRESOLVABLE_PRIVAT, RESOLVABLE_PRIVAT, RESERVED, STATIC_PUBLIC, UNDEFINED);
    JAU_MAKE_ENUM_STRING_CODE(HCILEPeerAddressType, PUBLIC, RANDOM, PUBLIC_IDENTITY, RANDOM_STATIC_IDENTITY, UNDEFINED);
    JAU_MAKE_ENUM_STRING_CODE(HCILEOwnAddressType, PUBLIC, RANDOM, RESOLVABLE_OR_PUBLIC, RESOLVABLE_OR_RANDOM, UNDEFINED);
}

BLERandomAddressType BDAddressAndType::getBLERandomAddressType(const jau::io::net::EUI48& address, const BDAddressType addressType) noexcept {
    if( BDAddressType::BDADDR_LE_RANDOM != addressType ) {
        return BLERandomAddressType::UNDEFINED;
    }
    const uint8_t high2 = ( address.b[5] >> 6 ) & 0x03;
    switch( high2 ) {
        case 0x00: return BLERandomAddressType::UNRESOLVABLE_PRIVAT;
        case 0x01: return BLERandomAddressType::RESOLVABLE_PRIVAT;
        case 0x02: return BLERandomAddressType::RESERVED;
        case 0x03: return BLERandomAddressType::STATIC_PUBLIC;
        default: return BLERandomAddressType::UNDEFINED;
    }
}

std::string BDAddressAndType::getBLERandomAddressTypeString(const jau::io::net::EUI48& address, const BDAddressType addressType, const std::string& prefix) noexcept {
    if( BDAddressType::BDADDR_LE_RANDOM != addressType ) {
        return std::string();
    }
    std::string res;
    jau::reserve_string(res, prefix.size() + 20);
    jau::append_string(res, prefix);
    jau::append_string(res, to_string(BDAddressAndType::getBLERandomAddressType(address, addressType)));
    return res;
}

const BDAddressAndType direct_bt::BDAddressAndType::ANY_BREDR_DEVICE(jau::io::net::EUI48::ANY_DEVICE, BDAddressType::BDADDR_BREDR);
const BDAddressAndType direct_bt::BDAddressAndType::ANY_DEVICE(jau::io::net::EUI48::ANY_DEVICE, BDAddressType::BDADDR_UNDEFINED);

std::string BDAddressAndType::toString() const noexcept {
    return jau_format_string("[%s, %s%s]", address, type, getBLERandomAddressTypeString(address, type, ", "));
}

// *************************************************
// *************************************************
// *************************************************

static inline const int8_t * const_uint8_to_const_int8_ptr(const uint8_t* p) noexcept {
    return static_cast<const int8_t *>( static_cast<void *>( const_cast<uint8_t*>( p ) ) ); // NOLINT(bugprone-casting-through-void): Alignment OK - same as reinterpret_cast<T*>( p )
}

namespace direct_bt { // from BTTypes0.hpp
    JAU_MAKE_ENUM_STRING_CODE(LogLevel, none, error, warning, info, debug, cond);
    JAU_MAKE_ENUM_STRING_CODE(EventSource, hci, mgmt, dbt);
    JAU_MAKE_ENUM_STRING_CODE(BTRole, None, Master, Slave);
    JAU_MAKE_ENUM_STRING_CODE(GATTRole, None, Server, Client);
    JAU_MAKE_ENUM_STRING_CODE(BTMode, NONE, DUAL, BREDR);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(LE_Features,
       LE_Encryption, Conn_Param_Req_Proc, Ext_Rej_Ind, SlaveInit_Feat_Exchg, LE_Ping,
       LE_Data_Pkt_Len_Ext, LL_Privacy, Ext_Scan_Filter_Pol, LE_2M_PHY,
       Stable_Mod_Idx_Tx, Stable_Mod_Idx_Rx, LE_Coded_PHY, LE_Ext_Adv, LE_Per_Adv,
       Chan_Sel_Algo_2, LE_Pwr_Cls_1, Min_Num_Used_Chan_Proc,
       Conn_CTE_Req, Conn_CTE_Res, ConnLess_CTE_Tx, ConnLess_CTE_Rx,
       AoD, AoA, Rx_Const_Tone_Ext, Per_Adv_Sync_Tx_Sender, Per_Adv_Sync_Tx_Rec,
       Zzz_Clk_Acc_Upd, Rem_Pub_Key_Val, Conn_Iso_Stream_Master, Conn_Iso_Stream_Slave,
       Iso_Brdcst, Sync_Rx, Iso_Chan, LE_Pwr_Ctrl_Req, LE_Pwr_Chg_Ind, LE_Path_Loss_Mon);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(LE_PHYs, LE_1M, LE_2M, LE_CODED);
    JAU_MAKE_ENUM_STRING_CODE(BTSecurityLevel, NONE, ENC_ONLY, ENC_AUTH, ENC_AUTH_FIPS);
    JAU_MAKE_ENUM_STRING_CODE(PairingMode, NEGOTIATING, JUST_WORKS, PASSKEY_ENTRY_ini, PASSKEY_ENTRY_res,
                                           NUMERIC_COMPARE_ini, NUMERIC_COMPARE_res, OUT_OF_BAND, PRE_PAIRED);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(ScanType, BREDR, LE);
    JAU_MAKE_ENUM_STRING_CODE(AD_PDU_Type, ADV_IND, ADV_DIRECT_IND, ADV_SCAN_IND, ADV_NONCONN_IND, SCAN_RSP,
                                           ADV_IND2, DIRECT_IND2, SCAN_IND2, NONCONN_IND2,
                                           SCAN_RSP_to_ADV_IND, SCAN_RSP_to_ADV_SCAN_IND, UNDEFINED);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(EAD_Event_Type, CONN_ADV, SCAN_ADV, DIR_ADV, SCAN_RSP, LEGACY_PDU, DATA_B0, DATA_B1);
    JAU_MAKE_ENUM_STRING_CODE(L2CAP_CID, UNDEFINED, SIGNALING, CONN_LESS, A2MP, ATT, LE_SIGNALING, SMP, SMP_BREDR, DYN_START, DYN_END, LE_DYN_END);
    JAU_MAKE_ENUM_STRING_CODE(L2CAP_PSM, UNDEFINED, SDP, RFCOMM, TCSBIN, TCSBIN_CORDLESS, BNEP, HID_CONTROL, HID_INTERRUPT,
                                         UPNP, AVCTP, AVDTP, AVCTP_BROWSING, UDI_C_PLANE, ATT, LE_DYN_START, LE_DYN_END,
                                         DYN_START, DYN_END, AUTO_END);
    JAU_MAKE_ENUM_STRING_CODE(AppearanceCat, UNKNOWN, GENERIC_PHONE, GENERIC_COMPUTER, GENERIC_WATCH, SPORTS_WATCH, GENERIC_CLOCK,
         GENERIC_DISPLAY, GENERIC_REMOTE_CLOCK, GENERIC_EYE_GLASSES, GENERIC_TAG, GENERIC_KEYRING,
         GENERIC_MEDIA_PLAYER, GENERIC_BARCODE_SCANNER, GENERIC_THERMOMETER, GENERIC_THERMOMETER_EAR,
         GENERIC_HEART_RATE_SENSOR, HEART_RATE_SENSOR_BELT, GENERIC_BLOD_PRESSURE,
         BLOD_PRESSURE_ARM, BLOD_PRESSURE_WRIST, HID, HID_KEYBOARD, HID_MOUSE, HID_JOYSTICK, HID_GAMEPAD,
         HID_DIGITIZER_TABLET, HID_CARD_READER, HID_DIGITAL_PEN, HID_BARCODE_SCANNER, GENERIC_GLUCOSE_METER,
         GENERIC_RUNNING_WALKING_SENSOR, RUNNING_WALKING_SENSOR_IN_SHOE, RUNNING_WALKING_SENSOR_ON_SHOE,
         RUNNING_WALKING_SENSOR_HIP, GENERIC_CYCLING, CYCLING_COMPUTER, CYCLING_SPEED_SENSOR, CYCLING_CADENCE_SENSOR,
         CYCLING_POWER_SENSOR, CYCLING_SPEED_AND_CADENCE_SENSOR, GENERIC_PULSE_OXIMETER, PULSE_OXIMETER_FINGERTIP,
         PULSE_OXIMETER_WRIST, GENERIC_WEIGHT_SCALE, GENERIC_PERSONAL_MOBILITY_DEVICE, PERSONAL_MOBILITY_DEVICE_WHEELCHAIR,
         PERSONAL_MOBILITY_DEVICE_SCOOTER, GENERIC_CONTINUOUS_GLUCOSE_MONITOR, GENERIC_INSULIN_PUMP, INSULIN_PUMP_DURABLE,
         INSULIN_PUMP_PATCH, INSULIN_PUMP_PEN, GENERIC_MEDICATION_DELIVERY, GENERIC_OUTDOOR_SPORTS_ACTIVITY,
         OUTDOOR_SPORTS_ACTIVITY_LOCATION_DISPLAY_DEVICE, OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_DISPLAY_DEVICE,
         OUTDOOR_SPORTS_ACTIVITY_LOCATION_POD, OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_POD);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(GAPFlags, LE_Ltd_Disc, LE_Gen_Disc, BREDR_UNSUP, Dual_SameCtrl, Dual_SameHost, RESERVED1, RESERVED2, RESERVED3);
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(EIRDataType, EVT_TYPE, EXT_EVT_TYPE, BDADDR_TYPE, BDADDR, FLAGS, NAME, NAME_SHORT, RSSI, TX_POWER,
                                                    MANUF_DATA, DEVICE_CLASS, APPEARANCE, HASH, RANDOMIZER, DEVICE_ID, CONN_IVAL,
                                                    SERVICE_UUID);
}  // namespace direct_bt

BTMode direct_bt::to_BTMode(const std::string & value) noexcept {
    if( "DUAL" == value ) {
        return BTMode::DUAL;
    }
    if( "BREDR" == value ) {
        return BTMode::BREDR;
    }
    if( "LE" == value ) {
        return BTMode::LE;
    }
    return BTMode::NONE;
}

// *************************************************
// *************************************************
// *************************************************

ScanType direct_bt::to_ScanType(BTMode btMode) {
    switch ( btMode ) {
        case BTMode::DUAL:
            return ScanType::DUAL;
        case BTMode::BREDR:
            return ScanType::BREDR;
        case BTMode::LE:
            return ScanType::LE;
        default:
            throw jau::IllegalArgumentError("Unsupported BTMode "+to_string(btMode), E_FILE_LINE);
    }
}

// *************************************************
// *************************************************
// *************************************************

static std::string bt_compidtostr(const uint16_t companyid) noexcept {
    return std::to_string(companyid);
}

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_)
: company(company_), companyName(std::string(bt_compidtostr(company_))),
  data(jau::lb_endian_t::little /* intentional zero sized */)
{ }

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_, uint8_t const * const data_, jau::nsize_t const data_len)
: company(company_), companyName(std::string(bt_compidtostr(company_))),
  data(data_, data_len, jau::lb_endian_t::little)
{ }

std::string ManufactureSpecificData::toString() const noexcept {
  return jau_format_string("MSD[company[%u %s], data[%s]]", company, companyName, data);
}

// *************************************************
// *************************************************
// *************************************************

void EInfoReport::clear() {
    EInfoReport eir_clean;
    *this = eir_clean;
}

EIRDataType EInfoReport::set(const EInfoReport& eir) {
    EIRDataType res = EIRDataType::NONE;

    if( eir.isSet( EIRDataType::EVT_TYPE ) ) {
        if( !isSet( EIRDataType::EVT_TYPE ) || getEvtType() != eir.getEvtType() ) {
            setEvtType(eir.getEvtType());
            res |= EIRDataType::EVT_TYPE;
            res |= EIRDataType::EVT_TYPE;
        }
    }
    if( eir.isSet( EIRDataType::EXT_EVT_TYPE ) ) {
        if( !isSet( EIRDataType::EXT_EVT_TYPE ) || getExtEvtType() != eir.getExtEvtType() ) {
            setExtEvtType(eir.getExtEvtType());
            res |= EIRDataType::EXT_EVT_TYPE;
        }
    }
    if( eir.isSet( EIRDataType::BDADDR_TYPE ) ) {
        if( !isSet( EIRDataType::BDADDR_TYPE ) || getAddressType() != eir.getAddressType() ) {
            setAddressType(eir.getAddressType());
            res |= EIRDataType::BDADDR_TYPE;
        }
    }
    if( eir.isSet( EIRDataType::BDADDR ) ) {
        if( !isSet( EIRDataType::BDADDR ) || getAddress() != eir.getAddress() ) {
            setAddress(eir.getAddress());
            res |= EIRDataType::BDADDR;
        }
    }
    if( eir.isSet( EIRDataType::RSSI ) ) {
        if( !isSet( EIRDataType::RSSI ) || getRSSI() != eir.getRSSI() ) {
            setRSSI(eir.getRSSI());
            res |= EIRDataType::RSSI;
        }
    }
    if( eir.isSet( EIRDataType::TX_POWER ) ) {
        if( !isSet( EIRDataType::TX_POWER ) || getTxPower() != eir.getTxPower() ) {
            setTxPower(eir.getTxPower());
            res |= EIRDataType::TX_POWER;
        }
    }
    if( eir.isSet( EIRDataType::FLAGS ) ) {
        if( !isSet( EIRDataType::FLAGS ) || getFlags() != eir.getFlags() ) {
            addFlags(eir.getFlags());
            res |= EIRDataType::FLAGS;
        }
    }
    if( eir.isSet( EIRDataType::NAME) ) {
        if( !isSet( EIRDataType::NAME ) || getName() != eir.getName() ) {
            setName(eir.getName());
            res |= EIRDataType::NAME;
        }
    }
    if( eir.isSet( EIRDataType::NAME_SHORT) ) {
        if( !isSet( EIRDataType::NAME_SHORT ) || getShortName() != eir.getShortName() ) {
            setShortName(eir.getShortName());
            res |= EIRDataType::NAME_SHORT;
        }
    }
    if( eir.isSet( EIRDataType::MANUF_DATA) ) {
        std::shared_ptr<ManufactureSpecificData> o_msd = eir.getManufactureSpecificData();
        if( nullptr != o_msd && ( !isSet( EIRDataType::MANUF_DATA ) || nullptr == getManufactureSpecificData() || *getManufactureSpecificData() != *o_msd ) ) {
            setManufactureSpecificData(*o_msd);
            res |= EIRDataType::MANUF_DATA;
        }
    }
    if( eir.isSet( EIRDataType::SERVICE_UUID) ) {
        const jau::darray<std::shared_ptr<const jau::uuid_t>>& services_ = eir.getServices();
        bool added = false;
        for(const auto& uuid : services_) {
            added = addService(uuid) | added;
        }
        if( added ) {
            setServicesComplete(eir.getServicesComplete());
            res |= EIRDataType::SERVICE_UUID;
        }
    }
    if( eir.isSet( EIRDataType::DEVICE_CLASS) ) {
        if( !isSet( EIRDataType::DEVICE_CLASS ) || getDeviceClass() != eir.getDeviceClass() ) {
            setDeviceClass(eir.getDeviceClass());
            res |= EIRDataType::DEVICE_CLASS;
        }
    }
    if( eir.isSet( EIRDataType::APPEARANCE) ) {
        if( !isSet( EIRDataType::APPEARANCE ) || getAppearance() != eir.getAppearance() ) {
            setAppearance(eir.getAppearance());
            res |= EIRDataType::APPEARANCE;
        }
    }
    if( eir.isSet( EIRDataType::HASH) ) {
        if( !isSet( EIRDataType::HASH ) || getHash() != eir.getHash() ) {
            setHash(eir.getHash().get_ptr());
            res |= EIRDataType::HASH;
        }
    }
    if( eir.isSet( EIRDataType::RANDOMIZER) ) {
        if( !isSet( EIRDataType::RANDOMIZER ) || getRandomizer() != eir.getRandomizer() ) {
            setRandomizer(eir.getRandomizer().get_ptr());
            res |= EIRDataType::RANDOMIZER;
        }
    }
    if( eir.isSet( EIRDataType::DEVICE_ID) ) {
        uint16_t source_=0, vendor_=0, product_=0, version_=0;
        eir.getDeviceID(source_, vendor_, product_, version_);
        if( !isSet( EIRDataType::DEVICE_ID ) ||
            did_source != source_ || did_vendor != vendor_ || did_product != product_ || did_version != version_ )
        {
            setDeviceID(source_, vendor_, product_, version_);
            res |= EIRDataType::DEVICE_ID;
        }
    }
    if( eir.isSet( EIRDataType::CONN_IVAL) ) {
        uint16_t min=0, max=0;
        eir.getConnInterval(min, max);
        if( !isSet( EIRDataType::CONN_IVAL ) || conn_interval_min != min || conn_interval_max != max ) {
            setConnInterval(min, max);
            res |= EIRDataType::CONN_IVAL;
        }
    }
    if( EIRDataType::NONE != res ) {
        setSource(eir.getSource(), eir.getSourceExt());
        setTimestamp(eir.getTimestamp());
    }
    return res;
}

EInfoReport::ssize_type EInfoReport::findService(const jau::uuid_t& uuid) const noexcept
{
    const size_type size = services.size();
    for (size_type i = 0; i < size; ++i) {
        const std::shared_ptr<const jau::uuid_t> & e = services[i];
        if ( nullptr != e && uuid.equivalent(*e) ) {
            return (ssize_type)i;
        }
    }
    return -1;
}

std::string direct_bt::to_string(EInfoReport::Source source) noexcept {
    switch (source) {
        case EInfoReport::Source::NA: return "N/A";
        case EInfoReport::Source::AD_IND: return "AD_IND";
        case EInfoReport::Source::AD_SCAN_RSP: return "AD_SCAN_RSP";
        case EInfoReport::Source::EIR: return "EIR";
        case EInfoReport::Source::EIR_MGMT: return "EIR_MGMT";
    }
    return "N/A";
}

void EInfoReport::setADAddressType(uint8_t adAddressType) noexcept {
    ad_address_type = adAddressType;
    switch( ad_address_type ) {
        case 0x00: addressType = BDAddressType::BDADDR_LE_PUBLIC; break;
        case 0x01: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        case 0x02: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        case 0x03: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        default: addressType = BDAddressType::BDADDR_UNDEFINED; break;
    }
    set(EIRDataType::BDADDR_TYPE);
}

void EInfoReport::setAddressType(BDAddressType at) noexcept {
    addressType = at;
    switch( addressType ) {
        case BDAddressType::BDADDR_BREDR: ad_address_type = 0; break;
        case BDAddressType::BDADDR_LE_PUBLIC: ad_address_type = 0; break;
        case BDAddressType::BDADDR_LE_RANDOM: ad_address_type = 1; break;
        case BDAddressType::BDADDR_UNDEFINED: ad_address_type = 4; break;
    }
    set(EIRDataType::BDADDR_TYPE);
}

void EInfoReport::setName(const uint8_t *buffer, int buffer_len) noexcept {
    name = jau::get_string(buffer, buffer_len, 30);
    set(EIRDataType::NAME);
}
void EInfoReport::setName(const std::string& name_) noexcept {
    name = name_;
    set(EIRDataType::NAME);
}

void EInfoReport::setShortName(const uint8_t *buffer, int buffer_len) noexcept {
    name_short = jau::get_string(buffer, buffer_len, 30);
    set(EIRDataType::NAME_SHORT);
}
void EInfoReport::setShortName(const std::string& name_short_) noexcept {
    name_short = name_short_;
    set(EIRDataType::NAME_SHORT);
}

void EInfoReport::setManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len) {
    if( nullptr == data || 0 >= data_len ) {
        msd = std::make_shared<ManufactureSpecificData>(company);
    } else {
        msd = std::make_shared<ManufactureSpecificData>(company, data, data_len);
    }
    set(EIRDataType::MANUF_DATA);
}
void EInfoReport::setManufactureSpecificData(const ManufactureSpecificData& msd_) {
    msd = std::make_shared<ManufactureSpecificData>(msd_);
    set(EIRDataType::MANUF_DATA);
}
void EInfoReport::setDeviceID(const uint16_t source_, const uint16_t vendor, const uint16_t product, const uint16_t version) noexcept {
    did_source = source_;
    did_vendor = vendor;
    did_product = product;
    did_version = version;
    set(EIRDataType::DEVICE_ID);
}

bool EInfoReport::addService(const std::shared_ptr<const jau::uuid_t>& uuid)
{
    auto begin = services.begin();
    auto it = std::find_if(begin, services.end(), [&](std::shared_ptr<const jau::uuid_t> const& p) {
        return nullptr != p && uuid->equivalent(*p);
    });
    if ( it == std::end(services) ) {
        services.push_back(uuid);
        set(EIRDataType::SERVICE_UUID);
        return true;
    }
    return false;
}
bool EInfoReport::addService(const jau::uuid_t& uuid) {
    return addService( uuid.clone() );
}

std::string EInfoReport::eirDataMaskToString() const noexcept {
    return jau_format_string("Set%s", (EIR_DATA_TYPE_MASK & eir_data_mask));
}
std::string EInfoReport::toString(const bool includeServices) const noexcept {
    const std::string_view source_ext_s = source_ext ? "bt5" : "bt4";
    std::string out = jau_format_string("%s[%s, address[%s, %s/%u], %s, ",
        source, source_ext_s, address, getAddressType(), ad_address_type, eirDataMaskToString());

    if( isSet(EIRDataType::NAME) || isSet(EIRDataType::NAME_SHORT) ) {
        jau_append_string(out, "name['%s'/'%s'], ", name, name_short);
    }
    if( isSet(EIRDataType::EVT_TYPE) || isSet(EIRDataType::EXT_EVT_TYPE) ) {
        jau_append_string(out, "type[evt %s, ead %s], ", evt_type, ead_type);
    }
    if( isSet(EIRDataType::FLAGS) ) {
        jau_append_string(out, "flags%s, ", flags);
    }
    if( isSet(EIRDataType::RSSI) ) {
        jau_append_string(out, "rssi %d, ", rssi);
    }
    if( isSet(EIRDataType::TX_POWER) ) {
        jau_append_string(out, "tx-power %d, ", tx_power);
    }
    if( isSet(EIRDataType::CONN_IVAL) ) {
        jau_append_string(out, "conn[%.6fms - %.6fms], ", 1.25f * (float)conn_interval_min, 1.25f * (float)conn_interval_max);
    }
    if( isSet(EIRDataType::DEVICE_CLASS) ) {
        jau_append_string(out, "dev-class %#x, ", device_class);
    }
    if( isSet(EIRDataType::APPEARANCE) ) {
        jau_append_string(out, "appearance %#x (%s), ", *appearance, appearance);
    }
    if( isSet(EIRDataType::HASH) ) {
        jau_append_string(out, "hash[%s], ", hash);
    }
    if( isSet(EIRDataType::RANDOMIZER) ) {
        jau_append_string(out, "randomizer[%s], ", randomizer);
    }
    if( isSet(EIRDataType::DEVICE_ID) ) {
        jau_append_string(out, "device-id[source %#x, vendor %#x, product %#x, version %#x], ",
            did_source, did_vendor, did_product, did_version);
    }
    if( isSet(EIRDataType::SERVICE_UUID) ) {
        jau_append_string(out, "services[complete %s, count %zu], ", services_complete, services.size());
    }
    if( isSet(EIRDataType::MANUF_DATA) ) {
        if (nullptr != msd) {
            jau_append_string(out, "%s, ", msd->toString());
        } else {
            jau_append_string(out, "MSD[null], ");
        }
    }
    jau_append_string(out, "]");

    if( includeServices && services.size() > 0 && isSet(EIRDataType::SERVICE_UUID) ) {
        jau_append_string(out, "\n");
        for(const auto& p : services) {
            jau_append_string(out, "  %s, %2zu bytes\n", p->toUUID128String(), *(p->getTypeSize()));
        }
    }
    return out;
}

bool EInfoReport::operator==(const EInfoReport& o) const noexcept {
    if( this == &o ) {
        return true;
    }
    return o.eir_data_mask == eir_data_mask &&
           o.evt_type == evt_type &&
           o.ead_type == ead_type &&
           o.flags == flags &&
           o.ad_address_type == ad_address_type &&
           o.address == address &&
           o.name == name &&
           o.name_short == name_short &&
           o.rssi == rssi &&
           o.tx_power == tx_power &&
           ( ( nullptr == o.msd && nullptr == msd ) ||
             ( nullptr != o.msd && nullptr != msd && *o.msd == *msd )
           ) &&
           o.conn_interval_min == conn_interval_min &&
           o.conn_interval_max == conn_interval_max &&
           o.device_class == device_class &&
           o.appearance == appearance &&
           o.hash == hash &&
           o.randomizer == randomizer &&
           o.did_source == did_source &&
           o.did_vendor == did_vendor &&
           o.did_product == did_product &&
           o.did_version == did_version &&
           o.services.size() == services.size() &&
           std::equal( o.services.cbegin(), o.services.cend(), services.cbegin(),
                       [](const std::shared_ptr<const jau::uuid_t>&a, const std::shared_ptr<const jau::uuid_t>&b)
                       -> bool { return *a == *b; } );
}

std::string EInfoReport::getDeviceIDModalias() const noexcept {
    switch (did_source) {
        case 0x0001:
            return jau_format_string("bluetooth:v%04Xp%04Xd%04X", did_vendor, did_product, did_version);
        case 0x0002:
            return jau_format_string("usb:v%04Xp%04Xd%04X", did_vendor, did_product, did_version);
        default:
            return jau_format_string("source<0x%X>:v%04Xp%04Xd%04X", did_source, did_vendor, did_product, did_version);
    }
}

// *************************************************
// *************************************************
// *************************************************

int EInfoReport::next_data_elem(uint8_t *eir_elem_len, uint8_t *eir_elem_type, uint8_t const **eir_elem_data,
                               uint8_t const * data, int offset, int const size) noexcept
{
    if (offset < size) {
        uint8_t len = data[offset]; // covers: type + data, less len field itself

        if (len == 0) {
            return 0; // end of significant part
        }

        if (len + offset > size) {
            return -ENOENT;
        }

        *eir_elem_type = data[offset + 1];
        *eir_elem_data = data + offset + 2; // net data ptr
        *eir_elem_len = len - 1; // less type -> net data length

        return offset + 1 + len; // next ad_struct offset: + len + type + data
    }
    return -ENOENT;
}

int EInfoReport::read_data(uint8_t const * data, uint8_t const data_length) {
    int count = 0;
    int offset = 0;
    uint8_t elem_len, elem_type;
    uint8_t const *elem_data;

    while( 0 < ( offset = next_data_elem( &elem_len, &elem_type, &elem_data, data, offset, data_length ) ) )
    {
        count++;

        // Guaranteed: elem_len >= 0!
        switch ( static_cast<GAP_T>(elem_type) ) {
            case GAP_T::FLAGS:
                if( 1 <= elem_len ) {
                    setFlags(static_cast<GAPFlags>(*elem_data));
                }
                break;

            case GAP_T::UUID16_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID16_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(jau::nsize_t j=0; j<elem_len/2U; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid16_t>(elem_data + j*2, jau::lb_endian_t::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID32_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID32_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(jau::nsize_t j=0; j<elem_len/4U; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid32_t>(elem_data + j*4, jau::lb_endian_t::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID128_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID128_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(jau::nsize_t j=0; j<elem_len/16U; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid128_t>(elem_data + j*16, jau::lb_endian_t::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::NAME_LOCAL_SHORT:
                // INFO: Bluetooth Core Specification V5.2 [Vol. 3, Part C, 8, p 1341]
                // INFO: A remote name request is required to obtain the full name, if needed.
                setShortName(elem_data, elem_len);
                break;

            case GAP_T::NAME_LOCAL_COMPLETE:
                setName(elem_data, elem_len);
                break;

            case GAP_T::TX_POWER_LEVEL:
                if( 1 <= elem_len ) {
                    setTxPower(*const_uint8_to_const_int8_ptr(elem_data));
                }
                break;

            case GAP_T::SSP_CLASS_OF_DEVICE:
                if( 3 <= elem_len ) {
                    setDeviceClass(  elem_data[0] |
                                   ( elem_data[1] << 8 ) |
                                   ( elem_data[2] << 16 ) );
                }
                break;

            case GAP_T::DEVICE_ID:
                if( 8 <= elem_len ) {
                    setDeviceID(
                        data[0] | ( data[1] << 8 ), // source
                        data[2] | ( data[3] << 8 ), // vendor
                        data[4] | ( data[5] << 8 ), // product
                        data[6] | ( data[7] << 8 )); // version
                }
                break;

            case GAP_T::SLAVE_CONN_IVAL_RANGE:
                if( 4 <= elem_len ) {
                    const uint16_t min = jau::get_uint16(elem_data + 0, jau::lb_endian_t::little);
                    const uint16_t max = jau::get_uint16(elem_data + 2, jau::lb_endian_t::little);
                    setConnInterval(min, max);
                }
                break;

            case GAP_T::SOLICIT_UUID16:
                [[fallthrough]];
            case GAP_T::SOLICIT_UUID128:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID16:
                [[fallthrough]];
            case GAP_T::PUB_TRGT_ADDR:
                [[fallthrough]];
            case GAP_T::RND_TRGT_ADDR:
                break;

            case GAP_T::GAP_APPEARANCE:
                if( 2 <= elem_len ) {
                    setAppearance(static_cast<AppearanceCat>( jau::get_uint16(elem_data + 0, jau::lb_endian_t::little) ));
                }
                break;

            case GAP_T::SSP_HASH_C192:
                if( 16 <= elem_len ) {
                    setHash(elem_data);
                }
                break;

            case GAP_T::SSP_RANDOMIZER_R192:
                if( 16 <= elem_len ) {
                    setRandomizer(elem_data);
                }
                break;

            case GAP_T::SOLICIT_UUID32:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID32:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID128:
                break;

            case GAP_T::MANUFACTURE_SPECIFIC:
                if( 2 <= elem_len ) {
                    const uint16_t company = jau::get_uint16(elem_data + 0, jau::lb_endian_t::little);
                    const int data_size = elem_len-2;
                    setManufactureSpecificData(company, data_size > 0 ? elem_data+2 : nullptr, data_size);
                }
                break;

            default:
                // FIXME: Use a data blob!!!!
                jau_DBG_PRINT("%s-Element @ [%d/%d]: Unhandled type 0x%.2X with %d bytes net\n",
                          source, offset, data_length, elem_type, elem_len);
                break;
        }
    }
    return count;
}

#define _WARN_OOB(a) jau_DBG_PRINT("%s: Out of buffer: count %zu + 1 + ad_sz %zu > data_len %zu -> drop %s\n", (a), count, ad_sz, data_length, true);

jau::nsize_t EInfoReport::write_data(EIRDataType write_mask, uint8_t * data, jau::nsize_t const data_length) const {
    jau::nsize_t count = 0;
    uint8_t * data_i = data;
    const EIRDataType mask = write_mask & eir_data_mask;

    if( is_set(mask, EIRDataType::FLAGS) ) {
        const jau::nsize_t ad_sz = 2;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("FLAGS");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::FLAGS );
        *data_i++ = direct_bt::number( getFlags() );
    }
    if( is_set(mask, EIRDataType::NAME) ) {
        const jau::nsize_t ad_sz = 1 + name.size();
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("NAME");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::NAME_LOCAL_COMPLETE );
        memcpy(data_i, name.c_str(), ad_sz-1);
        data_i   += ad_sz-1;
    } else if( is_set(mask, EIRDataType::NAME_SHORT) ) {
        const jau::nsize_t ad_sz = 1 + name_short.size();
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("NAME_SHORT");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::NAME_LOCAL_SHORT );
        memcpy(data_i, name_short.c_str(), ad_sz-1);
        data_i   += ad_sz-1;
    }
    if( is_set(mask, EIRDataType::MANUF_DATA) && nullptr != msd ) {
        const jau::nsize_t msd_data_sz = msd->getData().size();
        const jau::nsize_t ad_sz = 1 + 2 + msd_data_sz;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("MANUF_DATA");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::MANUFACTURE_SPECIFIC );
        jau::put_uint16(data_i + 0, msd->getCompany(), jau::lb_endian_t::little);
        data_i += 2;
        if( 0 < msd_data_sz ) {
            memcpy(data_i, msd->getData().get_ptr(), msd_data_sz);
            data_i +=msd_data_sz;
        }
    }
    if( is_set(mask, EIRDataType::SERVICE_UUID) ) {
        jau::darray<std::shared_ptr<const jau::uuid_t>> uuid16s, uuid32s, uuid128s;
        for(const auto& p : services) {
            switch( p->getTypeSizeInt() ) {
                case 2:
                    uuid16s.push_back(p);
                    break;
                case 4:
                    uuid32s.push_back(p);
                    break;
                case 16:
                    uuid128s.push_back(p);
                    break;
                default:
                    jau_WARN_PRINT("Undefined UUID of size %zu: %s -> drop\n", p->getTypeSizeInt(), p->toString());
            }
        }
        if( uuid16s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid16s.size() * 2;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID16");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID16_COMPLETE : GAP_T::UUID16_INCOMPLETE);
            for(const auto& p : uuid16s) {
                data_i += p->put(data_i + 0, jau::lb_endian_t::little);
            }
        }
        if( uuid32s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid32s.size() * 4;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID32");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID32_COMPLETE : GAP_T::UUID32_INCOMPLETE);
            for(const auto& p : uuid32s) {
                data_i += p->put(data_i + 0, jau::lb_endian_t::little);
            }
        }
        if( uuid128s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid128s.size() * 16;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID128");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID128_COMPLETE : GAP_T::UUID128_INCOMPLETE);
            for(const auto& p : uuid128s) {
                data_i += p->put(data_i + 0, jau::lb_endian_t::little);
            }
        }
    }
    if( is_set(mask, EIRDataType::CONN_IVAL) ) {
        const jau::nsize_t ad_sz = 5;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("CONN_IVAL");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::SLAVE_CONN_IVAL_RANGE );
        jau::put_uint16(data_i + 0, conn_interval_min, jau::lb_endian_t::little);
        jau::put_uint16(data_i + 2, conn_interval_max, jau::lb_endian_t::little);
        data_i += 4;
    }
    if( is_set(mask, EIRDataType::TX_POWER) ) {
        const jau::nsize_t ad_sz = 2;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("TX_POWER");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::TX_POWER_LEVEL );
        *data_i++ = getTxPower();
    }
    // TODO:
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_CLASS) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::APPEARANCE) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::HASH) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::RANDOMIZER) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_ID) ) {}

#if 0
    // Not required: Mark end of significant part
    if( ( count + 1 ) <= data_length ) {
        count    += 1;
        *data_i++ = 0; // zero length
    }
#endif

    return count;
}
// #define AD_DEBUG 1

EInfoReport::Source EInfoReport::toSource(const AD_PDU_Type type) {
    switch( type ) {
        case AD_PDU_Type::ADV_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_DIRECT_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_SCAN_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_NONCONN_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_IND2:
            [[fallthrough]];
        case AD_PDU_Type::DIRECT_IND2:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_IND2:
            [[fallthrough]];
        case AD_PDU_Type::NONCONN_IND2:
            return Source::AD_IND;

        case AD_PDU_Type::SCAN_RSP:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_RSP_to_ADV_IND:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_RSP_to_ADV_SCAN_IND:
            return Source::AD_SCAN_RSP;
        default:
            return Source::NA;
    }
}

EInfoReport::Source EInfoReport::toSource(const EAD_Event_Type type) {
    if( is_set(type, EAD_Event_Type::CONN_ADV) ||
        is_set(type, EAD_Event_Type::SCAN_ADV) ||
        is_set(type, EAD_Event_Type::DIR_ADV) ) {
        return Source::AD_IND;
    }
    if( is_set(type, EAD_Event_Type::SCAN_RSP) ) {
        return Source::AD_SCAN_RSP;
    }
    return Source::NA;
}


jau::darray<std::unique_ptr<EInfoReport>> EInfoReport::read_ad_reports(uint8_t const * data, jau::nsize_t const data_length) {
    jau::nsize_t const num_reports = (jau::nsize_t) data[0];
    jau::darray<std::unique_ptr<EInfoReport>> ad_reports;

    if( 0 == num_reports || num_reports > 0x19 ) {
        jau_DBG_PRINT("AD-Reports: Invalid reports count: %zu", num_reports);
        return ad_reports;
    }
    uint8_t const *limes = data + data_length;
    uint8_t const *i_octets = data + 1;
    uint8_t ad_data_len[0x19];
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    const int seg4_size = 1 + 1 + 6 + 1;

    for(i = 0; i < num_reports && i_octets < limes; i++) { // seg 1
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource(Source::AD_IND, false /* ext */); // first guess
        ad_reports[i]->setTimestamp(timestamp);

        if( i_octets + seg4_size > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            jau_WARN_PRINT("AD-Reports: Insufficient data length (1) %zu: report %zu/%zu: min_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, seg4_size, bytes_left);
            ad_reports.pop_back();
            goto errout;
        }

        // seg 1: 1
        {
            const AD_PDU_Type ad_type = static_cast<AD_PDU_Type>(*i_octets++);
            ad_reports[i]->setEvtType( ad_type );
            ad_reports[i]->setSource( toSource( ad_type ), false /* ext */);
        }

        // seg 2: 1
        ad_reports[i]->setADAddressType(*i_octets++);

        // seg 3: 6
        ad_reports[i]->setAddress( jau::io::net::le_to_cpu( *((jau::io::net::EUI48 const *)i_octets) ) );
        i_octets += 6;

        // seg 4: 1
        ad_data_len[i] = *i_octets++;

        // seg 5: ADV Response Data (EIR)
        if( i_octets + ad_data_len[i] + 1 > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            jau_WARN_PRINT("AD-Reports: Insufficient data length (2) %zu: report %zu/%zu: eir_data_len + rssi %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, (ad_data_len[i] + 1), bytes_left);
            ad_reports.pop_back();
            goto errout;
        }
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
        }

        // seg 6: 1
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
    }

errout:
    {
        const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
        const jau::snsize_t bytes_took = static_cast<jau::snsize_t>(i_octets - data);
        if( 0 > bytes_left ) {
            jau_ERR_PRINT("AD-Reports: Buffer overflow: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
#ifdef AD_DEBUG
        else {
            jau_DBG_PRINT("AD-Reports: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
        if( jau::environment::get().debug ) {
            for(i=0; i<num_reports; i++) {
                jau_INFO_PRINT("AD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false));
            }
        }
#endif
    }
    return ad_reports;
}

jau::darray<std::unique_ptr<EInfoReport>> EInfoReport::read_ext_ad_reports(uint8_t const * data, jau::nsize_t const data_length) {
    jau::nsize_t const num_reports = (jau::nsize_t) data[0];
    jau::darray<std::unique_ptr<EInfoReport>> ad_reports;

    if( 0 == num_reports || num_reports > 0x19 ) {
        jau_DBG_PRINT("EAD-Reports: Invalid reports count: %zu", num_reports);
        return ad_reports;
    }
    uint8_t const *limes = data + data_length;
    uint8_t const *i_octets = data + 1;
    uint8_t ad_data_len[0x19];
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    const int seg12_size = 2 + 1 + 6 + 1 + 1 + 1 + 1 + 1 + 2 + 1 + 6 + 1;

    for(i = 0; i < num_reports; i++) {
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource( Source::AD_IND, true /* ext */); // first guess
        ad_reports[i]->setTimestamp(timestamp);

        if( i_octets + seg12_size > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            jau_WARN_PRINT("EAD-Reports: Insufficient data length (1) %zu: report %zu/%zu: min_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, seg12_size, bytes_left);
            ad_reports.pop_back();
            goto errout;
        }

        // seg 1: 2
        {
            const EAD_Event_Type ead_type = static_cast<EAD_Event_Type>(jau::get_uint16(i_octets + 0, jau::lb_endian_t::little));
            ad_reports[i]->setExtEvtType(ead_type);
            i_octets+=2;
            if( is_set(ead_type, EAD_Event_Type::LEGACY_PDU) ) {
                const AD_PDU_Type ad_type = static_cast<AD_PDU_Type>( ::number(ead_type) );
                ad_reports[i]->setEvtType( ad_type );
                ad_reports[i]->setSource( toSource( ad_type ), true /* ext */);
            } else {
                ad_reports[i]->setSource( toSource( ead_type ), true /* ext */);
            }
        }

        // seg 2: 1
        ad_reports[i]->setADAddressType(*i_octets++);

        // seg 3: 6
        ad_reports[i]->setAddress( jau::io::net::le_to_cpu( *((jau::io::net::EUI48 const *)i_octets) ) );
        i_octets += 6;

        // seg 4: 1
        // Primary_PHY: 0x01 = LE_1M, 0x03 = LE_CODED (TODO)
        i_octets++;

        // seg 5: 1
        // Sexondary_PHY: 0x00 None, 0x01 = LE_1M, 0x02 = LE_2M, 0x03 = LE_CODED (TODO)
        i_octets++;

        // seg 6: 1
        // Advertising_SID (TODO)
        i_octets++;

        // seg 7: 1
        ad_reports[i]->setTxPower(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;

        // seg 8: 1
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;

        // seg 9: 2
        // Periodic_Advertising_Interval (TODO)
        i_octets+=2;

        // seg 10: 1
        // Direct_Address_Type (TODO)
        i_octets++;

        // seg 11: 6
        // Direct_Address (TODO)
        i_octets+=6;

        // seg 12: 1
        ad_data_len[i] = *i_octets++;

        // seg 13: ADV Response Data (EIR)
        if( i_octets + ad_data_len[i] > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            jau_WARN_PRINT("EAD-Reports: Insufficient data length (2) %zu: report %zu/%zu: eir_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, ad_data_len[i], bytes_left);
            ad_reports.pop_back();
            goto errout;
        }
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
        }
    }

errout:
    {
        const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
        const jau::snsize_t bytes_took = static_cast<jau::snsize_t>(i_octets - data);
        if( 0 > bytes_left ) {
            jau_ERR_PRINT("EAD-Reports: Buffer overflow: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
#ifdef AD_DEBUG
        else {
            jau_DBG_PRINT("EAD-Reports: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
        if( jau::environment::get().debug ) {
            for(i=0; i<num_reports; i++) {
                jau_INFO_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false));
            }
        }
#endif
    }
    return ad_reports;
}

// *************************************************
// *************************************************
// *************************************************

