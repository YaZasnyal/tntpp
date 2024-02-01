cmake_minimum_required(VERSION 3.14)

project(tarantool_connectorTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if (PROJECT_IS_TOP_LEVEL)
    find_package(tarantool_connector REQUIRED)
    enable_testing()
endif ()

find_package(GTest REQUIRED)

# ---- Tests ----

add_executable(tarantool_connector_test
        source/tarantool_connector_test.cpp
        source/mutable_buffer_test.cpp
)
target_link_libraries(tarantool_connector_test PRIVATE
        tarantool_connector::tarantool_connector
        GTest::gtest_main
)
target_compile_features(tarantool_connector_test PRIVATE cxx_std_20)

# ---- End-of-file commands ----

add_folders(Test)
