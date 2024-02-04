//
// Created by root on 2/4/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_PING_H
#define TARANTOOL_CONNECTOR_TNTPP_PING_H

#include "tntpp_iproto_framing.h"
#include "tntpp_typedefs.h"

namespace tntpp::detail
{

class PingRequest
{
};

}  // namespace tntpp::detail

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
  namespace adaptor
  {

  template<>
  struct pack<tntpp::detail::PingRequest>
  {
    template<typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& s,
                                        tntpp::detail::PingRequest const&) const
    {
      s.pack_map(0);
      return s;
    }
  };

  }  // namespace adaptor
}
}  // namespace msgpack

#endif  // TARANTOOL_CONNECTOR_TNTPP_PING_H
