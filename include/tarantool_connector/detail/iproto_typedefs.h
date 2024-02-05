//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
#define TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H

#include <cstdint>

#include <msgpack.hpp>

#include "tntpp_defines.h"

namespace tntpp::detail::iproto
{

/// iproto format details can be found in the tarantool documentation
/// https://www.tarantool.io/en/doc/latest/dev_guide/internals/iproto

using MpUint = std::uint64_t;

using OperationId = MpUint;
/// IProto message cannot be more that uint32::max() bytes long according to spec
inline static const unsigned char size_tag = 0xce;
using SizeType = std::uint32_t;

enum class RequestType : MpUint
{
  Ok = 0x00,
  Chunk = 0x80,
  //  TypeError =

  Select = 0x01,
  Insert = 0x02,
  Replace = 0x03,
  Update = 0x04,
  Delete = 0x05,
  Auth = 0x07,
  Eval = 0x08,
  Upsert = 0x09,
  Call = 0x0a,
  Nop = 0x0c,
  Ping = 0x40,
  Id = 0x49,

  Unknown = std::numeric_limits<MpUint>::max()
};

std::string req_type_to_str(RequestType op_type)
{
  switch (op_type) {
    case RequestType::Ok:
      return "Ok";
    case RequestType::Chunk:
      return "Chunk";
    case RequestType::Select:
      return "Select";
    case RequestType::Insert:
      return "Insert";
    case RequestType::Replace:
      return "Replace";
    case RequestType::Update:
      return "Update";
    case RequestType::Delete:
      return "Delete";
    case RequestType::Auth:
      return "Auth";
    case RequestType::Eval:
      return "Eval";
    case RequestType::Upsert:
      return "Upsert";
    case RequestType::Call:
      return "Call";
    case RequestType::Nop:
      return "Nop";
    case RequestType::Ping:
      return "Ping";
    case RequestType::Id:
      return "Id";
    case RequestType::Unknown:
      return "Unknown";
  }
  TNTPP_UNREACHABLE;
};

RequestType int_to_req_type(MpUint type)
{
  switch (static_cast<RequestType>(type)) {
    case RequestType::Ok:
    case RequestType::Chunk:
    case RequestType::Select:
    case RequestType::Insert:
    case RequestType::Replace:
    case RequestType::Update:
    case RequestType::Delete:
    case RequestType::Auth:
    case RequestType::Eval:
    case RequestType::Upsert:
    case RequestType::Call:
    case RequestType::Nop:
    case RequestType::Ping:
    case RequestType::Id:
    case RequestType::Unknown:
      return static_cast<RequestType>(type);
  }
  return RequestType::Unknown;
}

enum class FieldType : MpUint
{
  Version = 0x54,  // Binary protocol version supported by the client (MP_UINT)
  Features = 0x55,  // Supported binary protocol features (MP_ARRAY)
  Sync = 0x01,  // Unique request identifier (MP_UINT)
  SchemaVersion = 0x05,  // Version of the database schema (MP_UINT)
  Timestamp = 0x04,  // Time in seconds since the Unix epoch (MP_DOUBLE)
  RequestType = 0x00,  // Request type or response type (MP_UINT)

  // ...

  StreamId = 0x0a,  // Unique stream identifier (MP_UINT)

  Tuple = 0x21,  // Tuple, arguments, operations, or authentication pair (MP_ARRAY)
  FunctionName = 0x22,  // Name of the called function. Used in IPROTO_CALL (MP_STR)
  Expr = 0x27,  // Command argument. Used in IPROTO_EVAL (MP_STR)
};

std::optional<FieldType> int_to_field_type(MpUint type)
{
  switch (static_cast<FieldType>(type)) {
    case FieldType::Version:
    case FieldType::Features:
    case FieldType::Sync:
    case FieldType::SchemaVersion:
    case FieldType::Timestamp:
    case FieldType::RequestType:
    case FieldType::StreamId:
    case FieldType::Tuple:
    case FieldType::FunctionName:
    case FieldType::Expr:
      return static_cast<FieldType>(type);
  }
  return std::nullopt;
}

class MessageHeader
{
public:
  RequestType request_type {RequestType::Unknown};
  MpUint sync {0};
  MpUint schema_version {0};
  std::optional<MpUint> stream_id {std::nullopt};
};

}  // namespace tntpp::detail::iproto

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
  namespace adaptor
  {

  template<>
  struct convert<tntpp::detail::iproto::MessageHeader>
  {
    msgpack::object const& operator()(msgpack::object const& o,
                                      tntpp::detail::iproto::MessageHeader& v) const
    {
      using FieldType = tntpp::detail::iproto::FieldType;
      if (o.type != msgpack::type::MAP) {
        throw msgpack::type_error();
      }

      for (const auto& obj : o.via.map) {
        if (obj.key.type != msgpack::type::POSITIVE_INTEGER) {
          throw msgpack::type_error();
        }

        auto field_type = tntpp::detail::iproto::int_to_field_type(obj.key.via.u64);
        if (!field_type.has_value()) {
          throw msgpack::type_error();
        }
        switch (*field_type) {
          case FieldType::RequestType: {
            tntpp::detail::iproto::MpUint value = obj.val.convert();
            v.request_type = tntpp::detail::iproto::int_to_req_type(value);
            break;
          }
          case FieldType::Sync: {
            v.sync = obj.val.convert();
            break;
          }
          case FieldType::SchemaVersion: {
            v.schema_version = obj.val.convert();
            break;
          }
          case FieldType::StreamId: {
            tntpp::detail::iproto::MpUint stream_id = obj.val.convert();
            v.stream_id = stream_id;
          }
          default:
            break;
        }
      }
      return o;
    }
  };

  template<>
  struct pack<tntpp::detail::iproto::MessageHeader>
  {
    template<typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& s,
                                        tntpp::detail::iproto::MessageHeader const& v) const
    {
      using FieldType = tntpp::detail::iproto::FieldType;
      s.pack_map(v.stream_id ? 3 : 2);
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::Sync));
      s.pack(v.sync);
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::RequestType));
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(v.request_type));
      if (v.stream_id) {
        s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::StreamId));
        s.pack(v.stream_id.value());
      }
      return s;
    }
  };

  }  // namespace adaptor
}
}  // namespace msgpack

#endif  // TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
