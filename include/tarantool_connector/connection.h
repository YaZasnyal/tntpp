//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_CONNECTION_H
#define TARANTOOL_CONNECTOR_CONNECTION_H

#include <cassert>
#include <optional>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include "detail/tntpp_iproto_framing.h"
#include "detail/tntpp_queue.h"
#include "detail/tntpp_typedefs.h"
#include "tntpp_config.h"
#include "tntpp_logger.h"

namespace tntpp::detail
{

class Connection;
using ConnectionSptr = std::shared_ptr<Connection>;

/**
 * Connection class manages connection itself
 *
 * This class is synchronized using strand
 */
class Connection
{
public:
  using Strand = boost::asio::strand<boost::asio::any_io_executor>;
  using Socket = boost::asio::ip::tcp::socket;

  Connection(const Connection&) = delete;
  Connection(Connection&&) = default;
  Connection& operator=(const Connection&) = delete;
  Connection& operator=(Connection&&) = default;

  void stop()
  {
    /// @todo implement
  }

  template<class H>
  static auto connect(boost::asio::any_io_executor exec,
                      Config cfg,
                      H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, ConnectionSptr)>(
        boost::asio::experimental::co_composed<void(error_code,
                                                    ConnectionSptr)>(
            [](auto state, boost::asio::any_io_executor exec, Config cfg)
                -> void
            {
              using tcp = boost::asio::ip::tcp;

              error_code ec;
              boost::asio::ip::tcp::resolver resolver(exec);
              auto addresses = co_await resolver.async_resolve(
                  cfg.host(),
                  "",
                  boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                TNTPP_LOG_MESSAGE(
                    cfg.logger(),
                    Info,
                    "unable to resolve address: {{host='{}', error='{}'}}",
                    cfg.host(),
                    ec.to_string());
                co_return state.complete(ec, nullptr);
              }

              // try all available addresses returned by resolver
              Strand strand = boost::asio::make_strand(exec);
              tcp::socket socket(strand);
              for (const auto& address : addresses) {
                socket.close();
                tcp::endpoint endpoint(address.endpoint().address(),
                                       cfg.port());
                TNTPP_LOG_MESSAGE(cfg.logger(),
                                  Debug,
                                  "trying to connect: {{endpoint={}}}",
                                  endpoint);
                co_await socket.async_connect(
                    endpoint,
                    boost::asio::redirect_error(boost::asio::deferred, ec));
                if (ec) {
                  TNTPP_LOG_MESSAGE(cfg.logger(),
                                    Info,
                                    "unable to establish connection: "
                                    "{{endpoint='{}', error='{}'}}",
                                    endpoint,
                                    ec.to_string());
                  continue;
                }
                socket.set_option(tcp::no_delay(cfg.nodelay()));
                TNTPP_LOG_MESSAGE(cfg.logger(),
                                  Info,
                                  "connection established; "
                                  "{{endpoint='{}', no_delay={}}}",
                                  socket.remote_endpoint(),
                                  cfg.nodelay());

                // @todo add timeout
                auto salt = co_await read_tarantool_hello(
                    socket,
                    boost::asio::redirect_error(boost::asio::deferred, ec));
                if (ec) {
                  TNTPP_LOG_MESSAGE(
                      cfg.logger(),
                      Info,
                      "unable to read hello message: {{error='{}'}}",
                      ec.to_string());
                  continue;
                }
                // @todo add auth

                auto conn = ConnectionSptr(new Connection(
                    std::move(strand), std::move(socket), std::move(cfg)));
                co_return state.complete(error_code {}, std::move(conn));
              }

              TNTPP_LOG_MESSAGE(cfg.logger(),
                                Info,
                                "unable to establish connection to any of "
                                "resolved endpoints: {{host={}}}",
                                cfg.host());
              co_return state.complete(
                  error_code(boost::system::errc::not_connected,
                             boost::system::system_category()),
                  nullptr);
            }),
        handler,
        exec,
        std::move(cfg));
  }

  // async reconnect

  void send_data(detail::Data data)
  {
    assert(m_strand.running_in_this_thread());

    if (m_sending) {
      m_queue.push(data);
      return;
    }
    init_single_send(data);
  }

  [[nodiscard]] const Strand& get_strand() const { return m_strand; }

  [[nodiscard]] const Config& get_config() const { return m_config; }

private:
  Connection(Strand exec, Socket socket, Config cfg)
      : m_strand(std::move(exec))
      , m_socket(std::move(socket))
      , m_config(std::move(cfg))
      , m_queue(cfg.send_queue_capacity())
  {
    // assert is_open
  }

  void init_single_send(detail::Data data)
  {
    m_sending = true;
    boost::asio::async_write(
        m_socket,
        boost::asio::const_buffer(data.data(), data.size()),
        boost::asio::bind_executor(m_strand,
                                   [this](error_code ec, std::size_t count)
                                   { on_data_sent(ec, count); }));
  }

  void init_queue_send()
  {
    m_sending = true;
    // MUST live long enough because it is stored in this class
    const std::vector<Data>& data = m_queue.swap();
    // push it to the socket and wait until all octets are written
    boost::asio::async_write(
        m_socket,
        data,
        boost::asio::bind_executor(m_strand,
                                   [this](error_code ec, std::size_t count)
                                   { on_data_sent(ec, count); }));
  }

  void on_data_sent(error_code ec, std::size_t)
  {
    assert(m_strand.running_in_this_thread());
    m_sending = false;
    // @todo log trace sent bytes
    if (ec) {
      // @todo log error
      m_socket.close();
      m_queue.clear();
    }
    // check if there are new requests available and send them
    if (m_queue.is_ready()) {
      init_queue_send();
    }
  }

  // receive message
  // async reconnect

  /**
   * local executor to synchronize all async operations if multi-threaded
   * runtime is used
   */
  Strand m_strand;
  Socket m_socket;
  Config m_config;

  bool m_sending {false};  ///< indicates if send operation is in progress
  DBQueue<Data> m_queue;  ///< queue for send buffers
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_CONNECTION_H
