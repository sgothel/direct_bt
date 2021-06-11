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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include "SMPKeyBin.hpp"

// #define USE_CXX17lib_FS 1
#if USE_CXX17lib_FS
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

using namespace direct_bt;

static bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

SMPKeyBin SMPKeyBin::create(const BTDevice& device) {
    const BTSecurityLevel sec_lvl = device.getConnSecurityLevel();
    const SMPPairingState pstate = device.getPairingState();
    const PairingMode pmode = device.getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)

    SMPKeyBin smpKeyBin(device.getAddressAndType(),
                        device.getConnSecurityLevel(), device.getConnIOCapability());

    if( ( BTSecurityLevel::NONE  < sec_lvl && SMPPairingState::COMPLETED == pstate && PairingMode::NEGOTIATING < pmode ) ||
        ( BTSecurityLevel::NONE == sec_lvl && SMPPairingState::NONE == pstate && PairingMode::NONE == pmode  ) )
    {
        const SMPKeyType keys_resp = device.getAvailableSMPKeys(true /* responder */);
        const SMPKeyType keys_init = device.getAvailableSMPKeys(false /* responder */);

        if( ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setLTKInit( device.getLongTermKeyInfo(false /* responder */) );
        }
        if( ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setLTKResp( device.getLongTermKeyInfo(true  /* responder */) );
        }

        if( ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setCSRKInit( device.getSignatureResolvingKeyInfo(false /* responder */) );
        }
        if( ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setCSRKResp( device.getSignatureResolvingKeyInfo(true  /* responder */) );
        }
    } else {
        smpKeyBin.size = 0; // explicitly mark invalid
    }
    return smpKeyBin;
}

bool SMPKeyBin::createAndWrite(const BTDevice& device, const std::string& path, const bool overwrite, const bool verbose_) {
    SMPKeyBin smpKeyBin = SMPKeyBin::create(device);
    if( smpKeyBin.isValid() ) {
        smpKeyBin.setVerbose( verbose_ );
        return smpKeyBin.write( getFilename(path, device.getAddressAndType()), overwrite );
    } else {
        if( verbose_ ) {
            jau::fprintf_td(stderr, "Create SMPKeyBin: Invalid %s, %s\n", smpKeyBin.toString().c_str(), device.toString().c_str());
        }
        return false;
    }
}

std::string SMPKeyBin::toString() const noexcept {
    std::string res = "SMPKeyBin["+addrAndType.toString()+", sec "+to_string(sec_level)+
                      ", io "+to_string(io_cap)+
                      ", ";
    if( isVersionValid() ) {
        res += "Init[";
        if( hasLTKInit() ) {
            res += ltk_init.toString();
        }
        if( hasCSRKInit() ) {
            if( hasLTKInit() ) {
                res += ", ";
            }
            res += csrk_init.toString();
        }
        res += "], Resp[";
        if( hasLTKResp() ) {
            res += ltk_resp.toString();
        }
        if( hasCSRKResp() ) {
            if( hasLTKResp() ) {
                res += ", ";
            }
            res += csrk_resp.toString();
        }
        res += "], ";
    }
    res += "ver["+jau::to_hexstring(version)+", ok "+std::to_string( isVersionValid() )+
           "], size["+std::to_string(size);
    if( verbose ) {
        res += ", calc "+std::to_string( calcSize() );
    }
    res += ", valid "+std::to_string( isSizeValid() )+
           "], ";
    {
        std::time_t t0 = static_cast<std::time_t>(ts_creation_sec);
        struct std::tm tm_0;
        if( nullptr == ::gmtime_r( &t0, &tm_0 ) ) {
            res += "1970-01-01 00:00:00"; // 19 + 1
        } else {
            char b[20];
            strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tm_0);
            res += std::string(b);
        }
    }
    res += ", valid "+std::to_string( isValid() )+"]";
    return res;
}

std::string SMPKeyBin::getFileBasename() const noexcept {
    std::string r("bd_"+addrAndType.address.toString()+":"+std::to_string(number(addrAndType.type))+"-smpkey.bin");
    std::replace( r.begin(), r.end(), ':', '_');
    return r;
}
std::string SMPKeyBin::getFileBasename(const BDAddressAndType& addrAndType_) {
    std::string r("bd_"+addrAndType_.address.toString()+":"+std::to_string(number(addrAndType_.type))+"-smpkey.bin");
    std::replace( r.begin(), r.end(), ':', '_');
    return r;
}

