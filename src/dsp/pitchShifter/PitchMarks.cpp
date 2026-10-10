#include "PitchMarks.h"
#include <cmath>

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

    // Estimated period of the signal in samples
    float period = sampleRate / frequency;

    // Start at the beginning of the frame
    float currentPosition = 0.0f;

    while (currentPosition < samples.size())
    {
        marks.push_back(
            static_cast<int>(std::round(currentPosition))
        );

        currentPosition += period;
    }

    return marks;
}