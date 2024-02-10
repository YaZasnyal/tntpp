//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
#define TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H

#include <array>
#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/endian.hpp>
#include <boost/system/detail/errc.hpp>

#include "iproto_typedefs.h"
#include "tntpp_mutable_buffer.h"
#include "tntpp_typedefs.h"

namespace tntpp::detail
{

template<typename T, typename Enable = void>
struct is_optional : std::false_type
{
};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type
{
};

/**
 * Read hello message from the tarantool server
 *
 * @tparam Stream
 * @tparam H
 * @param stream
 * @param handler
 * @return salt needed for auth
 */
template<class Stream, class H>
auto read_tarantool_hello(Stream& stream, H&& handler)
{
  return boost::asio::async_initiate<H, void(error_code, std::string)>(
      TNTPP_CO_COMPOSED<void(error_code, std::string)>(
          [](auto state, Stream& stream) -> void
          {
            // hello message is always 128 bytes long and contains two 64 bytes
            // long lines: first one with version information and the second is
            // with salt for auth
            // https://www.tarantool.io/en/doc/latest/dev_guide/internals/iproto/authentication/
            //
            // leaving it uninitialized because it will be rewritten
            std::array<char, 128> buffer;
            auto [ec, count] = co_await boost::asio::async_read(
                stream,
                boost::asio::mutable_buffer(buffer.data(), buffer.size()),
                boost::asio::as_tuple(boost::asio::deferred));
            if (ec) {
              co_return state.complete(ec, "");
            }

            // @todo log message
            if (buffer[buffer.size() / 2 - 1] != '\n' || buffer[buffer.size() - 1] != '\n') {
              co_return state.complete(
                  error_code(boost::system::errc::bad_message, boost::system::system_category()),
                  "");
            }

            /// @todo fix pointer arithmetic warning
            std::string salt(buffer.data() + (buffer.size() / 2), buffer.size() / 2);
            co_return state.complete(error_code {}, std::move(salt));
          }),
      handler,
      std::ref(stream));
}

/**
 * IprotoFrame represents a single response from the server
 *
 * This class does not know about request types and only holds the information to interpret it
 */
class IprotoFrame
{
public:
  IprotoFrame() = default;
  IprotoFrame(const iproto::MessageHeader& header, FrozenBuffer body)
      : m_header(header)
      , m_body(std::move(body))
  {
  }
  IprotoFrame(const IprotoFrame&) = default;
  IprotoFrame(IprotoFrame&&) = default;
  IprotoFrame& operator=(const IprotoFrame&) = default;
  IprotoFrame& operator=(IprotoFrame&&) = default;

  const iproto::MessageHeader& header() const { return m_header; }
  const FrozenBuffer& body() const { return m_body; }
  bool is_error() const { return m_header.is_error(); }

  operator bool() const { return !is_error(); }

  boost::system::error_code get_error_code() const noexcept
  {
    if (m_header.is_error()) {
      return m_header.get_error_code();
    }
    return error_code(iproto::TarantoolError::Unknown, iproto::tarantool_error_category());
  }

  [[nodiscard]] std::string get_error_string() const
  {
    assert(is_error());
    auto object = msgpack::unpack(static_cast<const char*>(body().data()), body().size(), 0);
    if (object->type != msgpack::type::MAP) {
      throw msgpack::type_error();
    }

    for (const auto& kv : object->via.map) {  // NOLINT(*-pro-type-union-access)
      if (kv.key.type != msgpack::type::POSITIVE_INTEGER) {
        throw msgpack::type_error();
      }

      auto key_type = detail::iproto::int_to_field_type(kv.key.via.u64);
      if (!key_type) {
        // unknown (does not care)
        continue;
      }
      switch (*key_type) {
        case detail::iproto::FieldType::Error_24:
          return kv.val.as<std::string>();
        default:
          continue;
      }
    }

    // no response found or nil error
    return "";
  }

