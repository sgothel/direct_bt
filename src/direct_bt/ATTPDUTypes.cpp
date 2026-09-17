/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020-2026 Gothel Software e.K.
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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include <jau/debug.hpp>

#include "ATTPDUTypes.hpp"

using namespace direct_bt;

namespace direct_bt {
    JAU_MAKE_ENUM_STRING2_CODE(AttPDUMsg::Opcode, Opcode, PDU_UNDEFINED, ERROR_RSP, EXCHANGE_MTU_REQ, EXCHANGE_MTU_RSP, \
        FIND_INFORMATION_REQ, FIND_INFORMATION_RSP, FIND_BY_TYPE_VALUE_REQ, FIND_BY_TYPE_VALUE_RSP, READ_BY_TYPE_REQ, \
        READ_BY_TYPE_RSP, READ_REQ, READ_RSP, READ_BLOB_REQ, READ_BLOB_RSP, READ_MULTIPLE_REQ, READ_MULTIPLE_RSP, \
        READ_BY_GROUP_TYPE_REQ, READ_BY_GROUP_TYPE_RSP, WRITE_REQ, WRITE_RSP, WRITE_CMD, PREPARE_WRITE_REQ, \
        PREPARE_WRITE_RSP, EXECUTE_WRITE_REQ, EXECUTE_WRITE_RSP, READ_MULTIPLE_VARIABLE_REQ, \
        READ_MULTIPLE_VARIABLE_RSP, MULTIPLE_HANDLE_VALUE_NTF, HANDLE_VALUE_NTF, HANDLE_VALUE_IND, HANDLE_VALUE_CFM, SIGNED_WRITE_CMD);
}

std::string AttPDUMsg::to_string(const Opcode opc) noexcept {
    return direct_bt::to_string(opc);
}
std::string_view AttPDUMsg::name(const Opcode opc) noexcept {
    return direct_bt::name(opc);
}

std::string AttPDUMsg::baseString() const noexcept {
    return jau_format_string("opcode=%#x %s, size[total %zu, param %zu]",
        number(getOpcode()), name(getOpcode()), pdu.size(), getPDUParamSize());
}

std::string AttPDUMsg::valueString() const noexcept {
    return jau_format_string("size %zu, data %s", getPDUValueSize(),
        jau::toHexString(pdu.get_ptr()+getPDUValueOffset(), getPDUValueSize(), jau::lb_endian_t::little));
}
std::string AttPDUMsg::toString() const noexcept {
    return jau_format_string("%s[%s, value[%s]]", getName(), baseString(), valueString());
}

std::string AttErrorRsp::valueString() const noexcept {
    const Opcode opc = getCausingOpcode();
    const ErrorCode ec = getErrorCode();
    return jau_format_string("error %#x: %s, cause(opc %#x: %s, handle %#x)",
        *ec, getErrorCodeString(ec), *opc, opc, getCausingHandle());
}

std::string AttErrorRsp::getErrorCodeString(const ErrorCode errorCode) noexcept {
    switch(errorCode) {
        case ErrorCode::NO_ERROR: return "No error";
        case ErrorCode::INVALID_HANDLE: return "Invalid Handle";
        case ErrorCode::NO_READ_PERM: return "Read Not Permitted";
        case ErrorCode::NO_WRITE_PERM: return "Write Not Permitted";
        case ErrorCode::INVALID_PDU: return "Invalid PDU";
        case ErrorCode::INSUFF_AUTHENTICATION: return "Insufficient Authentication";
        case ErrorCode::UNSUPPORTED_REQUEST: return "Request Not Supported";
        case ErrorCode::INVALID_OFFSET: return "Invalid Offset";
        case ErrorCode::INSUFF_AUTHORIZATION: return "Insufficient Authorization";
        case ErrorCode::PREPARE_QUEUE_FULL: return "Prepare Queue Full";
        case ErrorCode::ATTRIBUTE_NOT_FOUND: return "Attribute Not Found";
        case ErrorCode::ATTRIBUTE_NOT_LONG: return "Attribute Not Long";
        case ErrorCode::INSUFF_ENCRYPTION_KEY_SIZE: return "Insufficient Encryption Key Size";
        case ErrorCode::INVALID_ATTRIBUTE_VALUE_LEN: return "Invalid Attribute Value Length";
        case ErrorCode::UNLIKELY_ERROR: return "Unlikely Error";
        case ErrorCode::INSUFF_ENCRYPTION: return "Insufficient Encryption";
        case ErrorCode::UNSUPPORTED_GROUP_TYPE: return "Unsupported Group Type";
        case ErrorCode::INSUFFICIENT_RESOURCES: return "Insufficient Resources";
        case ErrorCode::DB_OUT_OF_SYNC: return "Database Out Of Sync";
        case ErrorCode::FORBIDDEN_VALUE: return "Value Not Allowed";
        default: ; // fall through intended
    }
    if( 0x80 <= number(errorCode) && number(errorCode) <= 0x9F ) {
        return "Application Error";
    }
    if( 0xE0 <= number(errorCode) /* && number(errorCode) <= 0xFF */ ) {
        return "Common Profile and Services Error";
    }
    return "Error Reserved for future use";
}