bool SMPKeyBin::remove_impl(const std::string& fname) {
#if USE_CXX17lib_FS
    const fs::path fname2 = fname;
    return fs::remove(fname);
#else
    return 0 == std::remove( fname.c_str() );
#endif
}

bool SMPKeyBin::write(const std::string& fname, const bool overwrite) const noexcept {
    if( !isValid() ) {
        if( verbose ) {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Invalid (skipped) %s\n", toString().c_str());
        }
        return false;
    }
    if( file_exists(fname) ) {
        if( overwrite ) {
            if( !remove_impl(fname) ) {
                jau::fprintf_td(stderr, "Write SMPKeyBin: Failed deletion of existing file %s, %s\n", fname.c_str(), toString().c_str());
                return false;
            }
        } else {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Not overwriting existing file %s, %s\n", fname.c_str(), toString().c_str());
            return false;
        }
    }
    std::ofstream file(fname, std::ios::out | std::ios::binary);

    if ( !file.good() || !file.is_open() ) {
        if( verbose ) {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Failed: File not open %s: %s\n", fname.c_str(), toString().c_str());
        }
        return false;
    }
    uint8_t buffer[8];

    jau::put_uint16(buffer, 0, version, true /* littleEndian */);
    file.write((char*)buffer, sizeof(version));

    jau::put_uint16(buffer, 0, size, true /* littleEndian */);
    file.write((char*)buffer, sizeof(size));

    jau::put_uint64(buffer, 0, ts_creation_sec, true /* littleEndian */);
    file.write((char*)buffer, sizeof(ts_creation_sec));

    file.write((char*)&addrAndType.address, sizeof(addrAndType.address));
    file.write((char*)&addrAndType.type, sizeof(addrAndType.type));
    file.write((char*)&sec_level, sizeof(sec_level));
    file.write((char*)&io_cap, sizeof(io_cap));

    file.write((char*)&keys_init, sizeof(keys_init));
    file.write((char*)&keys_resp, sizeof(keys_resp));

    if( hasLTKInit() ) {
        file.write((char*)&ltk_init, sizeof(ltk_init));
    }
    if( hasCSRKInit() ) {
        file.write((char*)&csrk_init, sizeof(csrk_init));
    }

    if( hasLTKResp() ) {
        file.write((char*)&ltk_resp, sizeof(ltk_resp));
    }
    if( hasCSRKResp() ) {
        file.write((char*)&csrk_resp, sizeof(csrk_resp));
    }

    const bool res = file.good() && file.is_open();
    if( verbose ) {
        if( res ) {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Success: %s: %s\n", fname.c_str(), toString().c_str());
        } else {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Failed: %s: %s\n", fname.c_str(), toString().c_str());
        }
    }
    file.close();
    return res;
}

