// main.cpp
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.h"
#include "scale/Scale.h"
// temporary
#include "dsp/PitchShifter/PitchMarks.h"
#include "dsp/PitchShifter/PSOLAProcessor.h"
#include <cmath>
#include "../external/dr_wav.h"

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
    return exePath.parent_path() / ".." / ".." / "assets" / "szklanki.wav";
}

// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
    FrameGenerator generator;
    PitchAnalyzer analyzer;
    Scale scale(60, ScaleType::Major);

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

    // Temporary --------------
    float testFrequency = 110.626f;
    float targetFrequency = 130.16f;

    float pitchRatio = targetFrequency / testFrequency;

    float period = 44100.0f / testFrequency;

    auto pitchMarks = PitchMarks::detect(
        reader.getSamples(),
        testFrequency,
        44100.0f);

    auto output = PSOLAProcessor::process(
        reader.getSamples(),
        pitchMarks,
        pitchRatio,
        static_cast<int>(std::round(period)));

    std::cout << "Pitch marks: "
              << pitchMarks.size() << '\n';

    std::cout << "Pitch ratio: "
              << pitchRatio << '\n';

    std::cout << "Output samples: "
              << output.size() << '\n';

    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = 44100;
    format.bitsPerSample = 32;

    drwav wav{};
    bool saved = drwav_init_file_write_sequential_pcm_frames(
        &wav,
        "output.wav",
        &format,
        static_cast<drwav_uint64>(output.size()),
        nullptr) != 0;

    if (saved)
    {
        const drwav_uint64 framesWritten = drwav_write_pcm_frames(
            &wav,
            static_cast<drwav_uint64>(output.size()),
            output.data());
        const drwav_result uninitResult = drwav_uninit(&wav);
        saved = framesWritten == output.size() && uninitResult == DRWAV_SUCCESS;
    }

    if (saved)
        std::cout << "Output saved to output.wav\n";
    else
        std::cout << "Failed to save output.wav\n";
    //------------------------

    for (size_t i = 0; i < frames.size(); ++i)
    {
        PitchAnalysisResult analysis = analyzer.analyze(
            frames[i],
            static_cast<float>(reader.getSampleRate()),
            50.0f,
            1000.0f,
            scale);

        float time = static_cast<float>(i * 512) / reader.getSampleRate();

        const char *noteName = analysis.note.name ? analysis.note.name : "Invalid";
        const char *targetNoteName = analysis.targetNote.name ? analysis.targetNote.name : "Invalid";

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