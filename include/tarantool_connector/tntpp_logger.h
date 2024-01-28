//
// Created by blade on 22.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_LOGGER_H
#define TARANTOOL_CONNECTOR_TNTPP_LOGGER_H

#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING 1

#include <source_location>

#include <fmt/format.h>

#include "detail/asio_printers.h"

namespace tntpp
{

enum class LogLevel
{
  Fatal,
  Error,
  Warn,
  Info,
  Debug,
  Trace,
};

static constexpr const char* log_level_to_string(LogLevel level)
{
  switch (level) {
    case LogLevel::Fatal:
      return "FATAL";
      break;
    case LogLevel::Error:
      return "ERROR";
      break;
    case LogLevel::Warn:
      return "WARN";
      break;
    case LogLevel::Info:
      return "INFO";
      break;
    case LogLevel::Debug:
      return "DEBUG";
      break;
    case LogLevel::Trace:
      return "TRACE";
      break;
  }

#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
  __assume(false);
#else  // GCC, Clang
  __builtin_unreachable();
#endif
}

class LogConsumer
{
public:
  /**
   * This function must current return logging level
   */
  virtual LogLevel max_log_level() const noexcept = 0;
  /**
   * This function is responsible for handling the message
   *
   * @param level - logging level of the message
   * @param message - the message itself
   * @param message_len - message len
   * @param location - where logging action was called
   *
   * @note message pointer MUST NOT be used after this function finishes.
   * Async loggers MUST copy message if lifetime extension is required
   */
  virtual void handle(LogLevel level,
                      const char* message,
                      std::size_t message_len,
                      std::source_location location) noexcept = 0;
};

}  // namespace tntpp

// TODO: logger

#ifndef TNTPP_LOG
#  define TNTPP_LOG(logger, level, ...)                                \
    {                                                                          \
      auto l_level__ = LogLevel::level;                                        \
      if (logger && logger->max_log_level() >= l_level__) {                    \
        auto l_out__ = fmt::memory_buffer();                                   \
        fmt::format_to(std::back_inserter(l_out__), __VA_ARGS__);              \
        logger->handle(l_level__,                                              \
                       l_out__.data(),                                         \
                       l_out__.size(),                                         \
                       std::source_location::current());                       \
      }                                                                        \
    }
#endif

#endif  // TARANTOOL_CONNECTOR_TNTPP_LOGGER_H
