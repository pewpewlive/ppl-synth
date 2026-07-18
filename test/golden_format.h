#ifndef APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_FORMAT_H_
#define APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_FORMAT_H_

// Shared layout for the golden reference data used by the synthesizer golden
// test. The actual values live in the generated golden_data.h.
//
// The reference describes each sound with characteristics that are invariant
// under small phase/timing drift, so that output-preserving optimizations
// (reordered floating-point math, a reformulated phase accumulator, a
// polynomial transcendental approximation, ...) keep passing, while genuine
// changes to a sound's shape, energy or pitch still fail. Concretely we record
// a coarse amplitude envelope and a coarse zero-crossing-rate profile rather
// than raw sample values: two waveforms that sound the same but are shifted by
// a fraction of a sample have nearly identical envelopes and crossing rates,
// yet wildly different per-sample values.

// Number of equal-width time windows the sound is divided into for the envelope
// and zero-crossing-rate profiles.
constexpr int kGoldenWindowCount = 32;

struct GoldenSound {
  const char* name;
  // Number of float samples the sound is expected to contain.
  int sample_count;
  // Root-mean-square amplitude over the whole sound.
  float rms;
  // Peak absolute amplitude over the whole sound.
  float peak;
  // Root-mean-square amplitude within each of kGoldenWindowCount equal-width
  // time windows: a coarse amplitude envelope, insensitive to phase drift.
  float rms_envelope[kGoldenWindowCount];
  // Zero crossings per sample within each window (roughly in [0, 1]): a coarse
  // pitch/brightness profile, also insensitive to phase drift.
  float zero_crossing_rate[kGoldenWindowCount];
};

#endif  // APPLICATION_AUDIO_SYNTHESIZER_TEST_GOLDEN_FORMAT_H_
