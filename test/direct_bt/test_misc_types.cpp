#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <jau/test/catch2_ext.hpp>

#include <jau/debug.hpp>

#include <direct_bt/HCITypes.hpp>

using namespace direct_bt;

TEST_CASE( "HCIStatusCodeCategory std::error_code Test", "[HCIStatusCode][HCIStatusCodeCategory][error_code]" ) {
    std::error_code ec_0 = HCIStatusCode::SUCCESS;
    std::error_code ec_12(HCIStatusCode::COMMAND_DISALLOWED);
    std::error_code ec_42 = make_error_code(HCIStatusCode::DIFFERENT_TRANSACTION_COLLISION);

    std::cerr << "ec_0: "  << ec_0 << ", " << ec_0.message() << std::endl;
    std::cerr << "ec_12: " << ec_12 << ", " << ec_12.message() << std::endl;
    std::cerr << "ec_42: " << ec_42 << ", " << ec_42.message() << std::endl;

    REQUIRE( HCIStatusCodeCategory::get() == ec_0.category() );
    REQUIRE( HCIStatusCodeCategory::get() == ec_12.category() );
    REQUIRE( HCIStatusCodeCategory::get() == ec_42.category() );
    REQUIRE( false == static_cast<bool>(ec_0) );  // explicit op bool
    REQUIRE( true  == static_cast<bool>(ec_12) ); // explicit op bool
    REQUIRE( true  == static_cast<bool>(ec_42) ); // explicit op bool
    REQUIRE( 0 == ec_0.value() );
    REQUIRE( 12 == ec_12.value() );
    REQUIRE( 42 == ec_42.value() );
}

static ScanType myChangeScanType(const ScanType current, const ScanType changeType, const bool changeEnable) noexcept {
    const ScanType res = changeScanType(current, changeType, changeEnable);
    jau_fprintf(stderr, "changeScanType: %4s | %4s (enable %5s) -> %5s\n", current, changeType, changeEnable, res);
    return res;
}

TEST_CASE( "HCIScanType Test", "[HCIStatusCode][HCIStatusCodeCategory][error_code]" ) {
    REQUIRE(ScanType::LE    == myChangeScanType(ScanType::LE,   ScanType::NONE, true));
    REQUIRE(ScanType::LE    == myChangeScanType(ScanType::NONE, ScanType::LE,   true));
    REQUIRE(ScanType::LE    == myChangeScanType(ScanType::LE,   ScanType::LE,   true));
    REQUIRE(ScanType::NONE  == myChangeScanType(ScanType::NONE, ScanType::NONE, true));
}