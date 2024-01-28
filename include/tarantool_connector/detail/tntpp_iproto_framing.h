//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
#define TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H

#include <array>

#include <boost/asio/experimental/co_composed.hpp>
#include <boost/asio/ip/tcp.hpp>

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
  Framing(Stream s)
      : s_(s)
  {
  }

  // read message

private:
  Stream s_;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_IPROTO_FRAMING_H
