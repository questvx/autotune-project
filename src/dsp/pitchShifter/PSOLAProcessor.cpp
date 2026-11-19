#include "PSOLAProcessor.h"
#include "PSOLAGrain.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

std::vector<float> PSOLAProcessor::process(
    const std::vector<float>& samples,
    const std::vector<int>& pitchMarks,
    float pitchRatio,
    int period)
{
    if (samples.empty() ||
        pitchMarks.empty() ||
        !std::isfinite(pitchRatio) ||
        pitchRatio <= 0.0f ||
        period <= 0)
    {
        return {};
    }

    std::vector<int> analysisMarks;
    analysisMarks.reserve(pitchMarks.size());
    for (int pitchMark : pitchMarks)
    {
        if (pitchMark >= 0 &&
            static_cast<size_t>(pitchMark) < samples.size())
        {
            analysisMarks.push_back(pitchMark);
        }
    }

    if (analysisMarks.empty())
        return {};

    std::sort(analysisMarks.begin(), analysisMarks.end());
    analysisMarks.erase(
        std::unique(analysisMarks.begin(), analysisMarks.end()),
        analysisMarks.end());

    const double synthesisPeriod =
        static_cast<double>(period) / pitchRatio;
    if (!std::isfinite(synthesisPeriod) ||
        synthesisPeriod < 1.0 ||
        synthesisPeriod >
            static_cast<double>(std::numeric_limits<size_t>::max() / 2))
        return {};

    const size_t synthesisHalfLength =
        static_cast<size_t>(std::ceil(synthesisPeriod));
    const size_t padding = synthesisHalfLength;
    if (padding > (std::numeric_limits<size_t>::max() - samples.size()) / 2)
        return {};

    const size_t paddedSize = samples.size() + 2 * padding;
    std::vector<float> output(paddedSize, 0.0f);
    std::vector<float> windowSum(paddedSize, 0.0f);

    double outputPosition = analysisMarks.front();
    size_t sourceMarkIndex = 0;

    while (outputPosition < static_cast<double>(samples.size()))
    {
        const size_t roundedPosition =
            static_cast<size_t>(std::round(outputPosition));

        while (sourceMarkIndex + 1 < analysisMarks.size() &&
               static_cast<size_t>(analysisMarks[sourceMarkIndex + 1]) <=
                   roundedPosition)
        {
            ++sourceMarkIndex;
        }

        if (sourceMarkIndex + 1 < analysisMarks.size())
        {
            const size_t currentMark =
                static_cast<size_t>(analysisMarks[sourceMarkIndex]);
            const size_t nextMark =
                static_cast<size_t>(analysisMarks[sourceMarkIndex + 1]);
            const size_t currentDistance = roundedPosition >= currentMark
                ? roundedPosition - currentMark
                : currentMark - roundedPosition;
            const size_t nextDistance = roundedPosition >= nextMark
                ? roundedPosition - nextMark
                : nextMark - roundedPosition;
            if (nextDistance < currentDistance)
                ++sourceMarkIndex;
        }

        std::vector<float> grain =
            PSOLAGrain::extract(
                samples,
                analysisMarks[sourceMarkIndex],
                period,
                synthesisHalfLength
            );

        if (grain.empty())
        {
            outputPosition += synthesisPeriod;
            continue;
        }

        const size_t outputStart = roundedPosition;

        for (size_t i = 0; i < grain.size(); ++i)
        {
            const size_t outputIndex = outputStart + i;

            output[outputIndex] += grain[i];
            windowSum[outputIndex] +=
                PSOLAGrain::hannWindow(i, grain.size());
        }

        outputPosition += synthesisPeriod;
    }

    for (size_t i = 0; i < output.size(); ++i)
    {
        if (windowSum[i] > std::numeric_limits<float>::epsilon())
            output[i] /= windowSum[i];
    }

    return std::vector<float>(
        output.begin() + static_cast<std::ptrdiff_t>(padding),
        output.begin() + static_cast<std::ptrdiff_t>(padding + samples.size()));
}