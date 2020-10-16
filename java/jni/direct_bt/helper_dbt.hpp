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

#ifndef HELPER_DBT_HPP_
#define HELPER_DBT_HPP_

#include "helper_base.hpp"

#include "direct_bt/BTAddress.hpp"

namespace direct_bt {

    class DirectBTJNISettings {
        private:
            bool unifyUUID128Bit = true;

        public:
            /**
             * Enables or disables uuid128_t consolidation
             * for native uuid16_t and uuid32_t values before string conversion.
             * <p>
             * Default is {@code true}, as this represent compatibility with original TinyB D-Bus behavior.
             * </p>
             */
            bool getUnifyUUID128Bit() { return unifyUUID128Bit; }
            void setUnifyUUID128Bit(bool v) { unifyUUID128Bit = v; }
    };
    extern DirectBTJNISettings directBTJNISettings;

    BDAddressType fromJavaAdressTypeToBDAddressType(JNIEnv *env, jstring jAddressType);
    jstring fromBDAddressTypeToJavaAddressType(JNIEnv *env, BDAddressType bdAddressType);

} // namespace direct_bt

#endif /* HELPER_DBT_HPP_ */
