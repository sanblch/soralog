#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

include_directories(SYSTEM
    PUBLIC
        $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
    PRIVATE
        $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src>
    )

set(INSTALL_TARGETS)
function (add_for_install target)
    list(APPEND INSTALL_TARGETS ${target})
    set(INSTALL_TARGETS ${INSTALL_TARGETS} PARENT_SCOPE)
endfunction()

add_library(sink INTERFACE)
target_link_libraries(sink INTERFACE
    fmt::fmt
    )

add_library(sink_to_nowhere
    impl/sink_to_nowhere.cpp
    )
target_link_libraries(sink_to_nowhere
    sink
    )

add_library(sink_to_console
    impl/sink_to_console.cpp
    )
target_link_libraries(sink_to_console
    sink
    pthread
    )

add_library(sink_to_file
    impl/sink_to_file.cpp
    )
target_link_libraries(sink_to_file
    sink
    pthread
    )

add_library(sink_to_syslog
    impl/sink_to_syslog.cpp
    )
target_link_libraries(sink_to_syslog
    sink
    pthread
    )

add_library(multisink
    impl/multisink.cpp
    )
target_link_libraries(multisink
    sink
    )

add_library(logger
    logger.cpp
    )
target_link_libraries(logger
    sink
    )

add_library(configurator INTERFACE)
target_link_libraries(configurator INTERFACE
    sink_to_nowhere
    sink_to_console
    sink_to_file
    sink_to_syslog
    multisink
    )

add_library(configurator_yaml
    impl/configurator_from_yaml.cpp
    )
target_link_libraries(configurator_yaml
    yaml-cpp::yaml-cpp
    configurator
    )

add_library(fallback_configurator
    impl/fallback_configurator.cpp
    )
target_link_libraries(fallback_configurator
    configurator
    )

add_library(logging_system
    group.cpp
    logging_system.cpp
    )
target_link_libraries(logging_system
    sink
    )

add_library(soralog soralog.cpp)
target_link_libraries(soralog
    logging_system
    sink
    configurator
    logger

    configurator_yaml
    fallback_configurator
    )

add_library(yaml ALIAS configurator_yaml)
add_library(fallback ALIAS fallback_configurator)

add_library(soralog::soralog ALIAS soralog)
add_library(soralog::yaml ALIAS configurator_yaml)
add_library(soralog::fallback ALIAS fallback_configurator)

set(INSTALL_TARGETS
    sink
    sink_to_nowhere
    sink_to_console
    sink_to_file
    sink_to_syslog
    multisink

    configurator
    fallback_configurator
    configurator_yaml

    logger
    logging_system

    soralog
    )

include(GNUInstallDirs)

install(
    DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include/soralog
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS ${INSTALL_TARGETS}
    EXPORT soralog
    LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}/soralog
    ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}/soralog
    RUNTIME       DESTINATION ${CMAKE_INSTALL_BINDIR}/soralog
    INCLUDES      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FRAMEWORK     DESTINATION ${CMAKE_INSTALL_PREFIX}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION
  ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

install(EXPORT soralog
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  EXPORT_LINK_INTERFACE_LIBRARIES
  )

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )
