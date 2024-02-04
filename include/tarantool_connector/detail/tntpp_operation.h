//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_OPERATION_H
#define TARANTOOL_CONNECTOR_TNTPP_OPERATION_H

#include "iproto_typedefs.h"
#include "tntpp_typedefs.h"

namespace tntpp::detail
{

class Operation
{
public:
  iproto::OperationId id;
  Data data;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_OPERATION_H
