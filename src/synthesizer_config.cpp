#include "synthesizer_config.h"

#include <cassert>
#include <cmath>
#include <cstdio>

#define PI static_cast<float>(M_PI)

std::vector<std::pair<const char*, int>> const&
SynthesizerConfig::FieldsOffsets() {
  static std::vector<std::pair<const char*, int>> offsets = {
      {"attack", offsetof(SynthesizerConfig, attack_)},
      {"sustain", offsetof(SynthesizerConfig, sustain_)},
      {"decay", offsetof(SynthesizerConfig, decay_)},
      {"sustainPunch", offsetof(SynthesizerConfig, sustain_punch_)},
      {"tremoloDepth", offsetof(SynthesizerConfig, tremolo_depth_)},
      {"tremoloFrequency", offsetof(SynthesizerConfig, tremolo_frequency_)},
      {"repeatFrequency", offsetof(SynthesizerConfig, repeat_frequency_)},
      {"flangerOffset", offsetof(SynthesizerConfig, flanger_offset_)},
      {"flangerOffsetSweep",
       offsetof(SynthesizerConfig, flanger_offset_sweep_)},
      {"frequency", offsetof(SynthesizerConfig, frequency_)},
      {"frequencySweep", offsetof(SynthesizerConfig, frequency_sweep_)},
      {"frequencyDeltaSweep",
       offsetof(SynthesizerConfig, frequency_delta_sweep_)},
      {"vibratoFrequency", offsetof(SynthesizerConfig, vibrato_frequency_)},
      {"frequencyJump1Onset",
       offsetof(SynthesizerConfig, frequency_jump1_onset_)},
      {"frequencyJump1Amount",
       offsetof(SynthesizerConfig, frequency_jump1_amount_)},
      {"frequencyJump2Onset",
       offsetof(SynthesizerConfig, frequency_jump2_onset_)},
      {"frequencyJump2Amount",
       offsetof(SynthesizerConfig, frequency_jump2_amount_)},
      {"vibratoDepth", offsetof(SynthesizerConfig, vibrato_depth_)},
      {"amplification", offsetof(SynthesizerConfig, amplification_)}};
  return offsets;
}

SynthesizerConfig::WaveGeneratorType
SynthesizerConfig::WaveGeneratorTypeFromString(std::string const& str) {
  int i = 0;
  for (auto& name : waveGeneratorNames) {
    if (str == name) {
      return (WaveGeneratorType)i;
    }
    i++;
  }
  assert(false);
  return SINE;
}

float SynthesizerConfig::Duration() {
  return attack_ + sustain_ + decay_;
}

float SynthesizerConfig::AmplitudeAt(float time) {
  float amplitude;
  if (time < attack_) {
    amplitude = time / attack_;
  } else if (time < attack_ + sustain_) {
    amplitude =
        1 + sustain_punch_normalized_ * (1 - (time - attack_) / sustain_);
  } else {
    amplitude = 1 - (time - attack_ - sustain_) / decay_;
  }
  if (tremolo_depth_normalized_ != 0) {
    amplitude *=
        1 - tremolo_depth_normalized_ *
                (0.5f + 0.5f * cosf(2.0f * PI * time * tremolo_frequency_));
  }
  return amplitude;
};

float SynthesizerConfig::FrequencyAt(float time) {
  float repeat_frequency = std::fmax(repeat_frequency_, 1.0f / Duration());
  float dummy;
  float fraction_in_repetition = modff(time * repeat_frequency, &dummy);
  float frequency =
      frequency_ + fraction_in_repetition * frequency_sweep_ +
      fraction_in_repetition * fraction_in_repetition * frequency_delta_sweep_;
  if (fraction_in_repetition > frequency_jump1_onset_normalized_) {
    frequency *= 1 + frequency_jump1_amount_normalized_;
  }
  if (fraction_in_repetition > frequency_jump2_onset_normalized_) {
    frequency *= 1 + frequency_jump2_amount_normalized_;
  }
  if (vibrato_depth_ != 0) {
    frequency +=
        1 - vibrato_depth_ *
                (0.5f - 0.5f * sinf(2.0f * PI * time * vibrato_frequency_));
  }
  return std::fmax(0.0, frequency);
}
