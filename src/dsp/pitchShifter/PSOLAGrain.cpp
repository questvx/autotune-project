
#include "PSOLAGrain.h"

#include <algorithm>
#include <cmath>

std::vector<float> PSOLAGrain::extract(
    const std::vector<float>& samples,
    int pitchMark,
    int analysisPeriod,
    size_t synthesisHalfLength)
{
    std::vector<float> grain;

    if (samples.empty() ||
        pitchMark < 0 ||
        static_cast<size_t>(pitchMark) >= samples.size() ||
        analysisPeriod <= 0 ||
        synthesisHalfLength == 0 ||
        synthesisHalfLength > grain.max_size() / 2)
    {
        return grain;
    }

    const size_t maximumGrainSize = grain.max_size();
    const size_t analysisRadiusSize =
        static_cast<size_t>(analysisPeriod);
    if (analysisRadiusSize > (maximumGrainSize - 1) / 2 ||
        synthesisHalfLength > (maximumGrainSize - 1) / 2)
        return grain;

    const size_t sourceGrainSize = analysisRadiusSize * 2 + 1;
    const size_t grainSize = synthesisHalfLength * 2 + 1;
    std::vector<float> sourceGrain(sourceGrainSize, 0.0f);
    const long long start =
        static_cast<long long>(pitchMark) - analysisPeriod;

    for (size_t i = 0; i < sourceGrainSize; ++i)
    {
        const long long sourceIndex = start + static_cast<long long>(i);
        if (sourceIndex >= 0 &&
            static_cast<size_t>(sourceIndex) < samples.size())
        {
            sourceGrain[i] = samples[static_cast<size_t>(sourceIndex)];
        }

    }

    grain.reserve(grainSize);
    const double analysisRadius = static_cast<double>(analysisPeriod);
    const double synthesisRadius =
        static_cast<double>(synthesisHalfLength);

    for (size_t i = 0; i < grainSize; ++i)
    {
        double sourcePosition;
        if (i <= synthesisHalfLength)
        {
            sourcePosition =
                static_cast<double>(i) * analysisRadius / synthesisRadius;
        }
        else
        {
            sourcePosition = analysisRadius +
                static_cast<double>(i - synthesisHalfLength) *
                    analysisRadius / synthesisRadius;
        }

        const size_t leftIndex =
            static_cast<size_t>(std::floor(sourcePosition));
        const size_t rightIndex =
            std::min(leftIndex + 1, sourceGrainSize - 1);
        const float fraction = static_cast<float>(
            sourcePosition - static_cast<double>(leftIndex));
        const float sample = sourceGrain[leftIndex] +
            (sourceGrain[rightIndex] - sourceGrain[leftIndex]) * fraction;

        grain.push_back(sample * hannWindow(i, grainSize));
    }

    return grain;
}

float PSOLAGrain::hannWindow(size_t index, size_t size)
{
    if (size == 0 || index >= size)
        return 0.0f;

    const float position =
        (static_cast<float>(index) + 0.5f) / static_cast<float>(size);
    return 0.5f * (
        1.0f - std::cos(2.0f * 3.14159265358979323846f * position));
}
