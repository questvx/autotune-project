#include "PitchMarks.h"
#include <cmath>
#include <algorithm>
#include <limits>

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

    const float period = sampleRate / frequency;
    if (!std::isfinite(period) || period <= 0.0f)
        return marks;

    int searchRadius = static_cast<int>(period * 0.1f);
    int firstSearchEnd = samples.size() - 1 >
            static_cast<size_t>(std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(samples.size() - 1);
    if (period < static_cast<float>(firstSearchEnd))
        firstSearchEnd = static_cast<int>(std::ceil(period));

    int firstMark = 0;
    float firstMarkValue = samples.front();
    for (int i = 1; i <= firstSearchEnd; ++i)
    {
        if (samples[static_cast<size_t>(i)] > firstMarkValue)
        {
            firstMarkValue = samples[static_cast<size_t>(i)];
            firstMark = i;
        }
    }

    marks.push_back(firstMark);
    float currentPosition = static_cast<float>(firstMark) + period;

    while (currentPosition < samples.size())
    {
        int predictedPosition =
            static_cast<int>(std::round(currentPosition));

        if (predictedPosition < 0 ||
            static_cast<size_t>(predictedPosition) >= samples.size())
        {
            break;
        }

        int searchStart =
            std::max(1, predictedPosition - searchRadius);

        int searchEnd =
            std::min(
                static_cast<int>(samples.size()) - 2,
                predictedPosition + searchRadius
            );

        int bestPosition = -1;
        int bestDistance = std::numeric_limits<int>::max();
        for (int i = searchStart; i <= searchEnd; ++i)
        {
            if (samples[i] <= 0.0f ||
                samples[i] < samples[i - 1] ||
                samples[i] <= samples[i + 1])
            {
                continue;
            }

            const int distance = std::abs(i - predictedPosition);
            if (distance < bestDistance ||
                (distance == bestDistance &&
                 (bestPosition < 0 ||
                  samples[i] > samples[bestPosition])))
            {
                bestPosition = i;
                bestDistance = distance;
            }
        }

        if (bestPosition < 0)
        {
            bestPosition = predictedPosition;
            float bestValue = samples[predictedPosition];
            for (int i = searchStart; i <= searchEnd; ++i)
            {
                if (samples[i] > bestValue)
                {
                    bestValue = samples[i];
                    bestPosition = i;
                }
            }
        }

        marks.push_back(bestPosition);

        currentPosition = static_cast<float>(bestPosition) + period;
    }

    return marks;
}