#include "synthesizer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace {

class WaveGenerator {
 public:
  virtual ~WaveGenerator(){};
  virtual float GetSample(float phase, float time) = 0;
};

class Synthesizer {
 public:
  Synthesizer(SynthesizerConfig const& config);
  std::vector<float> GeneratePCMData();

 private:
  SynthesizerConfig config_;
  std::unique_ptr<WaveGenerator> wave_generator_;
};

template <class T, class S>
[[nodiscard]] T Mix(T const& a, T const& b, S progress) {
  T diff = (b - a) * progress;
  return a + diff;
}

}  // namespace

#define DEFINE_WAVE_GENERATOR(name, code)                      \
  class name : public WaveGenerator {                          \
    float GetSample(float phase, float time) override { code } \
  };

DEFINE_WAVE_GENERATOR(SineWaveGenerator, return sinf(2 * M_PI * phase);)

DEFINE_WAVE_GENERATOR(
    TriangleWaveGenerator,
    if (phase < 0.25) { return 4 * phase; } if (phase < 0.75) {
      return 2 - 4 * phase;
    } return -4 +
        4 * phase;)

DEFINE_WAVE_GENERATOR(SquareWaveGenerator, return phase < 0.5 ? 1 : -1;)

DEFINE_WAVE_GENERATOR(SawtoothWaveGenerator,
                      return phase < 0.5 ? 2 * phase : -2 + 2 * phase;)

DEFINE_WAVE_GENERATOR(TangentWaveGenerator,
                      return std::clamp<float>(0.3 * tanf(M_PI * phase),
                                               -2,
                                               2);)

DEFINE_WAVE_GENERATOR(WhistleWaveGenerator,
                      return 0.75 * sinf(2 * M_PI * phase) +
                             0.25 * sinf(40 * M_PI * phase);)

DEFINE_WAVE_GENERATOR(BreakerWaveGenerator,
                      double dummy;
                      constexpr float factor = 0.866;  // sqrt(0.75)
                      float p = modf(phase + factor, &dummy);
                      return -1 + 2 * std::abs(1 - p * p * 2);)

class WhiteNoiseWaveGenerator : public WaveGenerator {
 public:
  explicit WhiteNoiseWaveGenerator(bool interpolate)
      : distribution_(-1.0, 1.0), interpolate_(interpolate) {}
  float GetSample(float phase, float time) override {
    double dummy;
    phase = modf(phase * 2, &dummy);
    if (phase < previous_phase_) {
      previous_random_ = current_random_;
      current_random_ = distribution_(rng_);
    }
    previous_phase_ = phase;
    if (interpolate_) {
      return Mix(previous_random_, current_random_, phase);
    }
    return current_random_;
  }
  std::mt19937 rng_;
  std::uniform_real_distribution<> distribution_;
  float previous_phase_ = 0;
  float previous_random_ = 0;
  float current_random_ = 0;
  bool interpolate_;
};

class PinkNoiseWaveGenerator : public WaveGenerator {
 public:
  explicit PinkNoiseWaveGenerator(bool interpolate)
      : distribution_(-1.0, 1.0), interpolate_(interpolate) {}
  float GetSample(float phase, float time) override {
    double dummy;
    phase = modf(phase * 2, &dummy);
    if (phase < previous_phase_) {
      previous_random_ = current_random_;
      float white = distribution_(rng_);
      b_[0] = 0.99886 * b_[0] + white * 0.0555179;
      b_[1] = 0.99332 * b_[1] + white * 0.0750759;
      b_[2] = 0.96900 * b_[2] + white * 0.1538520;
      b_[3] = 0.86650 * b_[3] + white * 0.3104856;
      b_[4] = 0.55000 * b_[4] + white * 0.5329522;
      b_[5] = -0.7616 * b_[5] + white * 0.0168980;
      current_random_ = (b_[0] + b_[1] + b_[2] + b_[3] + b_[4] + b_[5] + b_[6] +
                         white * 0.5362) /
                        7.0;
      b_[6] = white * 0.115926;
    }
    previous_phase_ = phase;
    if (interpolate_) {
      return Mix(previous_random_, current_random_, phase);
    }
    return current_random_;
  }
  std::mt19937 rng_;
  std::uniform_real_distribution<> distribution_;
  std::array<float, 7> b_ = {0, 0, 0, 0, 0, 0, 0};
  float previous_phase_ = 0;
  float previous_random_ = 0;
  float current_random_ = 0;
  bool interpolate_;
};

class BrownNoiseWaveGenerator : public WaveGenerator {
 public:
  explicit BrownNoiseWaveGenerator(bool interpolate)
      : distribution_(-0.01, 0.01), interpolate_(interpolate) {}
  float GetSample(float phase, float time) override {
    double dummy;
    phase = modf(phase * 2, &dummy);
    if (phase < previous_phase_) {
      previous_random_ = current_random_;
      current_random_ =
          std::clamp<double>(current_random_ + distribution_(rng_), -1, 1);
    }
    previous_phase_ = phase;
    if (interpolate_) {
      return Mix(previous_random_, current_random_, phase);
    }
    return current_random_;
  }
  std::mt19937 rng_;
  std::uniform_real_distribution<> distribution_;
  float previous_phase_ = 0;
  float previous_random_ = 0;
  float current_random_ = 0;
  bool interpolate_;
};

