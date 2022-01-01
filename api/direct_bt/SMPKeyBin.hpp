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
#include "HCITypes.hpp"

namespace direct_bt {

class BTDevice; // forward
class BTAdapter; // forward

/**
 * Storage for SMP keys including required connection parameter per local adapter and remote device.
 *
 * File format version 5.
 *
 * Storage for a device's BDAddressAndType, its security connection setup ::BTSecurityLevel + ::SMPIOCapability
 * and optionally the initiator and responder SMPLongTermKeyInfo (LTK), SMPSignatureResolvingKeyInfo (CSRK)
 * and SMPLinkKeyInfo (LK) within one file.
 * <p>
 * Since the SMPLongTermKeyInfo (LTK), SMPSignatureResolvingKeyInfo (CSRK)
 * and SMPLinkKeyInfo (LK)
 * are optionally set depending on their availability per initiator and responder,
 * implementation supports mixed mode for certain devices.
 * E.g. LTK responder key only etc.
 * </p>
 * <p>
 * Data is stored in endian::little format, native to Bluetooth.
 * </p>
 * <p>
 * Filename as retrieved by SMPKeyBin::getFileBasename()
 * has the following form `bd_010203040506_C026DA01DAB11.key`:
 * <ul>
 * <li>{@code 'bd_'} prefix</li>
 * <li>{@code '010203040506'} local {@link EUI48} local adapter address</li>
 * <li>{@code '_'} separator</li>
 * <li>{@code 'C026DA01DAB1'} remote {@link EUI48} remote device address</li>
 * <li>{@code '1'} {@link BDAddressType}</li>
 * <li>{@code '.key'} suffix</li>
 * </li>
 * </p>
 * @see BTDevice::setSMPKeyBin()
 * @see BTDevice::uploadKeys()
 */
class SMPKeyBin {
    public:
        constexpr static const uint16_t VERSION = (uint16_t)0b0101010101010101U + (uint16_t)5U; // bitpattern + version

    private:
        uint16_t version;                       //  2
        uint16_t size;                          //  2
        uint64_t ts_creation_sec;               //  8
        BDAddressAndType localAddress;          //  7
        BDAddressAndType remoteAddress;         //  7
        BTSecurityLevel sec_level;              //  1
        SMPIOCapability io_cap;                 //  1

        SMPKeyType keys_init;                   //  1
        SMPKeyType keys_resp;                   //  1

        SMPLongTermKey           ltk_init;      // 28 (optional)
        SMPIdentityResolvingKey  irk_init;      // 17 (optional)
        SMPSignatureResolvingKey csrk_init;     // 17 (optional)
        SMPLinkKey               lk_init;       // 19 (optional)

        SMPLongTermKey           ltk_resp;      // 28 (optional)
        SMPIdentityResolvingKey  irk_resp;      // 17 (optional)
        SMPSignatureResolvingKey csrk_resp;     // 17 (optional)
        SMPLinkKey               lk_resp;       // 19 (optional)

        // Min-Max: 30 - 190 bytes

        bool verbose;

        constexpr uint16_t calcSize() const {
            uint16_t s = 0;
            s += sizeof(version);
            s += sizeof(size);
            s += sizeof(ts_creation_sec);
            s += sizeof(localAddress.address);
            s += sizeof(localAddress.type);
            s += sizeof(remoteAddress.address);
            s += sizeof(remoteAddress.type);
            s += sizeof(sec_level);
            s += sizeof(io_cap);

            s += sizeof(keys_init);
            s += sizeof(keys_resp);

            if( hasLTKInit() ) {
                s += sizeof(ltk_init);
            }
            if( hasIRKInit() ) {
                s += sizeof(irk_init);
            }
            if( hasCSRKInit() ) {
                s += sizeof(csrk_init);
            }
            if( hasLKInit() ) {
                s += sizeof(lk_init);
            }

            if( hasLTKResp() ) {
                s += sizeof(ltk_resp);
            }
            if( hasIRKResp() ) {
                s += sizeof(irk_resp);
            }
            if( hasCSRKResp() ) {
                s += sizeof(csrk_resp);
            }
            if( hasLKResp() ) {
                s += sizeof(lk_resp);
            }
            return s;
        }

        static bool remove_impl(const std::string& fname);

    public:
        /**
         * Create a new SMPKeyBin instance based upon given BTDevice's
         * BTSecurityLevel, SMPPairingState, PairingMode and LTK keys.
         *
         * Returned SMPKeyBin shall be tested if valid via SMPKeyBin::isValid(),
         * whether the retrieved data from BTDevice is consistent and hence
         * having BTDevice is a well connected state.
         *
         * @param device the BTDevice from which all required data is derived
         * @return a valid SMPKeyBin instance if properly connected, otherwise an invalid instance.
         * @see BTDevice
         * @see isValid()
         */
        static SMPKeyBin create(const BTDevice& device);

