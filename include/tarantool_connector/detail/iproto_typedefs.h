//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
#define TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H

#include <cstdint>

namespace tntpp::detail::iproto
{

/// iproto format details can be found in the tarantool documentation
/// https://www.tarantool.io/en/doc/latest/dev_guide/internals/iproto

using OperationId = std::uint32_t;

enum class OperationType
{
};

}  // namespace tntpp::detail::iproto

#endif  // TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