void Flang(std::vector<float>& data, SynthesizerConfig const& config) {
  if (config.flanger_offset_ == 0 && config.flanger_offset_sweep_ == 0) {
    return;
  }
  std::vector<float> data_copy = data;
  for (size_t i = 0; i < data.size(); i++) {
    float offset_in_ms =
        (config.flanger_offset_ +
         config.flanger_offset_sweep_ * i / ((float)data.size()));
    int offset = (offset_in_ms * config.samples_per_second_) / 1000;
    size_t index = i + offset;
    if (index >= 0 && index < data.size()) {
      data[i] += data_copy[index];
    }
  }
}

void NormalizeAmplify(std::vector<float>& data,
                      SynthesizerConfig const& config) {
  float factor = config.amplification_;

  if (config.normalization_) {
    float max = 0;
    for (float const& f : data) {
      max = std::max(max, f);
    }
    if (max == 0) {
      return;
    }
    factor /= max;
  }
  if (factor == 1) {
    return;
  }
  for (float& f : data) {
    f *= factor;
  }
}

std::vector<int16_t> Synthetize(SynthesizerConfig const& config) {
  Synthesizer synth(config);
  auto data = synth.GeneratePCMData();
  std::vector<int16_t> final_data;
  final_data.reserve(data.size());
  // Convert to 16 bits.
  for (size_t i = 0; i < data.size(); i++) {
    int16_t transformed_value =
        std::clamp<int>(std::round(data[i] * 0x8000), -0x8000, 0x7FFF);
    final_data.push_back(transformed_value);
  }
  return final_data;
}

std::vector<float> SynthetizeFloatVector(SynthesizerConfig const& config) {
  Synthesizer synth(config);
  return synth.GeneratePCMData();
}

std::unique_ptr<WaveGenerator> WaveGeneratorFactory(
    SynthesizerConfig const& config) {
  switch (config.wave_generator_type_) {
    case SynthesizerConfig::SINE:
      return std::make_unique<SineWaveGenerator>();
    case SynthesizerConfig::TRIANGLE:
      return std::make_unique<TriangleWaveGenerator>();
    case SynthesizerConfig::SAWTOOTH:
      return std::make_unique<SawtoothWaveGenerator>();
    case SynthesizerConfig::SQUARE:
      return std::make_unique<SquareWaveGenerator>();
    case SynthesizerConfig::TANGENT:
      return std::make_unique<TangentWaveGenerator>();
    case SynthesizerConfig::WHISTLE:
      return std::make_unique<WhistleWaveGenerator>();
    case SynthesizerConfig::BREAKER:
      return std::make_unique<BreakerWaveGenerator>();
    case SynthesizerConfig::WHITE_NOISE:
      return std::make_unique<WhiteNoiseWaveGenerator>(
          config.interpolate_noise_);
    case SynthesizerConfig::PINK_NOISE:
      return std::make_unique<PinkNoiseWaveGenerator>(
          config.interpolate_noise_);
    case SynthesizerConfig::BROWN_NOISE:
      return std::make_unique<BrownNoiseWaveGenerator>(
          config.interpolate_noise_);
  }
}

Synthesizer::Synthesizer(SynthesizerConfig const& config) : config_(config) {
  config_.frequency_jump1_onset_normalized_ =
      config_.frequency_jump1_onset_ / 100.0;
  config_.frequency_jump1_amount_normalized_ =
      config_.frequency_jump1_amount_ / 100.0;
  config_.frequency_jump2_onset_normalized_ =
      config_.frequency_jump2_onset_ / 100.0;
  config_.frequency_jump2_amount_normalized_ =
      config_.frequency_jump2_amount_ / 100.0;
  config_.tremolo_depth_normalized_ = config_.tremolo_depth_ / 100.0;
  config_.sustain_punch_normalized_ = config_.sustain_punch_ / 100.0;
  wave_generator_ = WaveGeneratorFactory(config);
}

std::vector<float> Synthesizer::GeneratePCMData() {
  std::vector<float> data;
  float duration = config_.Duration();
  int sample_count = std::ceil(duration * config_.samples_per_second_);
  data.reserve(sample_count);
  float sample_to_time_factor = 1.0 / config_.samples_per_second_;
  float phase = 0;

  // Runs the wave generator and modulate the amplitude.
  for (int i = 0; i < sample_count; i++) {
    float time = i * sample_to_time_factor;
    double dummy;

    float current_frequency = config_.FrequencyAt(time);
    phase = modf(phase + current_frequency * sample_to_time_factor, &dummy);
    float value = wave_generator_->GetSample(phase, time);
    value *= config_.AmplitudeAt(time);
    data.push_back(value);
  }

  // Flang
  Flang(data, config_);

  // Normalize, Amplify
  NormalizeAmplify(data, config_);

  return data;
}
