#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <jau/basic_types.hpp>
#include <direct_bt/BTAddress.hpp>

using namespace direct_bt;
using namespace jau;

TEST_CASE( "EUI48 Test 01", "[datatype][eui48]" ) {
    EUI48 mac01;
    INFO_STR("EUI48 size: whole0 "+std::to_string(sizeof(EUI48)));
    INFO_STR("EUI48 size: whole1 "+std::to_string(sizeof(mac01)));
    INFO_STR("EUI48 size:  data1 "+std::to_string(sizeof(mac01.b)));
    REQUIRE_MSG("EUI48 struct and data size match", sizeof(EUI48) == sizeof(mac01));
    REQUIRE_MSG("EUI48 struct and data size match", sizeof(mac01) == sizeof(mac01.b));
}
