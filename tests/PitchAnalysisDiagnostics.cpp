#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "dsp/pitchAnalysis/pitchDetector/PitchDetector.h"
#include "dsp/pitchAnalysis/pitchToNote/PitchToNote.h"
#include "dsp/pitchAnalysis/pitchToNote/TargetNote.h"
#include "scale/Scale.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr float expectedSampleRate = 44100.0f;
constexpr float frequencyTolerance = 0.03f;
constexpr float pi = 3.14159265358979323846f;

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

bool checkAutomaticCorrection()
{
    constexpr float inputFrequency = 277.1826f;
    constexpr float targetFrequency = 261.6256f;
    const size_t sampleCount =
        static_cast<size_t>(expectedSampleRate * 2.0f);
    std::vector<float> input(sampleCount);
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.5f * std::sin(
            2.0f * pi * inputFrequency * static_cast<float>(i) /
            expectedSampleRate);
    }

    const Scale cMajor(60, ScaleType::Major);
    const AutoPitchCorrectionResult result = AutoPitchCorrector::process(
        input,
        expectedSampleRate,
        cMajor,
        2048,
        512,
        50.0f,
        1000.0f,
        0.2f);
    if (result.samples.size() != input.size() ||
        result.frameAnalysis.empty())
    {
        std::cout << "Automatic correction: FAIL, invalid output\n";
        return false;
    }

    size_t framesTargetingC = 0;
    double centsSum = 0.0;
    size_t voicedFrames = 0;
    for (const PitchAnalysisResult& frame : result.frameAnalysis)
    {
        if (frame.pitchResult.frequency > 0.0f)
        {
            ++voicedFrames;
            centsSum += frame.cents;
            if (frame.targetNote.midi == 60)
                ++framesTargetingC;
        }
    }

    const size_t start =
        static_cast<size_t>(expectedSampleRate * 0.5f);
    const size_t end =
        static_cast<size_t>(expectedSampleRate * 1.5f);
    size_t positiveCrossings = 0;
    for (size_t i = start + 1; i < end; ++i)
    {
        if (result.samples[i - 1] <= 0.0f && result.samples[i] > 0.0f)
            ++positiveCrossings;
    }

    const float measuredFrequency =
        static_cast<float>(positiveCrossings) * expectedSampleRate /
        static_cast<float>(end - start);

    const bool passed =
        framesTargetingC * 100 >= result.frameAnalysis.size() * 95 &&
        std::abs(measuredFrequency - targetFrequency) <= 3.0f;

    std::cout << "Automatic correction: C#4 to C4, measured "
              << measuredFrequency << " Hz, mean input deviation "
              << (voicedFrames > 0 ? centsSum / voicedFrames : 0.0)
              << " cents, "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}

bool checkUnvoicedPassThrough()
{
    const std::vector<float> input(
        static_cast<size_t>(expectedSampleRate * 0.25f),
        0.0f);
    const Scale cMajor(60, ScaleType::Major);
    const AutoPitchCorrectionResult result = AutoPitchCorrector::process(
        input,
        expectedSampleRate,
        cMajor,
        2048,
        512,
        50.0f,
        1000.0f,
        0.2f);

    const bool passed =
        result.samples == input &&
        !result.frameAnalysis.empty();
    std::cout << "Unvoiced pass-through: "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}

bool checkInScaleToneIsUnchanged()
{
    constexpr float inputFrequency = 440.0f;
    const size_t sampleCount = static_cast<size_t>(expectedSampleRate);
    std::vector<float> input(sampleCount);
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.5f * std::sin(
            2.0f * pi * inputFrequency * static_cast<float>(i) /
            expectedSampleRate);
    }

    const Scale cMajor(60, ScaleType::Major);
    const AutoPitchCorrectionResult result = AutoPitchCorrector::process(
        input,
        expectedSampleRate,
        cMajor,
        2048,
        512,
        50.0f,
        1000.0f,
        0.2f);
    if (result.samples.size() != input.size() ||
        result.frameAnalysis.empty())
    {
        std::cout << "In-scale 440 Hz pass-through: FAIL, invalid output\n";
        return false;
    }

    float maximumDifference = 0.0f;
    for (size_t i = 0; i < input.size(); ++i)
    {
        maximumDifference = std::max(
            maximumDifference,
            std::abs(input[i] - result.samples[i]));
    }

    const bool passed = maximumDifference < 1e-5f;
    std::cout << "In-scale 440 Hz pass-through: "
              << (passed ? "PASS" : "FAIL")
              << ", maximum sample difference "
              << maximumDifference << '\n';
    return passed;
}
}

int main()
{
    bool passed = true;
    passed = checkSineFrames("assets/sine_220.wav", 220.0f) && passed;
    passed = checkSineFrames("assets/sine_880.wav", 880.0f) && passed;
    passed = checkCMajorANotes() && passed;
    passed = checkAutomaticCorrection() && passed;
    passed = checkUnvoicedPassThrough() && passed;
    passed = checkInScaleToneIsUnchanged() && passed;
    return passed ? 0 : 1;
}
