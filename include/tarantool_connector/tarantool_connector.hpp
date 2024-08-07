#ifndef TARANTOOL_CONNECTOR_TARANTOOL_CONNECTOR_H
#define TARANTOOL_CONNECTOR_TARANTOOL_CONNECTOR_H

#include <atomic>
#include <cassert>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_map>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/redirect_error.hpp>

#include "connection.h"
#include "detail/tntpp_crypto.h"
#include "detail/tntpp_op_ping.h"
#include "detail/tntpp_operation.h"
#include "detail/tntpp_request.h"

namespace tntpp
{

class Connector;
using ConnectorSptr = std::shared_ptr<Connector>;
using ConnectorWptr = std::weak_ptr<Connector>;

using IprotoFrame = detail::IprotoFrame;

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
  /// The type of the executor associated with the object.
  using executor_type = typename detail::Connection::executor_type;

  Connector(const Connector&) = delete;
  Connector(Connector&&) = default;
  ~Connector()
  {
    m_state->m_conn->get_strand().execute(
        [state = m_state]()
        {
          state->m_s = Internal::State::Stopped;
          state->m_conn->stop();
        });
  }
  Connector& operator=(const Connector&) = delete;
  Connector& operator=(Connector&&) = default;

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
  auto send_request(detail::Operation request, H&& handler)
  {
    auto init = [this](auto handler, detail::Operation request)
    {
      // dispatch function to the connection strand if we are not there already
      m_state->m_conn->get_strand().dispatch(
          [this, handler = std::move(handler), request=std::move(request)]() mutable
          {
            boost::asio::any_completion_handler<void(error_code, detail::IprotoFrame)> any_handler(
                std::move(handler));
            if (m_state->m_s != Internal::State::Connected) {
              any_handler(
                  error_code(boost::system::errc::not_connected, boost::system::system_category()),
                  detail::IprotoFrame {});
            }

            m_state->m_requests.insert({request.id, std::move(any_handler)});
            m_state->m_conn->send_data(request.data);
          },
          boost::asio::recycling_allocator<void> {});
    };

    return boost::asio::async_initiate<H, void(error_code, detail::IprotoFrame)>(
        init, handler, std::move(request));
  }

  template<class H>
  auto send_request(detail::iproto::OperationId id, detail::RequestPacker&& buffer, H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](
                auto state, detail::iproto::OperationId id, detail::RequestPacker&& buffer) -> void
            {
              detail::RequestPacker buffer2(std::move(buffer));
                auto [ec, buf] = co_await send_request(
                  detail::Operation {
                      .id = id, .data = detail::Data(buffer2.str().data(), buffer2.str().size())},
                  boost::asio::as_tuple(boost::asio::deferred));

              co_return state.complete(ec, std::move(buf));
            }),
        handler,
        id,
        std::move(buffer));
  }

private:
  template<class H>
  auto async_auth(H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code)>(
        TNTPP_CO_COMPOSED<void(error_code)>(
            [this](auto state) -> void
            {
              const auto& credentials = m_state->m_conn->get_config().credentials();
              if (!credentials) {
                co_return state.complete(error_code {});
              }

              auto scramble =
                  detail::ScramblePassword(credentials->password, m_state->m_conn->get_salt());

              detail::RequestPacker packer;
              detail::iproto::MessageHeader header;
              header.sync = generate_id();
              header.set_request_type(detail::iproto::RequestType::Auth);
              packer.pack(header);
              packer.begin_map(2);
              packer.pack_map_entry(detail::iproto::FieldType::Username, credentials->username);
              packer.pack_map_entry(detail::iproto::FieldType::Tuple,
                                    std::make_tuple("chap-sha1", std::move(scramble)));
              packer.finalize();

              auto [ec, frame] = co_await send_request(
                  header.sync, std::move(packer), boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                TNTPP_LOG(m_state->get_logger(),
                          Info,
                          "error during auth request: {{error='{}'}}",
                          ec.message());
                co_return state.complete(ec);
              }
              if (frame.is_error()) {
                TNTPP_LOG(m_state->get_logger(),
                          Info,
                          "error during auth request: {{error_type='{}', error='{}'}}",
                          frame.get_error_code().what(),
                          frame.get_error_string());
                co_return state.complete(frame.get_error_code());
              }

              TNTPP_LOG(m_state->get_logger(), Info, "authenticated successfully");
              co_return state.complete(ec);
            }),
        handler);
  }

