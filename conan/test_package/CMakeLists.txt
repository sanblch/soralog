cmake_minimum_required(VERSION 3.1)
project(test_package)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(soralog REQUIRED CONFIG)

add_executable(${PROJECT_NAME} test_package.cpp logging_object.cpp)
target_include_directories(${PROJECT_NAME}
  PRIVATE ${${PROJECT_NAME}_SOURCE_DIR}/include
  )
target_link_libraries(${PROJECT_NAME}
  soralog::soralog
  soralog::configurator_yaml
  soralog::fallback_configurator
  pthread
  )
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
