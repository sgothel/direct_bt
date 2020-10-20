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

#ifndef OCTET_TYPES_HPP_
#define OCTET_TYPES_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>

#include <mutex>
#include <atomic>

#include <jau/basic_types.hpp>

#include "UUID.hpp"
#include "BTAddress.hpp"

// #define TRACE_MEM 1
#ifdef TRACE_MEM
    #define TRACE_PRINT(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); }
#else
    #define TRACE_PRINT(...)
#endif

namespace direct_bt {

    /**
     * Transient read only octet data, i.e. non persistent passthrough, owned by caller.
     * <p>
     * Either ATT value (Vol 3, Part F 3.2.4) or PDU data.
     * </p>
     */
    class TROOctets
    {
        private:
            /** Used memory size <= capacity, maybe zero. */
            size_t _size;
            /** Non-null memory pointer. Actual capacity known by owner. */
            uint8_t * _data;

        protected:
            static inline void checkPtr(uint8_t *d, size_t s) {
                if( nullptr == d && 0 < s ) {
                    throw jau::IllegalArgumentException("TROOctets::setData: nullptr with size "+std::to_string(s)+" > 0", E_FILE_LINE);
                }
            }

            inline uint8_t * data() noexcept { return _data; }

            /**
             * @param d a non nullptr memory, otherwise throws exception
             * @param s used memory size, may be zero
             */
            inline void setData(uint8_t *d, size_t s) {
                TRACE_PRINT("POctets setData: %d bytes @ %p -> %d bytes @ %p",
                        _size, _data, s, d);
                checkPtr(d, s);
                _size = s;
                _data = d;
            }
            inline void setSize(size_t s) noexcept { _size = s; }

        public:
            /**
             * Transient passthrough read-only memory, w/o ownership ..
             * @param source a non nullptr memory, otherwise throws exception. Actual capacity known by owner.
             * @param len readable size of the memory, may be zero
             */
            TROOctets(const uint8_t *source, const size_t len)
            : _size( len ), _data( const_cast<uint8_t *>(source) ) {
                checkPtr(_data, _size);
            }

            TROOctets(const TROOctets &o) noexcept = default;
            TROOctets(TROOctets &&o) noexcept = default;
            TROOctets& operator=(const TROOctets &o) noexcept = default;
            TROOctets& operator=(TROOctets &&o) noexcept = default;

            virtual ~TROOctets() noexcept {}

            inline void check_range(const size_t i, const size_t count, const char *file, int line) const {
                if( i+count > _size ) {
                    throw jau::IndexOutOfBoundsException(i, count, _size, file, line);
                }
            }
            #define check_range(I,C) check_range((I), (C), E_FILE_LINE)

            inline bool is_range_valid(const size_t i, const size_t count) const noexcept {
                return i+count <= _size;
            }

            /** Returns the used memory size for read and write operations, may be zero. */
            inline size_t getSize() const noexcept { return _size; }

            uint8_t get_uint8(const size_t i) const {
                check_range(i, 1);
                return _data[i];
            }
            inline uint8_t get_uint8_nc(const size_t i) const noexcept {
                return _data[i];
            }

            int8_t get_int8(const size_t i) const {
                check_range(i, 1);
                return jau::get_int8(_data, i);
            }
            inline int8_t get_int8_nc(const size_t i) const noexcept {
                return jau::get_int8(_data, i);
            }

            uint16_t get_uint16(const size_t i) const {
                check_range(i, 2);
                return jau::get_uint16(_data, i, true /* littleEndian */);
            }
            inline uint16_t get_uint16_nc(const size_t i) const noexcept {
                return jau::get_uint16(_data, i, true /* littleEndian */);
            }

            uint32_t get_uint32(const size_t i) const {
                check_range(i, 4);
                return jau::get_uint32(_data, i, true /* littleEndian */);
            }
            inline uint32_t get_uint32_nc(const size_t i) const noexcept {
                return jau::get_uint32(_data, i, true /* littleEndian */);
            }

