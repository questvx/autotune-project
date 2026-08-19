#pragma once
#include <vector>
#include "AudioFrame.h"

class FrameGenerator
{
    public:
        FrameGenerator(size_t frameSize, size_t hopSize);
        std::vector<AudioFrame> generateFrames(const std::vector<float>& audioData);
};