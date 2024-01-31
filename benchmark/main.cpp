//
// Created by blade on 26.01.2024.
//

#include <thread>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/use_future.hpp>

#include <benchmark/benchmark.h>
#include <tarantool_connector/tarantool_connector.hpp>
#include <tarantool_connector/tntpp_cout_logger.h>

static void simple_loop(benchmark::State& s)
{
  static boost::asio::io_context ctx;
  static std::jthread thread(
      [&]()
      {
        auto w = boost::asio::io_context::work(ctx);
        ctx.run();
      });

  static tntpp::StdoutLogger logger(tntpp::LogLevel::Error);
  boost::asio::co_spawn(
      ctx,
      [&]() -> boost::asio::awaitable<void>
      {
        for (auto _ : s) {
           auto [ec, conn] = co_await tntpp::Connector::connect(
              ctx.get_executor(),
              tntpp::Config().host("192.168.4.2").port(3301).logger(&logger),
              boost::asio::as_tuple(boost::asio::use_awaitable));
        }
        co_return;
      },
      boost::asio::use_future)
      .get();
  // ctx.stop();
}
BENCHMARK(
    simple_loop);  //->ThreadRange(1, std::thread::hardware_concurrency());

BENCHMARK_MAIN();
