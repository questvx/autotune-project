#pragma once

#include <vector>

class PitchMarks
{
public:
    static std::vector<int> detect(
        const std::vector<float>& samples,
        float frequency,
        float sampleRate
    );
};