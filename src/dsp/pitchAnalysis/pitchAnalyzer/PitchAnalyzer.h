//PitchAnalyzer.h

#pragma once

#include "../PitchAnalysisResult.h"
#include "../pitchDetector/PitchDetector.h"
#include "../../frameGenerator/AudioFrame.h"
#include "../../../scale/Scale.h"

class PitchAnalyzer
{
public:
    PitchAnalysisResult analyze(
        const AudioFrame& frame,
        float sampleRate,
        float minFrequency,
        float maxFrequency,
        const Scale& scale
    );

private:
    PitchDetector pitchDetector;
};