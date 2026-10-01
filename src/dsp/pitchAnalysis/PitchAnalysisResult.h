//PitchAnalysisResult.h

#pragma once

#include "pitchDetector/PitchResult.h"
#include "pitchToNote/Note.h"

struct PitchAnalysisResult
{
    PitchResult pitchResult;

    Note note;
    Note targetNote;

    float targetFrequency = 0.0f;
    float cents = 0.0f;
};