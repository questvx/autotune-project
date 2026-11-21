#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "dsp/pitchCorrection/PitchFrameStabilizer.h"
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

PitchAnalysisResult makeAnalysisFrame(
    float frequency,
    float confidence,
    int targetMidi,
    float targetFrequency)
{
    PitchAnalysisResult frame{};
    frame.pitchResult.frequency = frequency;
    frame.pitchResult.correlation = confidence;
    frame.note = {targetMidi, "Synthetic", frequency};
    frame.targetNote = {
        targetMidi,
        "Synthetic target",
        targetFrequency};
    frame.targetFrequency = targetFrequency;
    return frame;
}

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
        1.0f);
    if (result.samples.size() != input.size() ||
        result.frameAnalysis.empty() ||
        result.frameDiagnostics.size() != result.frameAnalysis.size())
    {
        std::cout << "Automatic correction: FAIL, invalid output\n";
        return false;
    }

    size_t framesTargetingC = 0;
    size_t stableFramesTargetingC = 0;
    double centsSum = 0.0;
    size_t voicedFrames = 0;
    const PitchCorrectionFrameDiagnostics* firstVoicedDiagnostics =
        nullptr;
    for (size_t i = 0; i < result.frameAnalysis.size(); ++i)
    {
        const PitchAnalysisResult& frame = result.frameAnalysis[i];
        const PitchCorrectionFrameDiagnostics& diagnostics =
            result.frameDiagnostics[i];
        if (frame.pitchResult.frequency > 0.0f)
        {
            ++voicedFrames;
            centsSum += frame.cents;
            if (frame.targetNote.midi == 60)
                ++framesTargetingC;
        }

        if (diagnostics.voiced && diagnostics.targetMidi == 60)
            ++stableFramesTargetingC;
        if (diagnostics.voiced && firstVoicedDiagnostics == nullptr)
            firstVoicedDiagnostics = &diagnostics;
        if (!std::isfinite(diagnostics.trackedFrequency) ||
            !std::isfinite(diagnostics.smoothedTargetFrequency) ||
            !std::isfinite(diagnostics.pitchRatio))
        {
            std::cout << "Automatic correction: FAIL, invalid frame diagnostics\n";
            return false;
        }
    }

    const size_t start =
        static_cast<size_t>(expectedSampleRate * 0.5f);
    const size_t end =
        static_cast<size_t>(expectedSampleRate * 1.5f);
    size_t positiveCrossings = 0;
    float maximumSampleStep = 0.0f;
    for (size_t i = start + 1; i < end; ++i)
    {
        if (result.samples[i - 1] <= 0.0f && result.samples[i] > 0.0f)
            ++positiveCrossings;
        maximumSampleStep = std::max(
            maximumSampleStep,
            std::abs(result.samples[i] - result.samples[i - 1]));
    }

    const float measuredFrequency =
        static_cast<float>(positiveCrossings) * expectedSampleRate /
        static_cast<float>(end - start);

    const PitchCorrectionFrameDiagnostics& finalDiagnostics =
        result.frameDiagnostics.back();
    const bool passed =
        framesTargetingC * 100 >= result.frameAnalysis.size() * 95 &&
        stableFramesTargetingC * 100 >=
            result.frameDiagnostics.size() * 95 &&
        firstVoicedDiagnostics != nullptr &&
        std::abs(firstVoicedDiagnostics->smoothedTargetFrequency -
                 targetFrequency) <= 0.5f &&
        std::abs(finalDiagnostics.smoothedTargetFrequency -
                 targetFrequency) <= 0.5f &&
        std::abs(measuredFrequency - targetFrequency) <= 3.0f &&
        firstVoicedDiagnostics->pitchShiftApplied &&
        std::abs(firstVoicedDiagnostics->pitchRatio -
                 targetFrequency / inputFrequency) < 0.02f &&
        std::abs(firstVoicedDiagnostics->smoothedCorrectionCents -
                 firstVoicedDiagnostics->requestedCorrectionCents) <
            0.01f &&
        maximumSampleStep < 0.12f;

    std::cout << "Automatic correction: C#4 to C4, measured "
              << measuredFrequency << " Hz, mean input deviation "
              << (voicedFrames > 0 ? centsSum / voicedFrames : 0.0)
              << " cents, maximum sample step "
              << maximumSampleStep
              << " samples, "
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
        !result.frameAnalysis.empty() &&
        result.frameDiagnostics.size() == result.frameAnalysis.size();
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
        result.frameAnalysis.empty() ||
        result.frameDiagnostics.size() != result.frameAnalysis.size())
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

