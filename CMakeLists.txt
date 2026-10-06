cmake_minimum_required(VERSION 3.16)
project(cfr_game_theory_solver CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ENABLE_ASAN "Build with Address/UndefinedBehavior sanitizers" OFF)

add_library(cfrkuhn INTERFACE)
target_include_directories(cfrkuhn INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(cfrkuhn INTERFACE /O2 /W4 /EHsc)
else()
  target_compile_options(cfrkuhn INTERFACE -O2 -Wall -Wextra -Wpedantic)
endif()

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_executable(run_cfr src/run_cfr.cpp)
target_link_libraries(run_cfr PRIVATE cfrkuhn)

add_executable(kuhn_test tests/kuhn_test.cpp)
target_link_libraries(kuhn_test PRIVATE cfrkuhn)
target_include_directories(kuhn_test PRIVATE tests)

enable_testing()
add_test(NAME kuhn COMMAND kuhn_test)
