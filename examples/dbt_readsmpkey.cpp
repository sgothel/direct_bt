/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <direct_bt/SMPKeyBin.hpp>

using namespace direct_bt;


/** \file
 * Reads and dumps an SMPKeyBin file.
 *
 * ### Invocation Examples:
 * Using `scripts/run-dbt_readsmpkey.sh` from `dist` directory:
 * * Read given SMPKeyBin file
 *   ~~~
 *   ../scripts/run-dbt_readsmpkey.sh keys/bd_001A7DDA7103_001A7DDA71081.key
 *   ~~~
 */

#include <cstdio>

int main(int argc, char *argv[])
{
    int res = 0;
    bool verbose=false;

    for(int i=1; i<argc; i++) {
        if( !strcmp("-verbose", argv[i]) || !strcmp("-v", argv[i]) ) {
            verbose = true;
        } else {
            const std::string fname(argv[i]);
            if( verbose ) {
                fprintf(stderr, "Read: '%s'\n", fname.c_str());
            }
            SMPKeyBin key = SMPKeyBin::read(fname, verbose);
            fprintf(stderr, "%s\n", key.toString().c_str());
            if( !key.isValid() ) {
                --res;
            }
        }
    }
    return res;
}
