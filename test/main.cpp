// Golden test: checks that the synthesizer still generates approximately the
// same sounds as when the reference data in golden_data.h was recorded.
//
// The comparison is deliberately based on characteristics that survive small
// phase/timing drift, so that output-preserving optimizations keep passing
// while genuine changes to a sound are caught. For each representative sound we
// compare, against a checked-in reference:
//   - the number of samples (exactly),
//   - the overall RMS and peak amplitude (within a relative tolerance),
//   - a coarse RMS amplitude envelope over kGoldenWindowCount time windows
//     (within a relative tolerance), and
//   - a coarse zero-crossing-rate profile over the same windows (within an
//     absolute tolerance).
// See golden_format.h for why raw sample values are intentionally not compared.
//
// When an intentional change to the synthesizer output is made, regenerate the
// reference with:
//   ./ppl_synth_test --generate > ../test/golden_data.h
//
// The reference values depend on the platform's math and RNG implementations,
// so they are meant to catch regressions on a given toolchain rather than to be
// portable across all of them.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "golden_data.h"
#include "synthesizer.h"
#include "synthesizer_config.h"

namespace {

// A sound matches if its amplitude features are within this *relative* tolerance
// (5%) of the reference. This tolerates the amplitude wobble introduced by
// reordered floating-point math or an approximate transcendental, while
// catching real changes such as a different envelope or a doubled amplitude.
constexpr float kAmplitudeRelTolerance = 0.05f;

// Amplitude features below this magnitude are compared with this as an absolute
// floor instead of the relative tolerance, so near-silent windows (e.g. the
// very start of the attack) don't demand impossible relative precision.
constexpr float kAmplitudeAbsFloor = 0.01f;

// Zero-crossing rates are compared with a *relative* tolerance so pitch changes
// are caught regardless of the base frequency (an octave is a 100% change,
// while a crossing drifting between adjacent windows is a percent or two). The
// absolute floor keeps near-silent windows from demanding impossible relative
// precision.
constexpr float kZeroCrossingRelTolerance = 0.15f;
constexpr float kZeroCrossingAbsFloor = 0.01f;

// All reference sounds last this long.
constexpr float kDurationSeconds = 2.0f;

struct NamedConfig {
  std::string name;
  SynthesizerConfig config;
};

SynthesizerConfig MakeConfig(SynthesizerConfig::WaveGeneratorType type) {
  SynthesizerConfig config;
  config.wave_generator_type_ = type;
  config.frequency_ = 440;
  config.attack_ = 0.1f;
  config.decay_ = 0.1f;
  config.sustain_ = kDurationSeconds - config.attack_ - config.decay_;
  return config;
}

// The representative set of sounds, matching the speed benchmark: one per wave
// generator, plus modulation, harmonics and flanger variants.
std::vector<NamedConfig> ReferenceConfigs() {
  std::vector<NamedConfig> configs;

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
    configs.push_back({name, MakeConfig(type)});
  }

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
    configs.push_back({"sine+modulation", config});
  }

  {
    SynthesizerConfig config = MakeConfig(SynthesizerConfig::SINE);
    config.harmonics_ = 5;
    config.harmonics_falloff_ = 0.95f;
    configs.push_back({"sine+5harmonics", config});
  }

  {
    SynthesizerConfig config = MakeConfig(SynthesizerConfig::SAWTOOTH);
    config.flanger_offset_ = 5;
    config.flanger_offset_sweep_ = 10;
    configs.push_back({"sawtooth+flanger", config});
  }

  return configs;
}

struct Fingerprint {
  int sample_count = 0;
  float rms = 0;
  float peak = 0;
  float rms_envelope[kGoldenWindowCount] = {};
  float zero_crossing_rate[kGoldenWindowCount] = {};
};

// Returns the [begin, end) sample range of window |w| over |sample_count|
// samples, split into kGoldenWindowCount roughly-equal windows.
void WindowBounds(int w, int sample_count, int* begin, int* end) {
  *begin = static_cast<int>(static_cast<long long>(w) * sample_count /
                            kGoldenWindowCount);
  *end = static_cast<int>(static_cast<long long>(w + 1) * sample_count /
                          kGoldenWindowCount);
}

Fingerprint ComputeFingerprint(SynthesizerConfig const& config) {
  std::vector<float> data = SynthesizeFloatVector(config);
  Fingerprint fp;
  fp.sample_count = static_cast<int>(data.size());

  double sum_of_squares = 0;
  float peak = 0;
  for (float sample : data) {
    sum_of_squares += static_cast<double>(sample) * sample;
    peak = std::max(peak, std::fabs(sample));
  }
  fp.rms = data.empty()
               ? 0.0f
               : static_cast<float>(std::sqrt(sum_of_squares / data.size()));
  fp.peak = peak;

  for (int w = 0; w < kGoldenWindowCount; w++) {
    int begin = 0;
    int end = 0;
    WindowBounds(w, fp.sample_count, &begin, &end);
    int count = end - begin;
    if (count <= 0) {
      continue;
    }

    double window_sum_of_squares = 0;
    int crossings = 0;
    for (int i = begin; i < end; i++) {
      window_sum_of_squares += static_cast<double>(data[i]) * data[i];
      // Count a zero crossing whenever consecutive samples straddle zero. The
      // comparison against the previous sample keeps crossings continuous
      // across window boundaries.
      if (i > 0 && ((data[i - 1] < 0) != (data[i] < 0))) {
        crossings++;
      }
    }
    fp.rms_envelope[w] =
        static_cast<float>(std::sqrt(window_sum_of_squares / count));
    fp.zero_crossing_rate[w] = static_cast<float>(crossings) / count;
  }
  return fp;
}

