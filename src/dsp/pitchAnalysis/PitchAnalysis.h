//PitchAnalysis.h

#pragma once

#include "../pitchDetector/PitchResult.h"
#include "../note/Note.h"

struct PitchAnalysis
{
    PitchResult pitchResult;
    Note note;
    float targetFrequency = 0.0f;
    float cents = 0.0f;
};