std::string AttExchangeMTU::valueString() const noexcept {
    return jau_format_string("mtu %u", getMTUSize());
}

std::string AttReadReq::valueString() const noexcept {
    return jau_format_string("handle %#x", getHandle());
}

std::string AttReadBlobReq::valueString() const noexcept {
    return jau_format_string("handle %#x, offset %u", getHandle(), getValueOffset());
}

std::string AttReadNRsp::valueString() const noexcept {
    return jau_format_string("size %zu, data %s", getPDUValueSize(), view.toString());
}

std::string AttWriteReq::valueString() const noexcept {
    return jau_format_string("handle %#x, data %s", getHandle(), view.toString());
}

std::string AttWriteCmd::valueString() const noexcept {
    return jau_format_string("handle %#x, data %s", getHandle(), view.toString());
}

std::string AttPrepWrite::valueString() const noexcept {
    return jau_format_string("handle %#x, offset %u, data %s", getHandle(), getValueOffset(), view.toString());
}

std::string AttHandleValueRcv::valueString() const noexcept {
    return jau_format_string("handle %#x, size %zu, data %s", getHandle(), getPDUValueSize(), view.toString());
}

std::string AttElementList::valueString() const noexcept {
    const jau::nsize_t count = getElementCount();
    std::string res = jau_format_string("size %zu, %selements[count %zu, size [total %zu, value %zu]: ",
        getPDUValueSize(), addValueString(), count, getElementSize(), getElementValueSize());
    for(jau::nsize_t i=0; i<count; i++) {
        jau_append_string(res, "%zu[%s],", i, elementString(i));
    }
    jau_append_string(res, "]");
    return res;
}

void AttElementList::setElementCount(const jau::nsize_t count) {
    const jau::nsize_t element_length = getElementSize();
    const jau::nsize_t new_size = getPDUValueOffset() + element_length * count;
    if( pdu.size() < new_size ) {
        const std::string m = jau_format_string("%s: %zu + element[len %zu * count %zu > pdu %zu",
            getName(), getPDUValueOffset(), element_length, count, pdu.size());
        throw jau::IllegalArgumentError(m, E_FILE_LINE);
    }
    resize( new_size );
    if( getPDUValueSize() % getElementSize() != 0 ) {
        const std::string m = jau_format_string("%s: Invalid packet size: pdu-value-size %zu not multiple of element-size %zu",
            getName(), getPDUValueSize(), getElementSize());
        throw AttValueException(m, E_FILE_LINE);
    }
}

std::string AttReadByNTypeReq::valueString() const noexcept {
    return jau_format_string("handle [%#x..%#x], uuid %s", getStartHandle(), getEndHandle(), getNType()->toString());
}

std::string AttReadByGroupTypeRsp::elementString(const jau::nsize_t idx) const {
    Element e = getElement(idx);
    return "handle ["+jau::toHexString(e.getStartHandle())+".."+jau::toHexString(e.getEndHandle())+
           "], data "+jau::toHexString(e.getValuePtr(), e.getValueSize(), jau::lb_endian_t::little);
}

std::string AttFindInfoReq::valueString() const noexcept {
    return jau_format_string("handle [%#x..%#x]", getStartHandle(), getEndHandle());
}

std::string AttFindInfoRsp::addValueString() const noexcept {
    return jau_format_string("format %u, ", pdu.get_uint8_nc(1));
}

std::string AttFindInfoRsp::elementString(const jau::nsize_t idx) const {
    Element e = getElement(idx);
    return "handle "+jau::toHexString(e.handle)+
           ", uuid "+e.uuid->toString();
}

std::string AttFindByTypeValueReq::valueString() const noexcept {
    return jau_format_string("handle [%#x..%#x], type %s, value %s",
        getStartHandle(), getEndHandle(), getAttType(), getAttValue()->toString());
}

std::string AttFindByTypeValueRsp::elementString(const jau::nsize_t idx) const {
    return jau_format_string("handle[%#x..%#x]", getElementHandle(idx), getElementHandleEnd(idx));
}

