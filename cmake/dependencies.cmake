#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

if (TESTING OR COVERAGE)
  if(PACMAN STREQUAL "HUNTER")
    hunter_add_package(GTest)
  endif()
  find_package(GTest CONFIG REQUIRED)
endif()

if(PACMAN STREQUAL "HUNTER")
  hunter_add_package(fmt)
  hunter_add_package(yaml-cpp)
endif()

find_package(yaml-cpp CONFIG REQUIRED)
if (NOT TARGET yaml-cpp::yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

find_package(fmt CONFIG REQUIRED)
