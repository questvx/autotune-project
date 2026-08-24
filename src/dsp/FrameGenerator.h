// FrameGenerator.h
#pragma once
#include <vector>
#include "AudioFrame.h"

class FrameGenerator
{
    public:
        // FrameGenerator(const std::vector<float>& audioData, size_t frameSize);
        
        std::vector<AudioFrame> generateFrames(const std::vector<float>& audioSamples, size_t frameSize);
        
};