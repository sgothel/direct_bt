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
#include "BTDevice.hpp"

namespace direct_bt {

/**
 * Storage for SMP keys including the required connection parameter.
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
 * has the following form `bd_C0_26_DA_01_DA_B1_1-smpkey.bin`:
 * <ul>
 * <li>{@code 'bd_'} denotes prefix</li>
 * <li>{@code 'C0_26_DA_01_DA_B1'} denotes the {@link EUI48} address</li>
 * <li>{@code '_1'} denotes the {@link BDAddressType}</li>
 * <li>{@code '-smpkey.bin'} denotes the suffix</li>
 * </li>
 * </p>
 */
class SMPKeyBin {
    public:
        constexpr static const uint16_t VERSION = (uint16_t)0b0101010101010101U + (uint16_t)3U; // bitpattern + version

    private:
        uint16_t version;                       //  2
        uint16_t size;                          //  2
        uint64_t ts_creation_sec;               //  8
        BDAddressAndType addrAndType;           //  7
        BTSecurityLevel sec_level;              //  1
        SMPIOCapability io_cap;                 //  1

        SMPKeyType keys_init;                   //  1
        SMPKeyType keys_resp;                   //  1

        SMPLongTermKeyInfo           ltk_init;  // 28 (optional)
        SMPSignatureResolvingKeyInfo csrk_init; // 17 (optional)
        SMPLinkKeyInfo               lk_init;   // 19 (optional)

        SMPLongTermKeyInfo           ltk_resp;  // 28 (optional)
        SMPSignatureResolvingKeyInfo csrk_resp; // 17 (optional)
        SMPLinkKeyInfo               lk_resp;   // 19 (optional)

        // Min-Max: 23 - 113 bytes

        bool verbose;

