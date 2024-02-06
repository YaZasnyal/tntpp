//
// Created by root on 2/3/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_BOX_H
#define TARANTOOL_CONNECTOR_TNTPP_BOX_H

#include <memory>
#include <string_view>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/deferred.hpp>

#include "tarantool_connector/detail/iproto_typedefs.h"
#include "tarantool_connector/detail/tntpp_operation.h"
#include "tarantool_connector/detail/tntpp_request.h"
#include "tarantool_connector/tarantool_connector.hpp"

namespace tntpp::box
{

class GenericResult
{
public:
  GenericResult() = default;
  GenericResult(detail::IprotoFrame frame)
      : m_frame(std::move(frame))
  {
    assert(m_frame.body().data() != nullptr);
  }
  GenericResult(GenericResult&&) = default;
  GenericResult(const GenericResult&) = delete;
  GenericResult& operator=(GenericResult&&) = default;
  GenericResult& operator=(const GenericResult&) = delete;

  /**
   * Convert raw buffer to the specified type
   *
   * @tparam T result type
   * @param v parsed object
   */
  template<class T>
  void as(T& v) const
  {
    auto object =
        msgpack::unpack(static_cast<const char*>(m_frame.body().data()), m_frame.body().size(), 0);
    if (object->type != msgpack::type::MAP) {
      throw msgpack::type_error();
    }

    for (auto& kv : object->via.map) {  // NOLINT(*-pro-type-union-access)
      if (kv.key.type != msgpack::type::POSITIVE_INTEGER) {
        throw msgpack::type_error();
      }

      auto key_type = detail::iproto::int_to_field_type(kv.key.via.u64);
      if (!key_type) {
        // unknown (does not care)
        continue;
      }
      switch (*key_type) {
        case detail::iproto::FieldType::Data:
          kv.val.convert(v);
          return;
        default:
          continue;
      }
    }

    // no response found
    throw msgpack::type_error();
  }

  /**
   * Convert raw buffer to the specified type
   *
   * @tparam T result type
   * @return parsed object
   */
  template<class T>
  T as() const
  {
    T result;
    as<T>(result);
    return result;
  }

  /**
   * Convert raw buffer to the specified type
   *
   * This function catches exceptions and forwards them as an error_code
   *
   * @tparam T result type
   * @param ec error code
   * @return parsed object
   */
  template<class T>
  T as(error_code& ec) const noexcept
  {
    try {
      return as<T>();
    } catch (const std::exception& e) {
      ec = error_code(boost::system::errc::protocol_error, boost::system::system_category());
    }
  }

  /**
   * Convert raw buffer to the specified type
   *
   * This function catches exceptions and forwards them as an error_code
   *
   * @tparam T result type
   * @param v result object
   * @param ec error code
   */
  template<class T>
  void as(T& v, error_code& ec) const noexcept
  {
    try {
      as<T>(v);
    } catch (const std::exception& e) {
      ec = error_code(boost::system::errc::protocol_error, boost::system::system_category());
    }
  }

  /**
   * Get raw msgpack response
   */
  boost::asio::const_buffer get_raw_body() const noexcept
  {
    return boost::asio::const_buffer(m_frame.body().data(), m_frame.body().size());
  }

private:
  detail::IprotoFrame m_frame;
};

class Box
{
public:
  explicit Box(ConnectorSptr& conn)
      : m_state(new BoxInternal {conn})
  {
  }
  Box(const Box&) = delete;
  Box(Box&&) = delete;
  Box& operator=(const Box&) = delete;
  Box& operator=(Box&&) = delete;
  ~Box() = default;

  // get
  // insert
  // streams
  //   insert
  //   commit
  //   rollback

  /**
   * Calls stored procedure
   *
   * @tparam H completion handler
   * @tparam Args procedure arguments type
   * @param function procedure name
   * @param args procedure arguments (MP_ARRAY)
   * @param handler completion handler
   * @returns CallResult - a struct that holds anything as a result or an error
   *
   * Args must be vector of arguments or a tuple. Argument list will be destructured into the named
   * arguments of the function.
   *
   * Example:
   * function foo(a) ...
   * call("foo", std::make_tuple(5));
   *
   * It may be more convenient to create a struct and specialize serialization function for it.
   * Look msgpack-cxx for information.
   */
  template<class H, class Args>
  auto call(std::string_view function, Args&& args, H&& handler)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.request_type = detail::iproto::RequestType::Call;
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::FunctionName, function);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(args)>(args));
    packer.finalize();

    return boost::asio::async_initiate<H, void(error_code, GenericResult)>(
        boost::asio::experimental::co_composed<void(error_code, GenericResult)>(
            [this](
                auto state, detail::iproto::OperationId id, detail::RequestPacker buffer) -> void
            {
              auto [ec, buf] = co_await m_state->m_conn->send_request(
                  detail::Operation {
                      .id = id, .data = detail::Data(buffer.str().data(), buffer.str().size())},
                  boost::asio::as_tuple(boost::asio::deferred));

              co_return state.complete(ec, GenericResult(buf));
            }),
        handler,
        header.sync,
        std::move(packer));
  }

  /**
   * Execute lua code
   *
   * @tparam H completion handler type
   * @tparam Args list of arguments (MP_ARRAY)
   * @param expression lua code
   * @param args arguments
   * @param handler completion handler
   */
  template<class H, class Args>
  auto eval(std::string_view expression, Args&& args, H&& handler)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.request_type = detail::iproto::RequestType::Eval;
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::Expr, expression);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(args)>(args));
    packer.finalize();

    return boost::asio::async_initiate<H, void(error_code, GenericResult)>(
        boost::asio::experimental::co_composed<void(error_code, GenericResult)>(
            [this](
                auto state, detail::iproto::OperationId id, detail::RequestPacker buffer) -> void
            {
              auto [ec, buf] = co_await m_state->m_conn->send_request(
                  detail::Operation {
                      .id = id, .data = detail::Data(buffer.str().data(), buffer.str().size())},
                  boost::asio::as_tuple(boost::asio::deferred));

              co_return state.complete(ec, GenericResult(buf));
            }),
        handler,
        header.sync,
        std::move(packer));
  }

  // sql

  /**
   * Allows to get the underlying connection. May be useful to create another
   * instances of Box.
   * @return
   */
  ConnectorSptr& get_connector() const { return m_state->m_conn; }

private:
  struct BoxInternal
  {
    ConnectorSptr m_conn;
    // space and index map
  };

  std::shared_ptr<BoxInternal> m_state;
};

}  // namespace tntpp::box

#endif  // TARANTOOL_CONNECTOR_TNTPP_BOX_H
