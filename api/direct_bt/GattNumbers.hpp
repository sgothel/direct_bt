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

#ifndef GATT_NUMBERS_HPP_
#define GATT_NUMBERS_HPP_

#include <cstdint>

#include <jau/basic_types.hpp>
#include <jau/darray.hpp>
#include <jau/octets.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"
#include "ieee11073/DataTypes.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * GattNumbers.hpp Module for Higher level GATT value and service types like GattServiceType, GattCharacteristicType,
 * GattCharacteristicProperty, GattRequirementSpec .. and finally GattServiceCharacteristic.
 *
 * - https://www.bluetooth.com/specifications/gatt/services/
 *
 * - https://www.bluetooth.com/specifications/gatt/ - See GATT Specification Supplement (GSS) Version 2
 *
 */
namespace direct_bt {

/** \addtogroup DBTUserAPI
 *
 *  @{
 */

/**
 * Following UUID16 GATT profile attribute types are listed under:
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.4 Summary of GATT Profile Attribute Types
 *
 * See BTGattDesc::Type and GattCharacteristicType for further declarations.
 */
enum GattAttributeType : uint16_t {
    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services, using AttPDUMsg::Opcode::READ_BY_GROUP_TYPE_REQ */
    PRIMARY_SERVICE                             = 0x2800,
    SECONDARY_SERVICE                           = 0x2801,

    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.5.1 Find Included Services, using AttPDUMsg::Opcode::READ_BY_TYPE_REQ */
    INCLUDE_DECLARATION                         = 0x2802,

    /* BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service, using , using AttPDUMsg::Opcode::READ_BY_TYPE_REQ */
    CHARACTERISTIC                              = 0x2803
};

/**
 * GATT Service Type, each encapsulating a set of Characteristics.
 *
 * <pre>
 * https://www.bluetooth.com/specifications/gatt/services/
 *
 * https://www.bluetooth.com/specifications/gatt/ - See GATT Specification Supplement (GSS) Version 2
 * </pre>
 */
enum GattServiceType : uint16_t {
    /** This service contains generic information about the device. This is a mandatory service. */
    GENERIC_ACCESS                              = 0x1800,
    /** The service allows receiving indications of changed services. This is a mandatory service. */
    GENERIC_ATTRIBUTE                           = 0x1801,
    /** This service exposes a control point to change the peripheral alert behavior. */
    IMMEDIATE_ALERT                             = 0x1802,
    /** The service defines behavior on the device when a link is lost between two devices. */
    LINK_LOSS                                   = 0x1803,
    /** This service exposes temperature and other data from a thermometer intended for healthcare and fitness applications. */
    HEALTH_THERMOMETER                          = 0x1809,
	/** This service exposes manufacturer and/or vendor information about a device. */
	DEVICE_INFORMATION                          = 0x180A,
    /** This service exposes the state of a battery within a device. */
    BATTERY_SERVICE                             = 0x180F,
};
std::string GattServiceTypeToString(const GattServiceType v) noexcept;

/**
 * GATT Assigned Characteristic Attribute Type for single logical value.
 *
 * https://www.bluetooth.com/specifications/gatt/characteristics/
 *
 * See GattAttributeType for further non BTGattChar related declarations.
 */
enum GattCharacteristicType : uint16_t {
    //
    // GENERIC_ACCESS
    //
    DEVICE_NAME                                 = 0x2A00,
    APPEARANCE                                  = 0x2A01,
    PERIPHERAL_PRIVACY_FLAG                     = 0x2A02,
    RECONNECTION_ADDRESS                        = 0x2A03,
    PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS  = 0x2A04,

    //
    // GENERIC_ATTRIBUTE
    //
    SERVICE_CHANGED                             = 0x2a05,

    /** Mandatory: sint16 10^-2: Celsius */
    TEMPERATURE                                 = 0x2A6E,

    /** Mandatory: sint16 10^-1: Celsius */
    TEMPERATURE_CELSIUS                         = 0x2A1F,
    TEMPERATURE_FAHRENHEIT                      = 0x2A20,

