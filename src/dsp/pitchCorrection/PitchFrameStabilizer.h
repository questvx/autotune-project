#pragma once

#include "../pitchAnalysis/PitchAnalysisResult.h"

#include <vector>

struct TrackedPitchFrame
{
    bool voiced = false;
    float frequency = 0.0f;
    float confidence = 0.0f;
    Note targetNote = {-1, "Invalid", 0.0f};
    float targetFrequency = 0.0f;
};

class PitchFrameStabilizer
{
public:
    static std::vector<TrackedPitchFrame> stabilize(
        const std::vector<PitchAnalysisResult>& analysis,
        float minFrequency,
        float maxFrequency);
};
