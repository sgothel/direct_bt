/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021-2026 Gothel Software e.K.
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

#include "BTGattCmd.hpp"

#include "BTDevice.hpp"

using namespace direct_bt;

using namespace jau::fractions_i64_literals;

void BTGattCmd::ResponseCharListener::store(const jau::TROOctets& char_value) {
    const jau::nsize_t rsp_pos = rsp_data.size();
    if( rsp_data.remaining() < char_value.size() ) {
        rsp_data.recapacity( rsp_pos + char_value.size() );
    }
    rsp_data.put_bytes_nc(rsp_pos, char_value.get_ptr(), char_value.size());
    rsp_data.resize(rsp_pos + char_value.size());
}

void BTGattCmd::ResponseCharListener::notificationReceived(BTGattCharRef charDecl,
                          const jau::TROOctets& char_value, const uint64_t timestamp) {
    std::unique_lock<std::mutex> lock(source.mtxRspReceived); // RAII-style acquire and relinquish via destructor
    jau_DBG_PRINT("BTGattCmd::notificationReceived: Resp %s, value[%s]", charDecl->toString(), char_value.toString());
    store(char_value);
    if( nullptr != source.dataCallback ) {
        source.dataCallback(charDecl, char_value, timestamp);
    }
    lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
    source.cvRspReceived.notify_all(); // notify waiting thread
    (void)timestamp;
}

void BTGattCmd::ResponseCharListener::indicationReceived(BTGattCharRef charDecl,
                        const jau::TROOctets& char_value, const uint64_t timestamp,
                        const bool confirmationSent)
{
    std::unique_lock<std::mutex> lock(source.mtxRspReceived); // RAII-style acquire and relinquish via destructor
    jau_DBG_PRINT("BTGattCmd::indicationReceived: Resp %s, value[%s]", charDecl->toString(), char_value.toString());
    store(char_value);
    if( nullptr != source.dataCallback ) {
        source.dataCallback(charDecl, char_value, timestamp);
    }
    lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
    source.cvRspReceived.notify_all(); // notify waiting thread
    (void)charDecl;
    (void)timestamp;
    (void)confirmationSent;
}

bool BTGattCmd::isConnected() const noexcept { return dev.getConnected(); }

HCIStatusCode BTGattCmd::setup() noexcept {
    if( setup_done ) {
        return isResolvedEq() ? HCIStatusCode::SUCCESS : HCIStatusCode::NOT_SUPPORTED;
    }
    setup_done = true;
    cmdCharRef = nullptr != service_uuid ? dev.findGattChar(*service_uuid, *cmd_uuid)
                                         : dev.findGattChar(*cmd_uuid);
    if( nullptr == cmdCharRef ) {
        if( verbose ) {
            jau_INFO_PRINT("Command not found: service %s, char %s", srvUUIDStr(), cmd_uuid->toString());
        }
        return HCIStatusCode::NOT_SUPPORTED;
    }
    if( !cmdCharRef->hasProperties(BTGattChar::PropertyBitVal::WriteNoAck) &&
        !cmdCharRef->hasProperties(BTGattChar::PropertyBitVal::WriteWithAck) ) {
        if( verbose ) {
            jau_INFO_PRINT("Command has no write property: %s", cmdCharRef->toString());
        }
        cmdCharRef = nullptr;
        return HCIStatusCode::NOT_SUPPORTED;
    }

    if( nullptr != rsp_uuid ) {
        rspCharRef = nullptr != service_uuid ? dev.findGattChar(*service_uuid, *rsp_uuid)
                                             : dev.findGattChar(*rsp_uuid);
        if( nullptr == rspCharRef ) {
            if( verbose ) {
                jau_INFO_PRINT("Response not found: service %s, char %s", srvUUIDStr(), rsp_uuid->toString());
            }
            cmdCharRef = nullptr;
            return HCIStatusCode::NOT_SUPPORTED;
        }
        try {
            bool cccdEnableResult[2];
            bool cccdRet = rspCharRef->addCharListener( rspCharListener, cccdEnableResult );
            if( cccdRet ) {
                return HCIStatusCode::SUCCESS;
            } else {
                if( verbose ) {
                    jau_INFO_PRINT("CCCD Notify/Indicate not supported on response %s", rspCharRef->toString());
                }
                cmdCharRef = nullptr;
                rspCharRef = nullptr;
                return HCIStatusCode::NOT_SUPPORTED;
            }
        } catch ( std::exception & e ) {
            jau_ERR_PRINT("Exception caught for %s: %s\n", e.what(), toString());
            cmdCharRef = nullptr;
            rspCharRef = nullptr;
            return HCIStatusCode::TIMEOUT;
        }
    } else {
        return HCIStatusCode::SUCCESS;
    }
}

HCIStatusCode BTGattCmd::close() noexcept {
    std::unique_lock<std::mutex> lockCmd(mtxCommand); // RAII-style acquire and relinquish via destructor
    const bool wasResolved = isResolvedEq();
    BTGattCharRef rspCharRefCopy = rspCharRef;
    cmdCharRef = nullptr;
    rspCharRef = nullptr;
    if( !setup_done ) {
        return HCIStatusCode::SUCCESS;
    }
    setup_done = false;
    if( !wasResolved ) {
        return HCIStatusCode::SUCCESS;
    }
    if( !isConnected() ) {
        return HCIStatusCode::DISCONNECTED;
    }
    if( nullptr != rspCharRefCopy ) {
        try {
            const bool res1 = rspCharRefCopy->removeCharListener(rspCharListener);
            const bool res2 = rspCharRefCopy->disableIndicationNotification();
            if( res1 && res2 ) {
                return HCIStatusCode::SUCCESS;
            } else {
                return HCIStatusCode::FAILED;
            }
        } catch ( std::exception & e ) {
            jau_ERR_PRINT("Exception caught for %s: %s\n", e.what(), toString());
            return HCIStatusCode::TIMEOUT;
        }
    } else {
        return HCIStatusCode::SUCCESS;
    }
}

