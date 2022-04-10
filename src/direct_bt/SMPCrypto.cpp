/**
 * Derived from zephyr/subsys/bluetooth/host/smp.c
 *
 * Currently disabled since no use w/o private jau::uint256_t dhkey. *
 *
 * Copyright (c) 2022 Gothel Software e.K.
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * _and_
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
#include <memory>
#include <cstdint>

#include <jau/int_types.hpp>
#include <jau/byte_util.hpp>
#include <jau/eui48.hpp>
#include <jau/debug.hpp>

#include "BTAddress.hpp"

#define USE_SMP_CRYPTO_ 0
inline constexpr const bool USE_SMP_CRYPTO = false;

#if USE_SMP_CRYPTO_
    // #include <tinycrypt/constants.h>
    // #include <tinycrypt/aes.h>
    // #include <tinycrypt/utils.h>
    #include <tinycrypt/cmac_mode.h>
#endif

namespace direct_bt {

/**
 * Cypher based Message Authentication Code (CMAC) with AES 128 bit
 * @param key 128-bit key
 * @param in message to be authenticated
 * @param len length of message in octets
 * @param out 128-bit message authentication code
 * @return
 */
static bool bt_smp_aes_cmac(const jau::uint128_t& key, const uint8_t *in, size_t len, jau::uint128_t& out)
{
#if USE_SMP_CRYPTO_
    struct tc_aes_key_sched_struct sched;
    struct tc_cmac_struct state;

    if (tc_cmac_setup(&state, key.data, &sched) == TC_CRYPTO_FAIL) {
        return false;
    }
    if (tc_cmac_update(&state, in, len) == TC_CRYPTO_FAIL) {
        return false;
    }
    if (tc_cmac_final(out.data, &state) == TC_CRYPTO_FAIL) {
        return false;
    }
    return true;
#else
    (void)key;
    (void)in;
    (void)len;
    (void)out;
    return false;
#endif
}

/**
 * SMP F5 algo according to BT Core Spec
 * @param w dhkey in littleEndian, which we have no access to!
 * @param n1 rrnd in littleEndian
 * @param n2 prnd in littleEndian
 * @param a1 init_address (master)
 * @param a2 responder_address (Slave)
 * @param mackey result in littleEndian
 * @param ltk result in littleEndian
 * @return
 */
bool smp_crypto_f5(const jau::uint256_t w, const jau::uint128_t n1, const jau::uint128_t n2,
                   const BDAddressAndType& a1, const BDAddressAndType& a2,
                   jau::uint128_t& mackey, jau::uint128_t& ltk)
{
    if constexpr ( !USE_SMP_CRYPTO ) {
        return false;
    }

    /** Salt random number in MSB, see BT Core Spec */
    static const jau::uint128_t salt( { 0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5,
                                        0xa5, 0x38, 0x60, 0x37, 0x0b, 0xdb,
                                        0x5a, 0x60, 0x83, 0xbe } );

    /** Value bag, all in MSB */
    uint8_t m[53] = { 0x00, /* counter */
              0x62, 0x74, 0x6c, 0x65, /* keyID 'btle' in MSB */
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*n1*/
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*n2*/
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* a1 */
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* a2 */
              0x01, 0x00 /* length 256 in MSB */ };
    uint8_t ws[32];
    jau::uint128_t t, temp_;

    DBG_PRINT("w %s", jau::bytesHexString(w.data, 0, 32, true /* lsbFirst */).c_str());
    DBG_PRINT("n1 %s", jau::bytesHexString(n1.data, 0, 16, true /* lsbFirst */).c_str());
    DBG_PRINT("n2 %s", jau::bytesHexString(n2.data, 0, 16, true /* lsbFirst */).c_str());

    jau::bswap(ws, w.data, 32); // little -> big

    if( bt_smp_aes_cmac(salt, ws, 32, t) ) {
        return false;
    }
    DBG_PRINT("t %s", jau::bytesHexString(t, 0, 16, false /* lsbFirst */).c_str());

    jau::bswap(m + 5, n1.data, 16); // little -> big
    jau::bswap(m + 21, n2.data, 16); // little -> big
    m[37] = a1.type;
    if constexpr ( jau::isLittleEndian() ) {
        jau::bswap(m + 38, a1.address.b, 6); // little -> big
    }
    m[44] = a2.type;
    if constexpr ( jau::isLittleEndian() ) {
        jau::bswap(m + 45, a2.address.b, 6); // little -> big
    }

    if( !bt_smp_aes_cmac(t, m, sizeof(m), temp_) ) { // temp_ received mackey in bigEndian
        return false;
    }
    mackey = jau::bswap(temp_); // big -> little
    DBG_PRINT("mackey %1s", jau::bytesHexString(mackey.data, 0, 16, true /* lsbFirst */).c_str());

    /* counter for ltk is 1 */
    m[0] = 0x01;

    if( !bt_smp_aes_cmac(t, m, sizeof(m), temp_) ) { // temp_ received ltk in bigEndian
        return false;
    }
    ltk = jau::bswap(temp_); // big -> little
    DBG_PRINT("ltk %s", jau::bytesHexString(ltk.data, 0, 16, true /* lsbFirst */).c_str());

    return true;
}

} // namespace direct_bt
