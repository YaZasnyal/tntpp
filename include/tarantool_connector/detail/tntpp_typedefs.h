//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H
#define TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H

#include <cstddef>
#include <span>

#include <boost/asio/buffer.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/system/error_code.hpp>

#if __has_include(<boost/asio/co_composed.hpp>)
#  include <boost/asio/co_composed.hpp>
#elif __has_include(<boost/asio/experimental/co_composed.hpp>)
#  include <boost/asio/experimental/co_composed.hpp>
#else
#  error Unable to find co_composed in boost::asio
#endif

namespace tntpp
{

using error_code = boost::system::error_code;

#if __has_include(<boost/asio/co_composed.hpp>)
#  define TNTPP_CO_COMPOSED = boost::asio::co_composed
#elif __has_include(<boost/asio/experimental/co_composed.hpp>)
#  define TNTPP_CO_COMPOSED boost::asio::experimental::co_composed
#endif

}  // namespace tntpp

namespace tntpp::detail
{

using Data = boost::asio::const_buffer;

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H
