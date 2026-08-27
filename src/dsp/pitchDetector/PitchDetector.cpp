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
    float threshold = 0.75f; // Threshold for correlation
    float correlation = 0.0f;
    float energy1 = 0.0f;
    float energy2 = 0.0f;

    float normalizedCorrelation = 0.0f;

    for (size_t lag = minLag; lag <= maxLag; ++lag)
    {
        correlation = 0.0f;
        energy1 = 0.0f;
        energy2 = 0.0f;

        for (size_t i = 0; i < frame.frameSamples.size() - lag; ++i)
        {
            float sample1 = frame.frameSamples[i];
            float sample2 = frame.frameSamples[i + lag];

            correlation += sample1 * sample2;
            energy1 += sample1 * sample1;
            energy2 += sample2 * sample2;
        }

        if (energy1 > 0.0f && energy2 > 0.0f)
        {
            normalizedCorrelation = correlation / (sqrt(energy1 * energy2));
        }
        else
        {
            normalizedCorrelation = 0.0f;
        }
        correlationResult[lag - minLag] = normalizedCorrelation;
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

    if (maxCorrelation < threshold)
    {
        std::cout << "CORRELATION BELOW THRESHOLD: " << maxCorrelation << std::endl;
        return 0.0f;
    }

    float detectedPitch = sampleRate / bestLag;

    std::cout << "Best lag: " << bestLag << "\n";
    std::cout << "Detected pitch: " << detectedPitch << " Hz\n";
    std::cout << "Correlation: " << maxCorrelation << "\n";

    return detectedPitch;
}