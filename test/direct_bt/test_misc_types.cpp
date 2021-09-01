#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <direct_bt/HCITypes.hpp>

using namespace direct_bt;

TEST_CASE( "HCIStatusCodeCategory std::error_code Test", "[HCIStatusCode][HCIStatusCodeCategory][error_code]" ) {
    std::error_code ec_0 = HCIStatusCode::SUCCESS;
    std::error_code ec_12(HCIStatusCode::COMMAND_DISALLOWED);
    std::error_code ec_42 = make_error_code(HCIStatusCode::DIFFERENT_TRANSACTION_COLLISION);

    std::cout << "ec_0: "  << ec_0 << std::endl;
    std::cerr << "ec_12: " << ec_12 << std::endl;
    std::cerr << "ec_42: " << ec_42 << std::endl;

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
