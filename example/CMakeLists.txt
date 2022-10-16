cmake_minimum_required (VERSION 3.6)
project(ppl_synth_example)

add_compile_options(-std=c++17 -Werror)

add_subdirectory(../ ppl_synth_example_build_dir)

add_executable(
  ppl_synth_example
  main.cpp
)

target_link_libraries(ppl_synth_example PRIVATE ppl_synth)