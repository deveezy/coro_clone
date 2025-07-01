include(CheckIPOSupported)
check_ipo_supported(LANGUAGES CXX)

option(LRW_LTO_CACHE "Use lto cache for link time optimizations" ON)
set(LRW_LTO_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/.ltocache" CACHE PATH "LTO cache directory")
set(LRW_LTO_CACHE_SIZE_MB "6000" CACHE STRING "LTO cache size limit in MB")

# Sets -flto-thin for Clang and -flto for GCC for compile and link time
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

if (NOT LRW_LTO_CACHE OR NOT LRW_LTO_CACHE_DIR)
  message(STATUS "LTO cache disabled: user request")
elseif(NOT LRW_USE_LD MATCHES "lld")
  message(STATUS "LTO cache disabled: linker should be 'lld'")
else()
  message(STATUS "LRW_LTO_CACHE enabled, cache directory is ${LRW_LTO_CACHE_DIR}")
  file(MAKE_DIRECTORY "${LRW_LTO_CACHE_DIR}")

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--thinlto-cache-dir=${LRW_LTO_CACHE_DIR}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--thinlto-cache-dir=${LRW_LTO_CACHE_DIR}")

  if (LRW_LTO_CACHE_SIZE_MB)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--thinlto-cache-policy,cache_size_bytes=${LRW_LTO_CACHE_SIZE_MB}m:cache_size=0%:prune_interval=5m")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--thinlto-cache-policy,cache_size_bytes=${LRW_LTO_CACHE_SIZE_MB}m:cache_size=0%:prune_interval=5m")
  endif()
endif()

# check_ipo_supported() may not check linkage that may fail due to wrong linker.
# Using a more complex check.
include(CheckIncludeFileCXX)
check_include_file_cxx(string LTO_SETUP_SUCCESSFUL)
if (NOT LTO_SETUP_SUCCESSFUL)
  message(FATAL_ERROR
      "LTO fails to compile a trivial program. See error logs for info. "
      "Try specifying another linker or changing the CMAKE_AR, "
      "CMAKE_RANLIB, CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS"
  )
endif()
