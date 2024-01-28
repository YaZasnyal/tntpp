//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H
#define TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H

#include <cstddef>
#include <span>

#include <boost/system/error_code.hpp>
#include <boost/asio/buffer.hpp>

namespace tntpp
{

using error_code = boost::system::error_code;

} // namespace tntpp

namespace tntpp::detail
{

using Data = boost::asio::const_buffer;

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_TYPEDEFS_H
