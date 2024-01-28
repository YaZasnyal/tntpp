//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_CONFIG_H
#define TARANTOOL_CONNECTOR_TNTPP_CONFIG_H

#include <cassert>
#include <optional>
#include <string>

#include "tntpp_logger.h"

namespace tntpp
{

/**
 * Class that holds all configurable options for the connector
 *
 * Defaults to localhost connection to the port 3301
 */
class Config
{
public:
  static constexpr std::uint16_t default_port = 3301;

  Config() = default;
  Config(const Config&) = default;
  Config(Config&&) = default;
  Config& operator=(const Config&) = default;
  Config& operator=(Config&&) = default;

  Config& host(const std::string& host)
  {
    m_host = host;
    return *this;
  }
  [[nodiscard]] const std::string& host() const noexcept { return m_host; }

  Config& port(std::uint16_t port) noexcept
  {
    m_port = port;
    return *this;
  }
  [[nodiscard]] std::uint16_t port() const noexcept { return m_port; }

  struct Credentials
  {
    std::string username;
    std::string password;
  };
  Config& credentials(const std::string& username, const std::string& password)
  {
    assert(!username.empty());
    assert(!password.empty());
    m_credentials.emplace(username, password);
    return *this;
  }
  [[nodiscard]] const std::optional<Credentials>& credentials() const noexcept
  {
    m_credentials;
  }

  Config& logger(LogConsumer* logger)
  {
    m_logger = logger;
    return *this;
  }
  [[nodiscard]] LogConsumer* logger() const noexcept { return m_logger; }

  /**
   * Indicated if no delay option should be enabled for the socket.
   *
   * This option may decrease latency for the operations but probably will
   * decrease throughput of the connector. This option is not recommended for
   * connections that handle many requests concurrently.
   */
  [[nodiscard]] bool nodelay() const noexcept { return m_nodelay; }

  /**
   * @brief capacity of the send queue
   *
   * Send queue is used to store new requests while there is an active send
   * operation and helps to reduce the number of allocations needed.
   *
   * Possible values:
   *   0 - unlimited
   *   n - limited max size
   *
   * Limiting this value does not mean that operations will fail if the queue is
   * exhausted but that the queue will shrink to this value after the operation
   * completes to reduce memory allocated when burst occurs.
   */
  [[nodiscard]] std::size_t send_queue_capacity() const noexcept
  {
    return m_send_queue_capacity;
  }

private:
  std::string m_host {"127.0.0.1"};
  std::uint16_t m_port {default_port};
  std::optional<Credentials> m_credentials {std::nullopt};
  LogConsumer* m_logger {nullptr};

  bool m_nodelay {false};
  std::size_t m_send_queue_capacity {32};
};

}  // namespace tntpp

#endif  // TARANTOOL_CONNECTOR_TNTPP_CONFIG_H
