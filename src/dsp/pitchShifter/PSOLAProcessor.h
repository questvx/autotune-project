#pragma once

#include <vector>

class PSOLAProcessor
{
public:
    static std::vector<float> process(
        const std::vector<float>& samples,
        const std::vector<int>& pitchMarks,
        float pitchRatio,
        int period
    );
};