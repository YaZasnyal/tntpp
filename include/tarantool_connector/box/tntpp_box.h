//
// Created by root on 2/3/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_BOX_H
#define TARANTOOL_CONNECTOR_TNTPP_BOX_H

#include <memory>
#include <string_view>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/deferred.hpp>

#include "tarantool_connector/detail/iproto_typedefs.h"
#include "tarantool_connector/detail/tntpp_request.h"
#include "tarantool_connector/tarantool_connector.hpp"

namespace tntpp::box
{

class SpaceVariant
{
public:
  using ValueType = std::variant<std::string_view, detail::iproto::MpUint>;

  explicit SpaceVariant(detail::iproto::MpUint val)
      : value(val)
  {
  }
  explicit SpaceVariant(std::string_view val)
      : value(val)
  {
  }
  ~SpaceVariant() = default;
  SpaceVariant(const SpaceVariant&) = default;
  SpaceVariant(SpaceVariant&&) = default;
  SpaceVariant& operator=(const SpaceVariant&) = default;
  SpaceVariant& operator=(SpaceVariant&&) = default;

  ValueType value;
};

/**
 * Strong typedef pattern that holds either MpUint or string_view as a space identifier
 *
 * @code
 * auto space = tntpp::box::Space(512);
 * auto space = tntpp::box::Space("my_space");
 * @endcode
 */
class Space : public SpaceVariant
{
public:
  using SpaceVariant::SpaceVariant;
};

/**
 * Strong typedef pattern that holds either MpUint or string_view as a index identifier
 * @code
 * auto index = tntpp::box::Index(512);
 * auto index = tntpp::box::Index("secondary_index");
 * @endcode
 */
class Index : public SpaceVariant
{
public:
  using SpaceVariant::SpaceVariant;
};

class Box
{
  template<class H>
  auto get_space_index(SpaceVariant space, H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, detail::iproto::SpaceId)>(
        TNTPP_CO_COMPOSED<void(error_code, detail::iproto::SpaceId)>(
            [this](auto state, SpaceVariant space) -> void
            {
              if (std::holds_alternative<detail::iproto::SpaceId>(space.value)) {
                co_return state.complete(error_code {},
                                         std::get<detail::iproto::SpaceId>(space.value));
              }

              if (!m_state->m_conn->get_executor().running_in_this_thread()) {
                co_await m_state->m_conn->enter_executor(boost::asio::deferred);
              }
              if (m_state->m_schema_version != m_state->m_conn->get_schema_version()) {
                // @todo refetch
              }
              // look for space id in the map
              // if schema_version mismatch -> refetch schema first

              co_return state.complete(error_code {}, 0);
            }),
        handle,
        space);
  }

  template<class H>
  auto get_index_id(Index index, H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, detail::iproto::IndexId)>(
        TNTPP_CO_COMPOSED<void(error_code, detail::iproto::IndexId)>(
            [this](auto state, SpaceVariant index) -> void
            {
              // no need to refetch schema here because indexes are fetched with spaces
              if (std::holds_alternative<detail::iproto::IndexId>(index.value)) {
                co_return state.complete(error_code {},
                                         std::get<detail::iproto::IndexId>(index.value));
              }

              if (!m_state->m_conn->get_executor().running_in_this_thread()) {
                co_await m_state->m_conn->enter_executor(boost::asio::deferred);
              }
              // @todo implement
              co_return state.complete(error_code {}, 0);
            }),
        handle,
        index);
  }

