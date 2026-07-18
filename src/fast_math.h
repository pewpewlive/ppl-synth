#ifndef APPLICATION_AUDIO_SYNTHESIZER_FAST_MATH_H_
#define APPLICATION_AUDIO_SYNTHESIZER_FAST_MATH_H_

#include <cmath>

// Polynomial approximations of the transcendental functions used by the
// synthesizer. They trade a small, bounded error (well within the golden
// test's tolerance) for being several times cheaper than the libm calls, which
// dominate the cost of the tone generators.
//
// Everything is expressed in *turns* (whole cycles) rather than radians,
// because the synthesizer already works with a phase in [0, 1) and with
// time * frequency products, so no multiply by 2*pi is needed.

namespace ppl_synth {

// Fractional part of a non-negative value, i.e. modff(x, &int_part) without
// materialising the integer part. truncf compiles to a single instruction on
// the platforms we target, unlike the modff library call.
inline float Frac(float x) {
  return x - std::truncf(x);
}

// Approximates sin(2*pi*turns) for any finite |turns|.
//
// Uses the well-known parabola + one refinement step (Nicolas Capens'
// "fast sine"), reformulated for turns. Maximum absolute error ~1.7e-3, which
// is far below the audible/tested threshold.
inline float FastSinTurns(float turns) {
  // Range-reduce to [-0.5, 0.5] cycles, where the approximation is defined.
  turns -= std::floor(turns + 0.5f);
  // Base parabola: exact at 0, +/-0.25 and +/-0.5 turns.
  float y = 8.0f * turns - 16.0f * turns * std::fabs(turns);
  // One refinement step; 0.225 is the standard constant for this form.
  y = 0.225f * (y * std::fabs(y) - y) + y;
  return y;
}

// Approximates cos(2*pi*turns) via the quarter-cycle phase shift.
inline float FastCosTurns(float turns) {
  return FastSinTurns(turns + 0.25f);
}

// Approximates tan(pi*phase) as sin/cos of the corresponding turn, matching the
// real tangent's behaviour near its poles (the caller clamps the result).
inline float FastTanHalfTurns(float phase) {
  // tan(pi*phase) = sin(2*pi*(phase/2)) / cos(2*pi*(phase/2)).
  float half_turn = phase * 0.5f;
  return FastSinTurns(half_turn) / FastCosTurns(half_turn);
}

}  // namespace ppl_synth

#endif  // APPLICATION_AUDIO_SYNTHESIZER_FAST_MATH_H_