    //
    // HEALTH_THERMOMETER
    //
    TEMPERATURE_MEASUREMENT                     = 0x2A1C,
    /** Mandatory: 8bit: 1 armpit, 2 body (general), 3(ear), 4 (finger), ... */
    TEMPERATURE_TYPE                            = 0x2A1D,
    INTERMEDIATE_TEMPERATURE                    = 0x2A1E,
    MEASUREMENT_INTERVAL                        = 0x2A21,

	//
	// DEVICE_INFORMATION
	//
	/** Mandatory: uint40 */
	SYSTEM_ID 									= 0x2A23,
	MODEL_NUMBER_STRING 						= 0x2A24,
	SERIAL_NUMBER_STRING 						= 0x2A25,
	FIRMWARE_REVISION_STRING 					= 0x2A26,
	HARDWARE_REVISION_STRING 					= 0x2A27,
	SOFTWARE_REVISION_STRING 					= 0x2A28,
	MANUFACTURER_NAME_STRING 					= 0x2A29,
	REGULATORY_CERT_DATA_LIST 					= 0x2A2A,
	PNP_ID 										= 0x2A50,
};
std::string GattCharacteristicTypeToString(const GattCharacteristicType v) noexcept;

enum GattCharacteristicProperty : uint8_t {
    Broadcast = 0x01,
    Read = 0x02,
    WriteNoAck = 0x04,
    WriteWithAck = 0x08,
    Notify = 0x10,
    Indicate = 0x20,
    AuthSignedWrite = 0x40,
    ExtProps = 0x80,
    /** FIXME: extension? */
    ReliableWriteExt = 0x81,
    /** FIXME: extension? */
    AuxWriteExt = 0x82
};
std::string GattCharacteristicPropertyToString(const GattCharacteristicProperty v) noexcept;

enum GattRequirementSpec : uint8_t {
    Excluded    = 0x00,
    Mandatory   = 0x01,
    Optional    = 0x02,
    Conditional = 0x03,
    if_characteristic_supported = 0x11,
    if_notify_or_indicate_supported = 0x12,
    C1 = 0x21,
};
std::string GattRequirementSpecToString(const GattRequirementSpec v) noexcept;

struct GattCharacteristicPropertySpec {
    GattCharacteristicProperty property;
    GattRequirementSpec requirement;

    std::string toString() const noexcept;
};

struct GattClientCharacteristicConfigSpec {
    GattRequirementSpec requirement;
    GattCharacteristicPropertySpec read;
    GattCharacteristicPropertySpec writeWithAck;

    std::string toString() const noexcept;
};

struct GattCharacteristicSpec {
    GattCharacteristicType characteristic;
    GattRequirementSpec requirement;

    enum PropertySpecIdx : int {
        ReadIdx = 0,
        WriteNoAckIdx,
        WriteWithAckIdx,
        AuthSignedWriteIdx,
        ReliableWriteExtIdx,
        NotifyIdx,
        IndicateIdx,
        AuxWriteExtIdx,
        BroadcastIdx
    };
    /** Aggregated in PropertySpecIdx order */
    jau::darray<GattCharacteristicPropertySpec> propertySpec;

    GattClientCharacteristicConfigSpec clientConfig;

    std::string toString() const noexcept;
};

struct GattServiceCharacteristic {
    GattServiceType service;
    jau::darray<GattCharacteristicSpec> characteristics;

