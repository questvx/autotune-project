// main.cpp
#include <iostream>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchDetector/PitchDetector.h"
#include "dsp/note/PitchToNote.h"
#include "dsp/cents/Cents.h"

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
    return exePath.parent_path() / ".." / ".." / "assets" / "guitar_h.wav";
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
        PitchResult pitchResult = detector.detectPitch(
            frames[i],
            static_cast<float>(reader.getSampleRate()),
            50.0f,
            1000.0f);

        float time = static_cast<float>(i * 512) / reader.getSampleRate();

        Note note = PitchToNote::frequencyToNote(pitchResult.frequency);
        float cents = Cents::calculate(pitchResult.frequency, note.frequency);
        std::cout << "\nTime: "
                  << std::fixed << std::setprecision(3)
                  << time
                  << " s      Pitch: "
                  << pitchResult.frequency
                  << " | Frequency: "
                  << note.frequency
                  << " Hz | Correlation: "
                  << pitchResult.correlation
                  << "      Note: "
                  << note.name
                  << " | MIDI: "
                  << note.midi
                  << "| Cents: "
                  << cents
                  << " ct";
    }

    return 0;
}