public:
  /**
   * Create new connection with specified endpoint
   *
   * @tparam H completion handler type
   * @param exec executor
   * @param cfg connector options
   * @param handler completion handler [void(error_code, ConnectorSptr)]
   */
  template<class H>
  static auto connect(boost::asio::any_io_executor exec, const Config& cfg, H&& handler)
  {
    return boost::asio::async_initiate<H, void(error_code, ConnectorSptr)>(
        TNTPP_CO_COMPOSED<void(error_code, ConnectorSptr)>(
            [](auto state, boost::asio::any_io_executor exec, Config cfg) -> void
            {
              auto conn = std::make_shared<detail::Connection>(exec, std::move(cfg));
              error_code ec {};
              co_await conn->connect(boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                co_return state.complete(ec, nullptr);
              }

              assert(conn != nullptr);
              // construct Connector
              ConnectorSptr connector(new Connector(std::move(conn)));
              boost::asio::co_spawn(
                  connector->m_state->m_conn->get_strand(),
                  [conn = ConnectorWptr(connector),
                   state = connector->m_state]() mutable -> boost::asio::awaitable<void>
                  { co_await Connector::start(std::move(conn), std::move(state)); },
                  boost::asio::detached);

              // auth
              co_await connector->async_auth(
                  boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                co_return state.complete(ec, nullptr);
              }

              co_return state.complete(error_code {}, std::move(connector));
            }),
        handler,
        exec,
        cfg);
  }

  /**
   * Generates new unique within this connection request id
   */
  detail::iproto::OperationId generate_id()
  {
    return m_state->m_request_id.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * Send ping request
   *
   * @tparam H completion handler type
   * @param handler completion handler
   * @return error if request is not fulfilled
   */
  template<class H>
  auto ping(H&& handler)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = generate_id();
    header.set_request_type(detail::iproto::RequestType::Ping);
    packer.pack(header);
    packer.pack(detail::PingRequest {});
    packer.finalize();

    return send_request(header.sync, std::move(packer), handler);
  }

  /**
   * Calls stored procedure
   *
   * @tparam H completion handler
   * @tparam Args procedure arguments type
   * @param function procedure name
   * @param args procedure arguments (MP_ARRAY)
   * @param handler completion handler [void(error_code, IprotoFrame)]
   * @returns CallResult - a struct that holds anything as a result or an error
   *
   * Args should be vector of arguments or a tuple. Argument list will be deconstructed into the
   * named arguments of the function.
   *
   * Example:
   * @code
   * function foo(a) ...
   * call("foo", std::make_tuple(5), boost::asio::use_future).get();
   * @endcode
   *
   * It may be more convenient to create a struct and specialize serialization function for it.
   * Look msgpack-cxx for information.
   */
  template<class H, class Args>
  auto call(std::string_view function, Args&& args, H&& handler)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = generate_id();
    header.set_request_type(detail::iproto::RequestType::Call);
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::FunctionName, function);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(args)>(args));
    packer.finalize();

    return send_request(header.sync, std::move(packer), handler);
  }

  /**
   * Execute lua code in the tarantool instance
   *
   * @tparam H completion handler type
   * @tparam Args list of arguments (MP_ARRAY)
   * @param expression lua code
   * @param args arguments
   * @param handler completion handler [void(error_code, IprotoFrame)]
   */
  template<class H, class Args>
  auto eval(std::string_view expression, Args&& args, H&& handler)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = generate_id();
    header.set_request_type(detail::iproto::RequestType::Eval);
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::Expr, expression);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(args)>(args));
    packer.finalize();

    return send_request(header.sync, std::move(packer), handler);
  }

  detail::iproto::MpUint get_schema_version() const
  {
    return m_state->m_schema_version;
  }

  /**
   * @brief get_executor obtains the executor object that the stream uses
   * to run asynchronous operations
   */
  executor_type get_executor() const noexcept { return m_state->m_conn->get_executor(); }

  /**
   * Enter into the internal executor if synchronization is required
   *
   * @tparam H completion handler type [void()]
   * @param handle completion handler
   */
  template<class H>
  auto enter_executor(H&& handle)
  {
    return m_state->m_conn->enter_executor(handle);
  }

