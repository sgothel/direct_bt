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
#include <filesystem>

#include "SMPKeyBin.hpp"

namespace fs = std::filesystem;

using namespace direct_bt;

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
           "], valid "+std::to_string( isValid() )+"]";
    return res;
}

bool SMPKeyBin::remove(const std::string& path, const std::string& basename) {
    const fs::path fname = path+"/"+basename;
    return fs::remove(fname);
}

bool SMPKeyBin::write(const std::string& path, const std::string& basename) const noexcept {
    if( !isValid() ) {
        if( verbose ) {
            fprintf(stderr, "****** WRITE SMPKeyBin: Invalid (skipped) %s\n", toString().c_str());
        }
        return false;
    }
    const std::string fname = path+"/"+basename;
    std::ofstream file(fname, std::ios::binary | std::ios::trunc);
    uint8_t buffer[2];

    jau::put_uint16(buffer, 0, version, true /* littleEndian */);
    file.write((char*)buffer, sizeof(version));

    jau::put_uint16(buffer, 0, size, true /* littleEndian */);
    file.write((char*)buffer, sizeof(size));

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

    file.close();
    if( verbose ) {
        fprintf(stderr, "****** WRITE SMPKeyBin: %s: %s\n", fname.c_str(), toString().c_str());
    }
    return true;
}

bool SMPKeyBin::read(const std::string& path, const std::string& basename) {
    const std::string fname = path+"/"+basename;
    std::ifstream file(fname, std::ios::binary);
    if (!file.is_open() ) {
        if( verbose ) {
            fprintf(stderr, "****** READ SMPKeyBin failed: %s\n", fname.c_str());
        }
        return false;
    }
    bool err = false;
    uint8_t buffer[2];

    file.read((char*)buffer, sizeof(version));
    version = jau::get_uint16(buffer, 0, true /* littleEndian */);
    err = file.fail();

    if( !err ) {
        file.read((char*)buffer, sizeof(size));
        size = jau::get_uint16(buffer, 0, true /* littleEndian */);
        err = file.fail();
    }
    uint16_t remaining = size - sizeof(version) - sizeof(size);

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

    file.close();
    if( verbose ) {
        fprintf(stderr, "****** READ SMPKeyBin: %s: %s, remaining %u\n",
                fname.c_str(), toString().c_str(), remaining);
    }
    return isValid();
}

HCIStatusCode SMPKeyBin::apply(BTDevice & device) const noexcept {
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    if( !isValid() || ( !hasLTKInit() && !hasLTKResp() ) ) {
        res = HCIStatusCode::INVALID_PARAMS;
        if( verbose ) {
            fprintf(stderr, "****** APPLY SMPKeyBin failed: SMPKeyBin Status: %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str());
        }
        return res;
    }
    if( !device.isValid() ) {
        res = HCIStatusCode::INVALID_PARAMS;
        if( verbose ) {
            fprintf(stderr, "****** APPLY SMPKeyBin failed: Device Invalid: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
        return res;
    }

    if( !device.setConnSecurity(BTSecurityLevel::ENC_ONLY, SMPIOCapability::NO_INPUT_NO_OUTPUT) ) {
        res = HCIStatusCode::CONNECTION_ALREADY_EXISTS;
        if( verbose ) {
            fprintf(stderr, "****** APPLY SMPKeyBin failed: Device Connected/ing: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
        return res;
    }

    if( hasLTKInit() ) {
        res = device.setLongTermKeyInfo( getLTKInit() );
        if( HCIStatusCode::SUCCESS != res && verbose ) {
            fprintf(stderr, "****** APPLY SMPKeyBin failed: Init-LTK Upload: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
    }

    if( HCIStatusCode::SUCCESS == res && hasLTKResp() ) {
        res = device.setLongTermKeyInfo( getLTKResp() );
        if( HCIStatusCode::SUCCESS != res && verbose ) {
            fprintf(stderr, "****** APPLY SMPKeyBin failed: Resp-LTK Upload: %s, %s, %s\n",
                    to_string(res).c_str(), this->toString().c_str(), device.toString().c_str());
        }
    }

    return res;
}

