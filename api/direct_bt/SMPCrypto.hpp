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

#ifndef SMP_CRYPTO_HPP_
#define SMP_CRYPTO_HPP_

#include <jau/int_types.hpp>
#include <jau/byte_util.hpp>
#include <jau/eui48.hpp>

#include "BTAddress.hpp"

namespace direct_bt {
    /** Returns true if the given IRK matches the given random private address (RPA). */
    bool smp_crypto_rpa_irk_matches(const jau::uint128dp_t irk, const EUI48& rpa) noexcept;

    bool smp_crypto_f5(const jau::uint256dp_t w, const jau::uint128dp_t n1, const jau::uint128dp_t n2,
                       const BDAddressAndType& a1, const BDAddressAndType& a2,
                       jau::uint128dp_t& mackey, jau::uint128dp_t& ltk) noexcept;

} // namespace direct_bt

#endif /* SMP_CRYPTO_HPP_ */