bool SMPKeyBin::read(const std::string& fname, const bool removeInvalidFile) {
    std::ifstream file(fname, std::ios::binary);
    if ( !file.is_open() ) {
        if( verbose ) {
            jau::fprintf_td(stderr, "Read SMPKeyBin failed: %s\n", fname.c_str());
        }
        size = 0; // explicitly mark invalid
        return false;
    }
    bool err = false;
    uint8_t buffer[8];

    file.read((char*)buffer, sizeof(version));
    version = jau::get_uint16(buffer, 0, true /* littleEndian */);
    err = file.fail();

    if( !err ) {
        file.read((char*)buffer, sizeof(size));
        size = jau::get_uint16(buffer, 0, true /* littleEndian */);
        err = file.fail();
    }
    uint16_t remaining = size - sizeof(version) - sizeof(size);

    if( !err && 8 <= remaining ) {
        file.read((char*)buffer, sizeof(ts_creation_sec));
        ts_creation_sec = jau::get_uint64(buffer, 0, true /* littleEndian */);
        remaining -= 8;
        err = file.fail();
    } else {
        err = true;
    }
    if( !err && 11 <= remaining ) {
        file.read((char*)&addrAndType.address, sizeof(addrAndType.address));
        file.read((char*)&addrAndType.type, sizeof(addrAndType.type));
        file.read((char*)&sec_level, sizeof(sec_level));
        file.read((char*)&io_cap, sizeof(io_cap));

        file.read((char*)&keys_init, sizeof(keys_init));
        file.read((char*)&keys_resp, sizeof(keys_resp));
        remaining -= 11;
        err = file.fail();
    } else {
        err = true;
    }
    addrAndType.clearHash();

    if( !err && hasLTKInit() ) {
        if( sizeof(ltk_init) <= remaining ) {
            file.read((char*)&ltk_init, sizeof(ltk_init));
            remaining -= sizeof(ltk_init);
            err = file.fail();
        } else {
            err = true;
        }
    }
    if( !err && hasCSRKInit() ) {
        if( sizeof(csrk_init) <= remaining ) {
            file.read((char*)&csrk_init, sizeof(csrk_init));
            remaining -= sizeof(csrk_init);
            err = file.fail();
        } else {
            err = true;
        }
    }

    if( !err && hasLTKResp() ) {
        if( sizeof(ltk_resp) <= remaining ) {
            file.read((char*)&ltk_resp, sizeof(ltk_resp));
            remaining -= sizeof(ltk_resp);
            err = file.fail();
        } else {
            err = true;
        }
    }
    if( !err && hasCSRKResp() ) {
        if( sizeof(csrk_resp) <= remaining ) {
            file.read((char*)&csrk_resp, sizeof(csrk_resp));
            remaining -= sizeof(csrk_resp);
            err = file.fail();
        } else {
            err = true;
        }
    }
    if( !err ) {
        err = !isValid();
    }

    file.close();
    if( err ) {
        if( removeInvalidFile ) {
            remove_impl( fname );
        }
        if( verbose ) {
            jau::fprintf_td(stderr, "Read SMPKeyBin: Failed %s (removed %d): %s, remaining %u\n",
                    fname.c_str(), removeInvalidFile, toString().c_str(), remaining);
        }
        size = 0; // explicitly mark invalid
    } else {
        if( verbose ) {
            jau::fprintf_td(stderr, "Read SMPKeyBin: OK %s: %s, remaining %u\n",
                    fname.c_str(), toString().c_str(), remaining);
        }
    }
    return err;
}

HCIStatusCode SMPKeyBin::apply(BTDevice & device) const noexcept {
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    // Must be a valid SMPKeyBin instance and at least one LTK key if using encryption.
    if( !isValid() || ( BTSecurityLevel::NONE != sec_level && !hasLTKInit() && !hasLTKResp() ) ) {
        res = HCIStatusCode::INVALID_PARAMS;
        if( verbose ) {
            jau::fprintf_td(stderr, "Apply SMPKeyBin failed: SMPKeyBin Status: %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str());
        }
        return res;
    }
    if( !device.isValid() ) {
        res = HCIStatusCode::INVALID_PARAMS;
        if( verbose ) {
            jau::fprintf_td(stderr, "Apply SMPKeyBin failed: Device Invalid: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
        return res;
    }

    // Allow no encryption at all, i.e. BTSecurityLevel::NONE
    const BTSecurityLevel applySecLevel = BTSecurityLevel::NONE == sec_level ?
                                          BTSecurityLevel::NONE : BTSecurityLevel::ENC_ONLY;

    if( !device.setConnSecurity(applySecLevel, SMPIOCapability::NO_INPUT_NO_OUTPUT) ) {
        res = HCIStatusCode::CONNECTION_ALREADY_EXISTS;
        if( verbose ) {
            jau::fprintf_td(stderr, "Apply SMPKeyBin failed: Device Connected/ing: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
        return res;
    }

    if( hasLTKInit() ) {
        res = device.setLongTermKeyInfo( getLTKInit() );
        if( HCIStatusCode::SUCCESS != res && verbose ) {
            jau::fprintf_td(stderr, "Apply SMPKeyBin failed: Init-LTK Upload: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
    }

    if( HCIStatusCode::SUCCESS == res && hasLTKResp() ) {
        res = device.setLongTermKeyInfo( getLTKResp() );
        if( HCIStatusCode::SUCCESS != res && verbose ) {
            jau::fprintf_td(stderr, "Apply SMPKeyBin failed: Resp-LTK Upload: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
    }

    return res;
}

