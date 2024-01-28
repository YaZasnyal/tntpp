if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_INSTALL_INCLUDEDIR include/tarantool_connector CACHE PATH "")
endif()

# Project is configured with no languages, so tell GNUInstallDirs the lib dir
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package tarantool_connector)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT tarantool_connector_Development
)

install(
    TARGETS tarantool_connector_tarantool_connector
    EXPORT tarantool_connectorTargets
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

# Allow package maintainers to freely override the path for the configs
set(
    tarantool_connector_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(tarantool_connector_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${tarantool_connector_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT tarantool_connector_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${tarantool_connector_INSTALL_CMAKEDIR}"
    COMPONENT tarantool_connector_Development
)

install(
    EXPORT tarantool_connectorTargets
    NAMESPACE tarantool_connector::
    DESTINATION "${tarantool_connector_INSTALL_CMAKEDIR}"
    COMPONENT tarantool_connector_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
