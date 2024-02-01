//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_COUT_LOGGER_H
#define TARANTOOL_CONNECTOR_TNTPP_COUT_LOGGER_H

#include <iomanip>
#include <iostream>
#include <ranges>
#include <string_view>

#include <fmt/chrono.h>

#include "tntpp_logger.h"

namespace tntpp
{

// template<typename T>
// auto type_name_helper(const std::source_location s =
// std::source_location::current())
//{
//   using std::operator""sv;
//   const std::string_view fun_name{s.function_name()};
//   constexpr auto prefix{"[with T = "sv};
//   const auto type_name_begin{fun_name.find(prefix)};
//   if (""sv.npos == type_name_begin)
//     return ""sv;
//   const std::size_t first{type_name_begin + prefix.length()};
//   return std::string_view{fun_name.cbegin() + first, fun_name.cend() - 1};
// }

inline constexpr std::string_view strip_filename(const char* filename)
{
  std::string_view filename_sv(filename);
  std::size_t sep = 0;
  if (auto it = filename_sv.rfind('/'); it != 0) {
    sep = ++it;
  }
  if (sep == 0) {
    if (auto it = filename_sv.rfind('\\'); it != 0) {
      sep = ++it;
    }
  }
  return filename_sv.substr(sep);
}

inline constexpr std::string_view strip_string(std::string_view s, std::size_t len = 25)
{
  if (s.length() <= len) {
    return s;
  }
  return s.substr(s.length() - len);
}

class StdoutLogger : public LogConsumer
{
public:
  StdoutLogger() = default;
  StdoutLogger(LogLevel level)
      : m_level(level)
  {
  }

  LogLevel max_log_level() const noexcept override { return m_level; }

  void handle(LogLevel level,
              const char* message,
              std::size_t message_len,
              std::source_location location) noexcept override
  {
    auto out = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(out),
                   "{:<15}:{:<3} [{:%F %T%z}] [{:<5}] ",
                   strip_string(strip_filename(location.file_name()), 15),
                   location.column(),
                   fmt::localtime(std::time(nullptr)),
                   log_level_to_string(level));
    std::cout << std::string_view(out.data(), out.size()) << std::string_view(message, message_len)
              << std::endl;
  }

private:
  LogLevel m_level {LogLevel::Info};
};

}  // namespace tntpp

#endif  // TARANTOOL_CONNECTOR_TNTPP_COUT_LOGGER_H
