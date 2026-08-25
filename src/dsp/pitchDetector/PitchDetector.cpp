#include "PitchDetector.h"
#include <iostream>

float PitchDetector::detectPitch(const AudioFrame &frame, float sampleRate, float minFrequency, float maxFrequency)
{
    std::cout << "\n--- Pitch Detector ---" << std::endl;
    std::cout << " Frame size: " << frame.frameSamples.size() << std::endl;
    std::cout << "Frequency range: " << minFrequency << " -" << maxFrequency << " Hz" << std::endl;

    size_t minLag = static_cast<size_t>(sampleRate / maxFrequency);
    size_t maxLag = static_cast<size_t>(sampleRate / minFrequency);

    // Handle edge case
    if (maxLag >= frame.frameSamples.size())
    {
        maxLag = frame.frameSamples.size() - 1;
    }

    // Correlation
    std::vector<float> correlationResult(maxLag - minLag + 1, 0.0f);

    for (size_t lag = minLag; lag <= maxLag; ++lag)
    {
        float correlation = 0.0f;
        for (size_t i = 0; i < frame.frameSamples.size() - lag; ++i)
        {
            correlation += frame.frameSamples[i] * frame.frameSamples[i + lag];
        }
        correlationResult[lag - minLag] = correlation;
    }

    // Find the lag with the maximum correlation
    auto maxIt = std::max_element(
        correlationResult.begin(),
        correlationResult.end());

    float maxCorrelation = *maxIt;

    size_t maxIndex = std::distance(
        correlationResult.begin(),
        maxIt);

    float bestLag = static_cast<float>(minLag + maxIndex);

    // Interpolation
    if (maxIndex > 0 && maxIndex < correlationResult.size() - 1)
    {
        float y0 = correlationResult[maxIndex - 1];
        float y1 = correlationResult[maxIndex];
        float y2 = correlationResult[maxIndex + 1];

        float denominator = y0 - 2.0f * y1 + y2;

        if (denominator != 0.0f)
        {
            float delta = 0.5f * (y0 - y2) / denominator;
            bestLag += delta;
        }
    }

    float detectedPitch = sampleRate / bestLag;

    std::cout << "Best lag: " << bestLag << "\n";
    std::cout << "Detected pitch: " << detectedPitch << " Hz\n";

    return detectedPitch;
}