// main.cpp
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "wavReader/WavReader.h"
#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "scale/Scale.h"
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
    return exePath.parent_path() / ".." / ".." / "assets" / "gosia1.wav";
}

// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
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

    const float sampleRate = static_cast<float>(reader.getSampleRate());
    AutoPitchCorrectionResult correction = AutoPitchCorrector::process(
        reader.getSamples(),
        sampleRate,
        scale,
        2048,
        512,
        50.0f,
        1000.0f,
        1.0f);
    if (correction.samples.size() != reader.getSamples().size() ||
        correction.frameAnalysis.empty() ||
        correction.frameDiagnostics.size() !=
            correction.frameAnalysis.size())
    {
        std::cerr << "Failed to automatically correct the input audio.\n";
        return 1;
    }

    std::cout << "Output samples: "
              << correction.samples.size() << '\n';
    std::cout << "Analyzed frames: "
              << correction.frameAnalysis.size() << '\n';

    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = reader.getSampleRate();
    format.bitsPerSample = 32;

    drwav wav{};
    bool saved = drwav_init_file_write_sequential_pcm_frames(
                     &wav,
                     "output_3.wav",
                     &format,
                     static_cast<drwav_uint64>(correction.samples.size()),
                     nullptr) != 0;

    if (saved)
    {
        const drwav_uint64 framesWritten = drwav_write_pcm_frames(
            &wav,
            static_cast<drwav_uint64>(correction.samples.size()),
            correction.samples.data());
        const drwav_result uninitResult = drwav_uninit(&wav);
        saved = framesWritten == correction.samples.size() &&
            uninitResult == DRWAV_SUCCESS;
    }

    if (saved)
        std::cout << "Output saved to output_3.wav\n";
    else
        std::cout << "Failed to save output_3.wav\n";
    //------------------------------------------------------------------------

    for (size_t i = 0; i < correction.frameAnalysis.size(); ++i)
    {
        const PitchAnalysisResult& analysis = correction.frameAnalysis[i];
        const PitchCorrectionFrameDiagnostics& diagnostics =
            correction.frameDiagnostics[i];
        const float time = static_cast<float>(i * 512) /
            static_cast<float>(reader.getSampleRate());

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
                  << analysis.targetNote.midi
                  << " | Tracked F0: "
                  << diagnostics.trackedFrequency
                  << " Hz | Stable target MIDI: "
                  << diagnostics.targetMidi
                  << " | Smoothed target: "
                  << diagnostics.smoothedTargetFrequency
                  << " Hz | Requested correction: "
                  << diagnostics.requestedCorrectionCents
                  << " ct | Applied correction: "
                  << diagnostics.smoothedCorrectionCents
                  << " ct | Ratio: "
                  << diagnostics.pitchRatio
                  << " | Local phase shift: "
                  << diagnostics.phaseShiftSamples
                  << " samples | PSOLA: "
                  << (diagnostics.pitchShiftApplied ? "yes" : "no")
                  << " | Phase alignment correlation: "
                  << diagnostics.phaseAlignmentCorrelation
                  << " | Search limit: "
                  << diagnostics.phaseShiftSearchLimitSamples
                  << " samples | Boundary hit: "
                  << (diagnostics.phaseShiftAtSearchLimit ? "yes" : "no")
                  << " | Alignment valid: "
                  << (diagnostics.phaseAlignmentValid ? "yes" : "no");
    }

    return 0;
}