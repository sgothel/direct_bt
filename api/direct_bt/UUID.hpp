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

#ifndef UUID_HPP_
#define UUID_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include <jau/basic_types.hpp>

namespace direct_bt {

class uuid128_t; // forward

/**
 * Bluetooth UUID <https://www.bluetooth.com/specifications/assigned-numbers/service-discovery/>
 * <p>
 * Bluetooth is LSB or Little-Endian!
 * </p>
 * <p>
 * BASE_UUID '00000000-0000-1000-8000-00805F9B34FB'
 * </p>
 */
extern uuid128_t BT_BASE_UUID;

class uuid_t {
public:
    /** Underlying integer value present octet count */
    enum class TypeSize : jau::nsize_t {
        UUID16_SZ=2, UUID32_SZ=4, UUID128_SZ=16
    };
    static constexpr jau::nsize_t number(const TypeSize rhs) noexcept {
        return static_cast<jau::nsize_t>(rhs);
    }

private:
    TypeSize type;

protected:
    uuid_t(TypeSize const type_) : type(type_) {}

public:
    static TypeSize toTypeSize(const jau::nsize_t size);
    static std::unique_ptr<const uuid_t> create(TypeSize const t, uint8_t const * const buffer, jau::nsize_t const byte_offset, bool const littleEndian);

    virtual ~uuid_t() noexcept {}

    uuid_t(const uuid_t &o) noexcept = default;
    uuid_t(uuid_t &&o) noexcept = default;
    uuid_t& operator=(const uuid_t &o) noexcept = default;
    uuid_t& operator=(uuid_t &&o) noexcept = default;

    virtual bool operator==(uuid_t const &o) const noexcept {
        if( this == &o ) {
            return true;
        }
        return type == o.type;
    }
    bool operator!=(uuid_t const &o) const noexcept
    { return !(*this == o); }

    TypeSize getTypeSize() const noexcept { return type; }
    jau::nsize_t getTypeSizeInt() const noexcept { return uuid_t::number(type); }

    uuid128_t toUUID128(uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const uuid32_le_octet_index=12) const noexcept;
    /** returns the pointer to the uuid data of size getTypeSize() */
    virtual const uint8_t * data() const noexcept { return nullptr; }
    virtual std::string toString() const noexcept { return ""; }
    virtual std::string toUUID128String(uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const le_octet_index=12) const noexcept;
};

class uuid16_t : public uuid_t {
public:
    uint16_t value;

    uuid16_t(uint16_t const v) noexcept
    : uuid_t(TypeSize::UUID16_SZ), value(v) { }

    uuid16_t(uint8_t const * const buffer, jau::nsize_t const byte_offset, bool const littleEndian) noexcept
    : uuid_t(TypeSize::UUID16_SZ), value(jau::get_uint16(buffer, byte_offset, littleEndian)) { }

    uuid16_t(const uuid16_t &o) noexcept = default;
    uuid16_t(uuid16_t &&o) noexcept = default;
    uuid16_t& operator=(const uuid16_t &o) noexcept = default;
    uuid16_t& operator=(uuid16_t &&o) noexcept = default;

    bool operator==(uuid_t const &o) const noexcept override {
        if( this == &o ) {
            return true;
        }
        return getTypeSize() == o.getTypeSize() && value == static_cast<uuid16_t const *>(&o)->value;
    }

    const uint8_t * data() const noexcept override { return static_cast<uint8_t*>(static_cast<void*>(const_cast<uint16_t*>(&value))); }
    std::string toString() const noexcept override;
    std::string toUUID128String(uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const le_octet_index=12) const noexcept override;
};

class uuid32_t : public uuid_t {
public:
    uint32_t value;

    uuid32_t(uint32_t const v) noexcept
    : uuid_t(TypeSize::UUID32_SZ), value(v) {}

    uuid32_t(uint8_t const * const buffer, jau::nsize_t const byte_offset, bool const littleEndian) noexcept
    : uuid_t(TypeSize::UUID32_SZ), value(jau::get_uint32(buffer, byte_offset, littleEndian)) { }

    uuid32_t(const uuid32_t &o) noexcept = default;
    uuid32_t(uuid32_t &&o) noexcept = default;
    uuid32_t& operator=(const uuid32_t &o) noexcept = default;
    uuid32_t& operator=(uuid32_t &&o) noexcept = default;

    bool operator==(uuid_t const &o) const noexcept override {
        if( this == &o ) {
            return true;
        }
        return getTypeSize() == o.getTypeSize() && value == static_cast<uuid32_t const *>(&o)->value;
    }

