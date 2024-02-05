//
// Created by blade on 26.01.2024.
//

#include <thread>

#include <benchmark/benchmark.h>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <tarantool_connector/box/tntpp_box.h>
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

  static tntpp::StdoutLogger logger(tntpp::LogLevel::Fatal);
  auto conn =
      tntpp::Connector::connect(ctx.get_executor(),
                                tntpp::Config().host("192.168.4.2").port(3301).logger(&logger),
                                boost::asio::use_future)
          .get();
  tntpp::box::Box box(conn);
  auto res = box.eval("return ...", std::make_tuple(1,2,3), boost::asio::use_future).get();
  std::cout << static_cast<const char*>(res.get_raw_body().data()) << "\n";
  res = box.call("help", std::make_tuple(std::vector{1,2,3}), boost::asio::use_future).get();
  std::cout << static_cast<const char*>(res.get_raw_body().data()) << "\n";
  std::exit(-1);

  for (auto _ : s) {
    auto f1 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));
    box.eval("help", std::vector<int>(), boost::asio::use_future).get();
    //    auto f2 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));
    //    auto f3 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));
    //    auto f4 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));
    //    auto f5 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));

    f1.get();
    //    f2.get();
    //    f3.get();
    //    f4.get();
    //    f5.get();
  }

  ctx.stop();
}
BENCHMARK(simple_loop);  //->ThreadRange(1, std::thread::hardware_concurrency());

BENCHMARK_MAIN();