        constexpr uint16_t calcSize() const {
            uint16_t s = 0;
            s += sizeof(version);
            s += sizeof(size);
            s += sizeof(ts_creation_sec);
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
            if( hasLKInit() ) {
                s += sizeof(lk_init);
            }

            if( hasLTKResp() ) {
                s += sizeof(ltk_resp);
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
         * Method returns `false` if resulting SMPKeyBin is not SMPKeyBin::isValid().
         *
         * Otherwise, method returns the SMPKeyBin::write() result.
         *
         * @param device the BTDevice from which all required data is derived
         * @param path the path for the stored SMPKeyBin file.
         * @param overwrite if `true` and file already exists, delete file first. If `false` and file exists, return `false` w/o writing.
         * @param verbose_ set to true to have detailed write processing logged to stderr, otherwise false
         * @return `true` if file has been successfully written, otherwise `false`.
         * @see BTDevice
         * @see Create()
         * @see write()
         * @see isValid()
         */
        static bool createAndWrite(const BTDevice& device, const std::string& path, const bool overwrite, const bool verbose_);

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
         * Create a new SMPKeyBin instance on the fly based upon stored file denoted by `path` and BTDevice::getAddressAndType(),
         * i.e. `path/` + getFileBasename().
         *
         * Method returns ::HCIStatusCode::INVALID_PARAMS if resulting SMPKeyBin is not SMPKeyBin::isValid().
         *
         * Otherwise, method returns the HCIStatusCode of SMPKeyBin::apply().
         *
         * If key file is invalid or key could not be applied, i.e. not returning ::HCIStatusCode::SUCCESS, it is removed.
         *
         * @param path the path of the stored SMPKeyBin file.
         * @param device the BTDevice for which address the stored SMPKeyBin file will be read and applied to
         * @param minSecLevel minimum BTSecurityLevel the read SMPKeyBin::sec_level must be compliant to.
         *        If SMPKeyBin::sec_level < minSecLevel method removes the key file and returns ::HCIStatusCode::ENCRYPTION_MODE_NOT_ACCEPTED.
         * @param verbose_ set to true to have detailed read processing logged to stderr, otherwise false
         * @return ::HCIStatusCode::SUCCESS or error code for failure
         * @see Read()
         * @see isValid()
         * @see read()
         * @see apply()
         */
        static HCIStatusCode readAndApply(const std::string& path, BTDevice& device, const BTSecurityLevel minSecLevel, const bool verbose_);

        SMPKeyBin(const BDAddressAndType& addrAndType_,
                  const BTSecurityLevel sec_level_, const SMPIOCapability io_cap_)
        : version(VERSION), size(0),
          ts_creation_sec( jau::getWallClockSeconds() ),
          addrAndType(addrAndType_), sec_level(sec_level_), io_cap(io_cap_),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), csrk_init(), lk_init(),
          ltk_resp(), csrk_resp(), lk_resp(),
          verbose(false)
        { size = calcSize(); }

        SMPKeyBin()
        : version(VERSION), size(0),
          ts_creation_sec(0),
          addrAndType(), sec_level(BTSecurityLevel::UNSET), io_cap(SMPIOCapability::UNSET),
          keys_init(SMPKeyType::NONE), keys_resp(SMPKeyType::NONE),
          ltk_init(), csrk_init(), lk_init(),
          ltk_resp(), csrk_resp(), lk_resp(),
          verbose(false)
        { size = calcSize(); }

        constexpr bool isVersionValid() const noexcept { return VERSION==version; }
        constexpr uint16_t getVersion() const noexcept { return version;}

        constexpr bool isSizeValid() const noexcept { return calcSize() == size;}
        constexpr uint16_t getSize() const noexcept { return size;}

        /** Returns the creation timestamp in seconds since Unix epoch */
        constexpr uint64_t getCreationTime() const noexcept { return ts_creation_sec; }

        constexpr const BDAddressAndType& getAddrAndType() const noexcept { return addrAndType; }
        constexpr BTSecurityLevel getSecLevel() const noexcept { return sec_level; }
        constexpr SMPIOCapability getIOCap() const noexcept { return io_cap; }

        constexpr bool hasLTKInit() const noexcept { return ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKInit() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr bool hasLKInit() const noexcept { return ( SMPKeyType::LINK_KEY & keys_init ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKeyInfo& getLTKInit() const noexcept { return ltk_init; }
        constexpr const SMPSignatureResolvingKeyInfo& getCSRKInit() const noexcept { return csrk_init; }
        constexpr const SMPLinkKeyInfo& getLKInit() const noexcept { return lk_init; }
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
        void setLKInit(const SMPLinkKeyInfo& v) noexcept {
            lk_init = v;
            keys_init |= SMPKeyType::LINK_KEY;
            size = calcSize();
        }

        constexpr bool hasLTKResp() const noexcept { return ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasCSRKResp() const noexcept { return ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr bool hasLKResp() const noexcept { return ( SMPKeyType::LINK_KEY & keys_resp ) != SMPKeyType::NONE; }
        constexpr const SMPLongTermKeyInfo& getLTKResp() const noexcept { return ltk_resp; }
        constexpr const SMPSignatureResolvingKeyInfo& getCSRKResp() const noexcept { return csrk_resp; }
        constexpr const SMPLinkKeyInfo& getLKResp() const noexcept { return lk_resp; }
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
        void setLKResp(const SMPLinkKeyInfo& v) noexcept {
            lk_resp = v;
            keys_resp |= SMPKeyType::LINK_KEY;
            size = calcSize();
        }

        void setVerbose(bool v) noexcept { verbose = v; }

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
        static std::string getFileBasename(const BDAddressAndType& addrAndType_);

        static std::string getFilename(const std::string& path, const BDAddressAndType& addrAndType_) {
            return path + "/" + getFileBasename(addrAndType_);
        }

        static bool remove(const std::string& path, const BDAddressAndType& addrAndType_) {
            return remove_impl(getFilename(path, addrAndType_));
        }

        bool write(const std::string& fname, const bool overwrite) const noexcept;

        bool read(const std::string& fname);

        /**
         * If this instance isValid() and initiator or responder LTK available, i.e. hasLTKInit() or hasLTKResp(),
         * the following procedure will be applied to the given BTDevice:
         *
         * - If BTSecurityLevel _is_ BTSecurityLevel::NONE
         *   + Setting security to ::BTSecurityLevel::NONE and ::SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
         * - else if BTSecurityLevel > BTSecurityLevel::NONE
         *   + Setting security to ::BTSecurityLevel::ENC_ONLY and ::SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
         *   + Setting initiator LTK from getLTKInit() via BTDevice::setLongTermKeyInfo(), if available
         *   + Setting responder LTK from getLTKResp() via BTDevice::setLongTermKeyInfo(), if available
         *
         * If all three operations succeed, ::HCIStatusCode::SUCCESS will be returned,
         * otherwise the appropriate status code below.
         *
         * ::BTSecurityLevel::ENC_ONLY is set to avoid a new SMP ::PairingMode negotiation,
         * which is undesired as this instances' stored LTK shall be used for ::PairingMode::PRE_PAIRED.
         *
         * Method may fail for any of the following reasons:
         *
         *  Reason                                                     | ::HCIStatusCode                             |
         *  :--------------------------------------------------------  | :------------------------------------------ |
         *  ! isValid()                                                | ::HCIStatusCode::INVALID_PARAMS             |
         *  | hasLTKInit() && ! hasLTKResp()                           | ::HCIStatusCode::INVALID_PARAMS             |
         *  | BTDevice::isValid() == false                             | ::HCIStatusCode::INVALID_PARAMS             |
         *  | BTDevice has already being connected                     | ::HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  | BTDevice::connectLE() or BTDevice::connectBREDR() called | ::HCIStatusCode::CONNECTION_ALREADY_EXISTS  |
         *  | BTDevice::setLongTermKeyInfo() failed                    | ::HCIStatusCode from BT adapter             |
         *  | BTDevice::setLinkKeyInfo() failed                        | ::HCIStatusCode from BT adapter             |
         *
         * On failure and after BTDevice::setConnSecurity() has been performed, the ::BTSecurityLevel
         * and ::SMPIOCapability pre-connect values have been written and must be set by the caller again.
         *
         * @param device the BTDevice for which this instances' LTK shall be applied
         *
         * @see isValid()
         * @see hasLTKInit()
         * @see hasLTKResp()
         * @see getLTKInit()
         * @see getLTKResp()
         * @see hasLKInit()
         * @see hasLKResp()
         * @see getLKInit()
         * @see getLKResp()
         * @see ::BTSecurityLevel
         * @see ::SMPIOCapability
         * @see BTDevice::isValid()
         * @see BTDevice::setConnSecurity()
         * @see BTDevice::setLongTermKeyInfo()
         * @see BTDevice::setLinkKeyInfo()
         */
        HCIStatusCode apply(BTDevice & device) const noexcept;
};

} // namespace direct_bt

#endif /* SMPKEYBIN_HPP_ */
