#include <fstream>

#include "synthesizer.h"
#include "synthesizer_config.h"

int main(int argc, char* argv[]) {
  SynthesizerConfig config;
  config.frequency_ = 200;
  config.sustain_ = 0.04;
  config.decay_ = 0.14;
  config.frequency_jump1_amount_ = 60;
  config.wave_generator_type_ = SynthesizerConfig::SAWTOOTH;

  auto pcm_data = Synthetize(config);

  std::ofstream file("out_16bit.pcm", std::ios::out | std::ios::binary);
  file.write((const char*)pcm_data.data(), pcm_data.size() * 2);

  return EXIT_SUCCESS;
}
