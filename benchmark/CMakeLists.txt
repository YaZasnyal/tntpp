cmake_minimum_required(VERSION 3.27)

project(tarantool_connector_benchmark)

find_package(benchmark REQUIRED)
add_executable(${PROJECT_NAME} main.cpp)
set_property(
        TARGET ${PROJECT_NAME} PROPERTY
        CXX_EXTENSIONS OFF
)
target_link_libraries(${PROJECT_NAME} PRIVATE
        benchmark::benchmark
        tarantool_connector::tarantool_connector
)
