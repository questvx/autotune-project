// main.cpp
#include <iostream>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchDetector/PitchDetector.h"

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
    return exePath.parent_path() / ".." / ".." / "assets" / "vocal_2.wav";
}

// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
    FrameGenerator generator;
    PitchDetector detector;

    filesystem::path exePath = getExecutablePath(argv[0]);
    filesystem::path assetPath = getAssetPath(exePath);

    cout << "Executable path: " << exePath << "\n";
    cout << "Asset path: " << assetPath << "\n";
    cout << "\n=======Launching AutoTune=======" << endl;

    if (!reader.load(assetPath))
    {
        return 1;
    }

    auto frames = generator.generateFrames(
        reader.getSamples(),
        2048,
        512);

    for (size_t i = 0; i < frames.size(); ++i)
    {
        float pitch = detector.detectPitch(
            frames[i],
            static_cast<float>(reader.getSampleRate()),
            50.0f,
            1000.0f);

        cout << "Frame " << i << ": Detected pitch: " << pitch << " Hz\n";
        float time = static_cast<float>(i * 512) / reader.getSampleRate();

        std::cout << "Time: "
                  << std::fixed << std::setprecision(3)
                  << time
                  << " s | Pitch: "
                  << pitch
                  << " Hz\n";
    }

    return 0;
}