            EUI48 get_eui48(const size_t i) const {
                check_range(i, sizeof(EUI48));
                return EUI48(_data+i);
            }
            inline EUI48 get_eui48_nc(const size_t i) const noexcept {
                return EUI48(_data+i);
            }

            /** Assumes a null terminated string */
            std::string get_string(const size_t i) const {
                check_range(i, 1); // minimum size
                return std::string( (const char*)(_data+i) );
            }
            /** Assumes a null terminated string */
            inline std::string get_string_nc(const size_t i) const noexcept {
                return std::string( (const char*)(_data+i) );
            }

            /** Assumes a string with defined length, not necessarily null terminated */
            std::string get_string(const size_t i, const size_t length) const {
                check_range(i, length);
                return std::string( (const char*)(_data+i), length );
            }

            uuid16_t get_uuid16(const size_t i) const {
                return uuid16_t(get_uint16(i));
            }
            inline uuid16_t get_uuid16_nc(const size_t i) const noexcept {
                return uuid16_t(get_uint16_nc(i));
            }

            uuid128_t get_uuid128(const size_t i) const {
                check_range(i, uuid_t::number(uuid_t::TypeSize::UUID128_SZ));
                return uuid128_t(jau::get_uint128(_data, i, true /* littleEndian */));
            }
            inline uuid128_t get_uuid128_nc(const size_t i) const noexcept {
                return uuid128_t(jau::get_uint128(_data, i, true /* littleEndian */));
            }

            std::shared_ptr<const uuid_t> get_uuid(const size_t i, const uuid_t::TypeSize tsize) const {
                check_range(i, uuid_t::number(tsize));
                return uuid_t::create(tsize, _data, i, true /* littleEndian */);
            }

            inline uint8_t const * get_ptr() const noexcept { return _data; }
            uint8_t const * get_ptr(const size_t i) const {
                check_range(i, 1);
                return _data + i;
            }
            inline uint8_t const * get_ptr_nc(const size_t i) const noexcept {
                return _data + i;
            }

            bool operator==(const TROOctets& rhs) const noexcept {
                return _size == rhs._size && 0 == memcmp(_data, rhs._data, _size);
            }
            bool operator!=(const TROOctets& rhs) const noexcept {
                return !(*this == rhs);
            }

            std::string toString() const noexcept {
                return "size "+std::to_string(_size)+", ro: "+jau::bytesHexString(_data, 0, _size, true /* lsbFirst */, true /* leading0X */);
            }
    };

    /**
     * Transient octet data, i.e. non persistent passthrough, owned by caller.
     * <p>
     * Either ATT value (Vol 3, Part F 3.2.4) or PDU data.
     * </p>
     */
    class TOctets : public TROOctets
    {
        public:
            /** Transient passthrough r/w memory, w/o ownership ..*/
            TOctets(uint8_t *source, const size_t len)
            : TROOctets(source, len) {}

            TOctets(const TOctets &o) noexcept = default;
            TOctets(TOctets &&o) noexcept = default;
            TOctets& operator=(const TOctets &o) noexcept = default;
            TOctets& operator=(TOctets &&o) noexcept = default;

            virtual ~TOctets() noexcept override {}

            void put_int8(const size_t i, const int8_t v) {
                check_range(i, 1);
                data()[i] = static_cast<uint8_t>(v);
            }
            void put_int8_nc(const size_t i, const int8_t v) noexcept {
                data()[i] = static_cast<uint8_t>(v);
            }

            void put_uint8(const size_t i, const uint8_t v) {
                check_range(i, 1);
                data()[i] = v;
            }
            void put_uint8_nc(const size_t i, const uint8_t v) noexcept {
                data()[i] = v;
            }

            void put_uint16(const size_t i, const uint16_t v) {
                check_range(i, 2);
                jau::put_uint16(data(), i, v, true /* littleEndian */);
            }
            void put_uint16_nc(const size_t i, const uint16_t v) noexcept {
                jau::put_uint16(data(), i, v, true /* littleEndian */);
            }

