#ifndef APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_H_
#define APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_H_

#include <vector>

#include "synthesizer_config.h"

// Some platforms prefer int16s, some platforms prefer floats.
std::vector<int16_t> Synthesize(SynthesizerConfig const& config);
std::vector<float> SynthesizeFloatVector(SynthesizerConfig const& config);

#endif  // APPLICATION_AUDIO_SYNTHESIZER_SYNTHESIZER_H_
