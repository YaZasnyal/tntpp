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

  static tntpp::StdoutLogger logger(tntpp::LogLevel::Debug);
  auto conn =
      tntpp::Connector::connect(ctx.get_executor(),
                                tntpp::Config().host("192.168.4.2").port(3301).logger(&logger),
                                //.credentials("test", "test123"),
                                boost::asio::use_future)
          .get();
  auto res = conn->eval("ghjghjg ...", std::vector {1, 2, 3}, boost::asio::use_future).get();
  TNTPP_LOG(&logger,
            Info,
            "is_error={}; {}; {}",
            res.is_error(),
            res.get_error_code().what(),
            res.get_error_string());
  //            fmt::join(*res.as<std::optional<std::vector<int>>>(), ", "));
  auto res2 = conn->call("help", std::make_tuple(), boost::asio::use_future)
                  .get()
                  .as<std::vector<std::string>>();
  TNTPP_LOG(&logger, Info, "\n{}", fmt::join(std::get<0>(res2), " "));

  tntpp::error_code ec {};
  auto res3 = conn->eval("return nil, \"qwerty\"", std::make_tuple(), boost::asio::use_future)
                  .get()
                  .as<std::optional<std::vector<int>>, std::string>(ec);
  TNTPP_LOG(&logger, Info, "has_value={}, error={}", std::get<0>(res3).has_value(), ec.message());

  conn->ping(boost::asio::use_future).get();

  // check box
  tntpp::box::Box box(conn);

  auto create_res = conn->eval(
          R"--(
bands = box.schema.space.create('bands',
    {id=512,
     if_not_exists=true,
     format={
         { name = 'id', type = 'unsigned' },
         { name = 'band_name', type = 'string' },
         { name = 'year', type = 'unsigned' }
     }
});
)--",
          std::make_tuple(),
          boost::asio::use_future)
      .get().as<std::string>();
  TNTPP_LOG(&logger, Info, "error={}", std::get<0>(create_res));

  box.remove(tntpp::box::Space(512), std::make_tuple(1), boost::asio::use_future).get();
  box.remove(tntpp::box::Space("bands"), std::make_tuple(1), boost::asio::use_future).get();

  std::exit(-1);

  for (auto _ : s) {
    auto f1 = conn->ping(boost::asio::as_tuple(boost::asio::use_future));
    conn->eval("help", std::vector<int>(), boost::asio::use_future).get();
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
