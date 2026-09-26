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

#include <jau/darray.hpp>
#include <jau/debug.hpp>
#include <jau/string_cfmt.hpp>
#include <jau/string_util.hpp>

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

namespace direct_bt {
    JAU_MAKE_ENUM_STRING_CODE(GattAttributeType,
        PRIMARY_SERVICE, SECONDARY_SERVICE, INCLUDE_DECLARATION, CHARACTERISTIC);
    JAU_MAKE_ENUM_STRING_CODE(GattServiceType,
        GENERIC_ACCESS, GENERIC_ATTRIBUTE, IMMEDIATE_ALERT, LINK_LOSS,
        HEALTH_THERMOMETER, DEVICE_INFORMATION, BATTERY_SERVICE);
    JAU_MAKE_ENUM_STRING_CODE(GattCharacteristicType,
        DEVICE_NAME, APPEARANCE, PERIPHERAL_PRIVACY_FLAG,
        RECONNECTION_ADDRESS, PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS,
        SERVICE_CHANGED, TEMPERATURE, TEMPERATURE_CELSIUS, TEMPERATURE_FAHRENHEIT,
        TEMPERATURE_MEASUREMENT, TEMPERATURE_TYPE, INTERMEDIATE_TEMPERATURE,
        MEASUREMENT_INTERVAL, SYSTEM_ID, MODEL_NUMBER_STRING, SERIAL_NUMBER_STRING,
        FIRMWARE_REVISION_STRING, HARDWARE_REVISION_STRING, SOFTWARE_REVISION_STRING,
        MANUFACTURER_NAME_STRING, REGULATORY_CERT_DATA_LIST, PNP_ID);
    JAU_MAKE_ENUM_STRING_CODE(GattCharacteristicProperty,
        Broadcast, Read, WriteNoAck, WriteWithAck, Notify, Indicate,
        AuthSignedWrite, ExtProps, ReliableWriteExt, AuxWriteExt);
    JAU_MAKE_ENUM_STRING_CODE(GattRequirementSpec,
        Excluded, Mandatory, Optional, Conditional,
        if_characteristic_supported, if_notify_or_indicate_supported, C1);

    JAU_MAKE_ENUM_STRING2_CODE(GattCharacteristicSpec::PropertySpecIdx, PropertySpecIdx,
        ReadIdx, WriteNoAckIdx, WriteWithAckIdx, AuthSignedWriteIdx,
        ReliableWriteExtIdx, NotifyIdx, IndicateIdx, AuxWriteExtIdx, BroadcastIdx);
}

std::string GattCharacteristicPropertySpec::toString() const noexcept {
    return jau_format_string("%s: %s", property, requirement);
}

std::string GattClientCharacteristicConfigSpec::toString() const noexcept {
    return jau_format_string("ClientCharCfg[%s[%s, %s]]", requirement, read, writeWithAck);
}

std::string GattCharacteristicSpec::toString() const noexcept {
    std::string res = jau_format_string("%s: %s, Properties[", characteristic, requirement);
    for(size_t i=0; i<propertySpec.size(); i++) {
        if(0<i) {
            jau::append_string(res, ", ");
        }
        jau::append_string(res, propertySpec[i].toString());
    }
    jau::append_string(res, "], ");
    jau::append_string(res, clientConfig.toString());
    return res;
}

std::string GattServiceCharacteristic::toString() const noexcept {
    std::string res = jau_format_string("%s: [", service);
    for(size_t i=0; i<characteristics.size(); i++) {
        if(0<i) {
            jau::append_string(res, ", ");
        }
        jau::append_string(res, "[");
        jau::append_string(res, characteristics[i].toString());
        jau::append_string(res, "]");
    }
    jau::append_string(res, "]");
    return res;
}

/********************************************************/
/********************************************************/
/********************************************************/

std::string direct_bt::GattNameToString(const jau::TROOctets &v) noexcept {
    const jau::nsize_t str_len = v.size();
    if (0 == str_len) {
        return std::string();  // empty
    }
    uint8_t const *const v_p = v.get_ptr();
    std::string res;
    jau::reserve_append_string(res, str_len + 1, str_len);
    if (!v_p || res.capacity() < str_len + 1 || res.length() < str_len) {
        return std::string();
    }
    ::memcpy(res.data(), v_p, str_len);
    return res;
}

GattPeriphalPreferredConnectionParameters::GattPeriphalPreferredConnectionParameters(const jau::TROOctets &source)
: minConnectionInterval(source.get_uint16(0)), maxConnectionInterval(source.get_uint16(2)),
  slaveLatency(source.get_uint16(4)), connectionSupervisionTimeoutMultiplier(source.get_uint16(6))
{
}

std::shared_ptr<GattPeriphalPreferredConnectionParameters> GattPeriphalPreferredConnectionParameters::get(const jau::TROOctets &source) {
    const jau::nsize_t reqSize = 8;
    if( source.size() < reqSize ) {
        jau_ERR_PRINT("GattPeriphalPreferredConnectionParameters: Insufficient data, less than %zu bytes in %s", reqSize, source.toString());
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
    return "'"+deviceName+"'[appearance "+jau::toHexString(static_cast<uint16_t>(appearance))+" ("+to_string(appearance)+"), "+pcp+"]";
}

GattPnP_ID::GattPnP_ID(const jau::TROOctets &source)
: vendor_id_source(source.get_uint8(0)), vendor_id(source.get_uint16(1)),
  product_id(source.get_uint16(3)), product_version(source.get_uint16(5)) {}

std::shared_ptr<GattPnP_ID> GattPnP_ID::get(const jau::TROOctets &source) {
    const jau::nsize_t reqSize = 7;
    if( source.size() < reqSize ) {
        jau_ERR_PRINT("GattPnP_ID: Insufficient data, less than %zu bytes in %s", reqSize, source.toString());
        return nullptr;
    }
    return std::make_shared<GattPnP_ID>(source);
}

std::string GattPnP_ID::toString() const noexcept {
    return jau_format_string("vendor_id[source %#x, id %#x], product[id %#x, version %#x]",
        vendor_id_source, vendor_id, product_id, product_version);
}

std::string GattDeviceInformationSvc::toString() const noexcept {
    return jau_format_string("DeviceInfo[manufacturer '%s', model '%s', serial '%s', systemID '%s', "
        "revisions[firmware '%s', hardware '%s', software '%s'], pnpID[%s], repCertData '%s']",
        manufacturer, modelNumber, serialNumber, systemID,
        firmwareRevision, hardwareRevision, softwareRevision,
        (nullptr != pnpID ? pnpID->toString() : ""), regulatoryCertDataList);
}

std::shared_ptr<GattTemperatureMeasurement> GattTemperatureMeasurement::get(const jau::TROOctets &source) {
    const jau::nsize_t size = source.size();
    jau::nsize_t reqSize = 1 + 4; // max size = 13
    if( reqSize > size ) {
        // min size: flags + temperatureValue
        jau_ERR_PRINT("GattTemperatureMeasurement: Insufficient data, less than %zu bytes in %s", reqSize, source.toString());
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
    std::string res = jau_format_string("%.3f %c", temperatureValue, (isFahrenheit() ? 'F' : 'C'));
    if( hasTimestamp() ) {
        jau_append_string(res, ", %s", timestamp);
    }
    if( hasTemperatureType() ) {
        jau_append_string(res, ", type %u", temperature_type);
    }
    return res;
}
