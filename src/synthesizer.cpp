#include "synthesizer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "fast_math.h"

using ppl_synth::FastSinTurns;
using ppl_synth::FastTanHalfTurns;
using ppl_synth::Frac;

namespace {

class Synthesizer {
 public:
  Synthesizer(SynthesizerConfig const& config);
  std::vector<float> GeneratePCMData();

 private:
  template <class Generator>
  std::vector<float> GenerateWith(Generator generator);

  SynthesizerConfig config_;
};

template <class T, class S>
[[nodiscard]] T Mix(T const& a, T const& b, S progress) {
  T diff = (b - a) * progress;
  return a + diff;
}

}  // namespace

#define DEFINE_WAVE_GENERATOR(name, code)             \
  class name {                                        \
   public:                                            \
    float GetSample(float phase, float time) { code } \
  };

DEFINE_WAVE_GENERATOR(SineWaveGenerator, return FastSinTurns(phase);)

DEFINE_WAVE_GENERATOR(
    TriangleWaveGenerator,
    if (phase < 0.25f) { return 4 * phase; } if (phase < 0.75f) {
      return 2 - 4 * phase;
    } return -4 +
        4 * phase;)

class SquareWaveGenerator {
 public:
  SquareWaveGenerator(SynthesizerConfig const& config) : config_(config) {}
  float GetSample(float phase, float time) {
    return phase < config_.SquareDutyAt(time) ? 1 : -1;
  }

 private:
  SynthesizerConfig const& config_;
};

DEFINE_WAVE_GENERATOR(SawtoothWaveGenerator,
                      return phase < 0.5f ? 2 * phase : -2 + 2 * phase;)

DEFINE_WAVE_GENERATOR(
    TangentWaveGenerator,
    return std::clamp<float>(0.3f * FastTanHalfTurns(phase), -2, 2);)

DEFINE_WAVE_GENERATOR(WhistleWaveGenerator,
                      return 0.75f * FastSinTurns(phase) +
                             0.25f * FastSinTurns(20.0f * phase);)

DEFINE_WAVE_GENERATOR(BreakerWaveGenerator,
                      constexpr float factor = 0.866f;  // sqrt(0.75)
                      float p = Frac(phase + factor);
                      return -1 + 2 * std::abs(1 - p * p * 2);)

