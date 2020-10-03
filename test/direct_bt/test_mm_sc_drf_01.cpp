#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <atomic>
#include <mutex>
#include <condition_variable>
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
    std::mutex mtx_value;
    std::condition_variable cvRead;

    void reset(int v) {
        std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
        value1 = v;
        for(int i=0; i<array_size; i++) {
            array[i] = v;
        }
    }

    void putThreadType01(int _len, int startValue) {
        const int len = std::min(number(array_size), _len);
        {
            std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
            value1 = startValue;

            for(int i=0; i<len; i++) {
                array[i] = startValue+i;
            }
            cvRead.notify_all(); // notify waiting getter
        }
    }
    void getThreadType01(const std::string msg, int _len, int startValue) {
        const int len = std::min(number(array_size), _len);

        std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
        while( startValue != value1 ) {
            cvRead.wait(lock);
        }
        CHECKM(msg+": %s: Wrong value at read value1 (start)", startValue, value1);

        for(int i=0; i<len; i++) {
            int v = array[i];
            CHECKM(msg+": %s: Wrong start value at read array #"+std::to_string(i), (startValue+i), v);
        }
    }

    void putThreadType11(int indexAndValue) {
        const int idx = std::min(number(array_size), indexAndValue);
        {
            std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
            value1 = idx;
            array[idx] = idx;
            cvRead.notify_all(); // notify waiting getter
        }
    }
    void getThreadType11(const std::string msg, int _idx) {
        const int idx = std::min(number(array_size), _idx);

        std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
        while( idx != value1 ) {
            cvRead.wait(lock);
        }
        CHECKM(msg+": %s: Wrong value at read value1 (idx), idx "+std::to_string(idx), idx, value1);
        CHECKM(msg+": %s: Wrong value at read array (idx), idx "+std::to_string(idx), idx, array[idx]);
    }
    void getThreadType12(const std::string msg) {
        std::unique_lock<std::mutex> lock(mtx_value); // SC-DRF acquire and release @ scope exit
        CHECKTM(msg+": %s: Wrong value at read value1: Not: sync "+std::to_string(value1)+" < "+std::to_string(number(array_size)), value1 < array_size);
        CHECKM(msg+": %s: Wrong value at read array (sync)", value1, array[value1]);
    }


  public:

    Cppunit_tests()
    : value1(0) {}

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
        for(int i=loops; i>0; i--) { test03_Read4Write1(); }
        // for(int i=loops; i>0; i--) { test11_Read10Write10(); }
        // for(int i=loops; i>0; i--) { test12_Read10Write10(); }
    }
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Cppunit_tests test1;
    return test1.run();
}