    std::string toString() const noexcept;
};

// #define DIRECTBT_BUILDIN_GATT_SERVICE_CHARACTERISTIC_SPEC 1
#if defined(DIRECTBT_BUILDIN_GATT_SERVICE_CHARACTERISTIC_SPEC)
/**
 * Intentionally ease compile and linker burden by using 'extern' instead of 'inline',
 * as the latter would require compile to crunch the structure
 * and linker to chose where to place the actual artifact.
 */
extern const GattServiceCharacteristic GATT_GENERIC_ACCESS_SRVC;
extern const GattServiceCharacteristic GATT_HEALTH_THERMOMETER_SRVC;
extern const GattServiceCharacteristic GATT_DEVICE_INFORMATION_SRVC;
extern const jau::darray<const GattServiceCharacteristic*> GATT_SERVICES;

/**
 * Find the GattServiceCharacteristic entry by given uuid16,
 * denominating either a GattServiceType or GattCharacteristicType.
 */
const GattServiceCharacteristic * findGattServiceChar(const uint16_t uuid16) noexcept;

/**
 * Find the GattCharacteristicSpec entry by given uuid16,
 * denominating either a GattCharacteristicType.
 */
const GattCharacteristicSpec * findGattCharSpec(const uint16_t uuid16) noexcept;

#endif /* DIRECTBT_BUILDIN_GATT_SERVICE_CHARACTERISTIC_SPEC */

/********************************************************
 *
 * Known GATT Characteristic data value types.
 *
 ********************************************************/

/**
 * Converts a GATT Name (not null-terminated) UTF8 to a null-terminated C++ string
 */
std::string GattNameToString(const jau::TROOctets &v) noexcept;

/**
 * <i>Peripheral Preferred Connection Parameters</i> is a GATT Characteristic.
 * <pre>
 * https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.gap.peripheral_preferred_connection_parameters.xml
 * </pre>
 */
struct GattPeriphalPreferredConnectionParameters {
    /** mandatory [6..3200] x 1.25ms */
    const uint16_t minConnectionInterval;
    /** mandatory [6..3200] x 1.25ms and >= minConnectionInterval */
    const uint16_t maxConnectionInterval;
    /** mandatory [1..1000] */
    const uint16_t slaveLatency;
    /** mandatory [10..3200] */
    const uint16_t connectionSupervisionTimeoutMultiplier;

    static std::shared_ptr<GattPeriphalPreferredConnectionParameters> get(const jau::TROOctets &source) noexcept;

    GattPeriphalPreferredConnectionParameters(const jau::TROOctets &source) noexcept;

    std::string toString() const noexcept;
};

/**
 * <i>Generic Access Service</i> is a mandatory GATT service all peripherals are required to implement. (FIXME: Add reference)
 * <pre>
 * https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.generic_access.xml
 * </pre>
 */
class GattGenericAccessSvc {
	public:
        /** Characteristic: Mandatory [Read: Mandatory; Write: Optional; ...]*/
		const std::string deviceName;
		/** Characteristic: Mandatory [Read: Mandatory; Write: Excluded; ...]*/
		const AppearanceCat appearance;
        /** Characteristic: Optional [Read: Mandatory; Write: Conditional; ...]*/
        const std::string peripheralPrivacyFlag; // FIXME: Value
        /** Characteristic: Conditional [Read: Excluded; Write: Mandatory; ...]*/
        const std::string reconnectionAdress; // FIXME: Value
		/** Characteristic: Optional [Read: Mandatory; Write: Excluded; ...]*/
		const std::shared_ptr<GattPeriphalPreferredConnectionParameters> prefConnParam;

		GattGenericAccessSvc(std::string deviceName_, const AppearanceCat appearance_,
		                     std::shared_ptr<GattPeriphalPreferredConnectionParameters> prefConnParam_) noexcept
		: deviceName(std::move(deviceName_)), appearance(appearance_), prefConnParam(std::move(prefConnParam_)) {}

		std::string toString() const noexcept;
};

/**
 * <i>PnP ID</i> is a GATT Characteristic.
 * <pre>
 * https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.pnp_id.xml
 * </pre>
 */
struct GattPnP_ID {
    const uint8_t vendor_id_source;
    const uint16_t vendor_id;
    const uint16_t product_id;
    const uint16_t product_version;

    static std::shared_ptr<GattPnP_ID> get(const jau::TROOctets &source) noexcept;

    GattPnP_ID() noexcept
    : vendor_id_source(0), vendor_id(0), product_id(0), product_version(0) {}

    GattPnP_ID(const jau::TROOctets &source) noexcept;

    GattPnP_ID(const uint8_t vendor_id_source_, const uint16_t vendor_id_, const uint16_t product_id_, const uint16_t product_version_) noexcept
    : vendor_id_source(vendor_id_source_), vendor_id(vendor_id_), product_id(product_id_), product_version(product_version_) {}