bool checkPitchFrameStabilizer()
{
    const PitchAnalysisResult a4 = makeAnalysisFrame(
        440.0f,
        0.98f,
        69,
        440.0f);

    std::vector<PitchAnalysisResult> octaveOutlier(5, a4);
    octaveOutlier[2] = makeAnalysisFrame(
        880.0f,
        0.98f,
        81,
        880.0f);
    const std::vector<TrackedPitchFrame> stableOctave =
        PitchFrameStabilizer::stabilize(
            octaveOutlier,
            50.0f,
            1000.0f);
    const bool octavePassed =
        stableOctave.size() == octaveOutlier.size() &&
        stableOctave[2].voiced &&
        stableOctave[2].targetNote.midi == 69 &&
        std::abs(stableOctave[2].frequency - 440.0f) < 1.0f;

    std::vector<PitchAnalysisResult> shortGap(5, a4);
    shortGap[2] = makeAnalysisFrame(0.0f, 0.0f, -1, 0.0f);
    const std::vector<TrackedPitchFrame> stableGap =
        PitchFrameStabilizer::stabilize(
            shortGap,
            50.0f,
            1000.0f);
    const bool gapPassed =
        stableGap[2].voiced &&
        stableGap[2].targetNote.midi == 69 &&
        std::abs(stableGap[2].frequency - 440.0f) < 1.0f;

    std::vector<PitchAnalysisResult> lowConfidence(5, a4);
    lowConfidence[2] = makeAnalysisFrame(
        440.0f,
        0.5f,
        69,
        440.0f);
    const std::vector<TrackedPitchFrame> stableLowConfidence =
        PitchFrameStabilizer::stabilize(
            lowConfidence,
            50.0f,
            1000.0f);
    const bool confidencePassed =
        stableLowConfidence[2].voiced &&
        std::abs(stableLowConfidence[2].frequency - 440.0f) < 1.0f;

    std::vector<PitchAnalysisResult> targetFlicker{
        a4,
        makeAnalysisFrame(493.8833f, 0.98f, 71, 493.8833f),
        a4};
    const std::vector<TrackedPitchFrame> stableFlicker =
        PitchFrameStabilizer::stabilize(
            targetFlicker,
            50.0f,
            1000.0f);

    std::vector<PitchAnalysisResult> confirmedTargetChange{
        a4,
        a4,
        makeAnalysisFrame(493.8833f, 0.98f, 71, 493.8833f),
        makeAnalysisFrame(493.8833f, 0.98f, 71, 493.8833f),
        makeAnalysisFrame(493.8833f, 0.98f, 71, 493.8833f)};
    const std::vector<TrackedPitchFrame> stableTargetChange =
        PitchFrameStabilizer::stabilize(
            confirmedTargetChange,
            50.0f,
            1000.0f);
    const bool hysteresisPassed =
        stableFlicker[1].targetNote.midi == 69 &&
        stableTargetChange[2].targetNote.midi == 69 &&
        stableTargetChange[3].targetNote.midi == 71;

    std::vector<PitchAnalysisResult> octaveTargetChange{
        a4,
        makeAnalysisFrame(110.0f, 0.98f, 45, 110.0f),
        makeAnalysisFrame(110.0f, 0.98f, 45, 110.0f)};
    const std::vector<TrackedPitchFrame> stableOctaveTargetChange =
        PitchFrameStabilizer::stabilize(
            octaveTargetChange,
            50.0f,
            1000.0f);
    const bool octaveHysteresisPassed =
        stableOctaveTargetChange[1].targetNote.midi == 45 &&
        std::abs(stableOctaveTargetChange[1].targetFrequency -
                 110.0f) < 0.1f;

    std::vector<PitchAnalysisResult> largeTargetChange{
        a4,
        makeAnalysisFrame(587.3295f, 0.98f, 74, 587.3295f),
        makeAnalysisFrame(587.3295f, 0.98f, 74, 587.3295f)};
    const std::vector<TrackedPitchFrame> stableLargeTargetChange =
        PitchFrameStabilizer::stabilize(
            largeTargetChange,
            50.0f,
            1000.0f);
    const bool largeChangePassed =
        stableLargeTargetChange[1].targetNote.midi == 74;

    std::vector<PitchAnalysisResult> naturalPitchVariation{
        makeAnalysisFrame(430.0f, 0.98f, 69, 440.0f),
        makeAnalysisFrame(435.0f, 0.98f, 69, 440.0f),
        makeAnalysisFrame(440.0f, 0.98f, 69, 440.0f),
        makeAnalysisFrame(445.0f, 0.98f, 69, 440.0f),
        makeAnalysisFrame(450.0f, 0.98f, 69, 440.0f)};
    const std::vector<TrackedPitchFrame> stableNaturalVariation =
        PitchFrameStabilizer::stabilize(
            naturalPitchVariation,
            50.0f,
            1000.0f);
    bool naturalVariationPassed =
        stableNaturalVariation.size() == naturalPitchVariation.size();
    for (size_t i = 0; i < stableNaturalVariation.size(); ++i)
    {
        naturalVariationPassed = naturalVariationPassed &&
            stableNaturalVariation[i].voiced &&
            std::abs(stableNaturalVariation[i].frequency -
                     naturalPitchVariation[i].pitchResult.frequency) <
                0.1f;
    }

    const bool passed =
        octavePassed &&
        gapPassed &&
        confidencePassed &&
        hysteresisPassed &&
        octaveHysteresisPassed &&
        largeChangePassed &&
        naturalVariationPassed;
    std::cout << "Pitch-frame stabilization (octave spike, short gap, "
                 "confidence, target hysteresis, natural F0 movement): "
              << (passed ? "PASS" : "FAIL") << '\n';
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
    passed = checkPitchFrameStabilizer() && passed;
    return passed ? 0 : 1;
}
