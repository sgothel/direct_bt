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

#ifndef HCIUTIL_HPP_
#define HCIUTIL_HPP_

#pragma once
#include <cstring>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <vector>
#include <functional>
#include <map>

extern "C" {
	#include <byteswap.h>
}

namespace tinyb_hci {

	/**
	 * Returns current monotonic time in milliseconds.
	 */
	int64_t getCurrentMilliseconds();

	#define E_FILE_LINE __FILE__,__LINE__

	class RuntimeException : public std::exception {
	  protected:
		RuntimeException(std::string const type, std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: msg(std::string(type).append(" @ ").append(file).append(":").append(std::to_string(line)).append(": ").append(m)) { }

	  public:
		const std::string msg;

		RuntimeException(std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: RuntimeException("RuntimeException", m, file, line) {}

	    virtual ~RuntimeException() noexcept { }

	    virtual const char* what() const noexcept override;
	};

	class InternalError : public RuntimeException {
	  public:
		InternalError(std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: RuntimeException("InternalError", m, file, line) {}
	};

	class NullPointerException : public RuntimeException {
	  public:
		NullPointerException(std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: RuntimeException("NullPointerException", m, file, line) {}
	};

	class IllegalArgumentException : public RuntimeException {
	  public:
		IllegalArgumentException(std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: RuntimeException("IllegalArgumentException", m, file, line) {}
	};

	class UnsupportedOperationException : public RuntimeException {
	  public:
		UnsupportedOperationException(std::string const m, const char* file=__FILE__, int line=__LINE__) noexcept
		: RuntimeException("UnsupportedOperationException", m, file, line) {}
	};

	/**
	// *************************************************
	// *************************************************
	// *************************************************
	 */

	struct uint128_t {
		uint8_t data[16];

		bool operator==(uint128_t const &o) const {
			if( this == &o ) {
				return true;
			}
			return !std::memcmp(data, o.data, 16);
		}
		bool operator!=(uint128_t const &o) const
	    { return !(*this == o); }
	};

	inline uint128_t bswap(uint128_t const & source) {
		uint128_t dest;
		uint8_t const * const s = source.data;
		uint8_t * const d = dest.data;
		for(int i=0; i<16; i++) {
			d[i] = s[15-i];
		}
		return dest;
	}

	/**
	 * On the i386 the host byte order is Least Significant Byte first (LSB) or Little-Endian,
	 * whereas the network byte order, as used on the Internet, is Most Significant Byte first (MSB) or Big-Endian.
	 * See #include <arpa/inet.h>
	 *
	 * Bluetooth is LSB or Little-Endian!
	 */

#if __BYTE_ORDER == __BIG_ENDIAN
	inline uint16_t be_to_cpu(uint16_t const & n) {
		return n;
	}
	inline uint16_t cpu_to_be(uint16_t const & h) {
		return h;
	}
	inline uint16_t le_to_cpu(uint16_t const & l) {
		return bswap_16(l);
	}
	inline uint16_t cpu_to_le(uint16_t const & h) {
		return bswap_16(h);
	}

	inline uint32_t be_to_cpu(uint32_t const & n) {
		return n;
	}
	inline uint32_t cpu_to_be(uint32_t const & h) {
		return h;
	}
	inline uint32_t le_to_cpu(uint32_t const & l) {
		return bswap_32(l);
	}
	inline uint32_t cpu_to_le(uint32_t const & h) {
		return bswap_32(h);
	}

	inline uint128_t be_to_cpu(uint128_t const & n) {
		return n;
	}
	inline uint128_t cpu_to_be(uint128_t const & h) {
		return n;
	}
	inline uint128_t le_to_cpu(uint128_t const & l) {
		return bswap(l);
	}
	inline uint128_t cpu_to_le(uint128_t const & h) {
		return bswap(h);
	}
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	inline uint16_t be_to_cpu(uint16_t const & n) {
		return bswap_16(n);
	}
	inline uint16_t cpu_to_be(uint16_t const & h) {
		return bswap_16(h);
	}
	inline uint16_t le_to_cpu(uint16_t const & l) {
		return l;
	}
	inline uint16_t cpu_to_le(uint16_t const & h) {
		return h;
	}

	inline uint32_t be_to_cpu(uint32_t const & n) {
		return bswap_32(n);
	}
	inline uint32_t cpu_to_be(uint32_t const & h) {
		return bswap_32(h);
	}
	inline uint32_t le_to_cpu(uint32_t const & l) {
		return l;
	}
	inline uint32_t cpu_to_le(uint32_t const & h) {
		return h;
	}

	inline uint128_t be_to_cpu(uint128_t const & n) {
		return bswap(n);
	}
	inline uint128_t cpu_to_be(uint128_t const & h) {
		return bswap(h);
	}
	inline uint128_t le_to_cpu(uint128_t const & l) {
		return l;
	}
	inline uint128_t cpu_to_le(uint128_t const & h) {
		return h;
	}
#else
	#error "Unexpected __BYTE_ORDER"
#endif

	inline uint16_t get_uint16(uint8_t const * buffer, int const byte_offset)
	{
		uint16_t const * p = (uint16_t const *) ( buffer + byte_offset );
		return *p;
	}
	inline uint16_t get_uint16(uint8_t const * buffer, int const byte_offset, bool littleEndian)
	{
		uint16_t const * p = (uint16_t const *) ( buffer + byte_offset );
		return littleEndian ? le_to_cpu(*p) : be_to_cpu(*p);
	}

	inline uint32_t get_uint32(uint8_t const * buffer, int const byte_offset)
	{
		uint32_t const * p = (uint32_t const *) ( buffer + byte_offset );
		return *p;
	}
	inline uint32_t get_uint32(uint8_t const * buffer, int const byte_offset, bool littleEndian)
	{
		uint32_t const * p = (uint32_t const *) ( buffer + byte_offset );
		return littleEndian ? le_to_cpu(*p) : be_to_cpu(*p);
	}

	inline uint128_t get_uint128(uint8_t const * buffer, int const byte_offset)
	{
		uint128_t const * p = (uint128_t const *) ( buffer + byte_offset );
		return *p;
	}
	inline uint128_t get_uint128(uint8_t const * buffer, int const byte_offset, bool littleEndian)
	{
		uint128_t const * p = (uint128_t const *) ( buffer + byte_offset );
		return littleEndian ? le_to_cpu(*p) : be_to_cpu(*p);
	}

	/**
	 * Merge the given 'uuid16' into a 'base_uuid' copy at the given little endian 'uuid16_le_octet_index' position.
	 * <p>
	 * The given 'uuid16' value will be added with the 'base_uuid' copy at the given position.
	 * </p>
	 * <pre>
     * base_uuid: 00000000-0000-1000-8000-00805F9B34FB
     *    uuid16: DCBA
     * uuid16_le_octet_index: 12
     *    result: 0000DCBA-0000-1000-8000-00805F9B34FB
     *
     * LE: low-mem - FB349B5F8000-0080-0010-0000-ABCD0000 - high-mem
     *                                           ^ index 12
     * LE: uuid16 -> value.data[12+13]
     *
     * BE: low-mem - 0000DCBA-0000-1000-8000-00805F9B34FB - high-mem
     *                   ^ index 2
     * BE: uuid16 -> value.data[2+3]
     * </pre>
	 */
	uint128_t merge_uint128(uint128_t const & base_uuid, uint16_t const uuid16, int const uuid16_le_octet_index);

	/**
	 * Merge the given 'uuid32' into a 'base_uuid' copy at the given little endian 'uuid32_le_octet_index' position.
	 * <p>
	 * The given 'uuid32' value will be added with the 'base_uuid' copy at the given position.
	 * </p>
	 * <pre>
     * base_uuid: 00000000-0000-1000-8000-00805F9B34FB
     *    uuid32: 87654321
     * uuid32_le_octet_index: 12
     *    result: 87654321-0000-1000-8000-00805F9B34FB
     *
     * LE: low-mem - FB349B5F8000-0080-0010-0000-12345678 - high-mem
     *                                           ^ index 12
     * LE: uuid32 -> value.data[12..15]
     *
     * BE: low-mem - 87654321-0000-1000-8000-00805F9B34FB - high-mem
     *               ^ index 0
     * BE: uuid32 -> value.data[0..3]
     * </pre>
	 */
	uint128_t merge_uint128(uint128_t const & base_uuid, uint32_t const uuid32, int const uuid32_le_octet_index);

} // namespace tinyb_hci

#endif /* HCIUTIL_HPP_ */
