/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

#ifndef SMPKEYBIN_HPP_
#define SMPKEYBIN_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <fstream>
#include <iostream>

#include "SMPTypes.hpp"

namespace direct_bt {

/**
 * Storage for SMP keys including the required connection parameter.
 *
 * Storage for a device's BDAddressAndType, its security connection setup ::BTSecurityLevel + ::SMPIOCapability
 * and optionally the initiator and responder SMPLongTermKeyInfo (LTK) and SMPSignatureResolvingKeyInfo (CSRK) within one file.
 * <p>
 * Since the SMPLongTermKeyInfo (LTK) and SMPSignatureResolvingKeyInfo (CSRK)
 * can be optionally set due to their availability per initiator and responder,
 * implementation supports mixed mode for certain devices.
 * E.g. LTK responder key only etc.
 * </p>
 */
class SMPKeyBin {
    public:
        constexpr static const uint16_t VERSION = (uint16_t)0b0101010101010101 + (uint16_t)1; // bitpattern + version

    private:
        uint16_t version;                       //  2
        uint16_t size;                          //  2
        BDAddressAndType addrAndType;           //  7
        BTSecurityLevel sec_level;              //  1
        SMPIOCapability io_cap;                 //  1

        SMPKeyType keys_init;                   //  1
        SMPKeyType keys_resp;                   //  1

        SMPLongTermKeyInfo ltk_init;            // 28 (optional)
        SMPSignatureResolvingKeyInfo csrk_init; // 17 (optional)

        SMPLongTermKeyInfo ltk_resp;            // 28 (optional)
        SMPSignatureResolvingKeyInfo csrk_resp; // 17 (optional)

        // Min-Max: 15 - 105 bytes

        bool verbose;

        constexpr uint16_t calcSize() const {
            uint16_t s = 0;
            s += sizeof(version);
            s += sizeof(size);
            s += sizeof(addrAndType.address);
            s += sizeof(addrAndType.type);
            s += sizeof(sec_level);
            s += sizeof(io_cap);

            s += sizeof(keys_init);
            s += sizeof(keys_resp);

            if( hasLTKInit() ) {
                s += sizeof(ltk_init);
            }
            if( hasCSRKInit() ) {
                s += sizeof(csrk_init);
            }

            if( hasLTKResp() ) {
                s += sizeof(ltk_resp);
            }
            if( hasCSRKResp() ) {
                s += sizeof(csrk_resp);
            }
            return s;
        }

    public:
        SMPKeyBin(const BDAddressAndType& addrAndType_,
                      const BTSecurityLevel sec_level_, const SMPIOCapability io_cap_)
        : version(VERSION), size(0),
          addrAndType(addrAndType_), sec_level(sec_level_), io_cap(io_cap_),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), csrk_init(), ltk_resp(), csrk_resp(),
          verbose(false)
        { size = calcSize(); }

        SMPKeyBin()
        : version(VERSION), size(0),
          addrAndType(), sec_level(BTSecurityLevel::UNSET), io_cap(SMPIOCapability::UNSET),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), csrk_init(), ltk_resp(), csrk_resp(),
          verbose(false)
        { size = calcSize(); }

        constexpr bool isVersionValid() const noexcept { return VERSION==version; }
        constexpr uint16_t getVersion() const noexcept { return version;}

        constexpr bool isSizeValid() const noexcept { return calcSize() == size;}
        constexpr uint16_t getSize() const noexcept { return size;}

        constexpr const BDAddressAndType& getAddrAndType() const noexcept { return addrAndType; }
        constexpr BTSecurityLevel getSecLevel() const noexcept { return sec_level; }
        constexpr SMPIOCapability getIOCap() const noexcept { return io_cap; }

        constexpr bool hasLTKInit() const noexcept { return ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKInit() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKeyInfo& getLTKInit() const noexcept { return ltk_init; }
        constexpr const SMPSignatureResolvingKeyInfo& getCSRKInit() const noexcept { return csrk_init; }
        void setLTKInit(const SMPLongTermKeyInfo& v) noexcept {
            ltk_init = v;
            keys_init |= SMPKeyType::ENC_KEY;
            size = calcSize();
        }
        void setCSRKInit(const SMPSignatureResolvingKeyInfo& v) noexcept {
            csrk_init = v;
            keys_init |= SMPKeyType::SIGN_KEY;
            size = calcSize();
        }

        constexpr bool hasLTKResp() const noexcept { return ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKResp() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKeyInfo& getLTKResp() const noexcept { return ltk_resp; }
        constexpr const SMPSignatureResolvingKeyInfo& getCSRKResp() const noexcept { return csrk_resp; }
        void setLTKResp(const SMPLongTermKeyInfo& v) noexcept {
            ltk_resp = v;
            keys_resp |= SMPKeyType::ENC_KEY;
            size = calcSize();
        }
        void setCSRKResp(const SMPSignatureResolvingKeyInfo& v) noexcept {
            csrk_resp = v;
            keys_resp |= SMPKeyType::SIGN_KEY;
            size = calcSize();
        }

        void setVerbose(bool v) noexcept { verbose = v; }

        constexpr bool isValid() const noexcept {
            return isVersionValid() && isSizeValid() &&
                   BTSecurityLevel::UNSET != sec_level &&
                   SMPIOCapability::UNSET != io_cap &&
                   ( !hasLTKInit() || ltk_init.isValid() ) &&
                   ( !hasLTKResp() || ltk_resp.isValid() );
        }

        std::string toString() const noexcept;

        std::string getFileBasename() const noexcept {
            return "bd_"+addrAndType.address.toString()+":"+std::to_string(number(addrAndType.type))+".smpkey.bin";
        }
        static std::string getFileBasename(const BDAddressAndType& addrAndType_) {
            return "bd_"+addrAndType_.address.toString()+":"+std::to_string(number(addrAndType_.type))+".smpkey.bin";
        }

        bool write(const std::string path, const std::string basename) const noexcept;

        bool write(const std::string path) const noexcept {
            return write( path, getFileBasename() );
        }

        bool read(const std::string path, const std::string basename);

        bool read(const std::string path, const BDAddressAndType& addrAndType_) {
            return read(path, getFileBasename(addrAndType_));
        }
};

} // namespace direct_bt

#endif /* SMPKEYBIN_HPP_ */
