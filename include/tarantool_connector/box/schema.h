//
// Created by root on 2/12/24.
//

#ifndef TARANTOOL_CONNECTOR_SCHEMA_H
#define TARANTOOL_CONNECTOR_SCHEMA_H

#include <tarantool_connector/detail/iproto_typedefs.h>

namespace tntpp::detail
{

class SpaceSchema
{
public:
  iproto::MpUint schema_id;
  // spaces
  // indexes
};



class IndexSchema
{

};

}

#endif  // TARANTOOL_CONNECTOR_SCHEMA_H
