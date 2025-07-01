function(cut_filepath target)
  get_filename_component(BASE_PREFIX "${LRW_ROOT_DIR}/../" ABSOLUTE)
  file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/" SRC_LOG_PATH_BASE)
  file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/" BIN_LOG_PATH_BASE)

  # Depending on OS and CMake version the path may or may not have a trailing
  # '/'
  if(NOT SRC_LOG_PATH_BASE MATCHES ".*/$")
    set(SRC_LOG_PATH_BASE "${SRC_LOG_PATH_BASE}/")
  endif()
  if(NOT BIN_LOG_PATH_BASE MATCHES ".*/$")
    set(BIN_LOG_PATH_BASE "${BIN_LOG_PATH_BASE}/")
  endif()
  if(NOT BASE_PREFIX STREQUAL "/" AND NOT BASE_PREFIX MATCHES ".*/$")
    set(BASE_PREFIX "${BASE_PREFIX}/")
  endif()

  lrw_is_cxx_compile_option_supported(LRW_COMPILER_HAS_MACRO_PREFIX_MAP
                                      "-fmacro-prefix-map=a=b")
  if(LRW_COMPILER_HAS_MACRO_PREFIX_MAP)
    # Заменим absolute path на пустоту. Значит __FILE__ будет возвращать
    # реальный путь файла относительно проекта (common/src/filename.cpp)
    target_compile_options(
      ${target} PUBLIC -fmacro-prefix-map=${BIN_LOG_PATH_BASE}=
                       -fmacro-prefix-map=${BASE_PREFIX}=)
    string(FIND "${SRC_LOG_PATH_BASE}" "${BASE_PREFIX}" _base_prefix_pos)
    if(NOT _base_prefix_pos EQUAL 0 OR NOT "${LRW_ROOT_DIR}" MATCHES
                                       [=[/common$]=])
      target_compile_options(${target}
                             PUBLIC -fmacro-prefix-map=${SRC_LOG_PATH_BASE}=)
    endif()
  else()
    log_error("COMPILER does not support fmacro-prefix-map")
    target_compile_definitions(
      ${LIB_NAME}
      PRIVATE LRW_LOG_SOURCE_PATH_BASE=${SRC_LOG_PATH_BASE}
              LRW_LOG_BUILD_PATH_BASE=${BIN_LOG_PATH_BASE}
              LRW_LOG_PREFIX_PATH_BASE=${BASE_PREFIX})

    target_compile_definitions(
      userver-internal-compile-options
      INTERFACE USERVER_LOG_SOURCE_PATH_BASE=${SRC_LOG_PATH_BASE}
                USERVER_LOG_BUILD_PATH_BASE=${BIN_LOG_PATH_BASE}
                USERVER_LOG_PREFIX_PATH_BASE=${BASE_PREFIX})

  endif()

endfunction()