// Formats |value| as a valid, readable C++ float literal, e.g. "1.0f",
// "-0.5f" or "1.234e-06f". Always includes a decimal point or exponent so the
// result is a float rather than an integer literal.
std::string FloatLiteral(float value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.9g", value);
  std::string text(buffer);
  if (text.find_first_of(".eEnN") == std::string::npos) {
    text += ".0";  // e.g. "1" -> "1.0", "-3" -> "-3.0"
  }
  text += "f";
  return text;
}

// Prints an array of kGoldenWindowCount floats as a brace-enclosed initializer,
// four per line, with no trailing whitespace.
void PrintFloatArray(const float* values) {
  printf("     {\n      ");
  for (int w = 0; w < kGoldenWindowCount; w++) {
    printf("%s,", FloatLiteral(values[w]).c_str());
    bool last = w == kGoldenWindowCount - 1;
    if (last) {
      printf("\n");
    } else if ((w + 1) % 4 == 0) {
      printf("\n      ");
    } else {
      printf(" ");
    }
  }
  printf("     }");
}

void PrintGolden(std::vector<NamedConfig> const& configs) {
  printf(
      "// Auto-generated by `ppl_synth_test --generate`. Do not edit by "
      "hand.\n");
  printf(
      "// Regenerate after an intentional change to the synthesizer "
      "output.\n");
  printf("#ifndef APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_DATA_H_\n");
  printf("#define APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_DATA_H_\n\n");
  printf("#include \"golden_format.h\"\n\n");
  printf("static const GoldenSound kGoldenSounds[] = {\n");
  for (auto const& named : configs) {
    Fingerprint fp = ComputeFingerprint(named.config);
    printf("    {\"%s\", %d, %s, %s,\n", named.name.c_str(), fp.sample_count,
           FloatLiteral(fp.rms).c_str(), FloatLiteral(fp.peak).c_str());
    PrintFloatArray(fp.rms_envelope);
    printf(",\n");
    PrintFloatArray(fp.zero_crossing_rate);
    printf("},\n");
  }
  printf("};\n\n");
  printf(
      "constexpr int kGoldenSoundCount =\n"
      "    sizeof(kGoldenSounds) / sizeof(kGoldenSounds[0]);\n\n");
  printf("#endif  // APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_DATA_H_\n");
}

// Reports a mismatch and returns false when |actual| differs from |expected| by
// more than |rel_tolerance| relative to |expected|, or |abs_floor|, whichever
// is larger. The floor keeps tiny reference values from demanding impossible
// relative precision.
bool Close(const char* sound, const char* what, float expected, float actual,
           float rel_tolerance, float abs_floor) {
  float allowed = std::max(rel_tolerance * std::fabs(expected), abs_floor);
  if (std::fabs(expected - actual) <= allowed) {
    return true;
  }
  printf("  FAIL %s: %s expected %.9g, got %.9g (diff %.3g > %.3g)\n", sound,
         what, expected, actual, std::fabs(expected - actual), allowed);
  return false;
}

int RunChecks(std::vector<NamedConfig> const& configs) {
  if (static_cast<int>(configs.size()) != kGoldenSoundCount) {
    printf(
        "FAIL: %zu sounds to check but %d golden entries; regenerate "
        "golden_data.h\n",
        configs.size(), kGoldenSoundCount);
    return 1;
  }

  int failures = 0;
  for (int i = 0; i < kGoldenSoundCount; i++) {
    NamedConfig const& named = configs[i];
    GoldenSound const& golden = kGoldenSounds[i];
    if (named.name != golden.name) {
      printf("FAIL: sound %d is '%s' but golden is '%s'; regenerate\n", i,
             named.name.c_str(), golden.name);
      failures++;
      continue;
    }

    Fingerprint fp = ComputeFingerprint(named.config);
    bool ok = true;
    if (fp.sample_count != golden.sample_count) {
      printf("  FAIL %s: sample_count expected %d, got %d\n", golden.name,
             golden.sample_count, fp.sample_count);
      ok = false;
    }
    ok &= Close(golden.name, "rms", golden.rms, fp.rms,
                kAmplitudeRelTolerance, kAmplitudeAbsFloor);
    ok &= Close(golden.name, "peak", golden.peak, fp.peak,
                kAmplitudeRelTolerance, kAmplitudeAbsFloor);
    for (int w = 0; w < kGoldenWindowCount; w++) {
      if (!Close(golden.name, "rms_envelope", golden.rms_envelope[w],
                 fp.rms_envelope[w], kAmplitudeRelTolerance,
                 kAmplitudeAbsFloor)) {
        ok = false;
        break;  // One mismatch per feature is enough to flag the sound.
      }
    }
    for (int w = 0; w < kGoldenWindowCount; w++) {
      if (!Close(golden.name, "zero_crossing_rate",
                 golden.zero_crossing_rate[w], fp.zero_crossing_rate[w],
                 kZeroCrossingRelTolerance, kZeroCrossingAbsFloor)) {
        ok = false;
        break;
      }
    }

    printf("  %s %s\n", ok ? "ok  " : "FAIL", golden.name);
    if (!ok) {
      failures++;
    }
  }

  if (failures == 0) {
    printf("All %d sounds match the golden reference.\n", kGoldenSoundCount);
    return 0;
  }
  printf("%d of %d sounds changed.\n", failures, kGoldenSoundCount);
  return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<NamedConfig> configs = ReferenceConfigs();
  if (argc > 1 && std::strcmp(argv[1], "--generate") == 0) {
    PrintGolden(configs);
    return 0;
  }
  return RunChecks(configs);
}
