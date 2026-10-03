#pragma once

#include <vector>

class PitchMarkDetector
{
public:
    std::vector<int> detect(
        const std::vector<float>& samples,
        float frequency,
        float sampleRate
    );
};