#pragma once

#include <vector>

class PSOLAGrain
{
public:
    static std::vector<float> extract(
        const std::vector<float>& samples,
        int pitchMark,
        int period
    );
};