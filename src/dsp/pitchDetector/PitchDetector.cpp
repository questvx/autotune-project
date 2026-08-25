#include "PitchDetector.h"
#include <iostream>

float PitchDetector::detectPitch(const AudioFrame &frame, float sampleRate, float minFreq, float maxFreq)
{
    std::cout << "\n--- Pitch Detector ---" << std::endl;
    std::cout << " Frame size: " << frame.frameSamples.size() << std::endl;
    std::cout << "Frequency range: " << minFreq << " -" << maxFreq << " Hz" << std::endl;

    return 0.0f;
}