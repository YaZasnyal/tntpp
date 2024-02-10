//
// Created by root on 2/3/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_BOX_H
#define TARANTOOL_CONNECTOR_TNTPP_BOX_H

#include <memory>
#include <optional>
#include <string_view>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/deferred.hpp>

#include "tarantool_connector/detail/iproto_typedefs.h"
#include "tarantool_connector/detail/tntpp_operation.h"
#include "tarantool_connector/detail/tntpp_request.h"
#include "tarantool_connector/tarantool_connector.hpp"

namespace tntpp::box
{

class Box
{
public:
  explicit Box(ConnectorSptr& conn)
      : m_state(new BoxInternal {conn})
  {
  }
  Box(const Box&) = delete;
  Box(Box&&) = delete;
  Box& operator=(const Box&) = delete;
  Box& operator=(Box&&) = delete;
  ~Box() = default;

  // get
  // insert
  // streams
  //   insert
  //   commit
  //   rollback

  // sql

  /**
   * Allows to get the underlying connection. May be useful to create another
   * instances of Box.
   * @return
   */
  ConnectorSptr& get_connector() const { return m_state->m_conn; }

private:
  struct BoxInternal
  {
    ConnectorSptr m_conn;
    // space and index map
  };

  std::shared_ptr<BoxInternal> m_state;
};

}  // namespace tntpp::box

#endif  // TARANTOOL_CONNECTOR_TNTPP_BOX_H
