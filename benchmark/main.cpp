//
// Created by blade on 26.01.2024.
//

#include <thread>

#include <benchmark/benchmark.h>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <tarantool_connector/tarantool_connector.hpp>
#include <tarantool_connector/tntpp_cout_logger.h>

static void simple_loop(benchmark::State& s)
{
  boost::asio::io_context ctx;
  std::jthread thread(
      [&]()
      {
        auto w = boost::asio::io_context::work(ctx);
        ctx.run();
      });

  static tntpp::StdoutLogger logger(tntpp::LogLevel::Info);
  auto conn =
      tntpp::Connector::connect(ctx.get_executor(),
                                tntpp::Config().host("192.168.4.2").port(3301).logger(&logger),
                                boost::asio::use_future)
          .get();

  for (auto _ : s) {
    conn->ping(boost::asio::as_tuple(boost::asio::use_future)).get();
  }

  ctx.stop();
}
BENCHMARK(simple_loop);  //->ThreadRange(1, std::thread::hardware_concurrency());

BENCHMARK_MAIN();
