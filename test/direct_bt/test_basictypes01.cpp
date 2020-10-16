#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <cppunit.h>

#include <jau/basic_types.hpp>
#include <direct_bt/BTAddress.hpp>

using namespace direct_bt;
using namespace jau;

// Test examples.
class Cppunit_tests : public Cppunit {
  public:
    void single_test() override {
        {
            EUI48 mac01;
            PRINTM("EUI48 size: whole0 "+std::to_string(sizeof(EUI48)));
            PRINTM("EUI48 size: whole1 "+std::to_string(sizeof(mac01)));
            PRINTM("EUI48 size:  data1 "+std::to_string(sizeof(mac01.b)));
            CHECKM("EUI48 struct and data size not matching", sizeof(EUI48), sizeof(mac01));
            CHECKM("EUI48 struct and data size not matching", sizeof(mac01), sizeof(mac01.b));
        }

    }
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Cppunit_tests test1;
    return test1.run();
}

