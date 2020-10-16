/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef JAU_RINGBUFFER_HPP_
#define JAU_RINGBUFFER_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>

#include <cstring>
#include <string>
#include <cstdint>

#include <jau/debug.hpp>
#include <jau/ordered_atomic.hpp>
#include <jau/basic_types.hpp>
#include <jau/ringbuffer_if.hpp>

namespace jau {

/**
 * Simple implementation of {@link ringbuffer_if},
 * exposing <i>lock-free</i>
 * {@link #get() get*(..)} and {@link #put(Object) put*(..)} methods.
 * <p>
 * Implementation utilizes the <i>Always Keep One Slot Open</i>,
 * hence implementation maintains an internal array of <code>capacity</code> <i>plus one</i>!
 * </p>
 * <p>
 * Implementation is thread safe if:
 * <ul>
 *   <li>{@link #get() get*(..)} operations from multiple threads.</li>
 *   <li>{@link #put(Object) put*(..)} operations from multiple threads.</li>
 *   <li>{@link #get() get*(..)} and {@link #put(Object) put*(..)} thread may be the same.</li>
 * </ul>
 * </p>
 * <p>
 * Following methods acquire the global multi-read _and_ -write mutex:
 * <ul>
 *  <li>{@link #resetFull(Object[])}</li>
 *  <li>{@link #clear()}</li>
 *  <li>{@link #growEmptyBuffer(Object[])}</li>
 * </ul>
 * </p>
 * <p>
 * Characteristics:
 * <ul>
 *   <li>Read position points to the last read element.</li>
 *   <li>Write position points to the last written element.</li>
 * </ul>
 * <table border="1">
 *   <tr><td>Empty</td><td>writePos == readPos</td><td>size == 0</td></tr>
 *   <tr><td>Full</td><td>writePos == readPos - 1</td><td>size == capacity</td></tr>
 * </table>
 * </p>
 * See also:
 * <pre>
 * - Sequentially Consistent (SC) ordering or SC-DRF (data race free) <https://en.cppreference.com/w/cpp/atomic/memory_order#Sequentially-consistent_ordering>
 * - std::memory_order <https://en.cppreference.com/w/cpp/atomic/memory_order>
 * </pre>
 */
template <typename T, std::nullptr_t nullelem> class ringbuffer : public ringbuffer_if<T> {
    private:
        std::mutex syncRead, syncMultiRead;   // Memory-Model (MM) guaranteed sequential consistency (SC) between acquire and release
        std::mutex syncWrite, syncMultiWrite; // ditto
        std::condition_variable cvRead;
        std::condition_variable cvWrite;

        /* final */ int capacityPlusOne;  // not final due to grow
        /* final */ T * array;     // Synchronized due to MM's data-race-free SC (SC-DRF) between [atomic] acquire/release
        sc_atomic_int readPos;     // Memory-Model (MM) guaranteed sequential consistency (SC) between acquire (read) and release (write)
        sc_atomic_int writePos;    // ditto
        relaxed_atomic_int size;   // Non-SC atomic size, only atomic value itself is synchronized.

        T * newArray(const int count) noexcept {
            return new T[count];
        }
        void freeArray(T * a) noexcept {
            delete[] a;
        }

        void cloneFrom(const bool allocArrayAndCapacity, const ringbuffer & source) noexcept {
            if( allocArrayAndCapacity ) {
                capacityPlusOne = source.capacityPlusOne;
                if( nullptr != array ) {
                    freeArray(array, true);
                }
                array = newArray(capacityPlusOne);
            } else if( capacityPlusOne != source.capacityPlusOne ) {
                throw InternalError("capacityPlusOne not equal: this "+toString()+", source "+source.toString(), E_FILE_LINE);
            }

            readPos = source.readPos;
            writePos = source.writePos;
            size = source.size;
            int localWritePos = readPos;
            for(int i=0; i<size; i++) {
                localWritePos = (localWritePos + 1) % capacityPlusOne;
                array[localWritePos] = source.array[localWritePos];
            }
            if( writePos != localWritePos ) {
                throw InternalError("copy segment error: this "+toString()+", localWritePos "+std::to_string(localWritePos)+"; source "+source.toString(), E_FILE_LINE);
            }
        }

        void clearImpl() noexcept {
            // clear all elements, zero size
            const int _size = size; // fast access
            if( 0 < _size ) {
                int localReadPos = readPos;
                for(int i=0; i<_size; i++) {
                    localReadPos = (localReadPos + 1) % capacityPlusOne;
                    array[localReadPos] = nullelem;
                }
                if( writePos != localReadPos ) {
                    // Avoid exception, abort!
                    ABORT("copy segment error: this %s, readPos %d/%d; writePos %d", toString().c_str(), readPos.load(), localReadPos, writePos.load());
                }
                readPos = localReadPos;
                size = 0;
            }
        }

        void resetImpl(const T * copyFrom, const int copyFromCount) noexcept {
            clearImpl();

            // fill with copyFrom elements
            if( nullptr != copyFrom && 0 < copyFromCount ) {
                if( copyFromCount > capacityPlusOne-1 ) {
                    // new blank resized array
                    capacityPlusOne = copyFromCount + 1;
                    array = newArray(capacityPlusOne);
                    readPos = 0;
                    writePos = 0;
                }
                int localWritePos = writePos;
                for(int i=0; i<copyFromCount; i++) {
                    localWritePos = (localWritePos + 1) % capacityPlusOne;
                    array[localWritePos] = copyFrom[i];
                    size++;
                }
                writePos = localWritePos;
            }
        }

        T getImpl(const bool blocking, const bool peek, const int timeoutMS) noexcept {
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead); // acquire syncMultiRead, _not_ sync'ing w/ putImpl

            const int oldReadPos = readPos; // SC-DRF acquire atomic readPos, sync'ing with putImpl
            int localReadPos = oldReadPos;
            if( localReadPos == writePos ) { // SC-DRF acquire atomic writePos, sync'ing with putImpl
                if( blocking ) {
                    std::unique_lock<std::mutex> lockRead(syncRead); // SC-DRF w/ putImpl via same lock
                    while( localReadPos == writePos ) {
                        if( 0 == timeoutMS ) {
                            cvRead.wait(lockRead);
                        } else {
                            std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                            std::cv_status s = cvRead.wait_until(lockRead, t0 + std::chrono::milliseconds(timeoutMS));
                            if( std::cv_status::timeout == s && localReadPos == writePos ) {
                                return nullelem;
                            }
                        }
                    }
                } else {
                    return nullelem;
                }
            }
            localReadPos = (localReadPos + 1) % capacityPlusOne;
            T r = array[localReadPos]; // SC-DRF
            if( !peek ) {
                array[localReadPos] = nullelem;
                {
                    std::unique_lock<std::mutex> lockWrite(syncWrite); // SC-DRF w/ putImpl via same lock
                    size--;
                    readPos = localReadPos; // SC-DRF release atomic readPos
                    cvWrite.notify_all(); // notify waiting putter
                }
            } else {
                readPos = oldReadPos; // SC-DRF release atomic readPos (complete acquire-release even @ peek)
            }
            return r;
        }

