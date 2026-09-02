// PitchDetector.h
#pragma once
#include "../audioFrame/AudioFrame.h"
#include "PitchResult.h"

class PitchDetector
{
public:
    PitchResult detectPitch(const AudioFrame &frame, float sampleRate, float minFrequency, float maxFrequency);
};