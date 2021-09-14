#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <direct_bt/UUID.hpp>
// #include <direct_bt/BTAddress.hpp>
// #include <direct_bt/BTTypes1.hpp>
#include <direct_bt/ATTPDUTypes.hpp>
// #include <direct_bt/GATTHandler.hpp>
// #include <direct_bt/GATTIoctl.hpp>

using namespace direct_bt;

/**
 * EIR AD Test: Squeezing all-at-once .. fits in 31 bytes
 */
TEST_CASE( "AD EIR PDU Test 01", "[datatype][AD][EIR]" ) {
    const std::vector<uint8_t> msd_data = { 0x01, 0x02 };
    ManufactureSpecificData msd(0x0001, msd_data.data(), msd_data.size());

    const uuid16_t uuid_01 = uuid16_t(0x1234);
    const uuid16_t uuid_02 = uuid16_t(0x0a0b);
    {
        std::shared_ptr<const uuid_t> const p1 = std::make_shared<uuid16_t>( uuid_01 );
        std::shared_ptr<const uuid_t> const p2 = uuid_02.clone();
        std::cout << "uuid_01: " << uuid_01.toString() << ", [" << p1->toString() << "]" << std::endl;
        std::cout << "uuid_02: " << uuid_02.toString() << ", [" << p2->toString() << "]" << std::endl;
    }

    EInfoReport eir0;
    eir0.setFlags(GAPFlags::LE_Gen_Disc);
    eir0.setName("TestTempDev01"); // 13
    eir0.setManufactureSpecificData(msd); // 2 + 4
    eir0.addService(uuid_01);
    eir0.addService(uuid_02);

    std::cout << "eir0.0: "  << eir0.toString(true) << std::endl;

    std::vector<uint8_t> buffer;
    {
        buffer.resize(31); // max EIR size
        const jau::nsize_t eir_sz = eir0.write_data(EIRDataType::ALL, buffer.data(), buffer.capacity());
        buffer.resize(eir_sz);
        std::cout << "eir0.0: bytes-out " << eir_sz << ", " << buffer.size() << std::endl;
        std::cout << "eir0.0: " << jau::bytesHexString(buffer.data(), 0, buffer.size(), true /* lsb */) << std::endl;
    }
    std::cout << std::endl;

    EInfoReport eir1;
    const int eir_segments = eir1.read_data(buffer.data(), buffer.size());
    std::cout << "eir1.0: segments " << eir_segments << std::endl;
    std::cout << "eir1.0: "  << eir1.toString(true) << std::endl;

    REQUIRE(eir0 == eir1);
}

/**
 * EIR AD Test: Exceeding 31 bytes -> Using two advertising EIR chunks (init + scan_rsp)
 */
