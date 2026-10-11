#include "PSOLAProcessor.h"
#include "PSOLAGrain.h"

#include <algorithm>
#include <cmath>

std::vector<float> PSOLAProcessor::process(
    const std::vector<float>& samples,
    const std::vector<int>& pitchMarks,
    float pitchRatio,
    int period)
{
    if (samples.empty() ||
        pitchMarks.empty() ||
        pitchRatio <= 0.0f ||
        period <= 0)
    {
        return {};
    }

    // Goal synthesis period based on the pitch ratio
    float synthesisPeriod =
        static_cast<float>(period) / pitchRatio;

    // Initialize the output vector with zeros
    std::vector<float> output(samples.size(), 0.0f);

    // Keep track of the sum of windows, to normalize the Overlap-Add later.
    std::vector<float> windowSum(samples.size(), 0.0f);

    float outputPosition = 0.0f;

    for (int pitchMark : pitchMarks)
    {
        // Extract the grain around the pitch mark
        std::vector<float> grain =
            PSOLAGrain::extract(
                samples,
                pitchMark,
                period
            );

        if (grain.empty())
            continue;

        int grainSize =
            static_cast<int>(grain.size());

        int outputStart =
            static_cast<int>(
                std::round(
                    outputPosition - period
                )
            );

        for (int i = 0; i < grainSize; ++i)
        {
            int outputIndex = outputStart + i;

            if (outputIndex < 0 ||
                outputIndex >= static_cast<int>(output.size()))
            {
                continue;
            }

            output[outputIndex] += grain[i];

           //Hann Window for Overlap-Add normalization
            float position =
                static_cast<float>(i) /
                static_cast<float>(grainSize - 1);

            float window =
                0.5f * (
                    1.0f -
                    std::cos(
                        2.0f * 3.14159265358979323846f *
                        position
                    )
                );

            windowSum[outputIndex] += window;
        }

        outputPosition += synthesisPeriod;
    }

    // Overlap-Add normalization
    for (size_t i = 0; i < output.size(); ++i)
    {
        if (windowSum[i] > 0.0001f)
        {
            output[i] /= windowSum[i];
        }
    }

    return output;
}