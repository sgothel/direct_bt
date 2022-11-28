#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <direct_bt/ieee11073/DataTypes.hpp>

using namespace ieee11073;

static void test_float32_IEEE11073_to_IEEE754(const std::string& msg, const uint32_t raw, const float expFloat) {
    const float has = FloatTypes::float32_IEEE11073_to_IEEE754(raw);
    INFO_STR(msg+": has '"+std::to_string(has));
    INFO_STR(msg+": exp '"+std::to_string(expFloat)+"', diff "+std::to_string(fabsf(has-expFloat)));
    REQUIRE_EPSI_MSG(msg, has, expFloat);
}

static void test_AbsoluteTime_IEEE11073(const std::string& msg, const uint8_t * data_le, const int size, const std::string& expStr) {
    ieee11073::AbsoluteTime has(data_le, size);
    const std::string has_str = has.toString();
    INFO_STR(msg+": has '"+has_str+"', len "+std::to_string(has_str.length()));
    INFO_STR(msg+": exp '"+expStr+"', len "+std::to_string(expStr.length())+", equal: "+std::to_string(has_str==expStr));
    REQUIRE_MSG(msg, has_str.length() == expStr.length());
    REQUIRE_MSG(msg, has_str == expStr);
}

TEST_CASE( "Datatype IEEE11073 Test 01", "[datatype][IEEE11073]" ) {
    // 0x06 670100FF E40704040B1A00 00
    // 0x06 640100FF E40704040B2C00 00

    // 79 09 00 FE -> 24.25f
    test_float32_IEEE11073_to_IEEE754("IEEE11073-float01", 0xFE000979, 24.25f);
    // 670100FF -> 35.900002
    test_float32_IEEE11073_to_IEEE754("IEEE11073-float01", 0xFF000167, 35.900002f);
    // 640100FF -> 35.600002
    test_float32_IEEE11073_to_IEEE754("IEEE11073-float02", 0xFF000164, 35.600002f);

    {
        // E40704040B1A00 -> 2020-04-04 11:26:00
        const uint8_t input[] = { 0xE4, 0x07, 0x04, 0x04, 0x0B, 0x1A, 0x00 };
        test_AbsoluteTime_IEEE11073("IEEE11073 time01", input, 7, "2020-04-04 11:26:00");
    }
    {
        // E40704040B2C00 -> 2020-04-04 11:44:00
        const uint8_t input[] = { 0xE4, 0x07, 0x04, 0x04, 0x0B, 0x2C, 0x00 };
        test_AbsoluteTime_IEEE11073("IEEE11073 time02", input, 7, "2020-04-04 11:44:00");
    }
}