            void put_uint32(const size_t i, const uint32_t v) {
                check_range(i, 4);
                jau::put_uint32(data(), i, v, true /* littleEndian */);
            }
            void put_uint32_nc(const size_t i, const uint32_t v) noexcept {
                jau::put_uint32(data(), i, v, true /* littleEndian */);
            }

            void put_eui48(const size_t i, const EUI48 & v) {
                check_range(i, sizeof(v.b));
                memcpy(data() + i, v.b, sizeof(v.b));
            }
            void put_eui48_nc(const size_t i, const EUI48 & v) noexcept {
                memcpy(data() + i, v.b, sizeof(v.b));
            }

            void put_octets(const size_t i, const TROOctets & v) {
                check_range(i, v.getSize());
                memcpy(data() + i, v.get_ptr(), v.getSize());
            }
            void put_octets_nc(const size_t i, const TROOctets & v) noexcept {
                memcpy(data() + i, v.get_ptr(), v.getSize());
            }

            void put_string(const size_t i, const std::string & v, const size_t max_len, const bool includeEOS) {
                const size_t size1 = v.size() + ( includeEOS ? 1 : 0 );
                const size_t size = std::min(size1, max_len);
                check_range(i, size);
                memcpy(data() + i, v.c_str(), size);
                if( size < size1 && includeEOS ) {
                    *(data() + i + size - 1) = 0; // ensure EOS
                }
            }
            void put_string_nc(const size_t i, const std::string & v, const size_t max_len, const bool includeEOS) noexcept {
                const size_t size1 = v.size() + ( includeEOS ? 1 : 0 );
                const size_t size = std::min(size1, max_len);
                memcpy(data() + i, v.c_str(), size);
                if( size < size1 && includeEOS ) {
                    *(data() + i + size - 1) = 0; // ensure EOS
                }
            }

            void put_uuid(const size_t i, const uuid_t & v) {
                check_range(i, v.getTypeSizeInt());
                direct_bt::put_uuid(data(), i, v, true /* littleEndian */);
            }
            void put_uuid_nc(const size_t i, const uuid_t & v) noexcept {
                direct_bt::put_uuid(data(), i, v, true /* littleEndian */);
            }

            inline uint8_t * get_wptr() noexcept { return data(); }
            uint8_t * get_wptr(const size_t i) {
                check_range(i, 1);
                return data() + i;
            }
            uint8_t * get_wptr_nc(const size_t i) noexcept {
                return data() + i;
            }

            std::string toString() const noexcept {
                return "size "+std::to_string(getSize())+", rw: "+jau::bytesHexString(get_ptr(), 0, getSize(), true /* lsbFirst */, true /* leading0X */);
            }
    };

    class TOctetSlice
    {
        private:
            const TOctets & parent;
            size_t const offset;
            size_t const size;

        public:
            TOctetSlice(const TOctets &buffer_, const size_t offset_, const size_t size_)
            : parent(buffer_), offset(offset_), size(size_)
            {
                if( offset_+size > buffer_.getSize() ) {
                    throw jau::IndexOutOfBoundsException(offset_, size, buffer_.getSize(), E_FILE_LINE);
                }
            }

            size_t getSize() const noexcept { return size; }
            size_t getOffset() const noexcept { return offset; }
            const TOctets& getParent() const noexcept { return parent; }

            uint8_t get_uint8(const size_t i) const {
                return parent.get_uint8(offset+i);
            }
            inline uint8_t get_uint8_nc(const size_t i) const noexcept {
                return parent.get_uint8_nc(offset+i);
            }

            uint16_t get_uint16(const size_t i) const {
                return parent.get_uint16(offset+i);
            }
            inline uint16_t get_uint16_nc(const size_t i) const noexcept {
                return parent.get_uint16_nc(offset+i);
            }

            uint8_t const * get_ptr(const size_t i) const {
                return parent.get_ptr(offset+i);
            }
            inline uint8_t const * get_ptr_nc(const size_t i) const noexcept {
                return parent.get_ptr_nc(offset+i);
            }

