//FrameGenerator.cpp
#include "FrameGenerator.h"
#include <iostream>
#include <vector>

std::vector<AudioFrame> FrameGenerator::generateFrames(const std::vector<float>& audioSamples, size_t frameSize)
{
    std::vector<AudioFrame> frames;
    size_t totalSamples = audioSamples.size();
    size_t numFrames = totalSamples / frameSize;

    for (size_t i = 0; i < numFrames; ++i)
    {
        AudioFrame frame;
        // Copy the samples for this frame
        frame.frameSamples.insert(frame.frameSamples.end(),
                                  audioSamples.begin() + i * frameSize,
                                  audioSamples.begin() + (i + 1) * frameSize);
        frames.push_back(frame);
    }

    return frames;
}