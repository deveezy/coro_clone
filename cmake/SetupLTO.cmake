option(LRW_LTO "Use link time optimizations" OFF)

if(NOT LRW_LTO)
  message(STATUS "LTO: disabled (user request)")
  add_compile_options("-fdata-sections" "-ffunction-sections")

  set(LRW_IMPL_LTO_FLAGS "-Wl,--gc-sections")
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(LRW_IMPL_LTO_FLAGS "-Wl,-dead_strip -Wl,-dead_strip_dylibs")
  endif()

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LRW_IMPL_LTO_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LRW_IMPL_LTO_FLAGS}")
else()
  message(STATUS "LTO: on")
  message(STATUS "${LRW_ROOT_DIR}/RequireLTO.cmake")
  include("${LRW_ROOT_DIR}/RequireLTO.cmake")
endif()
