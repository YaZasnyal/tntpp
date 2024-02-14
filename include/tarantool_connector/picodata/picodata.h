//
// Created by root on 2/14/24.
//

#ifndef TARANTOOL_CONNECTOR_PICODATA_H
#define TARANTOOL_CONNECTOR_PICODATA_H

#include <tarantool_connector/tarantool_connector.hpp>

namespace tntpp::picodata
{

class Picodata
{
public:
  explicit Picodata(ConnectorSptr& conn)
      : m_state(new PicodataInternal {conn})
  {
  }
  Picodata(const Picodata&) = delete;
  Picodata(Picodata&&) = default;
  Picodata& operator=(const Picodata&) = delete;
  Picodata& operator=(Picodata&&) = default;
  ~Picodata() = default;

  // @todo Picodata sql

private:
  struct PicodataInternal
  {
    ConnectorSptr m_conn;
  };

  std::shared_ptr<PicodataInternal> m_state;
};

}

#endif  // TARANTOOL_CONNECTOR_PICODATA_H
