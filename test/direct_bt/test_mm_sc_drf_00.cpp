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

static int loops = 10;

/**
 * test_mm_sc_drf_00: Testing SC-DRF non-atomic global read and write within an atomic acquire/release critical block.
 * <p>
 * Modified non-atomic memory within the atomic acquire (load) and release (store) block,
 * must be visible for all threads according to memory model (MM) Sequentially Consistent (SC) being data-race-free (DRF).
 * <br>
 * See Herb Sutter's 2013-12-23 slides p19, first box "It must be impossible for the assertion to fail – wouldn’t be SC.".
 * </p>
 * <p>
 * This test's threads utilize a spin-lock, waiting for their turn.
 * Such busy cycles were chosen to simplify the test and are not recommended
 * as they expose poor performance on a high thread-count and hence long 'working thread pipe'.
 * </p>
 * See 'test_mm_sc_drf_01' implementing same test using mutex-lock and condition wait.
 */
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

    void reset(int v1, int array_value) {
        int _sync_value = sync_value; // SC-DRF acquire atomic
        (void) _sync_value;
        value1 = v1;
        for(int i=0; i<array_size; i++) {
            array[i] = array_value;
        }
        sync_value = v1; // SC-DRF release atomic
    }

    void putThreadType01(int _len, int startValue) {
        const int len = std::min(number(array_size), _len);
        {
            int _sync_value = sync_value; // SC-DRF acquire atomic
            _sync_value = startValue;
            for(int i=0; i<len; i++) {
                array[i] = _sync_value+i;
            }
            value1 = startValue;
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
        const int idx = std::min(number(array_size)-1, indexAndValue);
        {
            // idx is encoded on sync_value (v) as follows
            //   v > 0: get @ idx = v -1
            //   v < 0: put @ idx = abs(v) -1
            int _sync_value;
            // SC-DRF acquire atomic with spin-lock waiting for encoded idx
            do {
                _sync_value = sync_value;
            } while( idx != (_sync_value * -1) - 1 );
            // fprintf(stderr, "putThreadType11.done @ %d (has %d, exp %d)\n", idx, _sync_value, (idx+1)*-1);
            _sync_value = idx;
            value1 = idx;
            array[idx] = idx; // last written checked first, SC-DRF should handle...
            sync_value = _sync_value; // SC-DRF release atomic
        }
    }
    void getThreadType11(const std::string msg, int _idx) {
        const int idx = std::min(number(array_size)-1, _idx);

        // idx is encoded on sync_value (v) as follows
        //   v > 0: get @ idx = v -1
        //   v < 0: put @ idx = abs(v) -1
        int _sync_value;
        // SC-DRF acquire atomic with spin-lock waiting for idx
        do {
            _sync_value = sync_value;
        } while( idx != _sync_value );
        CHECKM(msg+": %s: Wrong value at read array (a), idx "+std::to_string(idx), idx, array[idx]); // check last-written first
        CHECKM(msg+": %s: Wrong value at read value1, idx "+std::to_string(idx), idx, value1);
        CHECKM(msg+": %s: Wrong value at read sync, idx "+std::to_string(idx), idx, _sync_value);
        // next write encoded idx
        _sync_value = (idx+1)%array_size;
        _sync_value = ( _sync_value + 1 ) * -1;
        // fprintf(stderr, "getThreadType11.done for %d, next %d (v %d)\n", idx, (idx+1)%array_size, _sync_value);
        value1 = _sync_value;
        sync_value = _sync_value; // SC-DRF release atomic
    }


  public:

    Cppunit_tests()
    : value1(0), sync_value(0) {}

    void test01_Read1Write1() {
        fprintf(stderr, "\n\ntest01_Read1Write1.a\n");
        reset(0, 1010);

        std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test01.get01", array_size, 3); // @suppress("Invalid arguments")
        std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 3); // @suppress("Invalid arguments")
        putThread01.join();
        getThread01.join();
    }

    void test02_Read2Write1() {
        fprintf(stderr, "\n\ntest01_Read2Write1.a\n");
        reset(0, 1021);
        {
            std::thread getThread00(&Cppunit_tests::getThreadType01, this, "test01.get00", array_size, 4); // @suppress("Invalid arguments")
            std::thread getThread01(&Cppunit_tests::getThreadType01, this, "test01.get01", array_size, 4); // @suppress("Invalid arguments")
            std::thread putThread01(&Cppunit_tests::putThreadType01, this, array_size, 4); // @suppress("Invalid arguments")
            putThread01.join();
            getThread00.join();
            getThread01.join();
        }

        fprintf(stderr, "\n\ntest01_Read2Write1.b\n");
        reset(0, 1022);
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
        reset(0, 1030);

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
        reset(-1, 1110); // start put idx 0

        std::thread reader[array_size];
        std::thread writer[array_size];
        for(int i=0; i<number(array_size); i++) {
            reader[i] = std::thread(&Cppunit_tests::getThreadType11, this, "test11.get11", i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
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

    void test12_Read10Write10() {
        fprintf(stderr, "\n\ntest12_Read10Write10\n");
        reset(-1, 1120); // start put idx 0

        std::thread reader[array_size];
        std::thread writer[array_size];
        for(int i=0; i<number(array_size); i++) {
            writer[i] = std::thread(&Cppunit_tests::putThreadType11, this, i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            reader[i] = std::thread(&Cppunit_tests::getThreadType11, this, "test12.get11", i); // @suppress("Invalid arguments") // @suppress("Symbol is not resolved")
        }
        for(int i=0; i<number(array_size); i++) {
            writer[i].join();
        }
        for(int i=0; i<number(array_size); i++) {
            reader[i].join();
        }
    }

    void test_list() override {
        for(int i=loops; i>0; i--) { test01_Read1Write1(); }
        for(int i=loops; i>0; i--) { test02_Read2Write1(); }
        for(int i=loops; i>0; i--) { test03_Read4Write1(); }
        for(int i=loops; i>0; i--) { test11_Read10Write10(); }
        for(int i=loops; i>0; i--) { test12_Read10Write10(); }
    }
};

int main(int argc, char *argv[]) {
    for(int i=1; i<argc; i++) {
        std::string arg(argv[i]);
        if( "-loops" == arg && argc > i+1 ) {
            loops = atoi(argv[i+1]);
        }
    }
    fprintf(stderr, "Loops %d\n", loops);

    Cppunit_tests test1;
    return test1.run();
}

