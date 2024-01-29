cmake_minimum_required(VERSION 3.27)

include(cmake/prelude.cmake)

project(
        tarantool_connector
        VERSION 0.1.0
        DESCRIPTION "C++ connector for Tarantool based on ASIO"
        HOMEPAGE_URL "https://example.com/"
        LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(tarantool_connector_tarantool_connector INTERFACE)
add_library(tarantool_connector::tarantool_connector ALIAS tarantool_connector_tarantool_connector)

target_sources(tarantool_connector_tarantool_connector PRIVATE
        include/tarantool_connector/tarantool_connector.hpp
        include/tarantool_connector/connection.h
        include/tarantool_connector/tntpp_config.h
        include/tarantool_connector/tntpp_logger.h
        include/tarantool_connector/tntpp_cout_logger.h
        include/tarantool_connector/detail/asio_printers.h
        include/tarantool_connector/detail/iproto_typedefs.h
        include/tarantool_connector/detail/tntpp_iproto_framing.h
        include/tarantool_connector/detail/tntpp_operation.h
        include/tarantool_connector/detail/tntpp_queue.h
        include/tarantool_connector/detail/tntpp_typedefs.h
)

set_property(
        TARGET tarantool_connector_tarantool_connector PROPERTY
        EXPORT_NAME tarantool_connector
)

target_include_directories(
        tarantool_connector_tarantool_connector ${warning_guard}
        INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(tarantool_connector_tarantool_connector INTERFACE cxx_std_20)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
target_link_libraries(tarantool_connector_tarantool_connector
        INTERFACE
        Boost::headers
        fmt::fmt
)

add_subdirectory(benchmark)

# ---- Install rules ----

if (NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/install-rules.cmake)
endif ()

# ---- Developer mode ----

if (NOT tarantool_connector_DEVELOPER_MODE)
    return()
elseif (NOT PROJECT_IS_TOP_LEVEL)
    message(
            AUTHOR_WARNING
            "Developer mode is intended for developers of tarantool_connector"
    )
endif ()

include(cmake/dev-mode.cmake)