        /**
         * Create a new SMPKeyBin instance on the fly based upon given BTDevice's
         * BTSecurityLevel, SMPPairingState, PairingMode and LTK keys.
         * If valid, instance is stored to a file denoted by `path` and `BTDevice::getAddressAndType()`.
         *
         * If BTDevice::getPairingMode() is PairingMode::PRE_PAIRED, an existing file will not be overwritten.
         * Otherwise, a new key is assumed and an existing file shall be overwritten.
         *
         * Method returns `false` if resulting SMPKeyBin is not SMPKeyBin::isValid().
         * Otherwise, method returns the SMPKeyBin::write() result.
         *
         * @param device the BTDevice from which all required data is derived
         * @param path the path for the stored SMPKeyBin file.
         * @param verbose_ set to true to have detailed write processing logged to stderr, otherwise false
         * @return `true` if file has been successfully written, otherwise `false`.
         * @see BTDevice
         * @see Create()
         * @see write()
         * @see isValid()
         */
        static bool createAndWrite(const BTDevice& device, const std::string& path, const bool verbose_);

        /**
         * Create a new SMPKeyBin instance based upon stored file denoted by `fname`.
         *
         * Returned SMPKeyBin shall be tested if valid via SMPKeyBin::isValid(),
         * whether the read() operation was successful and data is consistent.
         *
         * If file is invalid, it is removed.
         *
         * @param fname full path of the stored SMPKeyBin file.
         * @param verbose_ set to true to have detailed read processing logged to stderr, otherwise false
         * @return valid SMPKeyBin instance if file exist and read successfully, otherwise invalid SMPKeyBin instance.
         * @see isValid()
         * @see read()
         */
        static SMPKeyBin read(const std::string& fname, const bool verbose_) {
            SMPKeyBin smpKeyBin;
            smpKeyBin.setVerbose( verbose_ );
            smpKeyBin.read( fname ); // read failure -> !isValid()
            return smpKeyBin;
        }

        /**
         * Create a new SMPKeyBin instance based upon the given BTDevice's matching filename,
         * see SMPKeyBin API doc for filename naming scheme.
         *
         * Returned SMPKeyBin shall be tested if valid via SMPKeyBin::isValid(),
         * whether the read() operation was successful and data is consistent.
         *
         * If file is invalid, it is removed.
         *
         * @param path directory for the stored SMPKeyBin file.
         * @param device BTDevice used to derive the filename, see getFilename()
         * @param verbose_ set to true to have detailed read processing logged to stderr, otherwise false
         * @return valid SMPKeyBin instance if file exist and read successfully, otherwise invalid SMPKeyBin instance.
         * @see getFilename()
         * @see isValid()
         * @see read()
         */
        static SMPKeyBin read(const std::string& path, const BTDevice& device, const bool verbose_) {
            return read(getFilename(path, device), verbose_);
        }

        static std::vector<SMPKeyBin> readAll(const std::string& dname, const bool verbose_);
        static std::vector<SMPKeyBin> readAllForLocalAdapter(const BDAddressAndType& localAddress, const std::string& dname, const bool verbose_);

        SMPKeyBin(const BDAddressAndType& localAddress_,
                  const BDAddressAndType& remoteAddress_,
                  const BTSecurityLevel sec_level_, const SMPIOCapability io_cap_)
        : version(VERSION), size(0),
          ts_creation_sec( jau::getWallClockSeconds() ),
          localAddress(localAddress_), remoteAddress(remoteAddress_),
          sec_level(sec_level_), io_cap(io_cap_),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), irk_init(), csrk_init(), lk_init(),
          ltk_resp(), irk_resp(), csrk_resp(), lk_resp(),
          verbose(false)
        { size = calcSize(); }

