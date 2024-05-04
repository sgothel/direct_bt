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

#if defined(DIRECTBT_BUILDIN_GATT_SERVICE_CHARACTERISTIC_SPEC)


#if 0
// Testing jau::make_darray compliance ..
struct Lala0 { int a; int b; };

static jau::darray<GattCharacteristicPropertySpec> lala =
        jau::make_darray(
          GCPS{ Read, Mandatory },
          GCPS{ Notify, Excluded } );

static jau::darray<GattCharacteristicPropertySpec> lala2 =
        jau::make_darray(
          Lala0{ 0, 1 },
          Lala0{ 1, 2 } );

static jau::darray<GattCharacteristicPropertySpec> lala3 =
        jau::make_darray(
          GCPS{ Read, Mandatory },
          Lala0{ 1, 2 } );
#endif

typedef GattCharacteristicSpec GCS;
typedef GattCharacteristicPropertySpec GCPS;

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.generic_access.xml */
const GattServiceCharacteristic direct_bt::GATT_GENERIC_ACCESS_SRVC { GENERIC_ACCESS,
        jau::make_darray( // GattCharacteristicSpec
          GCS{ DEVICE_NAME, Mandatory,
            // jau::make_darray<GattCharacteristicPropertySpec, GattCharacteristicPropertySpec...>( // GattCharacteristicPropertySpec [9]:
            jau::make_darray( // GattCharacteristicPropertySpec [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Optional }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ APPEARANCE, Mandatory,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ PERIPHERAL_PRIVACY_FLAG, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, C1 }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ RECONNECTION_ADDRESS, Conditional,
            jau::make_darray( // [9]:
              GCPS{ Read, Excluded },
              GCPS{ WriteWithAck, Mandatory }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          }
        ) };

/** https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Services/org.bluetooth.service.health_thermometer.xml */
const GattServiceCharacteristic direct_bt::GATT_HEALTH_THERMOMETER_SRVC { HEALTH_THERMOMETER,
        jau::make_darray( // GattCharacteristicSpec
          GCS{ TEMPERATURE_MEASUREMENT, Mandatory,
            jau::make_darray( // [9]:
              GCPS{ Read, Excluded },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Mandatory, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          },
          GCS{ TEMPERATURE_TYPE, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ INTERMEDIATE_TEMPERATURE, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Excluded },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Mandatory }, GCPS{ Indicate, Excluded }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { if_characteristic_supported, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          },
          GCS{ MEASUREMENT_INTERVAL, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Optional }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Optional }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { if_notify_or_indicate_supported, { Read, Mandatory}, { WriteWithAck, Mandatory } }
          }
        ) };

const GattServiceCharacteristic direct_bt::GATT_DEVICE_INFORMATION_SRVC { DEVICE_INFORMATION,
        jau::make_darray( // GattCharacteristicSpec
          GCS{ MANUFACTURER_NAME_STRING, Optional,
            jau::make_darray( // [9]:
              GCPS{ Read, Mandatory },
              GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
              GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
            // GattClientCharacteristicConfigSpec:
            { Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
          },
          GCS{ MODEL_NUMBER_STRING, Optional,
            jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ SERIAL_NUMBER_STRING, Optional,
            jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ HARDWARE_REVISION_STRING, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ FIRMWARE_REVISION_STRING, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ SOFTWARE_REVISION_STRING, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ SYSTEM_ID, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ REGULATORY_CERT_DATA_LIST, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  },
		  GCS{ PNP_ID, Optional,
		    jau::make_darray( // [9]:
			  GCPS{ Read, Mandatory },
			  GCPS{ WriteWithAck, Excluded }, GCPS{ WriteNoAck, Excluded }, GCPS{ AuthSignedWrite, Excluded }, GCPS{ ReliableWriteExt, Excluded },
			  GCPS{ Notify, Excluded }, GCPS{ Indicate, Mandatory }, GCPS{ AuxWriteExt, Excluded }, GCPS{ Broadcast, Excluded } ),
			// GattClientCharacteristicConfigSpec:
			{ Excluded, { Read, Excluded}, { WriteWithAck, Excluded } }
		  }
        ) };

const jau::darray<const GattServiceCharacteristic*> direct_bt::GATT_SERVICES = jau::make_darray (
        &direct_bt::GATT_GENERIC_ACCESS_SRVC, &direct_bt::GATT_HEALTH_THERMOMETER_SRVC, &direct_bt::GATT_DEVICE_INFORMATION_SRVC );

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

#endif /* DIRECTBT_BUILDIN_GATT_SERVICE_CHARACTERISTIC_SPEC */

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
        res.append("[").append(characteristics.at(i).toString()).append("]");
    }
    res += "]";
    return res;
}

/********************************************************/
/********************************************************/
/********************************************************/

std::string direct_bt::GattNameToString(const jau::TROOctets &v) noexcept {
	const jau::nsize_t str_len = v.size();
	if( 0 == str_len ) {
	    return std::string(); // empty
	}
	jau::POctets s(str_len+1, jau::lb_endian::little); // dtor releases chunk
	{
	    // Prelim checking to avoid g++ 8.3 showing a warning: pointer overflow between offset 0 and size
        uint8_t const * const v_p = v.get_ptr();
        if( nullptr == v_p ) {
            return std::string(); // emptylb_lb_lb_
        }
        uint8_t * const s_p = s.get_wptr();
        if( nullptr == s_p ) {
            return std::string(); // empty
        }
        memcpy(s_p, v_p, str_len);
	}
	s.put_uint8_nc(str_len, 0); // EOS
	return std::string((const char*)s.get_ptr());
}

GattPeriphalPreferredConnectionParameters::GattPeriphalPreferredConnectionParameters(const jau::TROOctets &source) noexcept
: minConnectionInterval(source.get_uint16(0)), maxConnectionInterval(source.get_uint16(2)),
  slaveLatency(source.get_uint16(4)), connectionSupervisionTimeoutMultiplier(source.get_uint16(6))
{
}

std::shared_ptr<GattPeriphalPreferredConnectionParameters> GattPeriphalPreferredConnectionParameters::get(const jau::TROOctets &source) noexcept {
    const jau::nsize_t reqSize = 8;
    if( source.size() < reqSize ) {
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

GattPnP_ID::GattPnP_ID(const jau::TROOctets &source) noexcept
: vendor_id_source(source.get_uint8(0)), vendor_id(source.get_uint16(1)),
  product_id(source.get_uint16(3)), product_version(source.get_uint16(5)) {}

std::shared_ptr<GattPnP_ID> GattPnP_ID::get(const jau::TROOctets &source) noexcept {
    const jau::nsize_t reqSize = 7;
    if( source.size() < reqSize ) {
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

std::shared_ptr<GattTemperatureMeasurement> GattTemperatureMeasurement::get(const jau::TROOctets &source) noexcept {
    const jau::nsize_t size = source.size();
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
