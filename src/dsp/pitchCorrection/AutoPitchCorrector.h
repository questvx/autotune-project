#pragma once

#include "../pitchAnalysis/PitchAnalysisResult.h"
#include "../../scale/Scale.h"

#include <cstddef>
#include <vector>

struct PitchCorrectionFrameDiagnostics
{
    float trackedFrequency = 0.0f;
    float confidence = 0.0f;
    int targetMidi = -1;
    float targetFrequency = 0.0f;
    float smoothedTargetFrequency = 0.0f;
    float requestedCorrectionCents = 0.0f;
    float smoothedCorrectionCents = 0.0f;
    float pitchRatio = 1.0f;
    int phaseShiftSamples = 0;
    int phaseShiftSearchLimitSamples = 0;
    float phaseAlignmentCorrelation = 0.0f;
    bool voiced = false;
    bool pitchShiftApplied = false;
    bool phaseAlignmentValid = false;
    bool phaseShiftAtSearchLimit = false;
};

struct AutoPitchCorrectionResult
{
    std::vector<float> samples;
    std::vector<PitchAnalysisResult> frameAnalysis;
    std::vector<PitchCorrectionFrameDiagnostics> frameDiagnostics;
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
        float retuneSpeed,
        bool phaseAlignmentEnabled = true
    );
};