            std::string toString() const noexcept {
                return "offset "+std::to_string(offset)+", size "+std::to_string(size)+": "+jau::bytesHexString(parent.get_ptr(), offset, size, true /* lsbFirst */, true /* leading0X */);
            }
    };

    /**
     * Persistent octet data, i.e. owned memory allocation.
     * <p>
     * GATT value (Vol 3, Part F 3.2.4)
     * </p>
     */
    class POctets : public TOctets
    {
        private:
            size_t capacity;

            void freeData() {
                uint8_t * ptr = data();
                if( nullptr != ptr ) {
                    TRACE_PRINT("POctets release: %p", ptr);
                    free(ptr);
                } // else: zero sized POctets w/ nullptr are supported
            }

            static uint8_t * allocData(const size_t size) {
                if( size <= 0 ) {
                    return nullptr;
                }
                uint8_t * m = static_cast<uint8_t*>( std::malloc(size) );
                if( nullptr == m ) {
                    throw jau::OutOfMemoryError("allocData size "+std::to_string(size)+" -> nullptr", E_FILE_LINE);
                }
                return m;
            }

        public:
            /** Returns the memory capacity, never zero, greater or equal {@link #getSize()}. */
            inline size_t getCapacity() const noexcept { return capacity; }

            /** Intentional zero sized POctets instance. */
            POctets()
            : TOctets(nullptr, 0), capacity(0)
            {
                TRACE_PRINT("POctets ctor0: zero-sized");
            }

            /** Takes ownership (malloc and copy, free) ..*/
            POctets(const uint8_t *_source, const size_t size_)
            : TOctets( allocData(size_), size_),
              capacity( size_ )
            {
                std::memcpy(data(), _source, size_);
                TRACE_PRINT("POctets ctor1: %p", data());
            }

            /** New buffer (malloc, free) */
            POctets(const size_t _capacity, const size_t size_)
            : TOctets( allocData(_capacity), size_),
              capacity( _capacity )
            {
                if( capacity < getSize() ) {
                    throw jau::IllegalArgumentException("capacity "+std::to_string(capacity)+" < size "+std::to_string(getSize()), E_FILE_LINE);
                }
                TRACE_PRINT("POctets ctor2: %p", data());
            }

            /** New buffer (malloc, free) */
            POctets(const size_t size)
            : POctets(size, size)
            {
                TRACE_PRINT("POctets ctor3: %p", data());
            }

            POctets(const POctets &_source)
            : TOctets( allocData(_source.getSize()), _source.getSize()),
              capacity( _source.getSize() )
            {
                std::memcpy(data(), _source.get_ptr(), _source.getSize());
                TRACE_PRINT("POctets ctor-cpy0: %p", data());
            }

            POctets(POctets &&o) noexcept
            : TOctets( o.data(), o.getSize() ),
              capacity( o.getCapacity() )
            {
                // moved origin data references
                // purge origin
                o.setData(nullptr, 0);
                o.capacity = 0;
                TRACE_PRINT("POctets ctor-move0: %p", data());
            }

            POctets& operator=(const POctets &_source) {
                if( this == &_source ) {
                    return *this;
                }
                freeData();
                setData(allocData(_source.getSize()), _source.getSize());
                capacity = _source.getSize();
                std::memcpy(data(), _source.get_ptr(), _source.getSize());
                TRACE_PRINT("POctets assign0: %p", data());
                return *this;
            }

            POctets& operator=(POctets &&o) noexcept {
                // move origin data references
                setData(o.data(), o.getSize());
                capacity = o.capacity;
                // purge origin
                o.setData(nullptr, 0);
                o.capacity = 0;
                TRACE_PRINT("POctets assign-move0: %p", data());
                return *this;
            }

            virtual ~POctets() noexcept override {
                freeData();
                setData(nullptr, 0);
                capacity=0;
            }

