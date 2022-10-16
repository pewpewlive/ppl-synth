set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
  ppl_synth
  src/synthesizer_config.cpp
  src/synthesizer_config.h
  src/synthesizer.cpp
  src/synthesizer.h
)

source_group(src/.*)

target_include_directories(ppl_synth PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")