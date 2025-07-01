include_guard(GLOBAL)

set_property(GLOBAL PROPERTY lrw_cmake_dir "${CMAKE_CURRENT_LIST_DIR}")

function(sanitize_target TARGET_NAME)
  get_property(LRW_ROOT_DIR GLOBAL PROPERTY lrw_cmake_dir)

  set(LRW_SANITIZE_ENUM "mem, addr, thread, ub")

  set(LRW_SANITIZE "" CACHE STRING "Sanitizer, possible values: ${LRW_SANITIZE_ENUM}")
  set(LRW_SANITIZE_BLACKLIST "" CACHE FILEPATH "Blacklist for sanitizers")

  set(sanitizers_supported ON)
  if (LRW_SANITIZE AND
      CMAKE_SYSTEM_NAME MATCHES "Darwin" AND
      CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(WARNING
        "Sanitizers on arm64 macOS produce false positives "
        "on coroutine-context switching. Disabling")
    set(sanitizers_supported OFF)
  endif()

  set(sanitize_cxx_and_link_flags "-g")
  set(sanitize_cxx_flags)
  set(sanitize_link_flags)
  set(sanitizer_list)

  message(STATUS ${LRW_SANITIZE})
  if (LRW_SANITIZE AND sanitizers_supported)
    set(sanitizer_list ${LRW_SANITIZE})
    separate_arguments(sanitizer_list)
    list(REMOVE_DUPLICATES sanitizer_list)
    list(LENGTH sanitizer_list sanitizer_count)

    foreach(sanitizer IN LISTS sanitizer_list)
      if (sanitizer STREQUAL "thread")
        # https://clang.llvm.org/docs/ThreadSanitizer.html
        if (sanitizer_count GREATER 1)
          message(WARNING "ThreadSanitizer should not be combined with other sanitizers")
        endif()
        list(APPEND sanitize_cxx_and_link_flags -fsanitize=thread)

      elseif (sanitizer STREQUAL "ub")
        # https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
        list(APPEND sanitize_cxx_and_link_flags -fsanitize=undefined -fno-sanitize-recover=undefined)

      elseif (sanitizer STREQUAL "addr")
        # https://clang.llvm.org/docs/AddressSanitizer.html
        list(APPEND sanitize_cxx_and_link_flags -fsanitize=address)
        if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
          # gcc links with ASAN dynamically by default, and that leads to all sorts of problems
          # when we intercept dl_iterate_phdr, which ASAN uses in initialization before main.
          # list(APPEND sanitize_cxx_and_link_flags -static-libasan)
        endif()
        list(APPEND sanitize_cxx_flags -fno-omit-frame-pointer)

      elseif (sanitizer STREQUAL "mem")
        # https://clang.llvm.org/docs/MemorySanitizer.html
        list(APPEND sanitize_cxx_and_link_flags -fsanitize=memory)
        list(APPEND sanitize_cxx_flags -fno-omit-frame-pointer)

      else()
        message(FATAL_ERROR
          "-DLRW_SANITIZE has invalid value(s) (${SANITIZE_PENDING}), "
          "possible values: ${LRW_SANITIZE_ENUM}")
      endif()
    endforeach()

    message(STATUS "Sanitizers are ON: ${LRW_SANITIZE}")
  endif()

  message(STATUS ${TARGET_NAME})
  message(STATUS ${sanitize_cxx_flags})
  message(STATUS ${sanitize_cxx_and_link_flags})

  target_compile_options(${TARGET_NAME} PRIVATE ${sanitize_cxx_flags} ${sanitize_cxx_and_link_flags})
  target_link_options(${TARGET_NAME} PRIVATE ${sanitize_link_flags} ${sanitize_cxx_and_link_flags})

endfunction()


# function(_lrw_make_sanitize_target)
#   if (TARGET lrw-internal-sanitize-options)
#     return()
#   endif()
#
#   _lrw_get_sanitize_options(sanitizer_list sanitize_cxx_flags sanitize_link_flags)
#   add_library(lrw-internal-sanitize-options INTERFACE)
#   target_compile_options(lrw-internal-sanitize-options INTERFACE
#       ${sanitize_cxx_flags}
#   )
#   target_link_options(lrw-internal-sanitize-options INTERFACE
#       ${sanitize_link_flags}
#   )
# endfunction()