  /**
   * Convert raw buffer to the specified type
   *
   * @tparam T result type
   * @param v parsed object
   */
  template<class T>
  void as(T& v) const
  {
    if (is_error()) {
      throw boost::system::system_error(get_error_code());
    }

    auto object = msgpack::unpack(static_cast<const char*>(body().data()), body().size(), 0);
    if (object->type != msgpack::type::MAP) {
      throw msgpack::type_error();
    }

    for (const auto& kv : object->via.map) {  // NOLINT(*-pro-type-union-access)
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

    // if result type is optional<T> then it is not an error to not have FieldType::Data
    if constexpr (is_optional<T>()) {
      v = std::nullopt;
      return;
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
    if (is_error()) {
      ec = get_error_code();
      return;
    }

    try {
      return as<T>();
    } catch (const boost::system::system_error& e) {
      ec = e.code();
    } catch (const std::exception&) {
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
      if (is_error()) {
        ec = get_error_code();
        return;
      }

      as<T>(v);
    } catch (const boost::system::system_error& e) {
      ec = e.code();
    } catch (const std::exception&) {
      ec = error_code(boost::system::errc::protocol_error, boost::system::system_category());
    }
  }

private:
  iproto::MessageHeader m_header;
  FrozenBuffer m_body;
};

/**
 * IprotoFraming class allows to operate with messages instead of octets when
 * reading data over the wire
 *
 * @tparam Stream ASIO async_read_stream (usually a TCP socket)
 */
template<class Stream>
class IprotoFraming
{
public:
  using Self = IprotoFraming<Stream>;

  /// The type of the next layer.
  using next_layer_type = std::remove_reference_t<Stream>;
  /// The type of the lowest layer.
  using lowest_layer_type = typename next_layer_type::lowest_layer_type;
  /// The type of the executor associated with the object.
  using executor_type = typename lowest_layer_type::executor_type;

  IprotoFraming(Stream s, std::size_t default_size)
      : m_next_layer(std::move(s))
      , m_buffer(default_size)
  {
  }
  ~IprotoFraming() = default;
  IprotoFraming(const Self&) = delete;
  IprotoFraming(Self&&) = default;
  Self& operator=(const Self&) = delete;
  Self& operator=(Self&&) = default;

  template<class H>
  auto receive_message(H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state) -> void
            {
              auto [ec, length] =
                  co_await read_message_length(boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // todo log error
                co_return state.complete(ec, IprotoFrame {});
              }

              FrozenBuffer msg_body = co_await transfer_exactly(
                  length, boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                // todo log error
                co_return state.complete(ec, IprotoFrame {});
              }

              try {
                std::size_t offset = 0;
                auto object = msgpack::unpack(
                    static_cast<const char*>(msg_body.data()), msg_body.size(), offset);
                iproto::MessageHeader header = object->convert();
                co_return state.complete(error_code {},
                                         IprotoFrame(header, msg_body.slice(offset)));
              } catch (const std::bad_cast&) {
                // todo make custom error type
                co_return state.complete(error_code(boost::system::errc::illegal_byte_sequence,
                                                    boost::system::system_category()),
                                         IprotoFrame {});
              } catch (const std::exception&) {
                // todo log error
                co_return state.complete(error_code(boost::system::errc::state_not_recoverable,
                                                    boost::system::system_category()),
                                         IprotoFrame {});
              }

              co_return state.complete(error_code(boost::system::errc::state_not_recoverable,
                                                  boost::system::system_category()),
                                       IprotoFrame {});
            }),
        handler);
  }

  void clear() { m_buffer.clear(); }

  /**
   * @brief next_layer returns next layer in the stack of stream layers.
   */
  next_layer_type& next_layer() { return m_next_layer; }

  /**
   * @brief next_layer returns next layer in the stack of stream layers.
   */
  const next_layer_type& next_layer() const { return m_next_layer; }

  /**
   * @brief lowest_layer returns the lowest layer in the stack of stream
   * layers.
   */
  lowest_layer_type& lowest_layer() { return m_next_layer.lowest_layer(); }

  /**
   * @brief lowest_layer returns the lowest layer in the stack of stream
   * layers.
   */
  const lowest_layer_type& lowest_layer() const { return m_next_layer.lowest_layer(); }

