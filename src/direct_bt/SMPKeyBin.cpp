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
#include <limits>
#include <string>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>

#include <jau/io/file_util.hpp>

#include "SMPKeyBin.hpp"

#include "BTDevice.hpp"
#include "BTAdapter.hpp"
#include "jau/string_util.hpp"

using namespace direct_bt;

static std::vector<std::string> get_file_list(const std::string& dname) {
    std::vector<std::string> res;
    const jau::io::fs::consume_dir_item cs = jau::bind_capref(&res,
            ( bool(*)(std::vector<std::string>*, const jau::io::fs::dir_item&) ) /* help template type deduction of function-ptr */
                ( [](std::vector<std::string>* receiver, const jau::io::fs::dir_item& item) -> bool {
                    if( item.basename().starts_with("bd_") ) { // prefix checl
                        const jau::nsize_t suffix_pos = item.basename().size() - 4;
                        if( suffix_pos == item.basename().find(".key", suffix_pos) ) { // suffix check
                            receiver->push_back( item.path() ); // full path
                        }
                    }
                    return true;
                  } )
        );
    jau::io::fs::get_dir_content(dname, cs);
    return res;
}

bool SMPKeyBin::remove_impl(const std::string& fname) {
    return jau::io::fs::remove( fname );
}

SMPKeyBin SMPKeyBin::create(const BTDevice& device) {
    const BTSecurityLevel sec_lvl = device.getConnSecurityLevel();
    const SMPPairingState pstate = device.getPairingState();
    const PairingMode pmode = device.getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)

    SMPKeyBin smpKeyBin(device.getAdapter().getRole(), device.getAdapter().getAddressAndType(), device.getAddressAndType(),
                        device.getConnSecurityLevel(), device.getConnIOCapability());

    if( ( BTSecurityLevel::NONE  < sec_lvl && SMPPairingState::COMPLETED == pstate && PairingMode::NEGOTIATING < pmode ) ||
        ( BTSecurityLevel::NONE == sec_lvl && SMPPairingState::NONE == pstate && PairingMode::NONE == pmode  ) )
    {
        const SMPKeyType keys_resp = device.getAvailableSMPKeys(true /* responder */);
        const SMPKeyType keys_init = device.getAvailableSMPKeys(false /* responder */);

        if( is_set(keys_init, SMPKeyType::ENC_KEY) ) {
            smpKeyBin.setLTKInit( device.getLongTermKey(false /* responder */) );
        }
        if( is_set(keys_resp, SMPKeyType::ENC_KEY) ) {
            smpKeyBin.setLTKResp( device.getLongTermKey(true  /* responder */) );
        }

        if( is_set(keys_init, SMPKeyType::ID_KEY) ) {
            smpKeyBin.setIRKInit( device.getIdentityResolvingKey(false /* responder */) );
        }
        if( is_set(keys_resp, SMPKeyType::ID_KEY) ) {
            smpKeyBin.setIRKResp( device.getIdentityResolvingKey(true  /* responder */) );
        }

        if( is_set(keys_init, SMPKeyType::SIGN_KEY) ) {
            smpKeyBin.setCSRKInit( device.getSignatureResolvingKey(false /* responder */) );
        }
        if( is_set(keys_resp, SMPKeyType::SIGN_KEY) ) {
            smpKeyBin.setCSRKResp( device.getSignatureResolvingKey(true  /* responder */) );
        }

        if( is_set(keys_init, SMPKeyType::LINK_KEY) ) {
            smpKeyBin.setLKInit( device.getLinkKey(false /* responder */) );
        }
        if( is_set(keys_resp, SMPKeyType::LINK_KEY) ) {
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
            jau_fprintf_td(stderr, "Create SMPKeyBin: Invalid %s, %s\n", smpKeyBin.toString(), device.toString());
        }
        return false;
    }
}

