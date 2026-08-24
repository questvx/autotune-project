//WavReader.h
#pragma once
#include <filesystem>
#include <vector>

class WavReader
{
    private:
        std::vector<float> audioSamples; // Vector to hold audio samples
        unsigned int sampleRate = 0;
        unsigned int channels = 0;

    public:
        bool load(const std::filesystem::path& path);    
        // bool read(const std::filesystem::path& path);

        const std::vector<float>& getSamples() const;
        unsigned int getSampleRate() const;
        unsigned int getChannels() const;
}; 