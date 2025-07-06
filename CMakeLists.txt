cmake_minimum_required(VERSION 3.23)

set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

set(CMAKE_CXX_STANDARD 23)
set(LRW_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(LRW_CMAKE_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/cmake)
add_compile_options(-Wall -Wno-unused-function -Wno-unused-variable)

# add_definitions('-DRCOMMON_NAMESPACE=LRW')
# add_definitions('-DRCOMMON_NAMESPACE_BEGIN=namespace\ LRW\{')
# add_definitions('-DRCOMMON_NAMESPACE_END=\}')

include(${LRW_CMAKE_ROOT_DIR}/Init.cmake)
include(${LRW_CMAKE_ROOT_DIR}/SetupLTO.cmake)
include(${LRW_CMAKE_ROOT_DIR}/Sanitizers.cmake)
# include(${LRW_CMAKE_ROOT_DIR}/DetectPlatform.cmake)
include(${LRW_CMAKE_ROOT_DIR}/Logging.cmake)
include(${LRW_CMAKE_ROOT_DIR}/CompileOptionsSupport.cmake)
include(${LRW_CMAKE_ROOT_DIR}/Filepath.cmake)

log_info(${PLATFORM})
project(libcoro)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

set(LIB_NAME libcoro)

set(SUBMODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
option(CORO_BUILD_TESTS "Build tests" ON)

add_library(
  ${LIB_NAME} ${INCLUDE_DIR}/coro/concepts/range_of.hpp
              ${INCLUDE_DIR}/coro/thread_pool.hpp ${SRC_DIR}/thread_pool.cpp)

target_include_directories(${LIB_NAME} PUBLIC ${INCLUDE_DIR})
add_executable(libcoro_exec src/main.cpp)
target_include_directories(libcoro_exec PRIVATE ${INCLUDE_DIR})
target_link_libraries(libcoro_exec ${LIB_NAME})

# target_compile_definitions(${LIB_NAME} PUBLIC "RCOMMON_NAMESPACE=LRW")
# target_compile_definitions(${LIB_NAME} PUBLIC
# "RCOMMON_NAMESPACE_BEGIN=namespace\ LRW\{")
# target_compile_definitions(${LIB_NAME} PUBLIC "RCOMMON_NAMESPACE_END=\}")
# target_include_directories(${LIB_NAME} PRIVATE ${SRC_DIR})

# endif() cut_filepath(${LIB_NAME}) cut_filepath(libcoro_exec)

if(CORO_BUILD_TESTS)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

# sanitize_target(exe)
