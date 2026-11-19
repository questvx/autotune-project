#pragma once

#include <cstddef>
#include <vector>

class PSOLAGrain
{
public:
    static std::vector<float> extract(
        const std::vector<float>& samples,
        int pitchMark,
        int analysisPeriod,
        size_t synthesisHalfLength
    );

    static float hannWindow(size_t index, size_t size);
};