#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "synthesizer.h"
#include "synthesizer_config.h"

namespace {

constexpr float kDurationSeconds = 2.0f;

// Builds a config whose total duration (attack + sustain + decay) is exactly
// |kDurationSeconds|, so every benchmark synthesizes the same amount of audio.
SynthesizerConfig MakeConfig(SynthesizerConfig::WaveGeneratorType type) {
  SynthesizerConfig config;
  config.wave_generator_type_ = type;
  config.frequency_ = 440;
  config.attack_ = 0.1f;
  config.decay_ = 0.1f;
  config.sustain_ = kDurationSeconds - config.attack_ - config.decay_;
  return config;
}

struct Benchmark {
  std::string name;
  SynthesizerConfig config;
};

std::vector<Benchmark> MakeBenchmarks() {
  std::vector<Benchmark> benchmarks;

  // One representative sound per wave generator, using default modulation.
  const std::vector<
      std::pair<const char*, SynthesizerConfig::WaveGeneratorType>>
      generators = {
          {"sine", SynthesizerConfig::SINE},
          {"triangle", SynthesizerConfig::TRIANGLE},
          {"sawtooth", SynthesizerConfig::SAWTOOTH},
          {"square", SynthesizerConfig::SQUARE},
          {"tangent", SynthesizerConfig::TANGENT},
          {"whistle", SynthesizerConfig::WHISTLE},
          {"breaker", SynthesizerConfig::BREAKER},
          {"whitenoise", SynthesizerConfig::WHITE_NOISE},
          {"pinknoise", SynthesizerConfig::PINK_NOISE},
          {"brownnoise", SynthesizerConfig::BROWN_NOISE},
      };
  for (auto const& [name, type] : generators) {
    benchmarks.push_back({name, MakeConfig(type)});
  }

  // A sine sweep exercising the frequency-modulation paths (sweep, jumps,
  // vibrato) that a lot of game sounds use.
  {
    SynthesizerConfig config = MakeConfig(SynthesizerConfig::SINE);
    config.frequency_sweep_ = 200;
    config.frequency_delta_sweep_ = -100;
    config.frequency_jump1_onset_ = 33;
    config.frequency_jump1_amount_ = 50;
    config.vibrato_frequency_ = 8;
    config.vibrato_depth_ = 10;
    config.tremolo_frequency_ = 6;
    config.tremolo_depth_ = 40;
    benchmarks.push_back({"sine+modulation", config});
  }

  // Harmonics stack the wave generator, which is the most expensive path.
  {
    SynthesizerConfig config = MakeConfig(SynthesizerConfig::SINE);
    config.harmonics_ = 5;
    config.harmonics_falloff_ = 0.95f;
    benchmarks.push_back({"sine+5harmonics", config});
  }

  // Flanger allocates and reads back a delayed copy of the whole buffer.
  {
    SynthesizerConfig config = MakeConfig(SynthesizerConfig::SAWTOOTH);
    config.flanger_offset_ = 5;
    config.flanger_offset_sweep_ = 10;
    benchmarks.push_back({"sawtooth+flanger", config});
  }

  return benchmarks;
}

double SynthesizeAndTimeMs(SynthesizerConfig const& config, int iterations) {
  auto start = std::chrono::steady_clock::now();
  size_t checksum = 0;
  for (int i = 0; i < iterations; i++) {
    auto pcm_data = Synthesize(config);
    // Prevent the optimizer from eliding the work.
    checksum += pcm_data.size();
  }
  auto end = std::chrono::steady_clock::now();
  volatile size_t sink = checksum;
  (void)sink;
  double total_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  return total_ms / iterations;
}

}  // namespace

int main(int argc, char* argv[]) {
  int iterations = argc > 1 ? std::atoi(argv[1]) : 200;
  if (iterations < 1) {
    iterations = 1;
  }

  auto benchmarks = MakeBenchmarks();

  const int sample_count = static_cast<int>(
      kDurationSeconds * benchmarks.front().config.samples_per_second_);
  printf(
      "ppl-synth benchmark: %.1fs sounds, %d samples each, %d iterations\n\n",
      kDurationSeconds, sample_count, iterations);
  printf("%-20s %12s %16s\n", "sound", "ms/sound", "Msamples/s");
  printf("%-20s %12s %16s\n", "-----", "--------", "----------");

  double total_ms = 0;
  for (auto const& benchmark : benchmarks) {
    double ms = SynthesizeAndTimeMs(benchmark.config, iterations);
    total_ms += ms;
    double msamples_per_s = sample_count / (ms / 1000.0) / 1e6;
    printf("%-20s %12.4f %16.1f\n", benchmark.name.c_str(), ms, msamples_per_s);
  }
  printf("\n%-20s %12.4f\n", "TOTAL", total_ms);

  return 0;
}
