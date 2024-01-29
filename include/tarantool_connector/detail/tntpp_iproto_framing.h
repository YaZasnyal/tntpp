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
#include <boost/system/detail/errc.hpp>

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
            if (buffer[buffer.size() / 2 - 1] != '\n'
                || buffer[buffer.size() - 1] != '\n')
            {
              co_return state.complete(
                  error_code(boost::system::errc::bad_message,
                             boost::system::system_category()),
                  "");
            }

            /// @todo fix pointer arithmetic warning
            std::string salt(buffer.data() + (buffer.size() / 2),
                             buffer.size() / 2);
            co_return state.complete(error_code {}, std::move(salt));
          }),
      handler,
      std::ref(stream));
}

/**
 * Framing class allows to operate with messages instead of octets when
 * m_sending data over the wire
 */
template<class Stream>
class Framing
{
public:
  using Self = Framing<Stream>;

  /// The type of the next layer.
  using next_layer_type = std::remove_reference_t<Stream>;
  /// The type of the lowest layer.
  using lowest_layer_type = typename next_layer_type::lowest_layer_type;
  /// The type of the executor associated with the object.
  using executor_type = typename lowest_layer_type::executor_type;

  Framing(Stream s)
      : m_next_layer(std::move(s))
  {
  }
  Framing(const Self&) = delete;
  Framing(Self&&) = default;
  Self& operator=(const Self&) = delete;
  Self& operator=(Self&&) = default;

  template<class H>
  auto receive_message(H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, std::string)>(
        boost::asio::experimental::co_composed<void(error_code, std::string)>(
            [this](auto state) -> void
            {
              // read the type - 1 octet

              // read the rest of the length

              // read the rest of the packet

              // read message length (MP_UINT32) - 5 octets
              // According to:
              // https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
              //
              // uint 32 stores a 32-bit big-endian unsigned integer
              // +--------+--------+--------+--------+--------+
              // |  0xce  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|
              // +--------+--------+--------+--------+--------+

              // check if 0xcf

              // read the body
              char ch;
              auto [ec, count] = co_await m_next_layer.async_read_some(
                  boost::asio::mutable_buffer(&ch, 1),
                  boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                co_return state.complete(ec, std::string {});
              }
              co_return state.complete(error_code {}, std::string {});
            }),
        handler);
  }

  void clear()
  {
    // @todo clear
  }

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
  const lowest_layer_type& lowest_layer() const
  {
    return m_next_layer.lowest_layer();
  }

  /**
   * @brief get_executor obtains the executor object that the stream uses
   * to run asynchronous operations
   */
  executor_type get_executor() const noexcept
  {
    return m_next_layer.get_executor();
  }

private:
  Stream m_next_layer;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
