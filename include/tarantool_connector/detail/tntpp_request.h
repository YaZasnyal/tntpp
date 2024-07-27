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

  ~RequestPacker() { 
      int i = 0;
  }

  RequestPacker(const RequestPacker&) = delete;
  RequestPacker(RequestPacker&&) = default;

  template<class T>
  void pack(T&& data)
  {
    msgpack::pack(m_stream, std::forward<decltype(data)>(data));
  }

  void begin_map(uint32_t count) { msgpack::packer(m_stream).pack_map(count); }

  void begin_map()
  {
    uint16_t dummy = std::numeric_limits<uint16_t>::max();
    static_assert(sizeof(dummy) == 2);
    m_stream.write(reinterpret_cast<const char*>(&map_tag),  // NOLINT(*-pro-type-reinterpret-cast)
                   1);
    map_len_offset = m_stream.view().size();
    m_stream.write(reinterpret_cast<const char*>(&dummy),  // NOLINT(*-pro-type-reinterpret-cast)
                   sizeof(dummy));
  }

  void finalize_map()
  {
    assert(map_len_offset != 0);
    uint16_t header_count = m_header_count;
    boost::endian::native_to_big_inplace(header_count);
    std::memcpy(const_cast<char*>(m_stream.view().data())  // NOLINT(*-pro-type-const-cast)
                    + map_len_offset,  // NOLINT(*-pro-bounds-pointer-arithmetic)
                &header_count,
                sizeof(header_count));
  }

  template<class T>
  void pack_map_entry(iproto::FieldType field, T&& data)
  {
    msgpack::pack(m_stream, static_cast<iproto::MpUint>(field));
    msgpack::pack(m_stream, std::forward<decltype(data)>(data));
    ++m_header_count;
  }

  void finalize()
  {
    iproto::SizeType real_size = static_cast<iproto::SizeType>(m_stream.view().size())
        - sizeof(size_tag) - sizeof(real_size);
    boost::endian::native_to_big_inplace(real_size);
    std::memcpy(const_cast<char*>(m_stream.view().data())  // NOLINT(*-pro-type-const-cast)
                    + sizeof(size_tag),  // NOLINT(*-pro-bounds-pointer-arithmetic)
                &real_size,
                sizeof(real_size));
  }

  std::string_view str() const { return m_stream.view(); }

private:
  inline static const unsigned char size_tag = 0xce;
  inline static const unsigned char map_tag = 0xde;
  size_t map_len_offset {0};  // for finalize_map
  uint16_t m_header_count {0};
  std::stringstream m_stream;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_REQUEST_H