std::vector<SMPKeyBin> SMPKeyBin::readAll(const std::string& dname, const bool verbose_) {
    std::vector<SMPKeyBin> res;
    std::vector<std::string> fnames = get_file_list(dname);
    for(const std::string& fname : fnames) {
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
    for(const SMPKeyBin& f : all) {
        if( localAddress == f.getLocalAddrAndType() ) {
            res.push_back(f);
        }
    }
    return res;
}

std::string SMPKeyBin::toString() const noexcept {
    std::string res = jau_format_string("SMPKeyBin[local[%s, %s], remote %s, SC %s, sec %s, io %s, ",
        localRole, localAddress, remoteAddress, uses_SC(), sec_level, io_cap);

    if( isVersionValid() ) {
        bool comma = false;
        jau::append_string(res, "Init[");
        if( hasLTKInit() ) {
            jau::append_string(res, ltk_init.toString());
            comma = true;
        }
        if( hasIRKInit() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, irk_init.toString());
            comma = true;
        }
        if( hasCSRKInit() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, csrk_init.toString());
            comma = true;
        }
        // NOLINTBEGIN(clang-analyzer-deadcode.DeadStores)
        if( hasLKInit() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, lk_init.toString());
            comma = true;
        }
        comma = false;
        jau::append_string(res, "], Resp[");
        if( hasLTKResp() ) {
            jau::append_string(res, ltk_resp.toString());
            comma = true;
        }
        if( hasIRKResp() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, irk_resp.toString());
            comma = true;
        }
        if( hasCSRKResp() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, csrk_resp.toString());
            comma = true;
        }
        if( hasLKResp() ) {
            if( comma ) {
                jau::append_string(res, ", ");
            }
            jau::append_string(res, lk_resp.toString());
            comma = true;
        }
        jau::append_string(res, "], ");
        // NOLINTEND(clang-analyzer-deadcode.DeadStores)
    }
    jau_append_string(res, "ver[%#x, ok %s], size[%zu", version, isVersionValid(), size);
    if( verbose ) {
        jau_append_string(res, ", calc %u", calcSize());
    }
    jau_append_string(res, ", valid %s], ", isSizeValid());
    {
        jau::fraction_timespec t0( (int64_t) std::min<uint64_t>(ts_creation_sec, std::numeric_limits<int64_t>::max()), 0 );
        jau::append_string(res, t0.toISO8601String());
    }
    jau_append_string(res, ", valid %s]", isValid());
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
            jau_fprintf_td(stderr, "Write SMPKeyBin: Invalid (skipped) %s\n", toString());
        }
        return false;
    }
    const std::string fname = getFilename(path);
    const jau::io::fs::file_stats fname_stat(fname);
    if( fname_stat.exists() ) {
        if( fname_stat.is_file() && overwrite ) {
            if( !remove_impl(fname) ) {
                jau_fprintf_td(stderr, "Write SMPKeyBin: Failed deletion of existing file %s, %s\n", fname_stat.toString(), toString());
                return false;
            }
        } else {
            if( verbose ) {
                jau_fprintf_td(stderr, "Write SMPKeyBin: Not overwriting existing %s, %s\n", fname_stat.toString(), toString());
            }
            return false;
        }
    }
    std::ofstream file(fname, std::ios::out | std::ios::binary);

    if ( !file.good() || !file.is_open() ) {
        jau_fprintf_td(stderr, "Write SMPKeyBin: Failed: File not open %s: %s\n", fname_stat.toString(), toString());
        file.close();
        return false;
    }
    uint8_t buffer[8];

    jau::put_uint16(buffer, version, jau::lb_endian_t::little);
    file.write((char*)buffer, sizeof(version));

    jau::put_uint16(buffer, size, jau::lb_endian_t::little);
    file.write((char*)buffer, sizeof(size));

    jau::put_uint64(buffer, ts_creation_sec, jau::lb_endian_t::little);
    file.write((char*)buffer, sizeof(ts_creation_sec));

    file.write((char*)&localRole, sizeof(localRole));
    {
        localAddress.address.put(buffer, jau::lb_endian_t::little);
        file.write((char*)buffer, sizeof(localAddress.address.b));
    }
    file.write((char*)&localAddress.type, sizeof(localAddress.type));
    {
        remoteAddress.address.put(buffer, jau::lb_endian_t::little);
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
            jau_fprintf_td(stderr, "Write SMPKeyBin: Success: %s: %s\n", fname, toString());
        }
    } else {
        jau_fprintf_td(stderr, "Write SMPKeyBin: Failed: %s: %s\n", fname, toString());
    }
    file.close();
    return res;
}

bool SMPKeyBin::read(const std::string& fname) {
    std::ifstream file(fname, std::ios::binary);
    if ( !file.is_open() ) {
        if( verbose ) {
            jau_fprintf_td(stderr, "Read SMPKeyBin failed: %s\n", fname);
        }
        size = 0; // explicitly mark invalid
        return false;
    }
    bool err = false;
    uint8_t buffer[8];

    file.read((char*)buffer, sizeof(version));
    version = jau::get_uint16(buffer, jau::lb_endian_t::little);
    err = file.fail();

    if( !err ) {
        file.read((char*)buffer, sizeof(size));
        size = jau::get_uint16(buffer, jau::lb_endian_t::little);
        err = file.fail();
    }
    uint16_t remaining = size - sizeof(version) - sizeof(size);

    if( !err && 8 <= remaining ) {
        file.read((char*)buffer, sizeof(ts_creation_sec));
        ts_creation_sec = jau::get_uint64(buffer, jau::lb_endian_t::little);
        remaining -= 8;
        err = file.fail();
    } else {
        err = true;
    }
    if( !err && 7+7+4 <= remaining ) {
        file.read((char*)&localRole, sizeof(localRole));
        {
            file.read((char*)buffer, sizeof(localAddress.address.b));
            localAddress.address = jau::io::net::EUI48(buffer, jau::lb_endian_t::little);
        }
        file.read((char*)&localAddress.type, sizeof(localAddress.type));
        {
            file.read((char*)buffer, sizeof(remoteAddress.address.b));
            remoteAddress.address = jau::io::net::EUI48(buffer, jau::lb_endian_t::little);
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
            jau_fprintf_td(stderr, "Read SMPKeyBin: Failed %s (removed): %s, remaining %u\n", fname, toString(), remaining);
        }
        size = 0; // explicitly mark invalid
    } else {
        if( verbose ) {
            jau_fprintf_td(stderr, "Read SMPKeyBin: OK %s: %s, remaining %u\n", fname, toString(), remaining);
        }
    }
    return err;
}

