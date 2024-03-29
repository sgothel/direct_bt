#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <jau/test/catch2_ext.hpp>

#include <jau/uuid.hpp>
// #include <direct_bt/BTAddress.hpp>
// #include <direct_bt/BTTypes1.hpp>
#include <direct_bt/ATTPDUTypes.hpp>
// #include <direct_bt/GATTHandler.hpp>
// #include <direct_bt/GATTIoctl.hpp>

using namespace direct_bt;

TEST_CASE( "ATT PDU Test 01", "[datatype][attpdu]" ) {
    const jau::uuid16_t uuid16 = jau::uuid16_t(0x1234);
    const AttReadByNTypeReq req(true /* group */, 1, 0xffff, uuid16);

    std::shared_ptr<const jau::uuid_t> uuid16_2 = req.getNType();
    REQUIRE(uuid16.getTypeSizeInt() == 2);
    REQUIRE(uuid16_2->getTypeSizeInt() == 2);
    REQUIRE( 0 == memcmp(uuid16.data(), uuid16_2->data(), 2) );
    REQUIRE( uuid16.toString() == uuid16_2->toString() );

    REQUIRE(req.getStartHandle() == 1);
    REQUIRE(req.getEndHandle() == 0xffff);
}

