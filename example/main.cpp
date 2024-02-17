#include <fstream>

#include "synthesizer.h"
#include "synthesizer_config.h"

void WritePCM(SynthesizerConfig const& config, const char* path) {
  auto pcm_data = Synthesize(config);
  std::ofstream file(path, std::ios::out | std::ios::binary);
  file.write((const char*)pcm_data.data(), pcm_data.size() * 2);
}

int main(int argc, char* argv[]) {
  {
    SynthesizerConfig config;
    config.frequency_ = 200;
    config.sustain_ = 0.04;
    config.decay_ = 0.14;
    config.frequency_jump1_amount_ = 60;
    config.wave_generator_type_ = SynthesizerConfig::SAWTOOTH;
    WritePCM(config, "out_16bit.pcm");
  }

  {
    SynthesizerConfig config;
    config.frequency_ = 200;
    config.attack_ = 0.1;
    config.sustain_ = 1;
    config.decay_ = 0.1;
    config.harmonics_ = 3;
    config.harmonics_falloff_ = 0.95;
    config.wave_generator_type_ = SynthesizerConfig::SINE;
    WritePCM(config, "out_harmonics_16bit.pcm");
  }

  return EXIT_SUCCESS;
}
