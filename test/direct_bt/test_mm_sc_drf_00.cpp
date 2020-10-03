#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <atomic>
#include <memory>

#include <thread>
#include <pthread.h>

#include <cppunit.h>

#include <direct_bt/OrderedAtomic.hpp>

using namespace direct_bt;

// Test examples.
class Cppunit_tests : public Cppunit {
  private:
    enum Defaults : int {
        array_size = 10
    };
    constexpr int number(const Defaults rhs) noexcept {
        return static_cast<int>(rhs);
    }

    int value1 = 0;
    int array[array_size] = { 0 };
    sc_atomic_int sync_value;

    void reset(int v) {
        int _sync_value = sync_value; // SC-DRF acquire atomic
        (void) _sync_value;
        value1 = v;
        for(int i=0; i<array_size; i++) {
            array[i] = v;
        }
        sync_value = v; // SC-DRF release atomic
    }

    void putThreadType01(int _len, int startValue) {
        const int len = std::min(number(array_size), _len);
        {
            int _sync_value = sync_value; // SC-DRF acquire atomic
            _sync_value = startValue;
            value1 = startValue;
            sync_value = _sync_value; // SC-DRF release atomic
        }

        for(int i=0; i<len; i++) {
            int _sync_value = sync_value; // SC-DRF acquire atomic
            array[i] = _sync_value+i;
            sync_value = _sync_value; // SC-DRF release atomic
        }
    }
    void getThreadType01(const std::string msg, int _len, int startValue) {
        const int len = std::min(number(array_size), _len);

        int _sync_value;
        while( startValue != ( _sync_value = sync_value ) ) ; // SC-DRF acquire atomic with spin-lock waiting for startValue
        CHECKM(msg+": %s: Wrong value at read value1 (sync)", _sync_value, value1);
        CHECKM(msg+": %s: Wrong value at read value1 (start)", startValue, value1);

        for(int i=0; i<len; i++) {
            int v = array[i];
            CHECKM(msg+": %s: Wrong sync value at read array #"+std::to_string(i), (_sync_value+i), v);
            CHECKM(msg+": %s: Wrong start value at read array #"+std::to_string(i), (startValue+i), v);
        }
        sync_value = _sync_value; // SC-DRF release atomic
    }

    void putThreadType11(int indexAndValue) {
        const int idx = std::min(number(array_size), indexAndValue);
        {
            int _sync_value = sync_value; // SC-DRF acquire atomic
            _sync_value = idx;
            value1 = idx;
            array[idx] = idx;
            sync_value = _sync_value; // SC-DRF release atomic
        }
    }
    void getThreadType11(const std::string msg, int _idx) {
        const int idx = std::min(number(array_size), _idx);

        int _sync_value;
        while( idx != ( _sync_value = sync_value ) ) ; // SC-DRF acquire atomic with spin-lock waiting for startValue
        CHECKM(msg+": %s: Wrong value at read value1 (sync), idx "+std::to_string(idx), _sync_value, value1);
        CHECKM(msg+": %s: Wrong value at read value1 (idx), idx "+std::to_string(idx), idx, value1);
        CHECKM(msg+": %s: Wrong value at read array (idx), idx "+std::to_string(idx), idx, array[idx]);
        sync_value = _sync_value; // SC-DRF release atomic
    }
    void getThreadType12(const std::string msg) {
        int _sync_value = sync_value; // SC-DRF acquire atomic with spin-lock waiting for startValue
        CHECKM(msg+": %s: Wrong value at read value1 (sync)", _sync_value, value1);
        CHECKTM(msg+": %s: Wrong value at read value1: Not: sync "+std::to_string(value1)+" < "+std::to_string(number(array_size)), value1 < array_size);
        CHECKM(msg+": %s: Wrong value at read array (sync)", value1, array[value1]);
        sync_value = _sync_value; // SC-DRF release atomic
    }


  public:

    Cppunit_tests()
    : value1(0), sync_value(0) {}

