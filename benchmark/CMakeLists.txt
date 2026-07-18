cmake_minimum_required (VERSION 3.6)
project(ppl_synth_benchmark)

add_compile_options(-std=c++17 -Werror -O2)

add_subdirectory(../ ppl_synth_benchmark_build_dir)

add_executable(
  ppl_synth_benchmark
  main.cpp
)

target_link_libraries(ppl_synth_benchmark PRIVATE ppl_synth)