TEST_CASE( "AD EIR PDU Test 02", "[datatype][AD][EIR]" ) {
    const std::vector<uint8_t> msd_data = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    ManufactureSpecificData msd(0x0001, msd_data.data(), msd_data.size());

    const uuid16_t uuid_01(0x1234);
    const uuid16_t uuid_02(0x0a0b);
    const uuid32_t uuid_11(0xabcd1234);
    const uuid128_t uuid_21("00001234-5678-100a-8000-00805F9B34FB");
    {
        std::shared_ptr<const uuid_t> const p1 = uuid_21.clone();
        std::cout << "uuid_21: " << uuid_21.toString() << ", [" << p1->toString() << "]" << std::endl;
    }

    // Complete version
    EInfoReport eir0a;
    eir0a.setFlags(GAPFlags::LE_Gen_Disc);
    eir0a.setName("TestTempDev02"); // 13
    eir0a.setManufactureSpecificData(msd); // 2 + 4
    eir0a.addService(uuid_01);
    eir0a.addService(uuid_02);
    eir0a.addService(uuid_11);
    eir0a.addService(uuid_21);

    // Less services (initial adv)
    const EIRDataType mask_0b = EIRDataType::FLAGS | EIRDataType::NAME | EIRDataType::MANUF_DATA;
    EInfoReport eir0b;
    eir0b.setFlags(GAPFlags::LE_Gen_Disc);
    eir0b.setName("TestTempDev02"); // 13
    eir0b.setManufactureSpecificData(msd); // 2 + 4

    // Only services (scan resp)
    const EIRDataType mask_0c = EIRDataType::SERVICE_UUID;
    EInfoReport eir0c;
    eir0c.addService(uuid_01);
    eir0c.addService(uuid_02);
    eir0c.addService(uuid_11);
    eir0c.addService(uuid_21);

    std::cout << "eir0a: "  << eir0a.toString(true) << std::endl;
    std::cout << "eir0b: "  << eir0b.toString(true) << std::endl;
    std::cout << "eir0c: "  << eir0c.toString(true) << std::endl;

    // Test: Less services (initial adv)
    {
        std::vector<uint8_t> buffer;
        buffer.resize(31); // max EIR size
        const jau::nsize_t eir_sz = eir0a.write_data(mask_0b, buffer.data(), buffer.capacity());
        buffer.resize(eir_sz);
        std::cout << "eir0a.1: bytes-out " << eir_sz << ", " << buffer.size() << std::endl;
        std::cout << "eir0a.1: " << jau::bytesHexString(buffer.data(), 0, buffer.size(), true /* lsb */) << std::endl;
        std::cout << std::endl;

        EInfoReport eir1;
        const int eir_segments = eir1.read_data(buffer.data(), buffer.size());
        std::cout << "eir1.0: segments " << eir_segments << std::endl;
        std::cout << "eir1.0: "  << eir1.toString(true) << std::endl;

        REQUIRE(eir0b == eir1);
    }

    // Test: Only Services (scan resp)
    {
        std::vector<uint8_t> buffer;
        buffer.resize(31); // max EIR size
        const jau::nsize_t eir_sz = eir0a.write_data(mask_0c, buffer.data(), buffer.capacity());
        buffer.resize(eir_sz);
        std::cout << "eir0a.2: bytes-out " << eir_sz << ", " << buffer.size() << std::endl;
        std::cout << "eir0a.2: " << jau::bytesHexString(buffer.data(), 0, buffer.size(), true /* lsb */) << std::endl;
        std::cout << std::endl;

        EInfoReport eir1;
        const int eir_segments = eir1.read_data(buffer.data(), buffer.size());
        std::cout << "eir2.0: segments " << eir_segments << std::endl;
        std::cout << "eir2.0: "  << eir1.toString(true) << std::endl;

        REQUIRE(eir0c == eir1);
    }

    // Test: Both
    {
        std::vector<uint8_t> buffer1;
        buffer1.resize(31); // max EIR size

        const jau::nsize_t eir_sz1 = eir0a.write_data(mask_0b, buffer1.data(), buffer1.capacity());
        buffer1.resize(eir_sz1);
        std::cout << "eir0a.3: bytes-out " << eir_sz1 << ", " << buffer1.size() << std::endl;
        std::cout << "eir0a.3: " << jau::bytesHexString(buffer1.data(), 0, buffer1.size(), true /* lsb */) << std::endl;
        std::cout << std::endl;

        std::vector<uint8_t> buffer2;
        buffer2.resize(31); // max EIR size
        const jau::nsize_t eir_sz2 = eir0a.write_data(mask_0c, buffer2.data(), buffer2.capacity());
        buffer2.resize(eir_sz2);
        std::cout << "eir0a.4: bytes-out " << eir_sz2 << ", " << buffer2.size() << std::endl;
        std::cout << "eir0a.4: " << jau::bytesHexString(buffer2.data(), 0, buffer2.size(), true /* lsb */) << std::endl;
        std::cout << std::endl;

        EInfoReport eir1;
        const int eir_segments1 = eir1.read_data(buffer1.data(), buffer1.size());
        std::cout << "eir3.1: segments " << eir_segments1 << std::endl;
        std::cout << "eir3.1: "  << eir1.toString(true) << std::endl;

        const int eir_segments2 = eir1.read_data(buffer2.data(), buffer2.size());
        std::cout << "eir3.2: segments " << eir_segments2 << std::endl;
        std::cout << "eir3.2: "  << eir1.toString(true) << std::endl;

        REQUIRE(eir0a == eir1);
    }
}
