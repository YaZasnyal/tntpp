//
// Created by blade on 28.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_QUEUE_H
#define TARANTOOL_CONNECTOR_TNTPP_QUEUE_H

#include <vector>

namespace tntpp::detail
{

/**
 * Double buffer queue
 */
template<class T>
class DBQueue
{
public:
  DBQueue(std::size_t default_size)
      : m_default_size(default_size)
  {
    m_active.reserve(m_default_size);
    m_inactive.reserve(m_default_size);
  }

  template<class V>
  void push(V&& value)
  {
    m_active.push_back(std::forward<decltype(value)>(value));
  }

  /**
   * Checks if there is data in the active queue
   */
  bool is_ready() {
    return !m_active.empty();
  }

  const std::vector<T>& get_inactive() const { return m_inactive; }

  [[nodiscard]] const std::vector<T>& swap()
  {
    std::swap(m_inactive, m_active);
    if (m_default_size != 0 && m_active.size() > m_default_size) {
      m_active.resize(m_default_size);
    }
    m_active.clear();

    return m_inactive;
  }

  void clear()
  {
    if (m_default_size != 0 && m_active.size() > m_default_size) {
      m_active.resize(m_default_size);
    }
    if (m_default_size != 0 && m_inactive.size() > m_default_size) {
      m_inactive.resize(m_default_size);
    }
    m_active.clear();
    m_inactive.clear();
  }

private:
  std::size_t m_default_size;
  std::vector<T> m_active;  /// the one that is used to store new events
  std::vector<T> m_inactive;  /// the one that is immutable
};

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_QUEUE_H
