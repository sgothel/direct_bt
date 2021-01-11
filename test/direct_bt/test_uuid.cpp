#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <direct_bt/UUID.hpp>

using namespace direct_bt;

TEST_CASE( "UUID Test 01", "[datatype][uuid]" ) {
    std::cout << "Hello COUT" << std::endl;
    std::cerr << "Hello CERR" << std::endl;

    uint8_t buffer[100];
    static uint8_t uuid128_bytes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                       0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB };

    {
        const uuid128_t v01 = uuid128_t(uuid128_bytes, 0, true);
        REQUIRE(v01.getTypeSizeInt() == 16);
        REQUIRE(v01.getTypeSizeInt() == sizeof(v01.value));
        REQUIRE(v01.getTypeSizeInt() == sizeof(v01.value.data));
        REQUIRE( 0 == memcmp(uuid128_bytes, v01.data(), 16) );

        put_uuid(buffer, 0, v01, true);
        std::shared_ptr<const uuid_t> v02 = uuid_t::create(uuid_t::TypeSize::UUID128_SZ, buffer, 0, true);
        REQUIRE(v02->getTypeSizeInt() == 16);
        REQUIRE( 0 == memcmp(v01.data(), v02->data(), 16) );
        REQUIRE( v01.toString() == v02->toString() );
    }

    {
        const uuid32_t v01 = uuid32_t(uuid32_t(0x12345678));
        REQUIRE(v01.getTypeSizeInt() == 4);
        REQUIRE(v01.getTypeSizeInt() == sizeof(v01.value));
        REQUIRE(0x12345678 == v01.value);

        put_uuid(buffer, 0, v01, true);
        std::shared_ptr<const uuid_t> v02 = uuid_t::create(uuid_t::TypeSize::UUID32_SZ, buffer, 0, true);
        REQUIRE(v02->getTypeSizeInt() == 4);
        REQUIRE( 0 == memcmp(v01.data(), v02->data(), 4) );
        REQUIRE( v01.toString() == v02->toString() );
    }

    {
        const uuid16_t v01 = uuid16_t(uuid16_t(0x1234));
        REQUIRE(v01.getTypeSizeInt() == 2);
        REQUIRE(v01.getTypeSizeInt() == sizeof(v01.value));
        REQUIRE(0x1234 == v01.value);

        put_uuid(buffer, 0, v01, true);
        std::shared_ptr<const uuid_t> v02 = uuid_t::create(uuid_t::TypeSize::UUID16_SZ, buffer, 0, true);
        REQUIRE(v02->getTypeSizeInt() == 2);
        REQUIRE( 0 == memcmp(v01.data(), v02->data(), 2) );
        REQUIRE( v01.toString() == v02->toString() );
    }
}