class WhiteNoiseWaveGenerator {
 public:
  explicit WhiteNoiseWaveGenerator(bool interpolate)
      : distribution_(-1.0f, 1.0f), interpolate_(interpolate) {}
  float GetSample(float phase, float time) {
    phase = Frac(phase * 2);
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
  std::uniform_real_distribution<float> distribution_;
  float previous_phase_ = 0;
  float previous_random_ = 0;
  float current_random_ = 0;
  bool interpolate_;
};

class PinkNoiseWaveGenerator {
 public:
  explicit PinkNoiseWaveGenerator(bool interpolate)
      : distribution_(-1.0, 1.0), interpolate_(interpolate) {}
  float GetSample(float phase, float time) {
    phase = Frac(phase * 2);
    if (phase < previous_phase_) {
      previous_random_ = current_random_;
      float white = distribution_(rng_);
      b_[0] = 0.99886f * b_[0] + white * 0.0555179f;
      b_[1] = 0.99332f * b_[1] + white * 0.0750759f;
      b_[2] = 0.96900f * b_[2] + white * 0.1538520f;
      b_[3] = 0.86650f * b_[3] + white * 0.3104856f;
      b_[4] = 0.55000f * b_[4] + white * 0.5329522f;
      b_[5] = -0.7616f * b_[5] + white * 0.0168980f;
      current_random_ = (b_[0] + b_[1] + b_[2] + b_[3] + b_[4] + b_[5] + b_[6] +
                         white * 0.5362f) /
                        7.0f;
      b_[6] = white * 0.115926f;
    }
    previous_phase_ = phase;
    if (interpolate_) {
      return Mix(previous_random_, current_random_, phase);
    }
    return current_random_;
  }
  std::mt19937 rng_;
  std::uniform_real_distribution<float> distribution_;
  std::array<float, 7> b_ = {0, 0, 0, 0, 0, 0, 0};
  float previous_phase_ = 0;
  float previous_random_ = 0;
  float current_random_ = 0;
  bool interpolate_;
};

class BrownNoiseWaveGenerator {
 public:
  explicit BrownNoiseWaveGenerator(bool interpolate)
      : distribution_(-0.01, 0.01), interpolate_(interpolate) {}
  float GetSample(float phase, float time) {
    phase = Frac(phase * 2);
    if (phase < previous_phase_) {
      previous_random_ = current_random_;
      current_random_ =
          std::clamp<float>(current_random_ + distribution_(rng_), -1, 1);
    }
    previous_phase_ = phase;
    if (interpolate_) {
      return Mix(previous_random_, current_random_, phase);
    }
    return current_random_;
  }
  std::mt19937 rng_;
  std::uniform_real_distribution<float> distribution_;
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

std::vector<int16_t> Synthesize(SynthesizerConfig const& config) {
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

std::vector<float> SynthesizeFloatVector(SynthesizerConfig const& config) {
  Synthesizer synth(config);
  return synth.GeneratePCMData();
}

Synthesizer::Synthesizer(SynthesizerConfig const& config) : config_(config) {
  config_.frequency_jump1_onset_normalized_ =
      config_.frequency_jump1_onset_ / 100.0f;
  config_.frequency_jump1_amount_normalized_ =
      config_.frequency_jump1_amount_ / 100.0f;
  config_.frequency_jump2_onset_normalized_ =
      config_.frequency_jump2_onset_ / 100.0f;
  config_.frequency_jump2_amount_normalized_ =
      config_.frequency_jump2_amount_ / 100.0f;
  config_.tremolo_depth_normalized_ = config_.tremolo_depth_ / 100.0f;
  config_.sustain_punch_normalized_ = config_.sustain_punch_ / 100.0f;
  config_.square_duty_normalized_ = config_.square_duty_ / 100.0f;
  config_.square_duty_sweep_normalized_ = config_.square_duty_sweep_ / 100.0f;
  config_.repeat_frequency_prepared_ =
      std::fmax(config_.repeat_frequency_, 1.0f / config_.Duration());
}

template <class Generator>
std::vector<float> Synthesizer::GenerateWith(Generator generator) {
  std::vector<float> data;
  float duration = config_.Duration();
  int sample_count = std::ceil(duration * config_.samples_per_second_);
  data.reserve(sample_count);
  float sample_to_time_factor = 1.0 / config_.samples_per_second_;
  float phase = 0;

  // Sanitize the inputs
  const int harmonics = std::clamp<int>(config_.harmonics_, 0, 5);
  const float harmonics_falloff =
      std::clamp<float>(config_.harmonics_falloff_, 0, 1);

  // When no modulation varies the frequency over time, FrequencyAt is constant
  // and the whole per-sample computation collapses to a fixed phase increment.
  const bool constant_frequency =
      config_.frequency_sweep_ == 0 && config_.frequency_delta_sweep_ == 0 &&
      config_.frequency_jump1_amount_ == 0 &&
      config_.frequency_jump2_amount_ == 0 && config_.vibrato_depth_ == 0;
  const float constant_phase_increment =
      std::fmax(0.0f, config_.frequency_) * sample_to_time_factor;

  // Runs the wave generator and modulate the amplitude.
  if (harmonics == 0 || harmonics_falloff <= 0.0f) {
    // Special case for the usual config (no harmonics)
    for (int i = 0; i < sample_count; i++) {
      float time = i * sample_to_time_factor;
      float phase_increment =
          constant_frequency
              ? constant_phase_increment
              : config_.FrequencyAt(time) * sample_to_time_factor;
      phase = Frac(phase + phase_increment);
      float sample = generator.GetSample(phase, time);
      sample *= config_.AmplitudeAt(time);
      data.push_back(sample);
    }
  } else {
    // With harmonics
    std::vector<float> harmonic_amplitudes;
    // Prepare harmonics amplitude
    // 1. Compute total_harmonic_ampl
    float total_harmonic_ampl = 0.0;
    {
      float harmonic_ampl = 1.0;
      for (int i = 0; i <= harmonics; i++) {
        total_harmonic_ampl += harmonic_ampl;
        harmonic_ampl *= harmonics_falloff;
      }
    }
    // 2. Compute the actual amplitudes
    const float first_harmonic_amp = 1.0f / total_harmonic_ampl;
    float harmonic_ampl = first_harmonic_amp;
    for (int i = 0; i <= harmonics; i++) {
      harmonic_amplitudes.push_back(harmonic_ampl);
      harmonic_ampl *= harmonics_falloff;
    }
    // Actually generate the samples
    for (int i = 0; i < sample_count; i++) {
      float time = i * sample_to_time_factor;
      float phase_increment =
          constant_frequency
              ? constant_phase_increment
              : config_.FrequencyAt(time) * sample_to_time_factor;
      phase = Frac(phase + phase_increment);
      float sample = 0;
      for (int j = 0; j <= harmonics; j++) {
        float harmonic_phase = Frac(phase * (j + 1));
        sample +=
            generator.GetSample(harmonic_phase, time) * harmonic_amplitudes[j];
      }
      sample *= config_.AmplitudeAt(time);
      data.push_back(sample);
    }
  }

  // Flang
  Flang(data, config_);

  // Normalize, Amplify
  NormalizeAmplify(data, config_);

  return data;
}

std::vector<float> Synthesizer::GeneratePCMData() {
  switch (config_.wave_generator_type_) {
    case SynthesizerConfig::SINE:
      return GenerateWith(SineWaveGenerator{});
    case SynthesizerConfig::TRIANGLE:
      return GenerateWith(TriangleWaveGenerator{});
    case SynthesizerConfig::SAWTOOTH:
      return GenerateWith(SawtoothWaveGenerator{});
    case SynthesizerConfig::SQUARE:
      return GenerateWith(SquareWaveGenerator{config_});
    case SynthesizerConfig::TANGENT:
      return GenerateWith(TangentWaveGenerator{});
    case SynthesizerConfig::WHISTLE:
      return GenerateWith(WhistleWaveGenerator{});
    case SynthesizerConfig::BREAKER:
      return GenerateWith(BreakerWaveGenerator{});
    case SynthesizerConfig::WHITE_NOISE:
      return GenerateWith(WhiteNoiseWaveGenerator{config_.interpolate_noise_});
    case SynthesizerConfig::PINK_NOISE:
      return GenerateWith(PinkNoiseWaveGenerator{config_.interpolate_noise_});
    case SynthesizerConfig::BROWN_NOISE:
      return GenerateWith(BrownNoiseWaveGenerator{config_.interpolate_noise_});
  }
  return {};
}
