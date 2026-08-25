#include "PitchDetector.h"
#include <iostream>

float PitchDetector::detectPitch(const AudioFrame &frame, float sampleRate, float minFrequency, float maxFrequency)
{
    std::cout << "\n--- Pitch Detector ---" << std::endl;
    std::cout << " Frame size: " << frame.frameSamples.size() << std::endl;
    std::cout << "Frequency range: " << minFrequency << " -" << maxFrequency << " Hz" << std::endl;

    size_t minLag = static_cast<size_t>(sampleRate / maxFrequency);
    size_t maxLag = static_cast<size_t>(sampleRate / minFrequency);

    //Handle edge case
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

    float maxCorrelation = *std::max_element(correlationResult.begin(), correlationResult.end());
    float bestLag = std::distance(correlationResult.begin(), std::max_element(correlationResult.begin(), correlationResult.end())) + minLag;
    float detectedPitch = sampleRate / bestLag;

    std::cout << "Best lag " << bestLag << "\nCorrelation " << maxCorrelation << "\nDetected pitch: " << detectedPitch << " Hz" << std::endl;
    return detectedPitch;
}