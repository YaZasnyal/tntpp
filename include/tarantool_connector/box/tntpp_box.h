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

class Box;

class SpaceVariant
{
public:
  using ValueType = std::variant<std::monostate, std::string_view, detail::iproto::MpUint>;

  SpaceVariant() = default;
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

  ValueType value {std::monostate{}};
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

enum class SelectIterator : detail::iproto::MpUint
{
  EQ = 0,
  REQ,
  ALL,
  LT,
  TE,
  GE,
  GT,
  BITS_ALL_SET,
  BITS_ANY_SET,
  BITS_ALL_NOT_SET,
  OVERLAPS,
  NEIGHBOR,
};

class SelectBuilder;

class SelectRequest
{
protected:
  SelectRequest() = default;
  friend class SelectBuilder;
  friend class Box;
  
  detail::iproto::MpUint m_sync;
  Space m_space;
  Index m_index;
  detail::RequestPacker packer;
};

/**
 * SelectBuilder allows to create any select statement
 *
 * Use Box::get_select_builder to create a new builder
 *
 * @warning each method MUST NOT be called more than once
 */
class SelectBuilder
{
public:
  SelectBuilder(Space space, detail::iproto::MpUint request_id)
  {
    m_req.m_sync = request_id;
    m_req.m_space = space;

    detail::iproto::MessageHeader header;
    header.sync = request_id;
    header.set_request_type(detail::iproto::RequestType::Select);
    m_req.packer.pack(header);
    m_req.packer.begin_map();
  }
  SelectBuilder(Space space, Index index, detail::iproto::MpUint request_id)
  {
    m_req.m_sync = request_id;
    m_req.m_space = space;
    m_req.m_index = index;

    detail::iproto::MessageHeader header;
    header.sync = request_id;
    header.set_request_type(detail::iproto::RequestType::Select);
    m_req.packer.pack(header);
    m_req.packer.begin_map();
  }
  SelectBuilder(const SelectBuilder&) = delete;
  SelectBuilder(SelectBuilder&&) = default;
  SelectBuilder& operator=(const SelectBuilder&) = delete;
  SelectBuilder& operator=(SelectBuilder&&) = default;

  SelectBuilder& index(Index index) noexcept
  {
    m_req.m_index = index;
    return *this;
  }

  SelectBuilder& limit(detail::iproto::MpUint value)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::Limit, value);
    return *this;
  }

  SelectBuilder& offset(detail::iproto::MpUint value)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::Offset, value);
    return *this;
  }

  SelectBuilder& iterator(SelectIterator it)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::Iterator,
                          static_cast<detail::iproto::MpUint>(it));
    return *this;
  }

  template<class K>
  SelectBuilder& key(K&& key)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::Key, std::forward<decltype(key)>(key));
    return *this;
  }

  SelectBuilder& after_position(std::string pos)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::AfterPosition, pos);
    return *this;
  }

  template<class K>
  SelectBuilder& after_tuple(K&& pos)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::AfterTuple, std::forward<decltype(pos)>(pos));
    return *this;
  }

  SelectBuilder& fetch_position(bool val)
  {
    m_req.packer.pack_map_entry(detail::iproto::FieldType::FetchPosition, val);
    return *this;
  }

  /**
   * @warning invalidates the object
   */
  SelectRequest build()
  {
    return std::move(m_req);
  }

private:
  friend class Box;

  SelectRequest extract_packer()
  {
    return std::move(m_req);
  }

  // Space m_space;
  // Index m_index;
  // detail::RequestPacker packer;
  SelectRequest m_req;
};

struct SelectResult
{
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
              if (std::holds_alternative<std::monostate>(space.value)) {
                co_return state.complete(error_code {boost::system::errc::invalid_argument,
                                                     boost::system::system_category()},
                                         0);
              }
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
              if (std::holds_alternative<std::monostate>(index.value)) {
                co_return state.complete(error_code {boost::system::errc::invalid_argument,
                                                     boost::system::system_category()},
                                         {});
              }
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
  template<class K, class H>
  auto select(Space space, K&& key, H&& handle)
  {
  }

  // template<class K, class H>
  // auto select(Space space, K&& key, SelectOptions o, H&& handle)
  // {
  // }

  template<class K, class H>
  auto select(Space space, Index index, K&& key, H&& handle)
  {
  }

  template<class H>
  auto select(SelectRequest&& builder, H&& handle)
  {
    return boost::asio::async_initiate<H, void(error_code, IprotoFrame)>(
        TNTPP_CO_COMPOSED<void(error_code, IprotoFrame)>(
            [this](auto state, SelectRequest builder) -> void
            {
              auto [ec, space_index] =
                  co_await get_space_index(builder.m_space, boost::asio::as_tuple(boost::asio::deferred));
              if (ec) {
                // @todo log error
                co_return state.complete(ec, IprotoFrame {});
              }
              auto packer = std::move(builder.packer);
              packer.pack_map_entry(detail::iproto::FieldType::SpaceId, space_index);
              
              // fetch index
              packer.finalize_map();
              packer.finalize();

              auto res = co_await m_state->m_conn->send_request(
                  builder.m_sync, std::move(packer), boost::asio::redirect_error(boost::asio::deferred, ec));
              co_return state.complete(ec, std::move(res));
            }),
        handle,
        std::move(builder));
  }

  SelectBuilder get_select_builder(Space space)
  {
    return SelectBuilder(space, m_state->m_conn->generate_id());
  }

  SelectBuilder get_select_builder(Space space, Index index)
  {
    return SelectBuilder(space, index, m_state->m_conn->generate_id());
  }

  // template<class K, class H>
  // auto select(Space space, Index index, K&& key, SelectOptions o, H&& handle)
  // {
  // }

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