    std::string toString() const noexcept;
};

/**
 * <i>Device Information</i> is a GATT service.
 * <pre>
 * https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.device_information.xml
 * </pre>
 */
class GattDeviceInformationSvc {
    public:
        /** Optional */
        const jau::POctets systemID;
        /** Optional */
        const std::string modelNumber;
        /** Optional */
        const std::string serialNumber;
        /** Optional */
        const std::string firmwareRevision;
        /** Optional */
        const std::string hardwareRevision;
        /** Optional */
        const std::string softwareRevision;
        /** Optional */
        const std::string manufacturer;
        /** Optional */
        const jau::POctets regulatoryCertDataList;
        /** Optional */
        const std::shared_ptr<GattPnP_ID> pnpID;

        GattDeviceInformationSvc(jau::POctets systemID_, std::string modelNumber_, std::string serialNumber_,
                                 std::string firmwareRevision_, std::string hardwareRevision_, std::string softwareRevision_,
                                 std::string manufacturer_, jau::POctets regulatoryCertDataList_, std::shared_ptr<GattPnP_ID> pnpID_) noexcept
        : systemID( std::move(systemID_) ), modelNumber( std::move(modelNumber_) ), serialNumber( std::move(serialNumber_) ), 
          firmwareRevision( std::move(firmwareRevision_) ), hardwareRevision( std::move(hardwareRevision_) ), 
          softwareRevision( std::move(softwareRevision_) ), manufacturer( std::move(manufacturer_) ),
          regulatoryCertDataList( std::move(regulatoryCertDataList_) ), pnpID( std::move(pnpID_) ) {}

        std::string toString() const noexcept;
};

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.battery_service.xml */
class GattBatteryServiceSvc {
    // TODO
};

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.temperature_measurement.xml */
class GattTemperatureMeasurement {
    public:
        enum Bits : uint8_t {
            /** bit 0: If set, temperature is in Fahrenheit, otherwise Celsius. */
            IS_TEMP_FAHRENHEIT  = 1,
            /** bit 1: If set, timestamp field present, otherwise not.. */
            HAS_TIMESTAMP       = 2,
            /** bit 2: If set, temperature type field present, otherwise not.. */
            HAS_TEMP_TYPE       = 4
        };
        /** Bitfields of Bits. 1 byte. */
        const uint8_t flags;

        /** In Celsius if IS_TEMP_FAHRENHEIT is set, otherwise Fahrenheit. 4 bytes. */
        const float temperatureValue;

        /** Timestamp, if HAS_TIMESTAMP is set. 7 bytes(!?) here w/o fractions. */
        const ieee11073::AbsoluteTime timestamp;

        /** Temperature Type, if HAS_TEMP_TYPE is set: Format ????. 1 byte (!?). */
        const uint8_t temperature_type;

        static std::shared_ptr<GattTemperatureMeasurement> get(const jau::TROOctets &source) noexcept;

        static std::shared_ptr<GattTemperatureMeasurement> get(const jau::TOctetSlice &source) noexcept {
            const jau::TROOctets o(source.get_ptr(0), source.size(), jau::lb_endian::little);
            return get(o);
        }

        GattTemperatureMeasurement(const uint8_t flags_, const float temperatureValue_,
                                   const ieee11073::AbsoluteTime &timestamp_, const uint8_t temperature_type_) noexcept
        : flags(flags_), temperatureValue(temperatureValue_), timestamp(timestamp_), temperature_type(temperature_type_) {}

        bool isFahrenheit() const noexcept { return 0 != ( flags & Bits::IS_TEMP_FAHRENHEIT ); }
        bool hasTimestamp() const noexcept { return 0 != ( flags & Bits::HAS_TIMESTAMP ); }
        bool hasTemperatureType() const noexcept { return 0 != ( flags & Bits::HAS_TEMP_TYPE ); }

        std::string toString() const noexcept;
};

/**@}*/

} // namespace direct_bt

#endif /* GATT_IOCTL_HPP_ */
