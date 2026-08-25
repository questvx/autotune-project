// main.cpp
#include <iostream>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/FrameGenerator.h"

using namespace std;

// Function to get the absolute path of the executable
static filesystem::path getExecutablePath(const char *argv0)
{
    filesystem::path exePath(argv0);
    if (exePath.is_relative())
    {
        exePath = filesystem::current_path() / exePath;
    }
    return filesystem::weakly_canonical(exePath);
}

static filesystem::path getAssetPath(const filesystem::path &exePath)
{
    return exePath.parent_path() / ".." / ".." / "assets" / "sine_440.wav";
}

// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
    FrameGenerator generator;
    filesystem::path exePath = getExecutablePath(argv[0]);
    filesystem::path assetPath = getAssetPath(exePath);

    cout << "Executable path: " << exePath << "\n";
    cout << "Asset path: " << assetPath << "\n";
    cout << "=======Launching AutoTune=======" << endl;

    if (!reader.load(assetPath))
    {
        return 1;
    }

    // Print out some information about the WAV file for debugging purposes
    cout << "Sample Rate (frames/second): " << reader.getSampleRate() << endl;
    cout << "Channels: " << reader.getChannels() << endl;
    cout << "First 10 samples of the first channel: ";
    for (int i = 0; i < 10; i++)
    {
        std::cout << i << ": " << reader.getSamples()[i] << "\n";
    }
    // number of samples, frame size, hop size
    generator.generateFrames(reader.getSamples(), 2048, 512);

    return 0;
}