public:
  explicit Box(ConnectorSptr& conn)
      : m_state(new BoxInternal {conn})
  {
  }
  Box(const Box&) = delete;
  Box(Box&&) = default;
  Box& operator=(const Box&) = delete;
  Box& operator=(Box&&) = default;
  ~Box() = default;

  // select

  /**
   * Insert new tuple into the space
   *
   * @tparam T key type
   * @tparam H completion handler type [void(error_code, IprotoFrame)]
   * @param space space index or name
   * @param tuple data to insert
   * @param handle completion handler
   * @return the inserted tuple
   */
  template<class T, class H>
  auto insert(Space space, T&& tuple, H&& handle)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.set_request_type(detail::iproto::RequestType::Insert);
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(tuple)>(tuple));

    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state,
                   detail::iproto::MpUint sync,
                   Space space,
                   detail::RequestPacker packer) -> void
            {
              auto [ec, space_index] =
                  co_await get_space_index(space, boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // @todo log error
                co_return state.complete(ec, IprotoFrame {});
              }
              packer.pack_map_entry(detail::iproto::FieldType::SpaceId, space_index);
              packer.finalize();

              auto res = co_await m_state->m_conn->send_request(
                  sync, std::move(packer), boost::asio::redirect_error(boost::asio::deferred, ec));
              co_return state.complete(ec, std::move(res));
            }),
        handle,
        header.sync,
        space,
        std::move(packer));
  }

  // upsert
  // update

  /**
   * Replace existing tuple with a new one
   *
   * @tparam T key type
   * @tparam H completion handler type [void(error_code, IprotoFrame)]
   * @param space space index or name
   * @param tuple data to insert
   * @param handle completion handler
   * @return the inserted tuple
   */
  template<class T, class H>
  auto replace(Space space, T&& tuple, H&& handle)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.set_request_type(detail::iproto::RequestType::Replace);
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::Tuple, std::forward<decltype(tuple)>(tuple));

    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state,
                   detail::iproto::MpUint sync,
                   Space space,
                   detail::RequestPacker packer) -> void
            {
              auto [ec, space_index] =
                  co_await get_space_index(space, boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // @todo log error
                co_return state.complete(ec, IprotoFrame {});
              }
              packer.pack_map_entry(detail::iproto::FieldType::SpaceId, space_index);
              packer.finalize();

              auto res = co_await m_state->m_conn->send_request(
                  sync, std::move(packer), boost::asio::redirect_error(boost::asio::deferred, ec));
              co_return state.complete(ec, std::move(res));
            }),
        handle,
        header.sync,
        space,
        std::move(packer));
  }

  /**
   * Remove tuple with specified index
   *
   * @tparam T key type
   * @tparam H completion handler type [void(error_code, IprotoFrame)]
   * @param space space index or name
   * @param key key to remove
   * @param handle completion handler
   * @return removed tuple
   */
  template<class T, class H>
  auto remove(Space space, T&& key, H&& handle)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.set_request_type(detail::iproto::RequestType::Delete);
    packer.pack(header);
    packer.begin_map(2);
    packer.pack_map_entry(detail::iproto::FieldType::Key, std::forward<decltype(key)>(key));

    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state,
                   detail::iproto::MpUint sync,
                   Space space,
                   detail::RequestPacker packer) -> void
            {
              auto [ec, space_index] =
                  co_await get_space_index(space, boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // log error
                co_return state.complete(ec, IprotoFrame {});
              }
              //  @todo implement
              packer.pack_map_entry(detail::iproto::FieldType::SpaceId, space_index);
              packer.finalize();

              auto res = co_await m_state->m_conn->send_request(
                  sync, std::move(packer), boost::asio::redirect_error(boost::asio::deferred, ec));
              // if error
              //   check no such space and no such index
              //     refetch schema and reexecute
              co_return state.complete(ec, std::move(res));
            }),
        handle,
        header.sync,
        space,
        std::move(packer));
  }

  /**
   *
   * @tparam T key type
   * @tparam H completion handler type [void(error_code, IprotoFrame)]
   * @param space space id or name
   * @param index index id or name
   * @param key key to remove
   * @param handle completion handler
   * @return removed tuple
   */
  template<class T, class H>
  auto remove(Space space, Space index, T&& key, H&& handle)
  {
    detail::RequestPacker packer;
    detail::iproto::MessageHeader header;
    header.sync = m_state->m_conn->generate_id();
    header.set_request_type(detail::iproto::RequestType::Delete);
    packer.pack(header);
    packer.begin_map(3);
    packer.pack_map_entry(detail::iproto::FieldType::Key, std::forward<decltype(key)>(key));

    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state,
                   detail::iproto::MpUint sync,
                   Space space,
                   Space index,
                   detail::RequestPacker packer) -> void
            {
              auto [ec, space_index] =
                  co_await get_space_index(space, boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // @todo log error
                co_return state.complete(ec, IprotoFrame {});
              }
              auto index_index = co_await get_index_id(
                  index, boost::asio::redirect_error(boost::asio::deferred, ec));
              if (ec) {
                // log error
                co_return state.complete(ec, IprotoFrame {});
              }
              //  @todo implement
              packer.pack_map_entry(detail::iproto::FieldType::SpaceId, space_index);
              packer.pack_map_entry(detail::iproto::FieldType::IndexId, index_index);
              packer.finalize();

              auto res = co_await m_state->m_conn->send_request(
                  sync, std::move(packer), boost::asio::redirect_error(boost::asio::deferred, ec));
              // if error
              //   check no such space and no such index
              //     refetch schema and reexecute
              co_return state.complete(ec, std::move(res));
            }),
        handle,
        header.sync,
        space,
        index,
        std::move(packer));
  }

  // streams
  //   insert
  //   commit
  //   rollback

  // sql

  /**
   * Allows to get the underlying connection. May be useful to create another
   * instances of Box.
   * @return connector
   */
  ConnectorSptr& get_connector() const { return m_state->m_conn; }

private:
  // fetch schema
  template<class H>
  auto fetch_schema(H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code)>(TNTPP_CO_COMPOSED<void(error_code)>(
                                                                [this](auto state)
                                                                {
                                                                  // @todo implement
                                                                }),
                                                            handle);
  }

  struct BoxInternal
  {
    ConnectorSptr m_conn {nullptr};
    detail::iproto::MpUint m_schema_version {0};
    // async lock
    // space and index map
  };

  std::shared_ptr<BoxInternal> m_state;
};

}  // namespace tntpp::box

#endif  // TARANTOOL_CONNECTOR_TNTPP_BOX_H