std::string AttFindByTypeValueRsp::valueString() const noexcept {
    const jau::nsize_t count = getElementCount();
    std::string res = jau_format_string("size %zu, elements[count %zu, size %zu: ",
        getPDUValueSize(), count, getElementSize());
    for(jau::nsize_t i=0; i<count; i++) {
        jau_append_string(res, "%zu[%s], ", i, elementString(i));
    }
    jau_append_string(res, "]");
    return res;
}

std::unique_ptr<const AttPDUMsg> AttPDUMsg::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const AttPDUMsg::Opcode opc = static_cast<AttPDUMsg::Opcode>(*buffer);
    switch( opc ) {
        case Opcode::PDU_UNDEFINED:                 return std::make_unique<AttPDUUndefined>(buffer, buffer_size);
        case Opcode::ERROR_RSP:                     return std::make_unique<AttErrorRsp>(buffer, buffer_size);
        case Opcode::EXCHANGE_MTU_REQ:              return std::make_unique<AttExchangeMTU>(buffer, buffer_size);
        case Opcode::EXCHANGE_MTU_RSP:              return std::make_unique<AttExchangeMTU>(buffer, buffer_size);
        case Opcode::FIND_INFORMATION_REQ:          return std::make_unique<AttFindInfoReq>(buffer, buffer_size);
        case Opcode::FIND_INFORMATION_RSP:          return std::make_unique<AttFindInfoRsp>(buffer, buffer_size);
        case Opcode::FIND_BY_TYPE_VALUE_REQ:        return std::make_unique<AttFindByTypeValueReq>(buffer, buffer_size);
        case Opcode::FIND_BY_TYPE_VALUE_RSP:        return std::make_unique<AttFindByTypeValueRsp>(buffer, buffer_size);
        case Opcode::READ_BY_TYPE_REQ:              return std::make_unique<AttReadByNTypeReq>(buffer, buffer_size);
        case Opcode::READ_BY_TYPE_RSP:              return std::make_unique<AttReadByTypeRsp>(buffer, buffer_size);
        case Opcode::READ_REQ:                      return std::make_unique<AttReadReq>(buffer, buffer_size);
        case Opcode::READ_RSP:                      return std::make_unique<AttReadNRsp>(buffer, buffer_size);
        case Opcode::READ_BLOB_REQ:                 return std::make_unique<AttReadBlobReq>(buffer, buffer_size);
        case Opcode::READ_BLOB_RSP:                 return std::make_unique<AttReadNRsp>(buffer, buffer_size);
        case Opcode::READ_MULTIPLE_REQ:             return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        case Opcode::READ_MULTIPLE_RSP:             return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        case Opcode::READ_BY_GROUP_TYPE_REQ:        return std::make_unique<AttReadByNTypeReq>(buffer, buffer_size);
        case Opcode::READ_BY_GROUP_TYPE_RSP:        return std::make_unique<AttReadByGroupTypeRsp>(buffer, buffer_size);
        case Opcode::WRITE_REQ:                     return std::make_unique<AttWriteReq>(buffer, buffer_size);
        case Opcode::WRITE_RSP:                     return std::make_unique<AttWriteRsp>(buffer, buffer_size);
        case Opcode::WRITE_CMD:                     return std::make_unique<AttWriteCmd>(buffer, buffer_size);
        case Opcode::PREPARE_WRITE_REQ:             return std::make_unique<AttPrepWrite>(buffer, buffer_size);
        case Opcode::PREPARE_WRITE_RSP:             return std::make_unique<AttPrepWrite>(buffer, buffer_size);
        case Opcode::EXECUTE_WRITE_REQ:             return std::make_unique<AttExeWriteReq>(buffer, buffer_size);
        case Opcode::EXECUTE_WRITE_RSP:             return std::make_unique<AttExeWriteRsp>(buffer, buffer_size);
        case Opcode::READ_MULTIPLE_VARIABLE_REQ:    return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        case Opcode::READ_MULTIPLE_VARIABLE_RSP:    return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        case Opcode::MULTIPLE_HANDLE_VALUE_NTF:     return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        case Opcode::HANDLE_VALUE_NTF:              return std::make_unique<AttHandleValueRcv>(buffer, buffer_size);
        case Opcode::HANDLE_VALUE_IND:              return std::make_unique<AttHandleValueRcv>(buffer, buffer_size);
        case Opcode::HANDLE_VALUE_CFM:              return std::make_unique<AttHandleValueCfm>(buffer, buffer_size);
        case Opcode::SIGNED_WRITE_CMD:              return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size); // TODO
        default:                                    return std::make_unique<AttPDUHeapMsg>(buffer, buffer_size);
    }
}
