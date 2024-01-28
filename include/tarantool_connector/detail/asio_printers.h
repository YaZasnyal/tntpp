//
// Created by blade on 28.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_ASIO_PRINTERS_H
#define TARANTOOL_CONNECTOR_ASIO_PRINTERS_H

#include <boost/asio/ip/tcp.hpp>
#include <fmt/core.h>

template<>
struct fmt::formatter<boost::asio::ip::tcp::endpoint>
    : fmt::formatter<string_view>
{
  auto format(const boost::asio::ip::tcp::endpoint& p,
              format_context& ctx) const
  {
    return fmt::format_to(
        ctx.out(), "{}:{}", p.address().to_string(), p.port());
  }
};

#endif  // TARANTOOL_CONNECTOR_ASIO_PRINTERS_H
