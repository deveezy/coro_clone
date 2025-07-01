include(${CMAKE_CURRENT_LIST_DIR}/Logging.cmake)

function(init_submodules)
  find_package(Git)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --remote
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE INIT_SUBMODULE_RESULT
    ERROR_VARIABLE INIT_SUBMODULE_ERROR)
  if(NOT INIT_SUBMODULE_RESULT EQUAL 0)
    message(
      FATAL_ERROR "Failed to initialize submodule: ${INIT_SUBMODULE_ERROR}")
  endif()
endfunction()

function(add_submodule TARGET_NAME SUBMODULE_SOURCE_DIR SUBMODULE_BINARY_DIR)
  # Проверяем, существует ли указанный таргет message(FATAL_ERROR
  if(NOT TARGET ${TARGET_NAME})
    # Добавляем директорию подмодуля в проект
    add_subdirectory(${SUBMODULE_SOURCE_DIR} ${SUBMODULE_BINARY_DIR})
  else()
    log_trace("Target ${TARGET_NAME} already exists. Skipping submodule setup.")
  endif()
endfunction()
