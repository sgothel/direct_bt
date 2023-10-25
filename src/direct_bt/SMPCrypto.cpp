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

#include <jau/debug.hpp>

#include "SMPCrypto.hpp"

#define USE_SMP_CRYPTO_AES128_ 1
// #define USE_SMP_CRYPTO_CMAC_ 1
#define USE_SMP_CRYPTO_F5_ 0

inline constexpr const bool USE_SMP_CRYPTO_IRK = true;
inline constexpr const bool USE_SMP_CRYPTO_F5 = false;

#include <tinycrypt/constants.h>

#if USE_SMP_CRYPTO_AES128_
    #include <tinycrypt/aes.h>
#endif
#if USE_SMP_CRYPTO_CMAC_
    // #include <tinycrypt/utils.h>
    #include <tinycrypt/cmac_mode.h>
#endif

namespace direct_bt {

/**
 * @brief Swap one buffer content into another
 *
 * Copy the content of src buffer into dst buffer in reversed order,
 * i.e.: src[n] will be put in dst[end-n]
 * Where n is an index and 'end' the last index in both arrays.
 * The 2 memory pointers must be pointing to different areas, and have
 * a minimum size of given length.
 *
 * @param dst A valid pointer on a memory area where to copy the data in
 * @param src A valid pointer on a memory area where to copy the data from
 * @param length Size of both dst and src memory areas
 */
static inline void sys_memcpy_swap(void *dst, const void *src, size_t length)
{
    uint8_t *pdst = (uint8_t *)dst;
    const uint8_t *psrc = (const uint8_t *)src;

    /**
    __ASSERT(((psrc < pdst && (psrc + length) <= pdst) ||
          (psrc > pdst && (pdst + length) <= psrc)),
         "Source and destination buffers must not overlap"); */

    psrc += length - 1;

    for (; length > 0; length--) {
        *pdst++ = *psrc--;
    }
}

/**
 * @brief Swap buffer content
 *
 * In-place memory swap, where final content will be reversed.
 * I.e.: buf[n] will be put in buf[end-n]
 * Where n is an index and 'end' the last index of buf.
 *
 * @param buf A valid pointer on a memory area to swap
 * @param length Size of buf memory area
 */
static inline void sys_mem_swap(void *buf, size_t length)
{
    size_t i;

    for (i = 0; i < (length/2); i++) {
        uint8_t tmp = ((uint8_t *)buf)[i];

        ((uint8_t *)buf)[i] = ((uint8_t *)buf)[length - 1 - i];
        ((uint8_t *)buf)[length - 1 - i] = tmp;
    }
}

static int bt_encrypt_le(const uint8_t key[16], const uint8_t plaintext[16],
                         uint8_t enc_data[16])
{
    struct tc_aes_key_sched_struct s;
    uint8_t tmp[16];

    // BT_DBG("key %s", bt_hex(key, 16));
    // BT_DBG("plaintext %s", bt_hex(plaintext, 16));

    sys_memcpy_swap(tmp, key, 16);

    if (tc_aes128_set_encrypt_key(&s, tmp) == TC_CRYPTO_FAIL) {
        return -EINVAL;
    }

    sys_memcpy_swap(tmp, plaintext, 16);

    if (tc_aes_encrypt(enc_data, tmp, &s) == TC_CRYPTO_FAIL) {
        return -EINVAL;
    }

    sys_mem_swap(enc_data, 16);

    // BT_DBG("enc_data %s", bt_hex(enc_data, 16));

    return 0;
}

static int smp_crypto_ah(const uint8_t irk[16], const uint8_t r[3], uint8_t out[3])
{
    uint8_t res[16];
    int err;

    // BT_DBG("irk %s", bt_hex(irk, 16));
    // BT_DBG("r %s", bt_hex(r, 3));

    /* r' = padding || r */
    std::memcpy(res, r, 3);
    (void)std::memset(res + 3, 0, 13);

    err = bt_encrypt_le(irk, res, res);
    if (err) {
        return err;
    }

    /* The output of the random address function ah is:
     *      ah(h, r) = e(k, r') mod 2^24
     * The output of the security function e is then truncated to 24 bits
     * by taking the least significant 24 bits of the output of e as the
     * result of ah.
     */
    std::memcpy(out, res, 3);

    return 0;
}

bool smp_crypto_rpa_irk_matches(const jau::uint128_t irk, const EUI48& rpa) noexcept {
    if constexpr ( !USE_SMP_CRYPTO_IRK ) {
        return false;
    }
    // DBG_PRINT("IRK %s bdaddr %s", bt_hex(irk, 16), bt_addr_str(addr));
    uint8_t hash[3];
    int err = smp_crypto_ah(irk.data, &rpa.b[3], hash);
    if (err) {
        return false;
    }
    return !memcmp(rpa.b, hash, 3);
}

#if USE_SMP_CRYPTO_F5_

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
#if defined(USE_SMP_CRYPTO_CMAC_) && defined(USE_SMP_CRYPTO_AES128_)
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
                   jau::uint128_t& mackey, jau::uint128_t& ltk) noexcept
{
    if constexpr ( !USE_SMP_CRYPTO_F5 ) {
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

#endif /* USE_SMP_CRYPTO_F5_ */

} // namespace direct_bt