    void test01_Read1Write1() {
        fprintf(stderr, "\n\ntest01_Read1Write1.a\n");
        reset(1010);

        std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test01.get01", array_size, 3); // @suppress("Invalid arguments")
        std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 3); // @suppress("Invalid arguments")
        putThread01.join();
        getThread01.join();
    }

    void test02_Read2Write1() {
        fprintf(stderr, "\n\ntest01_Read2Write1.a\n");
        reset(1021);
        {
            std::thread getThread00(&Cppunit_tests::getThreadType01, this, "test01.get00", array_size, 4); // @suppress("Invalid arguments")
            std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test01.get01", array_size, 4); // @suppress("Invalid arguments")
            std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 4); // @suppress("Invalid arguments")
            putThread01.join();
            getThread00.join();
            getThread01.join();
        }

        fprintf(stderr, "\n\ntest01_Read2Write1.b\n");
        reset(1022);
        {
            std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 5); // @suppress("Invalid arguments")
            std::thread getThread00(&Cppunit_tests::getThreadType01, this, "test01.get00", array_size, 5); // @suppress("Invalid arguments")
            std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test01.get01", array_size, 5); // @suppress("Invalid arguments")
            putThread01.join();
            getThread00.join();
            getThread01.join();
        }
    }

    void test03_Read4Write1() {
        fprintf(stderr, "\n\ntest02_Read4Write1\n");
        reset(1030);

        std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test02.get01", array_size, 6); // @suppress("Invalid arguments")
        std::thread getThread02(&Cppunit_tests::getThreadType01, this, "test02.get02", array_size, 6); // @suppress("Invalid arguments")
        std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 6); // @suppress("Invalid arguments")
        std::thread getThread03(&Cppunit_tests::getThreadType01, this, "test02.get03", array_size, 6); // @suppress("Invalid arguments")
        std::thread getThread04(&Cppunit_tests::getThreadType01, this, "test02.get04", array_size, 6); // @suppress("Invalid arguments")
        putThread01.join();
        getThread01.join();
        getThread02.join();
        getThread03.join();
        getThread04.join();
    }

    void test11_Read10Write10() {
        fprintf(stderr, "\n\ntest11_Read10Write10\n");
        reset(1110);

        std::thread reader[array_size];
        std::thread writer[array_size];
        for(int i=0; i<number(array_size); i++) {
            reader[i] = std::thread(&Cppunit_tests::getThreadType11, this, "test11.get11", i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
            writer[i] = std::thread(&Cppunit_tests::putThreadType11, this, i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            // reader[i] = std::thread(&Cppunit_tests::getThreadType11, this, "test11.get11", i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
            // reader[i] = std::thread(&Cppunit_tests::getThreadType12, this, "test11.get12"); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            writer[i].join();
        }
        for(int i=0; i<number(array_size); i++) {
            reader[i].join();
        }
    }

    void test12_Read10Write10() {
        fprintf(stderr, "\n\ntest12_Read10Write10\n");
        reset(1110);

        std::thread reader[array_size];
        std::thread writer[array_size];
        for(int i=0; i<number(array_size); i++) {
            reader[i] = std::thread(&Cppunit_tests::getThreadType11, this, "test12.get11", i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
            // reader[i] = std::thread(&Cppunit_tests::getThreadType12, this, "test12.get12"); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            writer[i] = std::thread(&Cppunit_tests::putThreadType11, this, i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            writer[i].join();
        }
        for(int i=0; i<number(array_size); i++) {
            reader[i].join();
        }
    }

    void test_list() override {
        const int loops = 1000;
        for(int i=loops; i>0; i--) { test01_Read1Write1(); }
        for(int i=loops; i>0; i--) { test02_Read2Write1(); }
        for(int i=loops; i>0; i--) { test03_Read4Write1(); } // fail: sync_value != array[idx] .. less often
        // for(int i=loops; i>0; i--) { test11_Read10Write10(); } // fail: value1 != sync_value
        // for(int i=loops; i>0; i--) { test12_Read10Write10(); } // fail: value1 != sync_value
    }
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Cppunit_tests test1;
    return test1.run();
}

