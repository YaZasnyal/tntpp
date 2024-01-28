#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/co_composed.hpp>
#include <fmt/core.h>

#include "connection.h"
#include "detail/tntpp_operation.h"

namespace tntpp
{

class Connector;
using ConnectorSptr = std::shared_ptr<Connector>;

/**
 * Connector class is a low level primitive for communication with the tarantool
 * instance.
 *
 * This class should not be used on it's own. It can be used to create higher
 * layers like Box.
 *
 * @threadsafe this class MUST be thead safe
 *
 * This class also handles reconnects
 */
class Connector
{
public:
  Connector(const Connector&) = delete;
  Connector(Connector&&) = default;
  ~Connector() = default;
  Connector& operator=(const Connector&) = delete;
  Connector& operator=(Connector&&) = default;

  /**
   * Create new connection with specified endpoint
   *
   * @tparam H completion handler type
   * @param exec executor
   * @param cfg connector options
   * @param handler completion handler [void(error_code, ConnectorSptr)]
   *
   * @note all arguments must live until async operation is finished
   */
  template<class H>
  static auto connect(boost::asio::any_io_executor exec,
                      const Config& cfg,
                      H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, ConnectorSptr)>(
        boost::asio::experimental::co_composed<void(error_code, ConnectorSptr)>(
            [](auto state, boost::asio::any_io_executor exec, Config cfg)
                -> void
            {
              // int answer = co_await get_answer(boost::asio::deferred);
              auto conn = std::make_shared<detail::Connection>(exec, cfg);
              error_code ec {};
              co_await conn->connect(
                  boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                co_return state.complete(ec, nullptr);
              }

              assert(conn != nullptr);
              // construct Connector and return it
              ConnectorSptr connector(new Connector(std::move(conn)));
              co_return state.complete(error_code {}, std::move(connector));
            }),
        handler,
        exec,
        cfg);
  }

  /**
   * Initiate a request operation.
   *
   * A request operation is the one that requires exactly one response from the
   * server like: get data, put data, call, sql and so on.
   *
   * @param data - serialized data WITH length, header and body parts already
   * assembled
   *
   * @note data param MUST live for the whole duration of the operation
   * @note returning from this function does not mean that the data has been
   * sent. Only that it was added to the send queue
   */
  template<class H>
  auto send_request(const detail::Operation& request, H&& handler)
  {
    auto init = [this](auto handler, const detail::Operation& request)
    {
      // dispatch this function to the connection strand if we are not there
      // already
      m_conn->get_strand().dispatch(
          [this, handler = std::move(handler), &request]()
          {
            if (s_ != State::Connected) {
              handler(boost::system::errc::not_connected);
            }

            m_conn->send_data(request.data);
            boost::asio::any_completion_handler<void(error_code)> any_handler =
                handler;
            m_requests.emplace({request.id, std::move(handler)});
          });
    };

    return boost::asio::async_initiate<H, void(error_code)>(
        init, handler, std::ref(request));
  }

  /**
   * Stop the Connector
   *
   * This functions takes care of gracefully cancelling all operations and
   * stopping the socket
   *
   * @tparam H
   * @param handler
   *
   * @note this function MUST be called before destructor is called.
   */
  template<class H>
  auto stop(H&& handler)
  {
    // @todo implement
  }

  /**
   * Generates new unique within this connection request id
   */
  detail::iproto::OperationId generate_id()
  {
    return m_request_id.fetch_add(1, std::memory_order_relaxed);
  }

  // create watcher(queue_size)
  //   get_event
  // class Watcher
  //   create_bounded(queue_size)
  //   create_unbounmded(queue_size)
  //   receive_event()

private:
  /**
   * @param conn - established connection
   *
   * Connector expects fully established connection
   */
  Connector(detail::ConnectionSptr conn)
      : m_conn(std::move(conn))
      , s_(State::Connected)
  {
    // @todo start receive operation
    // read event
    // if error and not stopped -> reconnect
    // if error and stopped -> return
    // decode header
    // call completion handler
  }

  // @todo start
  //   save sptr to this for graceful shutdown
  // @todo stop
  //   post stop

  enum class State
  {
    Connecting,
    Connected,
    Stopped,
  };

  State s_;

  std::atomic<detail::iproto::OperationId> m_request_id {0};
  std::unordered_map<detail::iproto::OperationId,
                     boost::asio::any_completion_handler<void(error_code)>>
      m_requests;
  // watchers

  // Destruction order matters. Connection must stay in the end.
  // @todo async future for async_stop
  detail::ConnectionSptr m_conn;
};

class Box
{
public:
  Box(Connector& conn)
      : m_conn(conn)
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
  // call
  // sql
  // eval

  /**
   * Allows to get the underlying connection. May be useful to create another
   * instances of Box.
   * @return
   */
  Connector& get_connector() const { return m_conn; }

private:
  Connector& m_conn;
};

}  // namespace tntpp