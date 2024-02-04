//
// Created by root on 2/4/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_REQUEST_H
#define TARANTOOL_CONNECTOR_TNTPP_REQUEST_H

#include <memory_resource>
#include <sstream>

#include <boost/asio/buffer.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/endian.hpp>
#include <msgpack.hpp>

#include "iproto_typedefs.h"

namespace tntpp::detail
{

/**
 * RequestPacker helps to build message efficiently
 */
class RequestPacker
{
public:
  RequestPacker()
  {
    m_stream.write(
        reinterpret_cast<const char*>(&size_tag),  // NOLINT(*-pro-type-reinterpret-cast)
        1);
    iproto::SizeType dummy = std::numeric_limits<iproto::SizeType>::max();
    static_assert(sizeof(dummy) == 4);
    m_stream.write(reinterpret_cast<const char*>(&dummy),  // NOLINT(*-pro-type-reinterpret-cast)
                   sizeof(dummy));
  }

  template<class T>
  void pack(T&& data)
  {
    msgpack::pack(m_stream, data);
  }

  void finalize()
  {
    iproto::SizeType real_size = static_cast<iproto::SizeType>(m_stream.view().size())
        - sizeof(size_tag) - sizeof(real_size);
    boost::endian::native_to_big_inplace(real_size);
    std::memcpy(const_cast<char*>(m_stream.view().data()) // NOLINT(*-pro-type-const-cast)
                    + sizeof(size_tag),  // NOLINT(*-pro-bounds-pointer-arithmetic)
                &real_size,
                sizeof(real_size));
  }

  std::string_view str() const { return m_stream.view(); }

private:
  inline static const unsigned char size_tag = 0xce;
  std::stringstream m_stream;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_REQUEST_H
