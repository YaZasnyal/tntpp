//
// Created by root on 2/1/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_MUTABLE_BUFFER_H
#define TARANTOOL_CONNECTOR_TNTPP_MUTABLE_BUFFER_H

#include <cassert>
#include <cstdint>
#include <memory>

#include <boost/asio/buffer.hpp>

namespace tntpp::detail
{
/**
 * Buffer that can be shared between multiple readers
 *
 * @todo add allocator
 */
class SharedBuffer
{
public:
  SharedBuffer(std::size_t size)
      : m_size(size)
      , m_data(std::make_unique<char[]>(size))
  {
  }

  void copy_rest_from(SharedBuffer& other) noexcept
  {
    assert(other.ready() < m_size);
    std::memcpy(m_data.get(), other.m_data.get() + other.m_read_pointer, other.ready());
    advance_writer(other.ready());
    other.advance_reader(other.ready());
  }

  boost::asio::mutable_buffer get_receive_buffer() noexcept
  {
    return boost::asio::mutable_buffer(m_data.get() + m_write_pointer, free());
  }

  boost::asio::const_buffer get_ready_buffer() noexcept
  {
    return boost::asio::const_buffer(m_data.get() + m_read_pointer, ready());
  }

  /**
   * @return number of free bytes in the buffer
   */
  std::size_t free() const noexcept
  {
    assert(m_size >= m_write_pointer);
    return m_size - m_write_pointer;
  }

  /**
   * @return number of bytes that can be processed by the consumer
   */
  std::size_t ready() const noexcept
  {
    assert(m_write_pointer >= m_read_pointer);
    return m_write_pointer - m_read_pointer;
  }

  void advance_reader(std::size_t bytes) noexcept
  {
    m_read_pointer += bytes;
    assert(m_write_pointer >= m_read_pointer);
  }

  void advance_writer(std::size_t bytes) noexcept
  {
    m_write_pointer += bytes;
    assert(m_write_pointer <= m_size);
  }

  bool is_empty() { return m_write_pointer == 0; }

private:
  std::size_t m_write_pointer {0};
  std::size_t m_read_pointer {0};
  const std::size_t m_size;
  std::unique_ptr<char[]> m_data;
};
using SharedBufferSptr = std::shared_ptr<SharedBuffer>;

/**
 * FrozenBuffer is immutable reference to the region in SharedBuffer.
 *
 * This class is cheap to copy and move around
 *
 * @threadsafe This class is thread safe
 */
class FrozenBuffer
{
public:
  FrozenBuffer() = default;
  FrozenBuffer(const void* data, const std::size_t size, const SharedBufferSptr buf)
      : m_data(data)
      , m_size(size)
      , m_buf(buf)
  {
  }
  ~FrozenBuffer() = default;
  FrozenBuffer(const FrozenBuffer&) = default;
  FrozenBuffer(FrozenBuffer&&) = default;
  FrozenBuffer& operator=(const FrozenBuffer&) = default;
  FrozenBuffer& operator=(FrozenBuffer&&) = default;

  const void* data() const noexcept { return m_data; }
  std::size_t size() const noexcept { return m_size; };

  FrozenBuffer slice(std::size_t start,
                     std::size_t length = std::numeric_limits<std::size_t>::max())
  {
    assert(start < m_size);
    if (length == std::numeric_limits<std::size_t>::max()) {
      length = m_size - start;
    }
    return {static_cast<const char*>(m_data) + start,  // NOLINT(*-pro-bounds-pointer-arithmetic)
            length,
            m_buf};
  }

private:
  const void* m_data {nullptr};
  std::size_t m_size {0};
  SharedBufferSptr m_buf {nullptr};
};

/**
 * MutableBuffer class is minimal implementation of bytes::BytesMut
 *
 * The main purpose of this class is to provide buffer for as many messages as possible per
 * allocation. Multiple buffers may refer the same memory region
 *
 * @threadsafety This class is not thread safe
 */
class MutableBuffer
{
public:
  explicit MutableBuffer(std::size_t default_length)
      : m_default_length(default_length)
      , m_buffer(std::make_shared<SharedBuffer>(m_default_length))
  {
  }

  /**
   * Prepares at least the provided number of bytes in the receive buffer
   */
  void prepare(std::size_t bytes)
  {
    if (m_buffer->free() >= bytes) {
      return;
    }

    auto new_buffer = std::make_shared<SharedBuffer>(m_buffer->ready() + bytes + m_default_length);
    new_buffer->copy_rest_from(*m_buffer);
    m_buffer = new_buffer;
  }

  /**
   * returns remaining part of the buffer.
   *
   * If there is no space for new data it gets reallocated so that it will have default_length
   * bytes free to use.
   */
  [[nodiscard]] boost::asio::mutable_buffer get_receive_buffer() noexcept
  {
    if (m_buffer->free() == 0) {
      prepare(m_default_length);
    }
    return m_buffer->get_receive_buffer();
  }

  /**
   * Marks bytes as written
   */
  void advance_writer(std::size_t bytes) noexcept { m_buffer->advance_writer(bytes); }

  /**
   * Marks bytes as consumed
   */
  [[nodiscard]] FrozenBuffer advance_reader(std::size_t bytes)
  {
    auto const_buf = m_buffer->get_ready_buffer();
    assert(const_buf.size() >= bytes);
    FrozenBuffer frozen_buf(const_buf.data(), bytes, m_buffer);
    m_buffer->advance_reader(bytes);
    return frozen_buf;
  }

  /**
   * Returns all ready to consume bytes
   *
   * @warning returned object may get invalidated after any write operation
   */
  [[nodiscard]] boost::asio::const_buffer get_ready_buffer() const noexcept
  {
    return m_buffer->get_ready_buffer();
  }

  std::size_t ready() const { return m_buffer->ready(); }

  void clear()
  {
    if (m_buffer->is_empty()) {
      return;
    }

    auto new_buffer = std::make_shared<SharedBuffer>(m_default_length);
    m_buffer = new_buffer;
  }

private:
  std::size_t m_default_length;
  SharedBufferSptr m_buffer;
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_MUTABLE_BUFFER_H
