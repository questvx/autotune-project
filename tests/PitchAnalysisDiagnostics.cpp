#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchAnalysis/pitchDetector/PitchDetector.h"
#include "dsp/pitchAnalysis/pitchToNote/PitchToNote.h"
#include "dsp/pitchAnalysis/pitchToNote/TargetNote.h"
#include "scale/Scale.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr float expectedSampleRate = 44100.0f;
constexpr float frequencyTolerance = 0.03f;

bool checkSineFrames(
    const std::string& path,
    float expectedFrequency)
{
    WavReader reader;
    if (!reader.load(path))
    {
        std::cerr << "Could not load " << path << '\n';
        return false;
    }

    if (reader.getSampleRate() !=
            static_cast<unsigned int>(expectedSampleRate) ||
        reader.getChannels() != 1)
    {
        std::cerr << "Unexpected WAV format in " << path << '\n';
        return false;
    }

    FrameGenerator frameGenerator;
    PitchDetector pitchDetector;
    const std::vector<AudioFrame> frames = frameGenerator.generateFrames(
        reader.getSamples(), 2048, 512);

    size_t failures = 0;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const PitchResult result = pitchDetector.detectPitch(
            frames[i],
            expectedSampleRate,
            50.0f,
            1000.0f);

        const float relativeError = std::abs(
            result.frequency - expectedFrequency) / expectedFrequency;
        if (!std::isfinite(result.frequency) ||
            relativeError > frequencyTolerance)
        {
            if (failures < 5)
            {
                std::cout << path << ", frame " << i
                          << ": expected " << expectedFrequency
                          << " Hz, detected " << result.frequency
                          << " Hz, correlation " << result.correlation
                          << '\n';
            }
            ++failures;
        }
    }

    std::cout << path << ": " << frames.size() << " frames, "
              << failures << " pitch errors"
              << (failures == 0 ? " PASS" : " FAIL") << '\n';
    return failures == 0;
}

bool checkCMajorANotes()
{
    const Scale cMajor(60, ScaleType::Major);
    bool passed = true;

    for (int midiNote : {45, 57, 69})
    {
        const Note note = PitchToNote::frequencyToNote(
            PitchToNote::midiNoteToFrequency(midiNote));
        const Note target = TargetNote::getTargetNote(note, cMajor);
        const bool notePassed =
            cMajor.isNoteAllowed(midiNote) && target.midi == midiNote;

        std::cout << note.name << " MIDI " << midiNote
                  << " in C major: "
                  << (notePassed ? "PASS" : "FAIL")
                  << ", target MIDI " << target.midi << '\n';
        passed = notePassed && passed;
    }

    return passed;
}
}

int main()
{
    bool passed = true;
    passed = checkSineFrames("assets/sine_220.wav", 220.0f) && passed;
    passed = checkSineFrames("assets/sine_880.wav", 880.0f) && passed;
    passed = checkCMajorANotes() && passed;
    return passed ? 0 : 1;
}
