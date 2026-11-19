#pragma once

#include "../pitchAnalysis/PitchAnalysisResult.h"
#include "../../scale/Scale.h"

#include <cstddef>
#include <vector>

struct AutoPitchCorrectionResult
{
    std::vector<float> samples;
    std::vector<PitchAnalysisResult> frameAnalysis;
};

class AutoPitchCorrector
{
public:
    static AutoPitchCorrectionResult process(
        const std::vector<float>& samples,
        float sampleRate,
        const Scale& scale,
        size_t frameSize,
        size_t hopSize,
        float minFrequency,
        float maxFrequency,
        float retuneSpeed
    );
};
