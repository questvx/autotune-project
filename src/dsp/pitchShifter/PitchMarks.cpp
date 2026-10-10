#include "PitchMarks.h"
#include <cmath>
#include <algorithm>

std::vector<int> PitchMarks::detect(
    const std::vector<float>& samples,
    float frequency,
    float sampleRate)
{
    std::vector<int> marks;

    if (samples.empty() ||
        frequency <= 0.0f ||
        sampleRate <= 0.0f)
    {
        return marks;
    }

    float period = sampleRate / frequency;

    float currentPosition = 0.0f;

    // How far from the predicted position we search
    int searchRadius = static_cast<int>(period * 0.1f);

    while (currentPosition < samples.size())
    {
        int predictedPosition =
            static_cast<int>(std::round(currentPosition));

        int searchStart =
            std::max(0, predictedPosition - searchRadius);

        int searchEnd =
            std::min(
                static_cast<int>(samples.size()) - 1,
                predictedPosition + searchRadius
            );

        int bestPosition = predictedPosition;
        float bestValue = samples[predictedPosition];

        // Search for the strongest positive peak
        for (int i = searchStart; i <= searchEnd; ++i)
        {
            if (samples[i] > bestValue)
            {
                bestValue = samples[i];
                bestPosition = i;
            }
        }

        marks.push_back(bestPosition);

        currentPosition += period;
    }

    return marks;
}