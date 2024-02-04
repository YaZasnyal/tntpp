//
// Created by root on 2/3/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_BOX_H
#define TARANTOOL_CONNECTOR_TNTPP_BOX_H

#include <tarantool_connector/tarantool_connector.hpp>

namespace tntpp::box
{

class Box
{
public:
  Box(ConnectorSptr& conn)
      : m_conn(conn)
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
  // call
  // sql
  // eval

  /**
   * Allows to get the underlying connection. May be useful to create another
   * instances of Box.
   * @return
   */
  ConnectorSptr& get_connector() const { return m_conn; }

private:
  struct BoxInternal
  {
    ConnectorSptr& m_conn;
    // space and index map
  };

  std::shared_ptr<BoxInternal> m_state;
};

}

#endif  // TARANTOOL_CONNECTOR_TNTPP_BOX_H
