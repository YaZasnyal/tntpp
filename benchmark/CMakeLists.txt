cmake_minimum_required(VERSION 3.27)

project(tarantool_connector_benchmark)

find_package(benchmark REQUIRED)
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE
        benchmark::benchmark
        tarantool_connector::tarantool_connector
)
