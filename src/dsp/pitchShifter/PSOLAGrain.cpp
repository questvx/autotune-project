
#include "PSOLAGrain.h"
#include <cmath>

std::vector<float> PSOLAGrain::extract(
    const std::vector<float>& samples,
    int pitchMark,
    int period)
{
    std::vector<float> grain;

    if (samples.empty() ||
        pitchMark < 0 ||
        static_cast<size_t>(pitchMark) >= samples.size() ||
        period <= 0)
    {
        return grain;
    }

    const size_t grainSize = static_cast<size_t>(period) * 2;
    if (grainSize > grain.max_size())
        return grain;

    grain.reserve(grainSize);
    const long long start =
        static_cast<long long>(pitchMark) - period;

    for (size_t i = 0; i < grainSize; ++i)
    {
        const long long sourceIndex = start + static_cast<long long>(i);
        float sample = 0.0f;
        if (sourceIndex >= 0 &&
            static_cast<size_t>(sourceIndex) < samples.size())
        {
            sample = samples[static_cast<size_t>(sourceIndex)];
        }

        float position = static_cast<float>(i)
            / static_cast<float>(grainSize - 1);

        float window = 0.5f * (
            1.0f - std::cos(
                2.0f * 3.14159265358979323846f * position
            )
        );

        grain.push_back(sample * window);
    }

    return grain;
}
