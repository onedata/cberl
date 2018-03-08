##
# Author: Krzysztof Trzepla
# Copyright (C) 2017: Krzysztof Trzepla
# This software is released under the MIT license cited in 'LICENSE.md'
#

cmake_minimum_required(VERSION 3.1.0)
project(cberl C CXX)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY True)
set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Setup Threads
find_package(Threads REQUIRED)

# Setup Erlang
find_package(Erlang REQUIRED)

# Setup Asio
set(ASIO_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include)

find_library(COUCHBASE_LIBRARY couchbase)

if(APPLE)
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS
        "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -flat_namespace -undefined dynamic_lookup")
endif(APPLE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED On)
add_compile_options(-Wall -Wshadow -Wpedantic)
add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
add_definitions(-DASIO_STANDALONE)

include_directories(src)

add_subdirectory(src)

add_library(cberl SHARED ${PROJECT_SOURCES})
target_link_libraries(cberl PRIVATE ${CBERL_LIBRARIES})
target_include_directories(cberl SYSTEM PRIVATE ${CBERL_SYSTEM_INCLUDE_DIRS})
target_include_directories(cberl PRIVATE ${CBERL_INCLUDE_DIRS})
set_target_properties(cberl PROPERTIES SUFFIX .so)

install(TARGETS cberl DESTINATION .)

#
# Below targets are used to build C++ only cberl benchmark
# executable without NIF compiled in
#
add_library(cberl_static STATIC EXCLUDE_FROM_ALL ${PROJECT_SOURCES})
target_link_libraries(cberl_static PRIVATE ${CBERL_LIBRARIES})
target_compile_definitions(cberl_static PUBLIC NO_ERLANG=1)
target_include_directories(cberl_static SYSTEM PRIVATE ${CBERL_SYSTEM_INCLUDE_DIRS})
target_include_directories(cberl_static PRIVATE ${CBERL_INCLUDE_DIRS})

add_executable(cberl_benchmark EXCLUDE_FROM_ALL benchmark/benchmark.cc)
target_link_libraries(cberl_benchmark cberl_static ${CBERL_LIBRARIES})
target_compile_definitions(cberl_benchmark PUBLIC NO_ERLANG=1)
target_include_directories(cberl_benchmark SYSTEM PRIVATE ${CBERL_SYSTEM_INCLUDE_DIRS})
target_include_directories(cberl_benchmark PRIVATE ${CBERL_INCLUDE_DIRS})

