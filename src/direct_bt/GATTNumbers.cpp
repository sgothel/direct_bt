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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include  <algorithm>

#include <jau/darray.hpp>
#include <jau/debug.hpp>

#include "GattNumbers.hpp"

using namespace direct_bt;

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.generic_access.xml */
const GattServiceCharacteristic direct_bt::GATT_GENERIC_ACCESS_SRVC = { GENERIC_ACCESS,
        { { DEVICE_NAME, Mandatory,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Optional }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          { APPEARANCE, Mandatory,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          { PERIPHERAL_PRIVACY_FLAG, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Excluded }, { WriteNoAck, C1 }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          { RECONNECTION_ADDRESS, Conditional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Excluded },
              { WriteWithAck, Mandatory }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          { PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
        } };

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.health_thermometer.xml */
const GattServiceCharacteristic direct_bt::GATT_HEALTH_THERMOMETER_SRVC = { HEALTH_THERMOMETER,
        { { TEMPERATURE_MEASUREMENT, Mandatory,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Excluded },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Mandatory, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          },
          { TEMPERATURE_TYPE, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          { INTERMEDIATE_TEMPERATURE, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Excluded },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Mandatory }, { Indicate, Excluded }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { if_characteristic_supported, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          },
          { MEASUREMENT_INTERVAL, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Optional }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Optional }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { if_notify_or_indicate_supported, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          },
        } };

