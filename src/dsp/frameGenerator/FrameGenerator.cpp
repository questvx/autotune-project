// FrameGenerator.cpp

#include "FrameGenerator.h"
#include <vector>
#include <algorithm>
#include <iostream>

std::vector<AudioFrame> FrameGenerator::generateFrames(
    const std::vector<float> &audioSamples,
    size_t frameSize,
    size_t hopSize)
{
    std::vector<AudioFrame> frames;

    size_t totalSamples = audioSamples.size();

    size_t overlapSamples = frameSize - hopSize;
    float overlapPercent = (static_cast<float>(overlapSamples) / frameSize) * 100.0f;

    std::cout << "\n--- Frame Generator ---\n";
    std::cout << "Total samples: " << totalSamples << "\n";
    std::cout << "Frame size: " << frameSize << "\n";
    std::cout << "Hop size: " << hopSize << "\n";
    std::cout << "Overlap: " << overlapPercent << "% \n";

    // Catch edge cases
    if (frameSize == 0)
    {
        std::cout << "Error: Frame size cannot be 0.\n";
        return frames;
    }

    if (hopSize == 0)
    {
        std::cout << "Error: Hop size cannot be 0.\n";
        return frames;
    }

    if (hopSize > frameSize)
    {
        std::cout << "Error: Hop size cannot be larger than frame size.\n";
        return frames;
    }

    // Ensure that all samples are included
    size_t numFrames = (totalSamples + hopSize - 1) / hopSize;

    std::cout << "Number of frames: " << numFrames << "\n";

    for (size_t i = 0; i < numFrames; ++i)
    {
        AudioFrame frame;

        size_t start = i * hopSize;
        size_t remainingSamples = totalSamples - start;

        size_t samplesToCopy = std::min(frameSize, remainingSamples);

        frame.frameSamples.insert(
            frame.frameSamples.end(),
            audioSamples.begin() + start,
            audioSamples.begin() + start + samplesToCopy);

        // Zero-padding
        size_t paddingSamples = hopSize - samplesToCopy;
        frame.frameSamples.resize(frameSize, 0.0f);

        frames.push_back(frame);
        // Debug - only for the last frame
        if (i == numFrames - 1)
        {
            std::cout << "Last frame: "
                      << samplesToCopy << " samples + "
                      << paddingSamples << " zero-padding"
                      << "\n";
        }
    }

    std::cout << "Generated frames: " << frames.size() << "\n";

    return frames;
}