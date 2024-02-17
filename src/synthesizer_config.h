#ifndef APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_CONFIG_H_
#define APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_CONFIG_H_

#include <array>
#include <string>
#include <vector>

class SynthesizerConfig {
 public:
  enum WaveGeneratorType {
    SINE,
    TRIANGLE,
    SAWTOOTH,
    SQUARE,
    TANGENT,
    WHISTLE,
    BREAKER,
    WHITE_NOISE,
    PINK_NOISE,
    BROWN_NOISE
  };
  static constexpr std::array<const char*, 10> waveGeneratorNames = {
      "sine",    "triangle", "sawtooth",   "square",    "tangent",
      "whistle", "breaker",  "whitenoise", "pinknoise", "brownnoise"};
  static std::vector<std::pair<const char*, int>> const& FieldsOffsets();

  static WaveGeneratorType WaveGeneratorTypeFromString(std::string const& str);

  // Returns the duration of the sound in seconds.
  float Duration();
  // |time| in seconds.
  float AmplitudeAt(float time);

  float FrequencyAt(float time);

  // In seconds.
  float attack_ = 0.0;
  float sustain_ = 0.0;
  float decay_ = 0.0;

  // Amount of times per second that the frequency is reset to its base value,
  // and starts its sweep cycle anew.
  float repeat_frequency_ = 0.0;

  // In miliseconds.
  float flanger_offset_ = 0.0;
  float flanger_offset_sweep_ = 0.0;

  // In hertz.
  float tremolo_frequency_ = 0.0;
  float frequency_ = 0.0;
  float frequency_sweep_ = 0.0;
  float frequency_delta_sweep_ = 0.0;
  float vibrato_frequency_ = 0.0;
  // Surprisingly, this is also in Hz.
  float vibrato_depth_ = 0.0;

  // Percentage (between 0 and 100).
  float frequency_jump1_onset_ = 0.0;
  float frequency_jump1_amount_ = 0.0;
  float frequency_jump2_onset_ = 0.0;
  float frequency_jump2_amount_ = 0.0;
  float tremolo_depth_ = 0.0;
  float sustain_punch_ = 0.0;

  // Percentage (between 0 and 1).
  float frequency_jump1_onset_normalized_ = 0.0;
  float frequency_jump1_amount_normalized_ = 0.0;
  float frequency_jump2_onset_normalized_ = 0.0;
  float frequency_jump2_amount_normalized_ = 0.0;
  float tremolo_depth_normalized_ = 0.0;
  float sustain_punch_normalized_ = 0.0;

  bool normalization_ = false;
  float amplification_ = 1.0;
  bool interpolate_noise_ = false;

  int samples_per_second_ = 11025 * 2;

  float harmonics_ = 0.0;
  float harmonics_falloff_ = 0.0;

  WaveGeneratorType wave_generator_type_ = SINE;
};

#endif  // APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_CONFIG_H_
