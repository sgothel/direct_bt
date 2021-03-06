#include "BluetoothUUID.hpp"

#include <cinttypes>
#include <cstring>
#include <iostream>

using namespace tinyb;

BluetoothUUID::BluetoothUUID(const BluetoothUUID &other) {
    uuid[0] = other.uuid[0];
    uuid[1] = other.uuid[1];
}

BluetoothUUID::BluetoothUUID(const char str[]) {
    int len = strlen(str);
    const char *err_msg = "UUID does not have a valid format";

    if (len == 4 || len == 8) {
       /* 16bit or 32bit UUID: number + base UUID */
       const uint64_t strval = strtoul(str, NULL, 16);
       uuid[0] = strval << 32 | 0x00001000ULL;
       uuid[1] = 0x800000805f9b34fbULL;
    } else if (len == 36) {
    /* 128bit UUID */
       char u[37];
       strcpy(u, str);

       if (u[8] == '-') {
            u[8] = ' ';
            const uint64_t uval = strtoul(u + 0, NULL, 16);
            uuid[0] = uval << 32;
       } else {
            throw std::invalid_argument(err_msg);
       }
       if (u[13] == '-') {
            u[13] = ' ';
            uuid[0] = uuid[0] | strtoul(u + 9, NULL, 16) << 16;
       } else throw std::invalid_argument(err_msg);
       if (u[18] == '-') {
            u[18] = ' ';
            uuid[0] = uuid[0] | strtoul(u + 14, NULL, 16);
       } else throw std::invalid_argument(err_msg);

       if (u[23] == '-') {
            u[23] = ' ';
            const uint64_t uval = strtoul(u + 19, NULL, 16);
            uuid[1] = uval << 48;
       } else throw std::invalid_argument(err_msg);

       uuid[1] = uuid[1] | strtoul(u + 24, NULL, 16);
    } else throw std::invalid_argument(err_msg);
}

BluetoothUUID::BluetoothUUID(const std::string &str) : BluetoothUUID(str.c_str()) {}

std::string BluetoothUUID::get_string()
{
    char u[37];
    snprintf(u, 37, "%08" PRIx64 "-%04" PRIx64 "-%04" PRIx64 "-%04" PRIx64 "-%012" PRIx64,
        (uint64_t)(uuid[0] >> 32),
        (uint64_t)((uuid[0] >> 16) & 0xFFFFULL),
        (uint64_t)(uuid[0] & 0xFFFFULL),
        (uint64_t)(uuid[1] >> 48),
        (uint64_t)(uuid[1] & ~(0xFFFFULL << 48)));
    return std::string(u);
}

std::string BluetoothUUID::get_short_string()
{
    char u[12];
    if (is_short()) {
        uint32_t suuid = get_short();
        if ( ( suuid & 0xFFFF ) == suuid )
            snprintf(u, sizeof(u), "%04ux", suuid);
        else
            snprintf(u, sizeof(u), "%08ux", suuid);
        return std::string(u);
    } else {
        return get_string();
    }
}

uint32_t BluetoothUUID::get_short() {
    if (is_short())
        return uuid[0] >> 32;
    return 0;
}

bool BluetoothUUID::is_short()
{
    if ( ( uuid[1] == 0x800000805f9b34fbULL ) && uuid[0] & ( 0xffffffffULL == 0x00001000ULL ) )
        return true;
    return false;
}
