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

#ifndef DBT_DEV_ACCOUNTING_HPP_
#define DBT_DEV_ACCOUNTING_HPP_

#include <string>
#include <cstdio>

#include <direct_bt/DirectBT.hpp>

namespace direct_bt {

    /**
     * Application toolkit providing BT device registration of processed and awaited devices.
     * The latter on a pattern matching basis, i.e. EUI48Sub or name-sub.
     */
    namespace BTDeviceRegistry {
        void addToWaitForDevices(const std::string& addrOrNameSub);
        bool isWaitingForDevice(const BDAddressAndType &mac, const std::string &name);
        bool isWaitingForAnyDevice();
        size_t getWaitForDevicesCount();
        void printWaitForDevices(FILE *out, const std::string &msg);

        void addToDevicesProcessed(const BDAddressAndType &a, const std::string& n);
        bool isDeviceProcessed(const BDAddressAndType & a);
        size_t getDeviceProcessedCount();
        bool allDevicesProcessed();
        void printDevicesProcessed(FILE *out, const std::string &msg);

        void addToDevicesProcessing(const BDAddressAndType &a, const std::string& n);
        bool removeFromDevicesProcessing(const BDAddressAndType &a);
        bool isDeviceProcessing(const BDAddressAndType & a);
        size_t getDeviceProcessingCount();
    }

} // namespace direct_bt

#endif /* DBT_DEV_ACCOUNTING_HPP_ */