private:
  class Internal
  {
  public:
    explicit Internal(detail::ConnectionSptr conn)
        : m_conn(std::move(conn))
    {
    }
    LogConsumer* get_logger() { return m_conn->get_config().logger(); }

    void reset()
    {
      if (m_s == State::Connected) {
        m_s = State::Connecting;
      }
      for (auto& request : m_requests) {
        request.second(error_code {m_s == State::Stopped ? boost::system::errc::operation_canceled
                                                         : boost::system::errc::broken_pipe,
                                   boost::system::system_category()},
                       detail::IprotoFrame {});
      }
      m_requests.clear();
      m_conn->stop();
    }

    enum class State
    {
      Connecting,
      Connected,
      Stopped,
    };
    State m_s {State::Connected};

    detail::iproto::MpUint m_schema_version {0};
    std::atomic<detail::iproto::OperationId> m_request_id {0};
    std::unordered_map<detail::iproto::OperationId,
                       boost::asio::any_completion_handler<void(error_code, detail::IprotoFrame)>>
        m_requests;

    // Destruction order matters. Connection must stay in the end.
    // @todo async future for async_stop
    detail::ConnectionSptr m_conn;
  };
  using InternalSptr = std::shared_ptr<Internal>;

  /**
   * @param conn - established connection
   *
   * Connector expects fully established connection
   */
  explicit Connector(detail::ConnectionSptr conn)
      : m_state(new Internal(std::move(conn)))
  {
  }

  /**
   * Receive loop
   *
   * @todo find out how to remove weak ptr to Connector that is needed to send auth request after
   * reconnect
   */
  static boost::asio::awaitable<void> start(ConnectorWptr conn, InternalSptr state)
  {
    /// @todo bind allocator
    TNTPP_LOG(state->get_logger(), Debug, "[receive loop] started");
    while (true) {
      try {
        if (state->m_s == Internal::State::Stopped) {
          state->reset();
          break;
        }
        if (state->m_s != Internal::State::Connected) {
          // @todo implement backoff policy
          TNTPP_LOG(state->get_logger(), Debug, "[receive loop] trying to reconnect");
          auto [ec] =
              co_await state->m_conn->connect(boost::asio::as_tuple(boost::asio::use_awaitable));
          if (ec) {
            TNTPP_LOG(state->get_logger(),
                      Debug,
                      "[receive loop] failed to reconnect: {{error='{}'}}",
                      ec.message());
            continue;  // try again until stopped
          }

          // auth
          auto conn_shared = conn.lock();
          if (!conn_shared) {
            break;
          }
          co_await conn_shared->async_auth(
              boost::asio::redirect_error(boost::asio::use_awaitable, ec));
          if (ec) {
            continue;  // try again until stopped
          }

          state->m_s = Internal::State::Connected;
          TNTPP_LOG(state->get_logger(), Debug, "[receive loop] reconnected successfully");
        }

        auto [ec, message] = co_await state->m_conn->receive_message(
            boost::asio::as_tuple(boost::asio::use_awaitable));
        if (ec) {
          TNTPP_LOG(state->get_logger(),
                    Info,
                    "[receive loop] read operation finished with an error; "
                    "connection will be reset: {{error='{}'}}",
                    ec.message());
          state->reset();
          continue;
        }
        state->m_schema_version = message.header().schema_version;

        auto it = state->m_requests.extract(message.header().sync);
        if (!it) {
          TNTPP_LOG(state->get_logger(),
                    Debug,
                    "[receive loop] no completion handle found: {{sync={}}}",
                    message.header().sync);
          continue;
        }

        try {
          it.mapped()(error_code {}, std::move(message));
        } catch (std::exception& e) {
          TNTPP_LOG(state->get_logger(),
                    Warn,
                    "[receive loop] unhandled exception in completion handler: {{error='{}'}}",
                    e.what());
        }
      } catch (const boost::system::system_error& err) {
        if (err.code() == boost::system::errc::operation_canceled) {
          TNTPP_LOG(state->get_logger(), Info, "stop requested");
          state->reset();
          break;
        }
        // all other errors MUST be handled by the error code above
        TNTPP_LOG(state->get_logger(),
                  Error,
                  "[receive loop] unhandled exception; resetting: {{error='{}'}}",
                  err.what());
        state->reset();
        continue;
      } catch (const std::exception& err) {
        // totally unexpected error (definitely a bug)
        TNTPP_LOG(state->get_logger(),
                  Fatal,
                  "[receive loop] unhandled exception; resetting: {{error='{}'}}",
                  err.what());
        assert(false && "must not be here");
        state->reset();
        continue;
      }
    }

    TNTPP_LOG(state->get_logger(), Info, "[receive loop] finished");
  }

  InternalSptr m_state;
};

}  // namespace tntpp

#endif  // TARANTOOL_CONNECTOR_TARANTOOL_CONNECTOR_H