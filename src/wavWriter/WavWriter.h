#pragma once

#include <filesystem>
#include <vector>

class WavWriter
{
public:
    static bool writeFloatWav(
        const std::filesystem::path& path,
        unsigned int sampleRate,
        const std::vector<float>& interleavedSamples,
        unsigned int channelCount);
};