            /** Makes a persistent POctets by copying the data from TROOctets. */
            POctets(const TROOctets & _source)
            : TOctets( allocData(_source.getSize()), _source.getSize()),
              capacity( _source.getSize() )
            {
                std::memcpy(data(), _source.get_ptr(), _source.getSize());
                TRACE_PRINT("POctets ctor-cpy1: %p", data());
            }

            POctets& operator=(const TROOctets &_source) {
                if( static_cast<TROOctets *>(this) == &_source ) {
                    return *this;
                }
                freeData();
                setData(allocData(_source.getSize()), _source.getSize());
                capacity = _source.getSize();
                std::memcpy(data(), _source.get_ptr(), _source.getSize());
                TRACE_PRINT("POctets assign1: %p", data());
                return *this;
            }

            /** Makes a persistent POctets by copying the data from TOctetSlice. */
            POctets(const TOctetSlice & _source)
            : TOctets( allocData(_source.getSize()), _source.getSize()),
              capacity( _source.getSize() )
            {
                std::memcpy(data(), _source.getParent().get_ptr() + _source.getOffset(), _source.getSize());
                TRACE_PRINT("POctets ctor-cpy2: %p", data());
            }

            POctets& operator=(const TOctetSlice &_source) {
                freeData();
                setData(allocData(_source.getSize()), _source.getSize());
                capacity = _source.getSize();
                std::memcpy(data(), _source.get_ptr(0), _source.getSize());
                TRACE_PRINT("POctets assign2: %p", data());
                return *this;
            }

            POctets & resize(const size_t newSize, const size_t newCapacity) {
                if( newCapacity < newSize ) {
                    throw jau::IllegalArgumentException("newCapacity "+std::to_string(newCapacity)+" < newSize "+std::to_string(newSize), E_FILE_LINE);
                }
                if( newCapacity != capacity ) {
                    if( newSize > getSize() ) {
                        recapacity(newCapacity);
                        setSize(newSize);
                    } else {
                        setSize(newSize);
                        recapacity(newCapacity);
                    }
                } else {
                    setSize(newSize);
                }
                return *this;
            }

            POctets & resize(const size_t newSize) {
                if( capacity < newSize ) {
                    throw jau::IllegalArgumentException("capacity "+std::to_string(capacity)+" < newSize "+std::to_string(newSize), E_FILE_LINE);
                }
                setSize(newSize);
                return *this;
            }

            POctets & recapacity(const size_t newCapacity) {
                if( newCapacity < getSize() ) {
                    throw jau::IllegalArgumentException("newCapacity "+std::to_string(newCapacity)+" < size "+std::to_string(getSize()), E_FILE_LINE);
                }
                if( newCapacity == capacity ) {
                    return *this;
                }
                uint8_t* data2 = allocData(newCapacity);
                if( getSize() > 0 ) {
                    memcpy(data2, get_ptr(), getSize());
                }
                TRACE_PRINT("POctets recapacity: %p -> %p", data(), data2);
                free(data());
                setData(data2, getSize());
                capacity = newCapacity;
                return *this;
            }

            POctets & operator+=(const TROOctets &b) {
                if( 0 < b.getSize() ) {
                    const size_t newSize = getSize() + b.getSize();
                    if( capacity < newSize ) {
                        recapacity( newSize );
                    }
                    memcpy(data()+getSize(), b.get_ptr(), b.getSize());
                    setSize(newSize);
                }
                return *this;
            }
            POctets & operator+=(const TOctetSlice &b) {
                if( 0 < b.getSize() ) {
                    const size_t newSize = getSize() + b.getSize();
                    if( capacity < newSize ) {
                        recapacity( newSize );
                    }
                    memcpy(data()+getSize(), b.getParent().get_ptr()+b.getOffset(), b.getSize());
                    setSize(newSize);
                }
                return *this;
            }

            std::string toString() const {
                return "size "+std::to_string(getSize())+", capacity "+std::to_string(getCapacity())+", l->h: "+jau::bytesHexString(get_ptr(), 0, getSize(), true /* lsbFirst */, true /* leading0X */);
            }
    };


}


#endif /* OCTET_TYPES_HPP_ */
