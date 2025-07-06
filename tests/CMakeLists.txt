# Включаем тесты
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# if(INIT_GTEST_SUBMODULE) set(gtest_disable_install ON) # init_submodules()
# add_submodule(gtest "${SUBMODULE_DIR}/googletest"
# "${SUBMODULE_DIR}/googletest/build") endif()

enable_testing()
add_executable(coro_tests "test_task.cpp")
target_include_directories(coro_tests PRIVATE ${INCLUDE_DIR})

target_link_libraries(coro_tests gtest gtest_main gmock)
