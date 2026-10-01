// main.cpp
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.h"
#include "scale/Scale.h"

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
    return exePath.parent_path() / ".." / ".." / "assets" / "sine_880.wav";
}

// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
    FrameGenerator generator;
    PitchAnalyzer analyzer;
    Scale scale(0, ScaleType::Major);

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
        PitchAnalysisResult analysis = analyzer.analyze(
            frames[i],
            static_cast<float>(reader.getSampleRate()),
            50.0f,
            1000.0f,
            scale);

        float time = static_cast<float>(i * 512) / reader.getSampleRate();

        const char* noteName = analysis.note.name ? analysis.note.name : "Invalid";
        const char* targetNoteName = analysis.targetNote.name ? analysis.targetNote.name : "Invalid";

        std::cout << "\nTime: "
                  << std::fixed << std::setprecision(3)
                  << time
                  << " s      Pitch: "
                  << analysis.pitchResult.frequency
                  << " | Frequency: "
                  << analysis.targetFrequency
                  << " Hz | Correlation: "
                  << analysis.pitchResult.correlation
                  << "      Note: "
                  << noteName
                  << " | MIDI: "
                  << analysis.note.midi
                  << "| Cents: "
                  << analysis.cents
                  << " ct | Target Note: "
                  << targetNoteName
                  << " | MIDI: "
                  << analysis.targetNote.midi;
    }

    return 0;
}