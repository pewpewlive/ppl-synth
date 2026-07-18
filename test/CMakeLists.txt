cmake_minimum_required (VERSION 3.6)
project(ppl_synth_test)

add_compile_options(-std=c++17 -Werror -O2)

add_subdirectory(../ ppl_synth_test_build_dir)

enable_testing()

add_executable(
  ppl_synth_test
  main.cpp
)

target_link_libraries(ppl_synth_test PRIVATE ppl_synth)
target_include_directories(ppl_synth_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME golden COMMAND ppl_synth_test)