const GattServiceCharacteristic direct_bt::GATT_DEVICE_INFORMATION_SRVC = { DEVICE_INFORMATION,
        { { MANUFACTURER_NAME_STRING, Optional,
            // GattCharacteristicPropertySpec[9]:
            { { Read, Mandatory },
              { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
              { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
		  { MODEL_NUMBER_STRING, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { SERIAL_NUMBER_STRING, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { HARDWARE_REVISION_STRING, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { FIRMWARE_REVISION_STRING, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { SOFTWARE_REVISION_STRING, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { SYSTEM_ID, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { REGULATORY_CERT_DATA_LIST, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  { PNP_ID, Optional,
			// GattCharacteristicPropertySpec[9]:
			{ { Read, Mandatory },
			  { WriteWithAck, Excluded }, { WriteNoAck, Excluded }, { AuthSignedWrite, Excluded }, { ReliableWriteExt, Excluded },
			  { Notify, Excluded }, { Indicate, Mandatory }, { AuxWriteExt, Excluded }, { Broadcast, Excluded } },
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  }
        } };

const jau::darray<const GattServiceCharacteristic*> direct_bt::GATT_SERVICES = {
        &direct_bt::GATT_GENERIC_ACCESS_SRVC, &direct_bt::GATT_HEALTH_THERMOMETER_SRVC, &direct_bt::GATT_DEVICE_INFORMATION_SRVC };

#define CASE_TO_STRING(V) case V: return #V;

#define SERVICE_TYPE_ENUM(X) \
    X(GENERIC_ACCESS) \
    X(HEALTH_THERMOMETER) \
	X(DEVICE_INFORMATION) \
    X(BATTERY_SERVICE)

std::string direct_bt::GattServiceTypeToString(const GattServiceType v) noexcept {
    switch(v) {
        SERVICE_TYPE_ENUM(CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown";
}

#define CHARACTERISTIC_TYPE_ENUM(X) \
    X(DEVICE_NAME) \
    X(APPEARANCE) \
    X(PERIPHERAL_PRIVACY_FLAG) \
    X(RECONNECTION_ADDRESS) \
    X(PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS) \
    X(TEMPERATURE) \
    X(TEMPERATURE_CELSIUS) \
    X(TEMPERATURE_FAHRENHEIT) \
    X(TEMPERATURE_MEASUREMENT) \
    X(TEMPERATURE_TYPE) \
    X(INTERMEDIATE_TEMPERATURE) \
    X(MEASUREMENT_INTERVAL) \
	X(SYSTEM_ID) \
	X(MODEL_NUMBER_STRING) \
    X(SERIAL_NUMBER_STRING) \
    X(FIRMWARE_REVISION_STRING) \
    X(HARDWARE_REVISION_STRING) \
    X(SOFTWARE_REVISION_STRING) \
    X(MANUFACTURER_NAME_STRING) \
    X(REGULATORY_CERT_DATA_LIST) \
    X(PNP_ID)


std::string direct_bt::GattCharacteristicTypeToString(const GattCharacteristicType v) noexcept {
    switch(v) {
        CHARACTERISTIC_TYPE_ENUM(CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown";
}

#define CHARACTERISTIC_PROP_ENUM(X) \
        X(Broadcast) \
        X(Read) \
        X(WriteNoAck) \
        X(WriteWithAck) \
        X(Notify) \
        X(Indicate) \
        X(AuthSignedWrite) \
        X(ExtProps) \
        X(ReliableWriteExt) \
        X(AuxWriteExt)

std::string direct_bt::GattCharacteristicPropertyToString(const GattCharacteristicProperty v) noexcept {
    switch(v) {
        CHARACTERISTIC_PROP_ENUM(CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown";
}

#define REQUIREMENT_SPEC_ENUM(X) \
    X(Excluded) \
    X(Mandatory) \
    X(Optional) \
    X(Conditional) \
    X(if_characteristic_supported) \
    X(if_notify_or_indicate_supported) \
    X(C1)

std::string direct_bt::GattRequirementSpecToString(const GattRequirementSpec v) noexcept {
    switch(v) {
        REQUIREMENT_SPEC_ENUM(CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown";
}

std::string GattCharacteristicPropertySpec::toString() const noexcept {
    return GattCharacteristicPropertyToString(property)+": "+GattRequirementSpecToString(requirement);
}

std::string GattClientCharacteristicConfigSpec::toString() const noexcept {
    return "ClientCharCfg["+GattRequirementSpecToString(requirement)+"["+read.toString()+", "+writeWithAck.toString()+"]]";
}

std::string GattCharacteristicSpec::toString() const noexcept {
    std::string res = GattCharacteristicTypeToString(characteristic)+": "+GattRequirementSpecToString(requirement)+", Properties[";
    for(size_t i=0; i<propertySpec.size(); i++) {
        if(0<i) {
            res += ", ";
        }
        res += propertySpec.at(i).toString();
    }
    res += "], "+clientConfig.toString();
    return res;
}

std::string GattServiceCharacteristic::toString() const noexcept {
    std::string res = GattServiceTypeToString(service)+": [";
    for(size_t i=0; i<characteristics.size(); i++) {
        if(0<i) {
            res += ", ";
        }
        res += "["+characteristics.at(i).toString()+"]";
    }
    res += "]";
    return res;
}

const GattServiceCharacteristic * direct_bt::findGattServiceChar(const uint16_t uuid16) noexcept {
    for(size_t i=0; i<GATT_SERVICES.size(); i++) {
        const GattServiceCharacteristic & serviceChar = *GATT_SERVICES.at(i);
        if( uuid16 == serviceChar.service ) {
            return &serviceChar;
        }
        for(size_t j=0; j<serviceChar.characteristics.size(); j++) {
            const GattCharacteristicSpec & charSpec = serviceChar.characteristics.at(i);
            if( uuid16 == charSpec.characteristic ) {
                return &serviceChar;
            }
        }
    }
    return nullptr;
}

const GattCharacteristicSpec * direct_bt::findGattCharSpec(const uint16_t uuid16) noexcept {
    for(size_t i=0; i<GATT_SERVICES.size(); i++) {
        const GattServiceCharacteristic & serviceChar = *GATT_SERVICES.at(i);
        for(size_t j=0; j<serviceChar.characteristics.size(); j++) {
            const GattCharacteristicSpec & charSpec = serviceChar.characteristics.at(i);
            if( uuid16 == charSpec.characteristic ) {
                return &charSpec;
            }
        }
    }
    return nullptr;
}

/********************************************************/
/********************************************************/
/********************************************************/

std::string direct_bt::GattNameToString(const TROOctets &v) noexcept {
	const jau::nsize_t str_len = v.getSize();
	if( 0 == str_len ) {
	    return std::string(); // empty
	}
	POctets s(str_len+1); // dtor releases chunk
	memcpy(s.get_wptr(), v.get_ptr(), str_len);
	s.put_uint8_nc(str_len, 0); // EOS
	return std::string((const char*)s.get_ptr());
}

GattPeriphalPreferredConnectionParameters::GattPeriphalPreferredConnectionParameters(const TROOctets &source) noexcept
: minConnectionInterval(source.get_uint16(0)), maxConnectionInterval(source.get_uint16(2)),
  slaveLatency(source.get_uint16(4)), connectionSupervisionTimeoutMultiplier(source.get_uint16(6))
{
}

std::shared_ptr<GattPeriphalPreferredConnectionParameters> GattPeriphalPreferredConnectionParameters::get(const TROOctets &source) noexcept {
    const jau::nsize_t reqSize = 8;
    if( source.getSize() < reqSize ) {
        ERR_PRINT("GattPeriphalPreferredConnectionParameters: Insufficient data, less than %d bytes in %s", reqSize, source.toString().c_str());
        return nullptr;
    }
    return std::make_shared<GattPeriphalPreferredConnectionParameters>(source);
}

std::string GattPeriphalPreferredConnectionParameters::toString() const noexcept {
  return "PrefConnectionParam[interval["+
		  std::to_string(minConnectionInterval)+".."+std::to_string(maxConnectionInterval)+
		  "], slaveLatency "+std::to_string(slaveLatency)+
		  ", csTimeoutMul "+std::to_string(connectionSupervisionTimeoutMultiplier)+"]";
}

std::string GattGenericAccessSvc::toString() const noexcept {
    std::string pcp(nullptr != prefConnParam ? prefConnParam->toString() : "");
    return "'"+deviceName+"'[appearance "+jau::to_hexstring(static_cast<uint16_t>(appearance))+" ("+to_string(appearance)+"), "+pcp+"]";
}

GattPnP_ID::GattPnP_ID(const TROOctets &source) noexcept
: vendor_id_source(source.get_uint8(0)), vendor_id(source.get_uint16(1)),
  product_id(source.get_uint16(3)), product_version(source.get_uint16(5)) {}

std::shared_ptr<GattPnP_ID> GattPnP_ID::get(const TROOctets &source) noexcept {
    const jau::nsize_t reqSize = 7;
    if( source.getSize() < reqSize ) {
        ERR_PRINT("GattPnP_ID: Insufficient data, less than %d bytes in %s", reqSize, source.toString().c_str());
        return nullptr;
    }
    return std::make_shared<GattPnP_ID>(source);
}

std::string GattPnP_ID::toString() const noexcept {
    return "vendor_id[source "+jau::to_hexstring(vendor_id_source)+
            ", id "+jau::to_hexstring(vendor_id)+
            "], product_id "+jau::to_hexstring(product_id)+
            ", product_version "+jau::to_hexstring(product_version);
}

std::string GattDeviceInformationSvc::toString() const noexcept {
    std::string pnp(nullptr != pnpID ? pnpID->toString() : "");
    return "DeviceInfo[manufacturer '"+manufacturer+"', model '"+modelNumber+"', serial '"+serialNumber+"', systemID '"+systemID.toString()+
            "', revisions[firmware '"+firmwareRevision+"', hardware '"+hardwareRevision+"', software '"+softwareRevision+
            "'], pnpID["+pnp+"], regCertData '"+regulatoryCertDataList.toString()+"']";
}

std::shared_ptr<GattTemperatureMeasurement> GattTemperatureMeasurement::get(const TROOctets &source) noexcept {
    const jau::nsize_t size = source.getSize();
    jau::nsize_t reqSize = 1 + 4; // max size = 13
    if( reqSize > size ) {
        // min size: flags + temperatureValue
        ERR_PRINT("GattTemperatureMeasurement: Insufficient data, less than %d bytes in %s", reqSize, source.toString().c_str());
        return nullptr;
    }

    uint8_t flags = source.get_uint8(0);
    bool hasTimestamp = 0 != ( flags & Bits::HAS_TIMESTAMP );
    if( hasTimestamp ) {
        reqSize += 7;
    }
    bool hasTemperatureType = 0 != ( flags & Bits::HAS_TEMP_TYPE );
    if( hasTemperatureType ) {
        reqSize += 1;
    }
    if( reqSize > size ) {
        return nullptr;
    }

    uint32_t raw_temp_value = source.get_uint32(1);
    float temperatureValue = ieee11073::FloatTypes::float32_IEEE11073_to_IEEE754(raw_temp_value);

    /** Timestamp, if HAS_TIMESTAMP is set. */
    ieee11073::AbsoluteTime timestamp;
    if( hasTemperatureType ) {
        timestamp = ieee11073::AbsoluteTime(source.get_ptr(1+4), 7);
    }

    /** Temperature Type, if HAS_TEMP_TYPE is set: Format ???? */
    uint8_t temperature_type=0;
    if( hasTemperatureType ) {
        temperature_type = source.get_uint8(1+4+7);
    }
    return std::make_shared<GattTemperatureMeasurement>(flags, temperatureValue, timestamp, temperature_type);
}

std::string GattTemperatureMeasurement::toString() const noexcept {
    std::string res = std::to_string(temperatureValue);
    res += isFahrenheit() ? " F" : " C";
    if( hasTimestamp() ) {
        res += ", "+timestamp.toString();
    }
    if( hasTemperatureType() ) {
        res += ", type "+std::to_string(temperature_type);
    }
    return res;
}
