//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_CONNECTION_H
#define TARANTOOL_CONNECTOR_CONNECTION_H

#include <cassert>
#include <memory>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>

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
  using Stream = IprotoFraming<Socket>;

  using Executor = Strand;

  Connection(boost::asio::any_io_executor exec, Config cfg)
      : m_strand(boost::asio::make_strand(exec))
      , m_stream(Socket(m_strand), cfg.receive_buffer_size())
      , m_config(std::move(cfg))
      , m_queue(cfg.send_queue_capacity())
  {
  }

  Connection(const Connection&) = delete;
  Connection(Connection&&) = default;
  Connection& operator=(const Connection&) = delete;
  Connection& operator=(Connection&&) = default;

  void stop() { m_stream.next_layer().close(); }

  template<class H>
  auto connect(H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code)>(
        TNTPP_CO_COMPOSED<void(error_code)>(
            [this](auto state) -> void
            {
              using tcp = boost::asio::ip::tcp;
              using boost::asio::redirect_error;
              using boost::asio::deferred;

              auto& socket = m_stream.next_layer();
              assert(!socket.is_open());

              error_code ec {};
              const tcp::resolver resolver(m_strand);
              auto addresses = co_await resolver.async_resolve(
                  m_config.host(), "", redirect_error(deferred, ec));
              if (ec) {
                TNTPP_LOG(m_config.logger(),
                          Info,
                          "unable to resolve address: {{host='{}', error='{}'}}",
                          m_config.host(),
                          ec.message());
                co_return state.complete(ec);
              }

              // try all available addresses returned by resolver
              // @todo extract to separate function
              for (const auto& address : addresses) {
                m_stream.clear();
                socket.close();
                tcp::endpoint endpoint(address.endpoint().address(), m_config.port());
                TNTPP_LOG(
                    m_config.logger(), Debug, "trying to connect: {{endpoint={}}}", endpoint);
                co_await socket.async_connect(endpoint, redirect_error(deferred, ec));
                if (ec) {
                  TNTPP_LOG(m_config.logger(),
                            Info,
                            "unable to establish connection: {{endpoint='{}', "
                            "error='{}'}}",
                            endpoint,
                            ec.message());
                  continue;
                }
                socket.set_option(tcp::no_delay(m_config.no_delay()));
                TNTPP_LOG(m_config.logger(),
                          Info,
                          "connection established; {{endpoint='{}', no_delay={}}}",
                          socket.remote_endpoint(),
                          m_config.no_delay());

                // @todo add timeout
                m_salt = co_await read_tarantool_hello(socket, redirect_error(deferred, ec));
                if (ec) {
                  TNTPP_LOG(m_config.logger(),
                            Info,
                            "unable to read hello message: {{error='{}'}}",
                            ec.message());
                  continue;
                }

                m_stream.clear();  // clear old data after reconnect
                co_return state.complete(error_code {});
              }

              TNTPP_LOG(m_config.logger(),
                        Info,
                        "unable to establish connection to any of resolved "
                        "endpoints: {{host={}}}",
                        m_config.host());
              co_return state.complete(error_code(boost::system::errc::not_connected,
                                                  boost::system::system_category()));
            }),
        handler);
  }

  template<class H>
  auto receive_message(H&& handler)
  {
    return m_stream.receive_message(std::forward<decltype(handler)>(handler));
  }

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

  [[nodiscard]] const std::string& get_salt() const { return m_salt; }

private:
  void init_single_send(detail::Data data)
  {
    m_sending = true;
    TNTPP_LOG(m_config.logger(), Trace, "initialing send operation: {{msg_count=1}}");
    boost::asio::async_write(
        m_stream.next_layer(),
        data,
        boost::asio::bind_executor(
            m_strand, [this](error_code ec, std::size_t count) { on_data_sent(ec, count); }));
  }

  void init_queue_send()
  {
    m_sending = true;
    // MUST live long enough because it is stored in this class
    const std::vector<Data>& data = m_queue.swap();
    // push it to the socket and wait until all octets are written
    TNTPP_LOG(
        m_config.logger(), Trace, "initialing send operation: {{msg_count={}}}", data.size());
    boost::asio::async_write(
        m_stream.next_layer(),
        data,
        boost::asio::bind_executor(
            m_strand, [this](error_code ec, std::size_t count) { on_data_sent(ec, count); }));
  }

  void on_data_sent(error_code ec, std::size_t count)
  {
    assert(m_strand.running_in_this_thread());
    m_sending = false;
    if (ec) {
      TNTPP_LOG(m_config.logger(),
                Error,
                "send operation failed: {{error='{}', bytes={}}}",
                ec.message(),
                count);
      m_stream.next_layer().close();
      m_queue.clear();
    }
    TNTPP_LOG(m_config.logger(), Trace, "finished send operation: {{bytes={}}}", count);
    // check if there are new requests available and send them
    if (m_queue.is_ready()) {
      init_queue_send();
    }
  }

  /**
   * local executor to synchronize all async operations if multi-threaded runtime is used
   */
  Strand m_strand;
  Stream m_stream;
  Config m_config;

  bool m_sending {false};  ///< indicates if send operation is in progress
  DBQueue<Data> m_queue;  ///< queue for send buffers

  std::string m_salt;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_CONNECTION_H
