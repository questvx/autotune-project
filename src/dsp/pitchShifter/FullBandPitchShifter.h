#pragma once

#include <vector>

class FullBandPitchShifter
{
public:
    static std::vector<float> processInterleaved(
        const std::vector<float>& interleavedSamples,
        unsigned int channelCount,
        int semitones);
};
