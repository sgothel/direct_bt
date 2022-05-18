/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

#ifndef DBT_UTILS_HPP_
#define DBT_UTILS_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include "dbt_constants.hpp"

#include <jau/file_util.hpp>

class DBTUtils {
    public:

        static bool mkdirKeyFolder() {
            if( jau::fs::mkdir( DBTConstants::CLIENT_KEY_PATH ) ) {
                if( jau::fs::mkdir( DBTConstants::SERVER_KEY_PATH ) ) {
                    return true;
                }
            }
            return false;
        }


        static bool rmKeyFolder() {
            if( jau::fs::remove( DBTConstants::CLIENT_KEY_PATH, true ) ) {
                if( jau::fs::remove( DBTConstants::SERVER_KEY_PATH, true ) ) {
                    return true;
                }
            }
            return false;
        }
};

#endif /* DBT_UTILS_HPP_ */
