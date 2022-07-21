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
#include <fstream>
#include <iostream>

#include <jau/file_util.hpp>

#include "SMPKeyBin.hpp"

#include "BTDevice.hpp"
#include "BTAdapter.hpp"

using namespace direct_bt;

static std::vector<std::string> get_file_list(const std::string& dname) {
    std::vector<std::string> res;
    const jau::fs::consume_dir_item cs = jau::bindCaptureRefFunc(&res,
            ( void(*)(std::vector<std::string>*, const jau::fs::dir_item&) ) /* help template type deduction of function-ptr */
                ( [](std::vector<std::string>* receiver, const jau::fs::dir_item& item) -> void {
                    if( 0 == item.basename().find("bd_") ) { // prefix checl
                        const jau::nsize_t suffix_pos = item.basename().size() - 4;
                        if( suffix_pos == item.basename().find(".key", suffix_pos) ) { // suffix check
                            receiver->push_back( item.path() ); // full path
                        }
                    }
                  } )
        );
    jau::fs::get_dir_content(dname, cs);
    return res;
}

bool SMPKeyBin::remove_impl(const std::string& fname) {
    return 0 == std::remove( fname.c_str() );
}

SMPKeyBin SMPKeyBin::create(const BTDevice& device) {
    const BTSecurityLevel sec_lvl = device.getConnSecurityLevel();
    const SMPPairingState pstate = device.getPairingState();
    const PairingMode pmode = device.getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)

    SMPKeyBin smpKeyBin(device.getAdapter().getAddressAndType(), device.getAddressAndType(),
                        device.getConnSecurityLevel(), device.getConnIOCapability());

    if( ( BTSecurityLevel::NONE  < sec_lvl && SMPPairingState::COMPLETED == pstate && PairingMode::NEGOTIATING < pmode ) ||
        ( BTSecurityLevel::NONE == sec_lvl && SMPPairingState::NONE == pstate && PairingMode::NONE == pmode  ) )
    {
        const SMPKeyType keys_resp = device.getAvailableSMPKeys(true /* responder */);
        const SMPKeyType keys_init = device.getAvailableSMPKeys(false /* responder */);

        if( ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setLTKInit( device.getLongTermKey(false /* responder */) );
        }
        if( ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setLTKResp( device.getLongTermKey(true  /* responder */) );
        }

        if( ( SMPKeyType::ID_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setIRKInit( device.getIdentityResolvingKey(false /* responder */) );
        }
        if( ( SMPKeyType::ID_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setIRKResp( device.getIdentityResolvingKey(true  /* responder */) );
        }

        if( ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setCSRKInit( device.getSignatureResolvingKey(false /* responder */) );
        }
        if( ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setCSRKResp( device.getSignatureResolvingKey(true  /* responder */) );
        }

        if( ( SMPKeyType::LINK_KEY & keys_init ) != SMPKeyType::NONE ) {
            smpKeyBin.setLKInit( device.getLinkKey(false /* responder */) );
        }
        if( ( SMPKeyType::LINK_KEY & keys_resp ) != SMPKeyType::NONE ) {
            smpKeyBin.setLKResp( device.getLinkKey(true  /* responder */) );
        }
    } else {
        smpKeyBin.size = 0; // explicitly mark invalid
    }
    return smpKeyBin;
}

bool SMPKeyBin::createAndWrite(const BTDevice& device, const std::string& path, const bool verbose_) {
    SMPKeyBin smpKeyBin = SMPKeyBin::create(device);
    if( smpKeyBin.isValid() ) {
        smpKeyBin.setVerbose( verbose_ );
        const bool overwrite = PairingMode::PRE_PAIRED != device.getPairingMode();
        return smpKeyBin.write( path, overwrite );
    } else {
        if( verbose_ ) {
            jau::fprintf_td(stderr, "Create SMPKeyBin: Invalid %s, %s\n", smpKeyBin.toString().c_str(), device.toString().c_str());
        }
        return false;
    }
}

std::vector<SMPKeyBin> SMPKeyBin::readAll(const std::string& dname, const bool verbose_) {
    std::vector<SMPKeyBin> res;
    std::vector<std::string> fnames = get_file_list(dname);
    for(std::string fname : fnames) {
        SMPKeyBin f = read(fname, verbose_);
        if( f.isValid() ) {
            res.push_back(f);
        }
    }
    return res;
}

std::vector<SMPKeyBin> SMPKeyBin::readAllForLocalAdapter(const BDAddressAndType& localAddress, const std::string& dname, const bool verbose_) {
    std::vector<SMPKeyBin> res;
    std::vector<SMPKeyBin> all = readAll(dname, verbose_);
    for(SMPKeyBin f : all) {
        if( localAddress == f.getLocalAddrAndType() ) {
            res.push_back(f);
        }
    }
    return res;
}

std::string SMPKeyBin::toString() const noexcept {
    std::string res = "SMPKeyBin[local "+localAddress.toString()+", remote "+remoteAddress.toString()+
                      ", SC "+std::to_string(uses_SC())+", sec "+to_string(sec_level)+", io "+to_string(io_cap)+
                      ", ";
    if( isVersionValid() ) {
        bool comma = false;
        res += "Init[";
        if( hasLTKInit() ) {
            res += ltk_init.toString();
            comma = true;
        }
        if( hasIRKInit() ) {
            if( comma ) {
                res += ", ";
            }
            res += irk_init.toString();
            comma = true;
        }
        if( hasCSRKInit() ) {
            if( comma ) {
                res += ", ";
            }
            res += csrk_init.toString();
            comma = true;
        }
        if( hasLKInit() ) {
            if( comma ) {
                res += ", ";
            }
            res += lk_init.toString();
            comma = true;
        }
        comma = false;
        res += "], Resp[";
        if( hasLTKResp() ) {
            res += ltk_resp.toString();
            comma = true;
        }
        if( hasIRKResp() ) {
            if( comma ) {
                res += ", ";
            }
            res += irk_resp.toString();
            comma = true;
        }
        if( hasCSRKResp() ) {
            if( comma ) {
                res += ", ";
            }
            res += csrk_resp.toString();
            comma = true;
        }
        if( hasLKResp() ) {
            if( comma ) {
                res += ", ";
            }
            res += lk_resp.toString();
            comma = true;
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
        jau::fraction_timespec t0( ts_creation_sec, 0 );
        res += t0.to_iso8601_string();
    }
    res += ", valid "+std::to_string( isValid() )+"]";
    return res;
}

std::string SMPKeyBin::getFileBasename() const noexcept {
    std::string r("bd_"+localAddress.address.toString()+"_"+remoteAddress.address.toString()+std::to_string(number(remoteAddress.type))+".key");
    auto it = std::remove( r.begin(), r.end(), ':');
    r.erase(it, r.end());
    return r;
}
std::string SMPKeyBin::getFileBasename(const BDAddressAndType& localAddress_, const BDAddressAndType& remoteAddress_) noexcept {
    std::string r("bd_"+localAddress_.address.toString()+"_"+remoteAddress_.address.toString()+std::to_string(number(remoteAddress_.type))+".key");
    auto it = std::remove( r.begin(), r.end(), ':');
    r.erase(it, r.end());
    return r;
}
std::string SMPKeyBin::getFilename(const std::string& path, const BTDevice& remoteDevice) noexcept {
    return getFilename(path, remoteDevice.getAdapter().getAddressAndType(), remoteDevice.getAddressAndType());
}

bool SMPKeyBin::remove(const std::string& path, const BTDevice& remoteDevice) {
    return remove(path, remoteDevice.getAdapter().getAddressAndType(), remoteDevice.getAddressAndType());
}

bool SMPKeyBin::write(const std::string& path, const bool overwrite) const noexcept {
    if( !isValid() ) {
        if( verbose ) {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Invalid (skipped) %s\n", toString().c_str());
        }
        return false;
    }
    const std::string fname = getFilename(path);
    const jau::fs::file_stats fname_stat(fname);
    if( fname_stat.exists() ) {
        if( fname_stat.is_file() && overwrite ) {
            if( !remove_impl(fname) ) {
                jau::fprintf_td(stderr, "Write SMPKeyBin: Failed deletion of existing file %s, %s\n", fname_stat.to_string().c_str(), toString().c_str());
                return false;
            }
        } else {
            if( verbose ) {
                jau::fprintf_td(stderr, "Write SMPKeyBin: Not overwriting existing %s, %s\n", fname_stat.to_string().c_str(), toString().c_str());
            }
            return false;
        }
    }
    std::ofstream file(fname, std::ios::out | std::ios::binary);

    if ( !file.good() || !file.is_open() ) {
        jau::fprintf_td(stderr, "Write SMPKeyBin: Failed: File not open %s: %s\n", fname_stat.to_string().c_str(), toString().c_str());
        file.close();
        return false;
    }
    uint8_t buffer[8];

    jau::put_uint16(buffer, 0, version, true /* littleEndian */);
    file.write((char*)buffer, sizeof(version));

    jau::put_uint16(buffer, 0, size, true /* littleEndian */);
    file.write((char*)buffer, sizeof(size));

    jau::put_uint64(buffer, 0, ts_creation_sec, true /* littleEndian */);
    file.write((char*)buffer, sizeof(ts_creation_sec));

    {
        localAddress.address.put(buffer, 0, jau::endian::little);
        file.write((char*)buffer, sizeof(localAddress.address.b));
    }
    file.write((char*)&localAddress.type, sizeof(localAddress.type));
    {
        remoteAddress.address.put(buffer, 0, jau::endian::little);
        file.write((char*)buffer, sizeof(remoteAddress.address.b));
    }
    file.write((char*)&remoteAddress.type, sizeof(remoteAddress.type));
    file.write((char*)&sec_level, sizeof(sec_level));
    file.write((char*)&io_cap, sizeof(io_cap));

    file.write((char*)&keys_init, sizeof(keys_init));
    file.write((char*)&keys_resp, sizeof(keys_resp));

    if( hasLTKInit() ) {
        file.write((char*)&ltk_init, sizeof(ltk_init));
    }
    if( hasIRKInit() ) {
        file.write((char*)&irk_init, sizeof(irk_init));
    }
    if( hasCSRKInit() ) {
        file.write((char*)&csrk_init, sizeof(csrk_init));
    }
    if( hasLKInit() ) {
        file.write((char*)&lk_init, sizeof(lk_init));
    }

    if( hasLTKResp() ) {
        file.write((char*)&ltk_resp, sizeof(ltk_resp));
    }
    if( hasIRKResp() ) {
        file.write((char*)&irk_resp, sizeof(irk_resp));
    }
    if( hasCSRKResp() ) {
        file.write((char*)&csrk_resp, sizeof(csrk_resp));
    }
    if( hasLKResp() ) {
        file.write((char*)&lk_resp, sizeof(lk_resp));
    }

    const bool res = file.good() && file.is_open();
    if( res ) {
        if( verbose ) {
            jau::fprintf_td(stderr, "Write SMPKeyBin: Success: %s: %s\n", fname.c_str(), toString().c_str());
        }
    } else {
        jau::fprintf_td(stderr, "Write SMPKeyBin: Failed: %s: %s\n", fname.c_str(), toString().c_str());
    }
    file.close();
    return res;
}

bool SMPKeyBin::read(const std::string& fname) {
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
    if( !err && 7+7+4 <= remaining ) {
        {
            file.read((char*)buffer, sizeof(localAddress.address.b));
            localAddress.address = jau::EUI48(buffer, jau::endian::little);
        }
        file.read((char*)&localAddress.type, sizeof(localAddress.type));
        {
            file.read((char*)buffer, sizeof(remoteAddress.address.b));
            remoteAddress.address = jau::EUI48(buffer, jau::endian::little);
        }
        file.read((char*)&remoteAddress.type, sizeof(remoteAddress.type));
        file.read((char*)&sec_level, sizeof(sec_level));
        file.read((char*)&io_cap, sizeof(io_cap));

        file.read((char*)&keys_init, sizeof(keys_init));
        file.read((char*)&keys_resp, sizeof(keys_resp));
        remaining -= 7+7+4;
        err = file.fail();
    } else {
        err = true;
    }
    remoteAddress.clearHash();

    if( !err && hasLTKInit() ) {
        if( sizeof(ltk_init) <= remaining ) {
            file.read((char*)&ltk_init, sizeof(ltk_init));
            remaining -= sizeof(ltk_init);
            err = file.fail();
        } else {
            err = true;
        }
    }
    if( !err && hasIRKInit() ) {
        if( sizeof(irk_init) <= remaining ) {
            file.read((char*)&irk_init, sizeof(irk_init));
            remaining -= sizeof(irk_init);
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
    if( !err && hasLKInit() ) {
        if( sizeof(lk_init) <= remaining ) {
            file.read((char*)&lk_init, sizeof(lk_init));
            remaining -= sizeof(lk_init);
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
    if( !err && hasIRKResp() ) {
        if( sizeof(irk_resp) <= remaining ) {
            file.read((char*)&irk_resp, sizeof(irk_resp));
            remaining -= sizeof(irk_resp);
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
    if( !err && hasLKResp() ) {
        if( sizeof(lk_resp) <= remaining ) {
            file.read((char*)&lk_resp, sizeof(lk_resp));
            remaining -= sizeof(lk_resp);
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
        remove_impl( fname );
        if( verbose ) {
            jau::fprintf_td(stderr, "Read SMPKeyBin: Failed %s (removed): %s, remaining %u\n",
                    fname.c_str(), toString().c_str(), remaining);
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

