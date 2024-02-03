//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
#define TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H

#include <array>
#include <utility>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/co_composed.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/endian.hpp>
#include <boost/system/detail/errc.hpp>

#include "iproto_typedefs.h"
#include "tntpp_mutable_buffer.h"
#include "tntpp_typedefs.h"

namespace tntpp::detail
{

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
      boost::asio::experimental::co_composed<void(error_code, std::string)>(
          [](auto state, Stream& stream) -> void
          {
            // hello message is always 128 bytes long and contains two 64 bytes
            // long lines: first one with version information and the second is
            // with salt for auth
            // https://www.tarantool.io/en/doc/latest/dev_guide/internals/iproto/authentication/
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
  IprotoFraming(const Self&) = delete;
  IprotoFraming(Self&&) = default;
  Self& operator=(const Self&) = delete;
  Self& operator=(Self&&) = default;

  template<class H>
  auto receive_message(H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, std::string)>(
        boost::asio::experimental::co_composed<void(error_code, std::string)>(
            [this](auto state) -> void
            {
              auto [ec, length] =
                  co_await read_message_length(boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // todo log error
                co_return state.complete(ec, std::string {});
              }

              FrozenBuffer msg_body = co_await transfer_exactly(
                  length, boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                // todo log error
                co_return state.complete(ec, std::string {});
              }

              try {
                std::size_t offset = 0;
                msgpack::unpacker unpacker;
                auto object = msgpack::unpack(
                    static_cast<const char*>(msg_body.data()), msg_body.size(), offset);
                iproto::MessageHeader header = object->convert();
                // todo return the message
              } catch (const std::bad_cast&) {
                // todo make custom error type
                co_return state.complete(error_code(boost::system::errc::illegal_byte_sequence,
                                                    boost::system::system_category()),
                                         std::string {});
              } catch (const std::exception&) {
                // todo log error
                co_return state.complete(error_code(boost::system::errc::state_not_recoverable,
                                                    boost::system::system_category()),
                                         std::string {});
              }

              co_return state.complete(error_code(boost::system::errc::state_not_recoverable,
                                                  boost::system::system_category()),
                                       std::string {});
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
  // transfer_exactly -> FrozenBuffer
  template<class H>
  auto transfer_exactly(std::size_t bytes, H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, FrozenBuffer)>(
        boost::asio::experimental::co_composed<void(error_code, FrozenBuffer)>(
            [this](auto state, std::size_t bytes) -> void
            {
              if (m_buffer.get_ready_buffer().size() >= bytes) {
                co_return state.complete(error_code {}, m_buffer.advance_reader(bytes));
              }

              // read more data
              m_buffer.prepare(bytes);
              auto [ec, read_bytes] =
                  co_await boost::asio::async_read(m_next_layer,
                                                   m_buffer.get_receive_buffer(),
                                                   boost::asio::transfer_at_least(bytes),
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
        boost::asio::experimental::co_composed<void(error_code, iproto::SizeType)>(
            [this](auto state) -> void
            {
              auto buf = co_await transfer_exactly(1, boost::asio::deferred);
              uint8_t type = *static_cast<const uint8_t*>(buf.data());
              std::int64_t result = 0;
              if ((type & 0b10000000) == 0) {
                // 7-bit positive number
                co_return state.complete(error_code {}, type & 0b01111111);
              } else if (type == 0xcc) {  // 8-bit unsigned big endian
                buf = co_await transfer_exactly(1, boost::asio::deferred);
                co_return state.complete(error_code {}, *static_cast<const uint8_t*>(buf.data()));
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
            }),
        handle);
  }

  Stream m_next_layer;
  MutableBuffer m_buffer;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
