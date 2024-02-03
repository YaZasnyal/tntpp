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
  Box(Connector& conn)
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
  Connector& get_connector() const { return m_conn; }

private:
  Connector& m_conn;
};

}

#endif  // TARANTOOL_CONNECTOR_TNTPP_BOX_H