        bool putImpl(const T &e, const bool sameRef, const bool blocking, const int timeoutMS) noexcept {
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite); // acquire syncMultiRead, _not_ sync'ing w/ getImpl

            int localWritePos = writePos; // SC-DRF acquire atomic writePos, sync'ing with getImpl
            localWritePos = (localWritePos + 1) % capacityPlusOne;
            if( localWritePos == readPos ) { // SC-DRF acquire atomic readPos, sync'ing with getImpl
                if( blocking ) {
                    std::unique_lock<std::mutex> lockWrite(syncWrite); // SC-DRF w/ getImpl via same lock
                    while( localWritePos == readPos ) {
                        if( 0 == timeoutMS ) {
                            cvWrite.wait(lockWrite);
                        } else {
                            std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                            std::cv_status s = cvWrite.wait_until(lockWrite, t0 + std::chrono::milliseconds(timeoutMS));
                            if( std::cv_status::timeout == s && localWritePos == readPos ) {
                                return false;
                            }
                        }
                    }
                } else {
                    return false;
                }
            }
            if( !sameRef ) {
                array[localWritePos] = e; // SC-DRF
            }
            {
                std::unique_lock<std::mutex> lockRead(syncRead); // SC-DRF w/ getImpl via same lock
                size++;
                writePos = localWritePos; // SC-DRF release atomic writePos
                cvRead.notify_all(); // notify waiting getter
            }
            return true;
        }

        int dropImpl (const int count) noexcept {
            // locks ringbuffer completely (read/write), hence no need for local copy nor wait/sync etc
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock); // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock); // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiRead, lockMultiWrite);

            const int dropCount = std::min(count, size.load());
            if( 0 == dropCount ) {
                return 0;
            }
            for(int i=0; i<dropCount; i++) {
                readPos = (readPos + 1) % capacityPlusOne;
                // T r = array[localReadPos];
                array[readPos] = nullelem;
                size--;
            }
            return dropCount;
        }

    public:
        std::string toString() const noexcept override {
            const std::string es = isEmpty() ? ", empty" : "";
            const std::string fs = isFull() ? ", full" : "";
            return "ringbuffer<?>[size "+std::to_string(size)+" / "+std::to_string(capacityPlusOne-1)+
                    ", writePos "+std::to_string(writePos)+", readPos "+std::to_string(readPos)+es+fs+"]";
        }

        void dump(FILE *stream, std::string prefix) const noexcept override {
            fprintf(stream, "%s %s {\n", prefix.c_str(), toString().c_str());
            for(int i=0; i<capacityPlusOne; i++) {
                // fprintf(stream, "\t[%d]: %p\n", i, array[i].get()); // FIXME
            }
            fprintf(stream, "}\n");
        }

        /**
         * Create a full ring buffer instance w/ the given array's net capacity and content.
         * <p>
         * Example for a 10 element Integer array:
         * <pre>
         *  Integer[] source = new Integer[10];
         *  // fill source with content ..
         *  ringbuffer<Integer> rb = new ringbuffer<Integer>(source);
         * </pre>
         * </p>
         * <p>
         * {@link #isFull()} returns true on the newly created full ring buffer.
         * </p>
         * <p>
         * Implementation will allocate an internal array with size of array <code>copyFrom</code> <i>plus one</i>,
         * and copy all elements from array <code>copyFrom</code> into the internal array.
         * </p>
         * @param copyFrom mandatory source array determining ring buffer's net {@link #capacity()} and initial content.
         * @throws IllegalArgumentException if <code>copyFrom</code> is <code>nullptr</code>
         */
        ringbuffer(const std::vector<T> & copyFrom) noexcept
        : capacityPlusOne(copyFrom.size() + 1), array(newArray(capacityPlusOne)),
          readPos(0), writePos(0), size(0)
        {
            resetImpl(copyFrom.data(), copyFrom.size());
        }

        ringbuffer(const T * copyFrom, const int copyFromSize) noexcept
        : capacityPlusOne(copyFromSize + 1), array(newArray(capacityPlusOne)),
          readPos(0), writePos(0), size(0)
        {
            resetImpl(copyFrom, copyFromSize);
        }

        /**
         * Create an empty ring buffer instance w/ the given net <code>capacity</code>.
         * <p>
         * Example for a 10 element Integer array:
         * <pre>
         *  ringbuffer<Integer> rb = new ringbuffer<Integer>(10, Integer[].class);
         * </pre>
         * </p>
         * <p>
         * {@link #isEmpty()} returns true on the newly created empty ring buffer.
         * </p>
         * <p>
         * Implementation will allocate an internal array of size <code>capacity</code> <i>plus one</i>.
         * </p>
         * @param arrayType the array type of the created empty internal array.
         * @param capacity the initial net capacity of the ring buffer
         */
        ringbuffer(const int capacity) noexcept
        : capacityPlusOne(capacity + 1), array(newArray(capacityPlusOne)),
          readPos(0), writePos(0), size(0)
        { }

        ~ringbuffer() noexcept {
            freeArray(array);
        }

        ringbuffer(const ringbuffer &_source) noexcept
        : capacityPlusOne(_source.capacityPlusOne), array(newArray(capacityPlusOne)),
          readPos(0), writePos(0), size(0)
        {
            std::unique_lock<std::mutex> lockMultiReadS(_source.syncMultiRead, std::defer_lock); // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWriteS(_source.syncMultiWrite, std::defer_lock); // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiReadS, lockMultiWriteS);                                          // *this instance does not exist yet
            cloneFrom(false, _source);
        }

        ringbuffer& operator=(const ringbuffer &_source) noexcept {
            std::unique_lock<std::mutex> lockMultiReadS(_source.syncMultiRead, std::defer_lock); // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWriteS(_source.syncMultiWrite, std::defer_lock); // otherwise RAII-style relinquish via destructor
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock);          // same for *this instance!
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);
            std::lock(lockMultiReadS, lockMultiWriteS, lockMultiRead, lockMultiWrite);

            if( this == &_source ) {
                return *this;
            }
            if( capacityPlusOne != _source.capacityPlusOne ) {
                cloneFrom(true, _source);
            } else {
                clearImpl(); // clear
                cloneFrom(false, _source);
            }
            return *this;
        }

        ringbuffer(ringbuffer &&o) noexcept = default;
        ringbuffer& operator=(ringbuffer &&o) noexcept = default;

        int capacity() const noexcept override { return capacityPlusOne-1; }

        void clear() noexcept override {
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock);          // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);        // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiRead, lockMultiWrite);
            clearImpl();
        }

        void reset(const T * copyFrom, const int copyFromCount) noexcept override {
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock);          // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);        // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiRead, lockMultiWrite);
            resetImpl(copyFrom, copyFromCount);
        }

        void reset(const std::vector<T> & copyFrom) noexcept override {
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock);          // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);        // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiRead, lockMultiWrite);
            resetImpl(copyFrom.data(), copyFrom.size());
        }

        int getSize() const noexcept override { return size; }

        int getFreeSlots() const noexcept override { return capacityPlusOne - 1 - size; }

        bool isEmpty() const noexcept override { return writePos == readPos; /* 0 == size */ }

        bool isFull() const noexcept override { return ( writePos + 1 ) % capacityPlusOne == readPos ; /* capacityPlusOne - 1 == size */; }

        T get() noexcept override { return getImpl(false, false, 0); }

        T getBlocking(const int timeoutMS=0) noexcept override {
            return getImpl(true, false, timeoutMS);
        }

        T peek() noexcept override {
            return getImpl(false, true, 0);
        }

        T peekBlocking(const int timeoutMS=0) noexcept override {
            return getImpl(true, true, timeoutMS);
        }

        int drop(const int count) noexcept override {
            return dropImpl(count);
        }

        bool put(const T & e) noexcept override {
            return putImpl(e, false, false, 0);
        }

        bool putBlocking(const T & e, const int timeoutMS=0) noexcept override {
            return !putImpl(e, false, true, timeoutMS);
        }

        bool putSame() noexcept override {
            return putImpl(nullelem, true, false, 0);
        }

        bool putSameBlocking(const int timeoutMS=0) noexcept override {
            return putImpl(nullelem, true, true, timeoutMS);
        }

        void waitForFreeSlots(const int count) noexcept override {
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);        // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockRead(syncRead, std::defer_lock);                    // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiWrite, lockRead);

            while( capacityPlusOne - 1 - size < count ) {
                cvRead.wait(lockRead);
            }
        }

        void recapacity(const int newCapacity) override {
            std::unique_lock<std::mutex> lockMultiRead(syncMultiRead, std::defer_lock);          // utilize std::lock(r, w), allowing mixed order waiting on read/write ops
            std::unique_lock<std::mutex> lockMultiWrite(syncMultiWrite, std::defer_lock);        // otherwise RAII-style relinquish via destructor
            std::lock(lockMultiRead, lockMultiWrite);
            const int _size = size; // fast access

            if( capacityPlusOne == newCapacity+1 ) {
                return;
            }
            if( _size > newCapacity ) {
                throw IllegalArgumentException("amount "+std::to_string(newCapacity)+" < size, "+toString(), E_FILE_LINE);
            }
            if( 0 > newCapacity ) {
                throw IllegalArgumentException("amount "+std::to_string(newCapacity)+" < 0, "+toString(), E_FILE_LINE);
            }

            // save current data
            int oldCapacityPlusOne = capacityPlusOne;
            T * oldArray = array;
            int oldReadPos = readPos;

            // new blank resized array
            capacityPlusOne = newCapacity + 1;
            array = newArray(capacityPlusOne);
            readPos = 0;
            writePos = 0;

            // copy saved data
            if( nullptr != oldArray && 0 < _size ) {
                int localWritePos = writePos;
                for(int i=0; i<_size; i++) {
                    localWritePos = (localWritePos + 1) % capacityPlusOne;
                    oldReadPos = (oldReadPos + 1) % oldCapacityPlusOne;
                    array[localWritePos] = oldArray[oldReadPos];
                }
                writePos = localWritePos;
            }
            freeArray(oldArray); // and release
        }
};

} /* namespace jau */

#endif /* JAU_RINGBUFFER_HPP_ */