bool BTGattCmd::isResolved() noexcept {
    std::unique_lock<std::mutex> lockCmd(mtxCommand); // RAII-style acquire and relinquish via destructor
    if( !setup_done ) {
        return HCIStatusCode::SUCCESS == setup();
    } else {
        return isResolvedEq();
    }
}

HCIStatusCode BTGattCmd::send(const bool prefNoAck, const jau::TROOctets& cmd_data, const jau::fraction_i64& timeout) noexcept {
    return sendImpl(prefNoAck, cmd_data, timeout, true);
}
HCIStatusCode BTGattCmd::sendOnly(const bool prefNoAck, const jau::TROOctets& cmd_data) noexcept {
    return sendImpl(prefNoAck, cmd_data, 0_s, false);
}
HCIStatusCode BTGattCmd::sendImpl(const bool prefNoAck, const jau::TROOctets& cmd_data, const jau::fraction_i64& timeout, bool allowResponse) noexcept {
    std::unique_lock<std::mutex> lockCmd(mtxCommand); // RAII-style acquire and relinquish via destructor
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    if( !isConnected() ) {
        return HCIStatusCode::DISCONNECTED;
    } else {
        std::unique_lock<std::mutex> lockRsp(mtxRspReceived); // RAII-style acquire and relinquish via destructor

        res = setup();
        if( HCIStatusCode::SUCCESS != res ) {
            return res;
        }
        rsp_data.clear();

        jau_DBG_PRINT("BTGattCmd::sendBlocking: Start: Cmd %s, args[%s], Resp %s, result[%s]",
                  cmdCharRef->toString(), cmd_data, rspCharStr(), rsp_data.toString());

        const bool hasWriteNoAck = cmdCharRef->hasProperties(BTGattChar::PropertyBitVal::WriteNoAck);
        const bool hasWriteWithAck = cmdCharRef->hasProperties(BTGattChar::PropertyBitVal::WriteWithAck);
        // Prefer WriteNoAck, if hasWriteNoAck and ( prefNoAck -or- !hasWriteWithAck )
        const bool prefWriteNoAck = hasWriteNoAck && ( prefNoAck || !hasWriteWithAck );

        if( prefWriteNoAck ) {
            try {
                if( !cmdCharRef->writeValueNoResp(cmd_data) ) {
                    jau_ERR_PRINT("Write (noAck) to command failed: Cmd %s, args[%s]", cmdCharRef->toString(), cmd_data.toString());
                    res = HCIStatusCode::FAILED;
                }
            } catch ( std::exception & e ) {
                jau_ERR_PRINT("Exception caught @ Write (noAck) to command failed: Cmd %s, args[%s]: %s",
                        cmdCharRef->toString(), cmd_data.toString(), e.what());
                res = HCIStatusCode::TIMEOUT;
            }
        } else if( hasWriteWithAck ) {
            try {
                if( !cmdCharRef->writeValue(cmd_data) ) {
                    jau_ERR_PRINT("Write (withAck) to command failed: Cmd %s, args[%s]", cmdCharRef->toString(), cmd_data);
                    res = HCIStatusCode::TIMEOUT;
                }
            } catch ( std::exception & e ) {
                jau_ERR_PRINT("Exception caught @ Write (withAck) to command failed: Cmd %s, args[%s]: %s",
                        cmdCharRef->toString(), cmd_data.toString(), e.what());
                res = HCIStatusCode::TIMEOUT;
            }
        } else {
            jau_ERR_PRINT("Command has no write property: %s: %s", cmdCharRef->toString(), toString());
            res = HCIStatusCode::FAILED;
        }

        if( nullptr != rspCharRef && allowResponse ) {
            const jau::fraction_timespec timeout_time = jau::getMonotonicTime() + jau::fraction_timespec(timeout);
            while( HCIStatusCode::SUCCESS == res &&
                   ( rspMinSize > rsp_data.size() || ( 0 == rspMinSize && 0 == rsp_data.size() ) )
                 )
            {
                if( jau::fractions_i64::zero == timeout ) {
                    cvRspReceived.wait(lockRsp);
                } else {
                    std::cv_status s = wait_until(cvRspReceived, lockRsp, timeout_time);
                    if( std::cv_status::timeout == s && 0 == rsp_data.size() ) {
                        jau_ERR_PRINT("BTGattCmd::sendBlocking: Timeout: Cmd %s, args[%s]",
                                  cmdCharRef->toString(), cmd_data.toString());
                        res = HCIStatusCode::TIMEOUT;
                    }
                }
            }
        }
    } // mtxRspReceived
    if( HCIStatusCode::SUCCESS == res ) {
        jau_DBG_PRINT("BTGattCmd::sendBlocking: OK: Cmd %s, args[%s], Resp %s, result[%s]",
                  cmdCharRef->toString(), cmd_data, rspCharStr(), rsp_data);
    }
    return res;
}

std::string BTGattCmd::toString() const noexcept {
    return "BTGattCmd["+dev.getName()+":"+name+", service "+srvUUIDStr()+
           ", char[cmd "+cmd_uuid->toString()+", rsp "+rspUUIDStr()+
           ", set["+std::to_string(setup_done)+", resolved "+std::to_string(isResolvedEq())+"]]]";
}
