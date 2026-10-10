
#include "PSOLAGrain.h"
#include <algorithm>
#include <cmath>

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

    if (start >= end)
        return grain;

    grain.reserve(end - start);

    const int grainSize = end - start;

    for (int i = start; i < end; ++i)
    {
        float sample = samples[i];

        // Sample position in the grain (0.0 to 1.0)
        float position = static_cast<float>(i - start)
            / static_cast<float>(grainSize - 1);

        //Hann Window
        float window = 0.5f * (
            1.0f - std::cos(
                2.0f * 3.14159265358979323846f * position
            )
        );

        grain.push_back(sample * window);
    }

    return grain;
}
