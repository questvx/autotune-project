// PitchDetector.h
#pragma once
#include "../audioFrame/AudioFrame.h"

class PitchDetector
{
public:
    float detectPitch(const AudioFrame &frame, float sampleRate, float minFreq, float maxFreq);
};