        SMPKeyBin()
        : version(VERSION), size(0),
          ts_creation_sec(0),
          localAddress(), remoteAddress(),
          sec_level(BTSecurityLevel::UNSET), io_cap(SMPIOCapability::UNSET),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), irk_init(), csrk_init(), lk_init(),
          ltk_resp(), irk_resp(), csrk_resp(), lk_resp(),
          verbose(false)
        { size = calcSize(); }

        constexpr bool isVersionValid() const noexcept { return VERSION==version; }
        constexpr uint16_t getVersion() const noexcept { return version;}

        constexpr bool isSizeValid() const noexcept { return calcSize() == size;}
        constexpr uint16_t getSize() const noexcept { return size;}

        /** Returns the creation timestamp in seconds since Unix epoch */
        constexpr uint64_t getCreationTime() const noexcept { return ts_creation_sec; }

        /** Return the local adapter address. */
        constexpr const BDAddressAndType& getLocalAddrAndType() const noexcept { return localAddress; }

        /** Return the remote device address. */
        constexpr const BDAddressAndType& getRemoteAddrAndType() const noexcept { return remoteAddress; }

        /** Return whether Secure Connection (SC) is being used via LTK keys. */
        constexpr bool uses_SC() const noexcept {
            return ( hasLTKInit() && SMPLongTermKey::Property::NONE != ( getLTKInit().properties & SMPLongTermKey::Property::SC ) ) ||
                   ( hasLTKResp() && SMPLongTermKey::Property::NONE != ( getLTKResp().properties & SMPLongTermKey::Property::SC ) );
        }

        constexpr BTSecurityLevel getSecLevel() const noexcept { return sec_level; }
        constexpr SMPIOCapability getIOCap() const noexcept { return io_cap; }

        constexpr bool hasLTKInit() const noexcept { return ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasIRKInit() const noexcept { return ( SMPKeyType::ID_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKInit() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasLKInit() const noexcept { return ( SMPKeyType::LINK_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKey& getLTKInit() const noexcept { return ltk_init; }
        constexpr const SMPIdentityResolvingKey& getIRKInit() const noexcept { return irk_init; }
        constexpr const SMPSignatureResolvingKey& getCSRKInit() const noexcept { return csrk_init; }
        constexpr const SMPLinkKey& getLKInit() const noexcept { return lk_init; }
        void setLTKInit(const SMPLongTermKey& v) noexcept {
            ltk_init = v;
            keys_init |= SMPKeyType::ENC_KEY;
            size = calcSize();
        }
        void setIRKInit(const SMPIdentityResolvingKey& v) noexcept {
            irk_init = v;
            keys_init |= SMPKeyType::ID_KEY;
            size = calcSize();
        }
        void setCSRKInit(const SMPSignatureResolvingKey& v) noexcept {
            csrk_init = v;
            keys_init |= SMPKeyType::SIGN_KEY;
            size = calcSize();
        }
        void setLKInit(const SMPLinkKey& v) noexcept {
            lk_init = v;
            keys_init |= SMPKeyType::LINK_KEY;
            size = calcSize();
        }

        constexpr bool hasLTKResp() const noexcept { return ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasIRKResp() const noexcept { return ( SMPKeyType::ID_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKResp() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasLKResp() const noexcept { return ( SMPKeyType::LINK_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKey& getLTKResp() const noexcept { return ltk_resp; }
        constexpr const SMPIdentityResolvingKey& getIRKResp() const noexcept { return irk_resp; }
        constexpr const SMPSignatureResolvingKey& getCSRKResp() const noexcept { return csrk_resp; }
        constexpr const SMPLinkKey& getLKResp() const noexcept { return lk_resp; }
        void setLTKResp(const SMPLongTermKey& v) noexcept {
            ltk_resp = v;
            keys_resp |= SMPKeyType::ENC_KEY;
            size = calcSize();
        }
        void setIRKResp(const SMPIdentityResolvingKey& v) noexcept {
            irk_resp = v;
            keys_resp |= SMPKeyType::ID_KEY;
            size = calcSize();
        }
        void setCSRKResp(const SMPSignatureResolvingKey& v) noexcept {
            csrk_resp = v;
            keys_resp |= SMPKeyType::SIGN_KEY;
            size = calcSize();
        }
        void setLKResp(const SMPLinkKey& v) noexcept {
            lk_resp = v;
            keys_resp |= SMPKeyType::LINK_KEY;
            size = calcSize();
        }

        void setVerbose(bool v) noexcept { verbose = v; }

        constexpr bool getVerbose() const noexcept { return verbose; }

        /**
         * Returns `true` if
         *
         *  isVersionValid() && isSizeValid() &&
         *  not BTSecurityLevel::UNSET &&
         *  not SMPIOCapability::UNSET &&
         *  has valid LTK, if at all
         *
         */
        constexpr bool isValid() const noexcept {
            return isVersionValid() && isSizeValid() &&
                   BTSecurityLevel::UNSET != sec_level &&
                   SMPIOCapability::UNSET != io_cap &&
                   ( !hasLTKInit() || ltk_init.isValid() ) &&
                   ( !hasLTKResp() || ltk_resp.isValid() ) &&
                   ( !hasLKInit()  || lk_init.isValid() )  &&
                   ( !hasLKResp()  || lk_resp.isValid() );
        }

        std::string toString() const noexcept;

        /**
         * Returns the base filename, see SMPKeyBin API doc for naming scheme.
         */
        std::string getFileBasename() const noexcept;

        /**
         * Returns the base filename, see SMPKeyBin API doc for naming scheme.
         */
        static std::string getFileBasename(const BDAddressAndType& localAddress_, const BDAddressAndType& remoteAddress_) noexcept;

        static std::string getFilename(const std::string& path, const BDAddressAndType& localAddress_, const BDAddressAndType& remoteAddress_) noexcept {
            return path + "/" + getFileBasename(localAddress_, remoteAddress_);
        }
        static std::string getFilename(const std::string& path, const BTDevice& remoteDevice) noexcept ;

        static bool remove(const std::string& path, const BDAddressAndType& localAddress_, const BDAddressAndType& remoteAddress_) {
            return remove_impl( getFilename(path, localAddress_, remoteAddress_) );
        }
        static bool remove(const std::string& path, const BTDevice& remoteDevice);

        std::string getFilename(const std::string& path) const noexcept {
            return getFilename(path, localAddress, remoteAddress);
        }
        bool remove(const std::string& path) {
            return remove_impl( getFilename(path) );
        }

        bool write(const std::string& path, const bool overwrite) const noexcept;

        bool read(const std::string& fname);
};

} // namespace direct_bt

#endif /* SMPKEYBIN_HPP_ */