  /**
   * @brief get_executor obtains the executor object that the stream uses
   * to run asynchronous operations
   */
  executor_type get_executor() const noexcept { return m_next_layer.get_executor(); }

private:
  template<class H>
  auto transfer_exactly(std::size_t bytes, H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, FrozenBuffer)>(
        TNTPP_CO_COMPOSED<void(error_code, FrozenBuffer)>(
            [this](auto state, std::size_t bytes) -> void
            {
              if (m_buffer.get_ready_buffer().size() >= bytes) {
                co_return state.complete(error_code {}, m_buffer.advance_reader(bytes));
              }

              // read more data
              m_buffer.prepare(bytes);
              auto [ec, read_bytes] = co_await boost::asio::async_read(
                  m_next_layer,
                  m_buffer.get_receive_buffer(),
                  boost::asio::transfer_at_least(bytes - m_buffer.ready()),
                  boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                co_return state.complete(ec, FrozenBuffer {});
              }

              m_buffer.advance_writer(read_bytes);
              co_return state.complete(error_code {}, m_buffer.advance_reader(bytes));
            }),
        handle,
        bytes);
  }

  template<class H>
  auto read_message_length(H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, iproto::SizeType)>(
        TNTPP_CO_COMPOSED<void(error_code, iproto::SizeType)>(
            [this](auto state) -> void
            {
              try {
                auto buf = co_await transfer_exactly(1, boost::asio::deferred);
                uint8_t type = *static_cast<const uint8_t*>(buf.data());
                std::int64_t result = 0;
                if ((type & 0b10000000) == 0) {
                  // 7-bit positive number
                  co_return state.complete(error_code {}, type & 0b01111111);
                } else if (type == 0xcc) {  // 8-bit unsigned big endian
                  buf = co_await transfer_exactly(1, boost::asio::deferred);
                  co_return state.complete(error_code {},
                                           *static_cast<const uint8_t*>(buf.data()));
                } else if (type == 0xcc) {  // 16-bit unsigned big endian
                  buf = co_await transfer_exactly(2, boost::asio::deferred);
                  co_return state.complete(
                      error_code {},
                      boost::endian::big_to_native(*static_cast<const uint16_t*>(buf.data())));
                } else if (type == 0xce) {  // 32-bit unsigned big endian
                  buf = co_await transfer_exactly(4, boost::asio::deferred);
                  co_return state.complete(
                      error_code {},
                      boost::endian::big_to_native(*static_cast<const uint32_t*>(buf.data())));
                } else if (type == 0xcf) {  // 64-bit unsigned big endian
                  buf = co_await transfer_exactly(8, boost::asio::deferred);
                  co_return state.complete(
                      error_code {},
                      boost::endian::big_to_native(*static_cast<const uint64_t*>(buf.data())));
                } else if ((type & 0b11100000) == 0b11100000) {
                  result = -1;
                } else if (type == 0xd0) {  // 8-bit signed big endian
                  buf = co_await transfer_exactly(1, boost::asio::deferred);
                  result = *static_cast<const int8_t*>(buf.data());
                } else if (type == 0xd1) {  // 16-bit signed big endian
                  buf = co_await transfer_exactly(2, boost::asio::deferred);
                  result = boost::endian::big_to_native(*static_cast<const int16_t*>(buf.data()));
                } else if (type == 0xd2) {  // 32-bit signed big endian
                  buf = co_await transfer_exactly(4, boost::asio::deferred);
                  result = boost::endian::big_to_native(*static_cast<const int32_t*>(buf.data()));
                } else if (type == 0xd3) {  // 64-bit signed big endian
                  buf = co_await transfer_exactly(8, boost::asio::deferred);
                  result = boost::endian::big_to_native(*static_cast<const int64_t*>(buf.data()));
                }

                if (result < 0 || result > std::numeric_limits<iproto::SizeType>::max()) {
                  // @todo log error negative length (5-bit negative)
                  co_return state.complete(error_code(boost::system::errc::message_size,
                                                      boost::system::system_category()),
                                           0);
                }

                co_return state.complete(error_code {}, result);
              } catch (const boost::system::system_error& err) {
                co_return state.complete(err.code(), 0);
              }
            }),
        handle);
  }

  Stream m_next_layer;
  MutableBuffer m_buffer;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
