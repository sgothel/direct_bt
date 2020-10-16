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

#ifndef JAU_ENV_HPP_
#define JAU_ENV_HPP_

#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <string>
#include <cstdio>

extern "C" {
    #include <errno.h>
}

#include <jau/basic_types.hpp>

namespace jau {

    /**
     * Base jau environment class,
     * merely to tag all environment settings by inheritance and hence documentation.
     * <p>
     * See main environment {@link environment} and
     * {@link environment::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class root_environment {
    };

    /**
     * Main jau environment class,
     * supporting environment variable access and fetching elapsed time using its stored startup-time.
     */
    class environment : public root_environment {
        private:
            environment() noexcept;

            static bool debug;

            static void envSet(std::string prefixDomain, std::string basepair) noexcept;
            static void envExplodeProperties(std::string prefixDomain, std::string list) noexcept;

        public:
            /**
             * Module startup time t0 in monotonic time in milliseconds.
             */
            static const uint64_t startupTimeMilliseconds;

            /**
             * Returns current elapsed monotonic time in milliseconds since module startup, see {@link #startupTimeMilliseconds}.
             */
            static uint64_t getElapsedMillisecond() noexcept {
                return getCurrentMilliseconds() - startupTimeMilliseconds;
            }

            /**
             * Returns the value of the environment's variable 'name'.
             * <p>
             * Note that only '[org.]tinyb.*' and 'direct_bt.*' Java JVM properties are passed via 'org.tinyb.BluetoothFactory'
             * </p>
             * <p>
             * Implementation attempts to also find a Unix conform environment name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.</br>
             *
             * Dots are not allowed as valid Unix envrionment variable identifier.
             * If the property 'name' isn't found and if the 'name' contains a dot ('.'),
             * all dots ('.') will be replaced y underscore ('_') and looked up again.</br>
             * This allows Unix shell user to set the property 'direct_bt_debug' instead of 'direct_bt.debug'.
             * </p>
             */
            static std::string getProperty(const std::string & name) noexcept;

            /**
             * Returns the value of the environment's variable 'name',
             * or the 'default_value' if the environment variable's value is null.
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * and hence attempts to also find a Unix conform name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.
             * </p>
             */
            static std::string getProperty(const std::string & name, const std::string & default_value) noexcept;

            /**
             * Returns the boolean value of the environment's variable 'name',
             * or the 'default_value' if the environment variable's value is null.
             * <p>
             * If the environment variable is set (value != null),
             * true is determined if the value equals 'true'.
             * </p>
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * and hence attempts to also find a Unix conform name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.
             * </p>
             */
            static bool getBooleanProperty(const std::string & name, const bool default_value) noexcept;

            /**
             * Returns the int32_t value of the environment's variable 'name',
             * or the 'default_value' if the environment variable's value is null
             * or not within int32_t value range or within the given value range.
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * and hence attempts to also find a Unix conform name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.
             * </p>
             */
            static int32_t getInt32Property(const std::string & name, const int32_t default_value,
                                            const int32_t min_allowed=INT32_MIN, const int32_t max_allowed=INT32_MAX) noexcept;

            /**
             * Returns the uint32_t value of the environment's variable 'name',
             * or the 'default_value' if the environment variable's value is null
             * or not within uint32_t value range or within the given value range.
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * and hence attempts to also find a Unix conform name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.
             * </p>
             */
            static uint32_t getUint32Property(const std::string & name, const uint32_t default_value,
                                              const uint32_t min_allowed=0, const uint32_t max_allowed=UINT32_MAX) noexcept;

            /**
             * Fetches exploding variable-name (prefixDomain) values.
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * and hence attempts to also find a Unix conform name,
             * e.g. 'direct_bt_debug' if ''direct_bt.debug' wasn't found.
             * </p>
             * <p>
             * If the value of a prefixDomain is neither 'true' or 'false',
             * it is treated as a list of sub-variable names including their optional value separated by comma ','.
             * <p>
             * If the value is not given for the sub-variable name, a boolean "true" will be used per default.
             * </p>
             * <p>
             * Example 1
             * <pre>
             * Input Environment:
             *   "direct_bt.debug" := "jni,adapter.event,gatt.data=false,hci.event,mgmt.event=true"
             *
             * Result Environment:
             *   "direct_bt.debug.jni"           := "true"
             *   "direct_bt.debug.adapter.event" := "true"
             *   "direct_bt.debug.gatt.data"     := "false"
             *   "direct_bt.debug.hci.event"     := "true"
             *   "direct_bt.debug.mgmt.event"    := "true"
             *   "direct_bt.debug"               := "true" (will be overwritten)
             * </pre>
             * Example 2
             * <pre>
             * Input Environment:
             *   "direct_bt.gatt" := "cmd.read.timeout=20000,cmd.write.timeout=20001,ringsize=256"
             *
             * Result Environment:
             *   "direct_bt.gatt.cmd.read.timeout"  := "20000"
             *   "direct_bt.gatt.cmd.write.timeout" := "20001"
             *   "direct_bt.gatt.ringsize"          := "256"
             *   "direct_bt.gatt"                   := "true" (will be overwritten)
             * </pre>
             * </p>
             * <p>
             * Each sub-variable name/value pair will be trimmed and if not zero-length
             * appended to the prefixDomain with a dot '.'.</br>
             *
             * Each new variable name will be set in the environment with value 'true'.</br>
             *
             * The prefixDomain will also be set to the new value 'true', hence gets overwritten.</br>
             *
             * This is supported for DEBUG 'direct_bt.debug' and VERBOSE 'direct_bt.verbose', per default.
             * </p>
             *
             * @param prefixDomain
             * @return
             */
            static bool getExplodingProperties(const std::string & prefixDomain) noexcept;

            static environment& get() noexcept {
                /**
                 * Thread safe starting with C++11 6.7:
                 *
                 * If control enters the declaration concurrently while the variable is being initialized,
                 * the concurrent execution shall wait for completion of the initialization.
                 *
                 * (Magic Statics)
                 *
                 * Avoiding non-working double checked locking.
                 */
                static environment e;
                return e;
            }

            /**
             * Debug logging enabled or disabled.
             * <p>
             * Environment variable 'direct_bt.debug', boolean, default 'false'.
             * </p>
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * </p>
             * <p>
             * Exploding variable-name values are implemented here,
             * see {@link #getExplodingProperties(const std::string & prefixDomain)}.
             * </p>
             */
            const bool DEBUG;

            /**
             * JNI Debug logging enabled or disabled.
             * <p>
             * Environment variable 'direct_bt.debug.jni', boolean, default 'false'.
             * </p>
             * <p>
             * Implementation uses getBooleanProperty().
             * </p>
             */
            const bool DEBUG_JNI;

            /**
             * Verbose info logging enabled or disabled.
             * <p>
             * Environment variable 'direct_bt.verbose', boolean, default 'false'.
             * </p>
             * <p>
             * Implementation uses {@link #getProperty(const std::string & name)}
             * </p>
             * <p>
             * VERBOSE is also enabled if DEBUG is enabled!
             * </p>
             * <p>
             * Exploding variable-name values are implemented here,
             * see {@link #getExplodingProperties(const std::string & prefixDomain)}.
             * </p>
             */
            const bool VERBOSE;
    };

} // namespace jau

#endif /* JAU_ENV_HPP_ */

