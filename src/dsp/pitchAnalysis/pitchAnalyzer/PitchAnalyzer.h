//PitchAnalyzer.h

#pragma once

#include "../PitchAnalysisResult.h"
#include "../pitchDetector/PitchDetector.h"
#include "../../frameGenerator/AudioFrame.h"

class PitchAnalyzer
{
public:
    PitchAnalysisResult analyze(
        const AudioFrame& frame,
        float sampleRate,
        float minFrequency,
        float maxFrequency
    );

private:
    PitchDetector pitchDetector;
};