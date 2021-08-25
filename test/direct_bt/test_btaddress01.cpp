#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <jau/basic_types.hpp>
#include <jau/darray.hpp>
#include <direct_bt/BTAddress.hpp>

using namespace direct_bt;
using namespace jau;

static void test_sub(const std::string& mac_str, const jau::darray<std::string>& mac_sub_strs, const jau::darray<jau::snsize_t>& indices) {
    const EUI48 mac(mac_str);
    printf("Test EUI48 mac: '%s' -> '%s'\n", mac_str.c_str(), mac.toString().c_str());
    REQUIRE(mac_str == mac.toString());

    int i=0;
    jau::for_each_const(mac_sub_strs, [&i, &mac, &indices](const std::string &mac_sub_str) {
        const EUI48Sub mac_sub(mac_sub_str);
        printf("EUI48Sub mac02_sub: '%s' -> '%s'\n", mac_sub_str.c_str(), mac_sub.toString().c_str());
        // cut-off pre- and post-colon in test string, but leave single colon
        std::string sub_str(mac_sub_str);
        if( sub_str.size() == 0 ) {
            sub_str = ":";
        } else if( sub_str != ":" )  {
            if( sub_str.size() > 0 && sub_str[0] == ':' ) {
                sub_str = sub_str.substr(1, sub_str.size());
            }
            if( sub_str.size() > 0 && sub_str[sub_str.size()-1] == ':' ) {
                sub_str = sub_str.substr(0, sub_str.size()-1);
            }
        }
        REQUIRE(sub_str == mac_sub.toString());

        jau::snsize_t idx = mac.indexOf(mac_sub);
        REQUIRE( idx == indices.at(i));
        if( idx >= 0 ) {
            REQUIRE( mac.contains(mac_sub) );
        } else {
            REQUIRE( !mac.contains(mac_sub) );
        }

        ++i;
    } );
    (void) indices;
}

static void test_sub(const std::string& mac_sub_str_exp, const std::string& mac_sub_str, const bool expected_result) {
    std::string errmsg;
    EUI48Sub mac_sub;
    const bool res = EUI48Sub::scanEUI48Sub(mac_sub_str, mac_sub, errmsg);
    if( res ) {
        printf("EUI48Sub mac_sub: '%s' -> '%s'\n", mac_sub_str.c_str(), mac_sub.toString().c_str());
        if( expected_result ) {
            REQUIRE(mac_sub_str_exp == mac_sub.toString());
        }
    } else {
        printf("EUI48Sub mac_sub: '%s' -> Error '%s'\n", mac_sub_str.c_str(), errmsg.c_str());
    }
    REQUIRE( expected_result == res );
}

TEST_CASE( "EUI48 Test 01", "[datatype][eui48]" ) {
    EUI48 mac01;
    INFO_STR("EUI48 size: whole0 "+std::to_string(sizeof(EUI48)));
    INFO_STR("EUI48 size: whole1 "+std::to_string(sizeof(mac01)));
    INFO_STR("EUI48 size:  data1 "+std::to_string(sizeof(mac01.b)));
    REQUIRE_MSG("EUI48 struct and data size match", sizeof(EUI48) == sizeof(mac01));
    REQUIRE_MSG("EUI48 struct and data size match", sizeof(mac01) == sizeof(mac01.b));

    {
        // index                      [high=5 ...   low=0]
        const std::string mac02_str = "C0:10:22:A0:10:00";
        const jau::darray<std::string> mac02_sub_strs = { "C0", "C0:10", ":10:22", "10:22", ":10:22:", "10:22:", "10", "10:00", "00", ":", "", "00:10", mac02_str};
        const jau::darray<jau::snsize_t> mac02_sub_idxs = {  5,       4,        3,       3,         3,        3,    1,       0,    0,   0,  0,      -1,         0};
        test_sub(mac02_str, mac02_sub_strs, mac02_sub_idxs);
    }

    {
        // index                      [high=5 ...   low=0]
        const std::string mac03_str = "01:02:03:04:05:06";
        const jau::darray<std::string> mac03_sub_strs = { "01", "01:02", ":03:04", "03:04", ":04:05:", "04:05:", "04", "05:06", "06", ":", "", "06:05", mac03_str};
        const jau::darray<jau::snsize_t> mac03_sub_idxs = {  5,       4,        2,       2,         1,        1,    2,       0,    0,   0,  0,      -1,         0};
        test_sub(mac03_str, mac03_sub_strs, mac03_sub_idxs);
    }
    {
        const std::string mac_sub_str = "C0:10:22:A0:10:00";
        test_sub(mac_sub_str, mac_sub_str, true /* expected_result */);
    }
    {
        const std::string mac_sub_str = "0600106";
        const std::string dummy;
        test_sub(dummy, mac_sub_str, false /* expected_result */);
    }
}
