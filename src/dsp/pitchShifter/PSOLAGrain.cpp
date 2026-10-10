#include "PSOLAGrain.h"
#include <algorithm>

std::vector<float> PSOLAGrain::extract(
    const std::vector<float>& samples,
    int pitchMark,
    int period)
{
    std::vector<float> grain;

    if (samples.empty() || period <= 0)
        return grain;

    int start = pitchMark - period;
    int end = pitchMark + period;

    start = std::max(0, start);
    end = std::min(
        static_cast<int>(samples.size()),
        end
    );

    for (int i = start; i < end; ++i)
    {
        grain.push_back(samples[i]);
    }

    return grain;
}