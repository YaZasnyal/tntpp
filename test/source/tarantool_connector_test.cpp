#include "tarantool_connector/tarantool_connector.hpp"

#include <boost/asio/use_future.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Name is tarantool_connector", "[library]")
{
  // boost::asio::io_context ctx;
  // auto f = tntpp::Connector::connect(
  //     ctx.get_executor(), "localhost", 3301, boost::asio::use_future);
  // ctx.run();
  // auto val = f.get();
  // REQUIRE(val == 42);
}