    const uint8_t * data() const noexcept override { return static_cast<uint8_t*>(static_cast<void*>(const_cast<uint32_t*>(&value))); }
    std::string toString() const noexcept override;
    std::string toUUID128String(uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const le_octet_index=12) const noexcept override;
};

class uuid128_t : public uuid_t {
public:
    jau::uint128_t value;

    uuid128_t() noexcept : uuid_t(TypeSize::UUID128_SZ) { bzero(value.data, sizeof(value)); }

    uuid128_t(jau::uint128_t const v) noexcept
    : uuid_t(TypeSize::UUID128_SZ), value(v) {}

    uuid128_t(const std::string str);

    uuid128_t(uint8_t const * const buffer, jau::nsize_t const byte_offset, bool const littleEndian) noexcept
    : uuid_t(TypeSize::UUID128_SZ), value(jau::get_uint128(buffer, byte_offset, littleEndian)) { }

    uuid128_t(uuid16_t const & uuid16, uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const uuid16_le_octet_index=12) noexcept;

    uuid128_t(uuid32_t const & uuid32, uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const uuid32_le_octet_index=12) noexcept;

    uuid128_t(const uuid128_t &o) noexcept = default;
    uuid128_t(uuid128_t &&o) noexcept = default;
    uuid128_t& operator=(const uuid128_t &o) noexcept = default;
    uuid128_t& operator=(uuid128_t &&o) noexcept = default;

    bool operator==(uuid_t const &o) const noexcept override {
        if( this == &o ) {
            return true;
        }
        return getTypeSize() == o.getTypeSize() && value == static_cast<uuid128_t const *>(&o)->value;
    }

    const uint8_t * data() const noexcept override { return value.data; }
    std::string toString() const noexcept override;
    std::string toUUID128String(uuid128_t const & base_uuid=BT_BASE_UUID, jau::nsize_t const le_octet_index=12) const noexcept override {
        (void)base_uuid;
        (void)le_octet_index;
        return toString();
    }
};

inline void put_uuid(uint8_t * buffer, jau::nsize_t const byte_offset, const uuid_t &v) noexcept
{
    switch(v.getTypeSize()) {
        case uuid_t::TypeSize::UUID16_SZ:
            jau::put_uint16(buffer, byte_offset, static_cast<const uuid16_t &>(v).value);
            break;
        case uuid_t::TypeSize::UUID32_SZ:
            jau::put_uint32(buffer, byte_offset, static_cast<const uuid32_t &>(v).value);
            break;
        case uuid_t::TypeSize::UUID128_SZ:
            jau::put_uint128(buffer, byte_offset, static_cast<const uuid128_t &>(v).value);
            break;
    }
}
inline void put_uuid(uint8_t * buffer, jau::nsize_t const byte_offset, const uuid_t &v, bool littleEndian) noexcept
{
    switch(v.getTypeSize()) {
        case uuid_t::TypeSize::UUID16_SZ:
            jau::put_uint16(buffer, byte_offset, static_cast<const uuid16_t &>(v).value, littleEndian);
            break;
        case uuid_t::TypeSize::UUID32_SZ:
            jau::put_uint32(buffer, byte_offset, static_cast<const uuid32_t &>(v).value, littleEndian);
            break;
        case uuid_t::TypeSize::UUID128_SZ:
            jau::put_uint128(buffer, byte_offset, static_cast<const uuid128_t &>(v).value, littleEndian);
            break;
    }
}

inline uuid16_t get_uuid16(uint8_t const * buffer, jau::nsize_t const byte_offset) noexcept
{
    return uuid16_t(jau::get_uint16(buffer, byte_offset));
}
inline uuid16_t get_uuid16(uint8_t const * buffer, jau::nsize_t const byte_offset, bool littleEndian) noexcept
{
    return uuid16_t(jau::get_uint16(buffer, byte_offset, littleEndian));
}
inline uuid32_t get_uuid32(uint8_t const * buffer, jau::nsize_t const byte_offset) noexcept
{
    return uuid32_t(jau::get_uint32(buffer, byte_offset));
}
inline uuid32_t get_uuid32(uint8_t const * buffer, jau::nsize_t const byte_offset, bool littleEndian) noexcept
{
    return uuid32_t(jau::get_uint32(buffer, byte_offset, littleEndian));
}
inline uuid128_t get_uuid128(uint8_t const * buffer, jau::nsize_t const byte_offset) noexcept
{
    return uuid128_t(jau::get_uint128(buffer, byte_offset));
}
inline uuid128_t get_uuid128(uint8_t const * buffer, jau::nsize_t const byte_offset, bool littleEndian) noexcept
{
    return uuid128_t(jau::get_uint128(buffer, byte_offset, littleEndian));
}

} /* namespace direct_bt */

#endif /* UUID_HPP_ */
