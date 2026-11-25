#include "wavReader/WavReader.h"
#include "dsp/frameGenerator/FrameGenerator.h"
#include "dsp/pitchCorrection/AutoPitchCorrector.h"
#include "dsp/pitchCorrection/PitchFrameStabilizer.h"
#include "dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.h"
#include "dsp/pitchAnalysis/pitchDetector/PitchDetector.h"
#include "dsp/pitchAnalysis/pitchToNote/PitchToNote.h"
#include "dsp/pitchAnalysis/pitchToNote/TargetNote.h"
#include "scale/Scale.h"
#include "../external/dr_wav.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
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

bool checkHarmonicRichToneKeepsFundamental()
{
    constexpr float fundamentalFrequency = 146.83f;
    constexpr float fundamentalAmplitude = 0.2f;
    constexpr float secondHarmonicAmplitude = 0.6f;
    AudioFrame frame;
    frame.frameSamples.resize(2048);
    for (size_t i = 0; i < frame.frameSamples.size(); ++i)
    {
        const float time = static_cast<float>(i) / expectedSampleRate;
        frame.frameSamples[i] =
            fundamentalAmplitude * std::sin(
                2.0f * pi * fundamentalFrequency * time) +
            secondHarmonicAmplitude * std::sin(
                4.0f * pi * fundamentalFrequency * time);
    }

    PitchDetector detector;
    const PitchResult result = detector.detectPitch(
        frame,
        expectedSampleRate,
        50.0f,
        1000.0f);
    const bool passed =
        std::abs(result.frequency - fundamentalFrequency) <= 2.0f &&
        result.correlation >= 0.9f;
    std::cout << "Harmonic-rich 146.83 Hz tone: "
              << (passed ? "PASS" : "FAIL")
              << ", detected=" << result.frequency
              << " Hz, correlation=" << result.correlation << '\n';
    return passed;
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

bool checkTargetSelectionUsesMeasuredPitch()
{
    constexpr size_t frameSize = 2048;
    constexpr float centsAboveFSharp = 40.0f;
    const float fSharpPlus40Cents =
        PitchToNote::midiNoteToFrequency(66) *
        std::pow(2.0f, centsAboveFSharp / 1200.0f);
    const float ePlus40Cents =
        PitchToNote::midiNoteToFrequency(64) *
        std::pow(2.0f, centsAboveFSharp / 1200.0f);

    AudioFrame fSharpFrame;
    AudioFrame eFrame;
    fSharpFrame.frameSamples.resize(frameSize);
    eFrame.frameSamples.resize(frameSize);
    for (size_t i = 0; i < frameSize; ++i)
    {
        const float time = static_cast<float>(i) / expectedSampleRate;
        fSharpFrame.frameSamples[i] = 0.5f * std::sin(
            2.0f * pi * fSharpPlus40Cents * time);
        eFrame.frameSamples[i] = 0.5f * std::sin(
            2.0f * pi * ePlus40Cents * time);
    }

    PitchAnalyzer analyzer;
    const Scale cMajor(60, ScaleType::Major);
    const Scale cMinor(60, ScaleType::Minor);
    const PitchAnalysisResult fSharpAnalysis = analyzer.analyze(
        fSharpFrame,
        expectedSampleRate,
        50.0f,
        1000.0f,
        cMajor);
    const PitchAnalysisResult eAnalysis = analyzer.analyze(
        eFrame,
        expectedSampleRate,
        50.0f,
        1000.0f,
        cMinor);

    const bool passed =
        std::abs(fSharpAnalysis.pitchResult.frequency -
                 fSharpPlus40Cents) < 1.0f &&
        fSharpAnalysis.targetNote.midi == 67 &&
        std::abs(eAnalysis.pitchResult.frequency - ePlus40Cents) < 1.0f &&
        eAnalysis.targetNote.midi == 65;
    std::cout << "Scale target uses measured pitch (F#+40c to G, "
                 "E+40c to F in C minor): "
              << (passed ? "PASS" : "FAIL")
              << ", targets "
              << fSharpAnalysis.targetNote.midi << " and "
              << eAnalysis.targetNote.midi << '\n';
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

struct PhaseAlignmentStatistics
{
    size_t validFrames = 0;
    size_t boundaryHits = 0;
    double meanCorrelation = 0.0;
    std::vector<double> wrappedFrameDeltas;
    std::vector<double> absoluteWrappedFrameDeltas;
    std::vector<double> expectedDriftResiduals;
};

PhaseAlignmentStatistics summarizePhaseAlignment(
    const AutoPitchCorrectionResult& result,
    float sampleRate,
    size_t hopSize)
{
    PhaseAlignmentStatistics statistics;
    double correlationSum = 0.0;
    const PitchCorrectionFrameDiagnostics* previous = nullptr;

    for (const PitchCorrectionFrameDiagnostics& diagnostics :
         result.frameDiagnostics)
    {
        if (!diagnostics.phaseAlignmentValid)
        {
            previous = nullptr;
            continue;
        }

        ++statistics.validFrames;
        statistics.boundaryHits +=
            diagnostics.phaseShiftAtSearchLimit ? 1 : 0;
        correlationSum += diagnostics.phaseAlignmentCorrelation;
        if (previous != nullptr)
        {
            const double period =
                sampleRate / diagnostics.targetFrequency;
            double delta = diagnostics.phaseShiftSamples -
                previous->phaseShiftSamples;
            delta -= std::round(delta / period) * period;
            const double expectedDrift =
                hopSize *
                (diagnostics.targetFrequency -
                 diagnostics.trackedFrequency) /
                diagnostics.targetFrequency;
            double driftResidual = delta - expectedDrift;
            driftResidual -= std::round(driftResidual / period) * period;
            statistics.wrappedFrameDeltas.push_back(delta);
            statistics.absoluteWrappedFrameDeltas.push_back(
                std::abs(delta));
            statistics.expectedDriftResiduals.push_back(
                std::abs(driftResidual));
        }
        previous = &diagnostics;
    }

    if (statistics.validFrames > 0)
    {
        statistics.meanCorrelation = correlationSum /
            static_cast<double>(statistics.validFrames);
    }
    std::sort(
        statistics.wrappedFrameDeltas.begin(),
        statistics.wrappedFrameDeltas.end());
    std::sort(
        statistics.absoluteWrappedFrameDeltas.begin(),
        statistics.absoluteWrappedFrameDeltas.end());
    std::sort(
        statistics.expectedDriftResiduals.begin(),
        statistics.expectedDriftResiduals.end());
    return statistics;
}

double percentile(
    const std::vector<double>& sortedValues,
    double percentileValue)
{
    if (sortedValues.empty())
        return 0.0;

    const size_t index = static_cast<size_t>(std::ceil(
        percentileValue * sortedValues.size())) - 1;
    return sortedValues[std::min(index, sortedValues.size() - 1)];
}

float estimatePositiveCrossingFrequency(
    const std::vector<float>& samples,
    float sampleRate)
{
    if (samples.size() < static_cast<size_t>(sampleRate * 0.2f))
        return 0.0f;

    const size_t start = static_cast<size_t>(sampleRate * 0.1f);
    const size_t end = samples.size() - start;
    size_t crossings = 0;
    for (size_t i = start + 1; i < end; ++i)
    {
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f)
            ++crossings;
    }

    return static_cast<float>(crossings) * sampleRate /
        static_cast<float>(end - start);
}

double calculateRmsDifference(
    const std::vector<float>& first,
    const std::vector<float>& second)
{
    if (first.size() != second.size() || first.empty())
        return 0.0;

    double sumSquares = 0.0;
    for (size_t i = 0; i < first.size(); ++i)
    {
        const double difference = first[i] - second[i];
        sumSquares += difference * difference;
    }
    return std::sqrt(sumSquares / static_cast<double>(first.size()));
}

bool checkPhaseAlignmentOnSteadyTone()
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
    const AutoPitchCorrectionResult aligned = AutoPitchCorrector::process(
        input,
        expectedSampleRate,
        cMajor,
        2048,
        512,
        50.0f,
        1000.0f,
        1.0f,
        true,
        false);
    const AutoPitchCorrectionResult unaligned = AutoPitchCorrector::process(
        input,
        expectedSampleRate,
        cMajor,
        2048,
        512,
        50.0f,
        1000.0f,
        1.0f,
        false,
        false);
    if (aligned.samples.size() != input.size() ||
        unaligned.samples.size() != input.size() ||
        aligned.frameAnalysis.size() != unaligned.frameAnalysis.size() ||
        aligned.frameDiagnostics.size() !=
            unaligned.frameDiagnostics.size())
    {
        std::cout << "Phase alignment steady-tone A/B: FAIL, invalid output\n";
        return false;
    }

    bool samePitchDecisions = true;
    bool validCorrelations = true;
    bool disabledAlignmentIsTransparent = true;
    for (size_t i = 0; i < aligned.frameAnalysis.size(); ++i)
    {
        const PitchCorrectionFrameDiagnostics& withAlignment =
            aligned.frameDiagnostics[i];
        const PitchCorrectionFrameDiagnostics& withoutAlignment =
            unaligned.frameDiagnostics[i];
        samePitchDecisions = samePitchDecisions &&
            aligned.frameAnalysis[i].targetNote.midi ==
                unaligned.frameAnalysis[i].targetNote.midi &&
            withAlignment.targetMidi == withoutAlignment.targetMidi &&
            withAlignment.trackedFrequency ==
                withoutAlignment.trackedFrequency &&
            withAlignment.pitchRatio == withoutAlignment.pitchRatio &&
            withAlignment.smoothedCorrectionCents ==
                withoutAlignment.smoothedCorrectionCents;
        validCorrelations = validCorrelations &&
            (!withAlignment.phaseAlignmentValid ||
             (std::isfinite(withAlignment.phaseAlignmentCorrelation) &&
              withAlignment.phaseAlignmentCorrelation >= -1.0001f &&
              withAlignment.phaseAlignmentCorrelation <= 1.0001f));
        disabledAlignmentIsTransparent =
            disabledAlignmentIsTransparent &&
            withoutAlignment.phaseShiftSamples == 0 &&
            !withoutAlignment.phaseAlignmentValid;
    }

    const PhaseAlignmentStatistics statistics =
        summarizePhaseAlignment(aligned, expectedSampleRate, 512);
    const double maximumWrappedDelta =
        statistics.absoluteWrappedFrameDeltas.empty()
        ? 0.0
        : statistics.absoluteWrappedFrameDeltas.back();
    const float alignedFrequency = estimatePositiveCrossingFrequency(
        aligned.samples, expectedSampleRate);
    const float unalignedFrequency = estimatePositiveCrossingFrequency(
        unaligned.samples, expectedSampleRate);
    const double renderDifference = calculateRmsDifference(
        aligned.samples, unaligned.samples);
    const bool passed =
        samePitchDecisions &&
        validCorrelations &&
        disabledAlignmentIsTransparent &&
        statistics.validFrames > 0 &&
        std::abs(alignedFrequency - targetFrequency) <= 3.0f &&
        std::isfinite(unalignedFrequency) &&
        unalignedFrequency > 0.0f;

    std::cout << "Phase alignment steady-tone A/B: "
              << (passed ? "PASS" : "FAIL")
              << ", valid=" << statistics.validFrames
              << ", mean correlation=" << statistics.meanCorrelation
              << ", boundary hits=" << statistics.boundaryHits
              << ", signed wrapped delta median="
              << percentile(
                     statistics.wrappedFrameDeltas, 0.50)
              << " samples, absolute delta p90/p99/max="
              << percentile(
                     statistics.absoluteWrappedFrameDeltas, 0.90) << '/'
              << percentile(
                     statistics.absoluteWrappedFrameDeltas, 0.99) << '/'
              << maximumWrappedDelta
              << " samples, expected-drift residual p90="
              << percentile(statistics.expectedDriftResiduals, 0.90)
              << " samples, output F0 with/without="
              << alignedFrequency << '/' << unalignedFrequency
              << " Hz, render RMS difference=" << renderDifference
              << '\n';
    return passed;
}

bool checkPhaseAlignmentOnChangingToneWithGap()
{
    const size_t sampleCount =
        static_cast<size_t>(expectedSampleRate * 2.0f);
    const size_t gapStart =
        static_cast<size_t>(expectedSampleRate * 0.9f);
    const size_t gapEnd =
        static_cast<size_t>(expectedSampleRate * 0.91f);
    std::vector<float> input(sampleCount);
    double phase = 0.0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        const float time = static_cast<float>(i) / expectedSampleRate;
        const float frequency = 270.0f + 20.0f * time;
        phase += 2.0 * pi * frequency / expectedSampleRate;
        input[i] = i >= gapStart && i < gapEnd
            ? 0.0f
            : 0.5f * static_cast<float>(std::sin(phase));
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
        0.2f,
        true,
        false);
    if (result.samples.size() != input.size() ||
        result.frameDiagnostics.size() != result.frameAnalysis.size())
    {
        std::cout << "Phase alignment changing-tone/gap: FAIL, invalid output\n";
        return false;
    }

    size_t validAlignments = 0;
    size_t shiftedFrames = 0;
    float minimumTrackedFrequency =
        std::numeric_limits<float>::infinity();
    float maximumTrackedFrequency = 0.0f;
    bool diagnosticsValid = true;
    bool finiteOutput = true;
    for (const PitchCorrectionFrameDiagnostics& diagnostics :
         result.frameDiagnostics)
    {
        if (diagnostics.voiced)
        {
            minimumTrackedFrequency = std::min(
                minimumTrackedFrequency,
                diagnostics.trackedFrequency);
            maximumTrackedFrequency = std::max(
                maximumTrackedFrequency,
                diagnostics.trackedFrequency);
        }
        shiftedFrames += diagnostics.pitchShiftApplied ? 1 : 0;
        if (diagnostics.phaseAlignmentValid)
        {
            ++validAlignments;
            diagnosticsValid = diagnosticsValid &&
                std::isfinite(diagnostics.phaseAlignmentCorrelation) &&
                diagnostics.phaseShiftSearchLimitSamples > 0 &&
                std::abs(diagnostics.phaseShiftSamples) <=
                    diagnostics.phaseShiftSearchLimitSamples &&
                diagnostics.phaseShiftAtSearchLimit ==
                    (std::abs(diagnostics.phaseShiftSamples) ==
                     diagnostics.phaseShiftSearchLimitSamples);
        }
    }
    for (float sample : result.samples)
        finiteOutput = finiteOutput && std::isfinite(sample);

    const bool trackedChangingPitch =
        maximumTrackedFrequency - minimumTrackedFrequency >= 15.0f;
    const bool passed =
        validAlignments > 0 &&
        shiftedFrames > 0 &&
        trackedChangingPitch &&
        diagnosticsValid &&
        finiteOutput;
    std::cout << "Phase alignment changing tone with short gap: "
              << (passed ? "PASS" : "FAIL")
              << ", voiced F0 range=" << minimumTrackedFrequency << '-'
              << maximumTrackedFrequency
              << " Hz, shifted frames=" << shiftedFrames
              << ", valid alignment frames=" << validAlignments << '\n';
    return passed;
}

bool writeFloatWav(
    const std::filesystem::path& path,
    unsigned int sampleRate,
    const std::vector<float>& samples)
{
    const std::string pathString = path.string();
    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav{};
    if (!drwav_init_file_write_sequential_pcm_frames(
            &wav,
            pathString.c_str(),
            &format,
            static_cast<drwav_uint64>(samples.size()),
            nullptr))
    {
        return false;
    }

    const drwav_uint64 framesWritten = drwav_write_pcm_frames(
        &wav,
        static_cast<drwav_uint64>(samples.size()),
        samples.data());
    const drwav_result uninitResult = drwav_uninit(&wav);
    return framesWritten == samples.size() &&
        uninitResult == DRWAV_SUCCESS;
}

bool comparePhaseAlignmentOnInput(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputDirectory,
    const Scale& scale,
    float retuneSpeed)
{
    WavReader reader;
    if (!reader.load(inputPath) ||
        reader.getSampleRate() == 0 ||
        reader.getChannels() != 1)
    {
        std::cerr << "Could not load mono WAV input "
                  << inputPath << '\n';
        return false;
    }
    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError || !std::filesystem::is_directory(outputDirectory))
    {
        std::cerr << "Output directory does not exist: "
                  << outputDirectory << '\n';
        return false;
    }

    const float sampleRate = static_cast<float>(reader.getSampleRate());
    const AutoPitchCorrectionResult aligned = AutoPitchCorrector::process(
        reader.getSamples(),
        sampleRate,
        scale,
        2048,
        512,
        50.0f,
        1000.0f,
        retuneSpeed,
        true,
        false);
    const AutoPitchCorrectionResult unaligned = AutoPitchCorrector::process(
        reader.getSamples(),
        sampleRate,
        scale,
        2048,
        512,
        50.0f,
        1000.0f,
        retuneSpeed,
        false,
        false);
    const AutoPitchCorrectionResult continuity = AutoPitchCorrector::process(
        reader.getSamples(),
        sampleRate,
        scale,
        2048,
        512,
        50.0f,
        1000.0f,
        retuneSpeed,
        true,
        true);
    if (aligned.samples.size() != reader.getSamples().size() ||
        unaligned.samples.size() != reader.getSamples().size() ||
        continuity.samples.size() != reader.getSamples().size() ||
        aligned.frameAnalysis.size() != unaligned.frameAnalysis.size() ||
        aligned.frameAnalysis.size() != continuity.frameAnalysis.size())
    {
        std::cerr << "Phase alignment A/B render failed\n";
        return false;
    }

    bool samePitchDecisions = true;
    for (size_t i = 0; i < aligned.frameAnalysis.size(); ++i)
    {
        const PitchCorrectionFrameDiagnostics& withAlignment =
            aligned.frameDiagnostics[i];
        const PitchCorrectionFrameDiagnostics& withoutAlignment =
            unaligned.frameDiagnostics[i];
        const PitchCorrectionFrameDiagnostics& regularized =
            continuity.frameDiagnostics[i];
        samePitchDecisions = samePitchDecisions &&
            aligned.frameAnalysis[i].targetNote.midi ==
                unaligned.frameAnalysis[i].targetNote.midi &&
            aligned.frameAnalysis[i].targetNote.midi ==
                continuity.frameAnalysis[i].targetNote.midi &&
            withAlignment.targetMidi == withoutAlignment.targetMidi &&
            withAlignment.targetMidi == regularized.targetMidi &&
            withAlignment.trackedFrequency ==
                withoutAlignment.trackedFrequency &&
            withAlignment.trackedFrequency ==
                regularized.trackedFrequency &&
            withAlignment.pitchRatio == withoutAlignment.pitchRatio &&
            withAlignment.pitchRatio == regularized.pitchRatio &&
            withAlignment.smoothedCorrectionCents ==
                withoutAlignment.smoothedCorrectionCents &&
            withAlignment.smoothedCorrectionCents ==
                regularized.smoothedCorrectionCents;
    }

    const std::string stem = inputPath.stem().string();
    const std::filesystem::path alignedPath =
        outputDirectory / (stem + "_phase_alignment_on.wav");
    const std::filesystem::path unalignedPath =
        outputDirectory / (stem + "_phase_alignment_off.wav");
    const std::filesystem::path continuityPath =
        outputDirectory / (stem + "_phase_continuity.wav");
    if (std::filesystem::exists(alignedPath) ||
        std::filesystem::exists(unalignedPath) ||
        std::filesystem::exists(continuityPath))
    {
        std::cerr << "A/B WAV files already exist in "
                  << outputDirectory
                  << "; choose a different output directory\n";
        return false;
    }
    const bool filesWritten =
        writeFloatWav(alignedPath, reader.getSampleRate(), aligned.samples) &&
        writeFloatWav(unalignedPath, reader.getSampleRate(), unaligned.samples) &&
        writeFloatWav(
            continuityPath,
            reader.getSampleRate(),
            continuity.samples);
    if (!filesWritten)
    {
        std::cerr << "Could not write both A/B WAV renders\n";
        return false;
    }

    const PhaseAlignmentStatistics statistics =
        summarizePhaseAlignment(aligned, sampleRate, 512);
    const PhaseAlignmentStatistics continuityStatistics =
        summarizePhaseAlignment(continuity, sampleRate, 512);
    std::cout << "Vocal phase-alignment A/B: "
              << (samePitchDecisions ? "same pitch decisions" :
                                      "pitch decisions differ")
              << ", valid alignment frames=" << statistics.validFrames
              << ", mean correlation=" << statistics.meanCorrelation
              << ", boundary hits=" << statistics.boundaryHits
              << ", signed wrapped delta median="
              << percentile(statistics.wrappedFrameDeltas, 0.50)
              << ", absolute delta p90/p99/max="
              << percentile(
                     statistics.absoluteWrappedFrameDeltas, 0.90) << '/'
              << percentile(
                     statistics.absoluteWrappedFrameDeltas, 0.99) << '/'
              << (statistics.absoluteWrappedFrameDeltas.empty()
                      ? 0.0
                      : statistics.absoluteWrappedFrameDeltas.back())
              << " samples, expected-drift residual p90="
              << percentile(statistics.expectedDriftResiduals, 0.90)
              << " samples, render RMS difference="
              << calculateRmsDifference(aligned.samples, unaligned.samples)
              << '\n'
              << "Alignment on: " << alignedPath << '\n'
              << "Alignment off: " << unalignedPath << '\n'
              << "Continuity regularization: valid alignment frames="
              << continuityStatistics.validFrames
              << ", mean correlation="
              << continuityStatistics.meanCorrelation
              << ", boundary hits="
              << continuityStatistics.boundaryHits
              << ", render RMS difference from alignment on="
              << calculateRmsDifference(aligned.samples, continuity.samples)
              << '\n'
              << "Continuity render: " << continuityPath << '\n';
    return samePitchDecisions;
}

struct PitchComparisonAudio
{
    unsigned int sampleRate = 0;
    std::vector<float> samples;
};

bool loadPitchComparisonAudio(
    const std::filesystem::path& path,
    PitchComparisonAudio& audio)
{
    const std::string pathString = path.string();
    drwav wav{};
    if (!drwav_init_file(&wav, pathString.c_str(), nullptr))
    {
        std::cerr << "Could not load WAV file " << path << '\n';
        return false;
    }

    if (wav.channels == 0 ||
        wav.totalPCMFrameCount >
            std::numeric_limits<size_t>::max() / wav.channels)
    {
        std::cerr << "Unsupported WAV dimensions in " << path << '\n';
        drwav_uninit(&wav);
        return false;
    }

    const size_t frameCount =
        static_cast<size_t>(wav.totalPCMFrameCount);
    const size_t channelCount = wav.channels;
    std::vector<float> interleaved(frameCount * channelCount);
    const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(
        &wav,
        wav.totalPCMFrameCount,
        interleaved.data());
    audio.sampleRate = wav.sampleRate;
    drwav_uninit(&wav);
    if (framesRead != frameCount)
    {
        std::cerr << "Could not read all samples from " << path << '\n';
        return false;
    }

    audio.samples.resize(frameCount);
    for (size_t frame = 0; frame < frameCount; ++frame)
    {
        double sum = 0.0;
        for (size_t channel = 0; channel < channelCount; ++channel)
            sum += interleaved[frame * channelCount + channel];
        audio.samples[frame] =
            static_cast<float>(sum / static_cast<double>(channelCount));
    }
    return audio.sampleRate > 0;
}

std::vector<PitchResult> measurePitchTrack(
    const std::vector<float>& samples,
    const std::vector<size_t>& frameStarts,
    float sampleRate)
{
    constexpr size_t frameSize = 2048;
    PitchDetector detector;
    AudioFrame frame;
    frame.frameSamples.resize(frameSize);
    std::vector<PitchResult> pitchTrack;

    for (size_t start : frameStarts)
    {
        std::copy_n(
            samples.begin() + static_cast<std::ptrdiff_t>(start),
            frameSize,
            frame.frameSamples.begin());
        pitchTrack.push_back(detector.detectPitch(
            frame,
            sampleRate,
            50.0f,
            1000.0f));
    }
    return pitchTrack;
}

bool compareF0AcrossRenders(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& ourRenderPath,
    const std::filesystem::path& antaresRenderPath,
    double antaresTimeScale,
    double antaresOffsetSeconds,
    double sourceStartSeconds,
    double sourceEndSeconds,
    size_t analysisHop)
{
    PitchComparisonAudio input;
    PitchComparisonAudio ourRender;
    PitchComparisonAudio antaresRender;
    if (!loadPitchComparisonAudio(inputPath, input) ||
        !loadPitchComparisonAudio(ourRenderPath, ourRender) ||
        !loadPitchComparisonAudio(antaresRenderPath, antaresRender))
    {
        return false;
    }
    if (input.sampleRate != ourRender.sampleRate ||
        input.sampleRate != antaresRender.sampleRate ||
        !std::isfinite(antaresTimeScale) ||
        antaresTimeScale <= 0.0 ||
        !std::isfinite(antaresOffsetSeconds) ||
        antaresOffsetSeconds < 0.0 ||
        !std::isfinite(sourceStartSeconds) ||
        sourceStartSeconds < 0.0 ||
        (sourceEndSeconds >= 0.0 &&
         (!std::isfinite(sourceEndSeconds) ||
          sourceEndSeconds <= sourceStartSeconds)) ||
        analysisHop == 0)
    {
        std::cerr << "Invalid sample rates or Antares time mapping\n";
        return false;
    }

    const double sampleRate = static_cast<double>(input.sampleRate);
    const double antaresOffsetSamples = antaresOffsetSeconds * sampleRate;
    if (antaresOffsetSamples >= antaresRender.samples.size())
    {
        std::cerr << "Antares offset is outside the render\n";
        return false;
    }

    const size_t mappedAntaresSampleCount = static_cast<size_t>(
        (antaresRender.samples.size() - antaresOffsetSamples) /
        antaresTimeScale);
    const size_t commonSampleCount = std::min(
        input.samples.size(),
        std::min(ourRender.samples.size(), mappedAntaresSampleCount));
    if (commonSampleCount < 2048)
    {
        std::cerr << "WAV files have no usable common audio region\n";
        return false;
    }

    constexpr size_t frameSize = 2048;
    std::vector<size_t> sourceFrameStarts;
    std::vector<size_t> antaresFrameStarts;
    const size_t sourceStartSample = static_cast<size_t>(
        sourceStartSeconds * sampleRate);
    const size_t sourceEndSample = sourceEndSeconds >= 0.0
        ? std::min(
              commonSampleCount,
              static_cast<size_t>(sourceEndSeconds * sampleRate))
        : commonSampleCount;
    if (sourceStartSample >= sourceEndSample)
    {
        std::cerr << "Requested source time range is outside the common audio\n";
        return false;
    }

    for (size_t sourceStart = sourceStartSample;
         sourceStart + frameSize <= commonSampleCount;
         sourceStart += analysisHop)
    {
        if (sourceStart >= sourceEndSample)
            break;
        const size_t antaresStart = static_cast<size_t>(std::llround(
            antaresOffsetSamples + antaresTimeScale * sourceStart));
        if (antaresStart + frameSize > antaresRender.samples.size())
            break;
        sourceFrameStarts.push_back(sourceStart);
        antaresFrameStarts.push_back(antaresStart);
    }

    std::ostringstream detectorMessages;
    std::streambuf* previousOutput = std::cout.rdbuf(
        detectorMessages.rdbuf());
    const float detectorSampleRate = static_cast<float>(input.sampleRate);
    const std::vector<PitchResult> inputTrack = measurePitchTrack(
        input.samples, sourceFrameStarts, detectorSampleRate);
    const std::vector<PitchResult> ourTrack = measurePitchTrack(
        ourRender.samples, sourceFrameStarts, detectorSampleRate);
    const std::vector<PitchResult> antaresTrack = measurePitchTrack(
        antaresRender.samples, antaresFrameStarts, detectorSampleRate);
    std::cout.rdbuf(previousOutput);

    const size_t trackFrameCount = std::min(
        inputTrack.size(),
        std::min(ourTrack.size(), antaresTrack.size()));
    std::vector<double> signedCentsDifferences;
    size_t voicedComparisons = 0;
    size_t ourRenderOneOctaveHigher = 0;
    size_t ourRenderOneOctaveLower = 0;
    size_t printedOctaveExamples = 0;

    for (size_t frame = 0; frame < trackFrameCount; ++frame)
    {
        const PitchResult& inputPitch = inputTrack[frame];
        const PitchResult& ourPitch = ourTrack[frame];
        const PitchResult& antaresPitch = antaresTrack[frame];
        if (inputPitch.frequency <= 0.0f ||
            ourPitch.frequency <= 0.0f ||
            antaresPitch.frequency <= 0.0f)
        {
            continue;
        }

        ++voicedComparisons;
        const double centsDifference = 1200.0 * std::log2(
            ourPitch.frequency / antaresPitch.frequency);
        signedCentsDifferences.push_back(centsDifference);

        const double time = static_cast<double>(sourceFrameStarts[frame]) /
            detectorSampleRate;
        if (std::abs(centsDifference - 1200.0) <= 100.0)
        {
            ++ourRenderOneOctaveHigher;
            if (printedOctaveExamples < 12)
            {
                std::cout << "Our render about one octave higher at "
                          << time << " s: input=" << inputPitch.frequency
                          << " Hz (corr " << inputPitch.correlation << ')'
                          << ", ours=" << ourPitch.frequency
                          << " Hz (corr " << ourPitch.correlation
                          << "), Antares=" << antaresPitch.frequency
                          << " Hz (corr " << antaresPitch.correlation
                          << ")\n";
                ++printedOctaveExamples;
            }
        }
        else if (std::abs(centsDifference + 1200.0) <= 100.0)
        {
            ++ourRenderOneOctaveLower;
            if (printedOctaveExamples < 12)
            {
                std::cout << "Our render about one octave lower at "
                          << time << " s: input=" << inputPitch.frequency
                          << " Hz (corr " << inputPitch.correlation << ')'
                          << ", ours=" << ourPitch.frequency
                          << " Hz (corr " << ourPitch.correlation
                          << "), Antares=" << antaresPitch.frequency
                          << " Hz (corr " << antaresPitch.correlation
                          << ")\n";
                ++printedOctaveExamples;
            }
        }
    }

    std::sort(
        signedCentsDifferences.begin(),
        signedCentsDifferences.end());
    const double inputDuration =
        static_cast<double>(input.samples.size()) / sampleRate;
    const double ourDuration =
        static_cast<double>(ourRender.samples.size()) / sampleRate;
    const double antaresDuration =
        static_cast<double>(antaresRender.samples.size()) / sampleRate;
    std::cout << "F0 comparison at 2048-sample windows every "
              << analysisHop << " samples\n"
              << "Durations (input/ours/Antares): "
              << inputDuration << '/' << ourDuration << '/'
              << antaresDuration << " s; Antares time mapping: "
              << antaresOffsetSeconds << " + "
              << antaresTimeScale
              << " * source time; compared source region="
              << static_cast<double>(sourceStartSample) /
                     detectorSampleRate
              << '-' << static_cast<double>(sourceEndSample) /
                            detectorSampleRate
              << " s\n"
              << "Stereo renders downmixed by channel average\n"
              << "Frames voiced in all three=" << voicedComparisons
              << ", our render about one octave higher/lower than Antares="
              << ourRenderOneOctaveHigher << '/'
              << ourRenderOneOctaveLower
              << ", median signed F0 difference="
              << percentile(signedCentsDifferences, 0.50)
              << " cents\n";
    return voicedComparisons > 0;
}

bool checkFlexTuneSoftZone()
{
    constexpr float targetFrequency = 261.6256f;
    constexpr float smallDeviationCents = 10.0f;
    constexpr float transitionDeviationCents = 40.0f;
    constexpr float largeDeviationCents = 80.0f;
    constexpr size_t sampleCount =
        static_cast<size_t>(expectedSampleRate);
    const Scale cMajor(60, ScaleType::Major);

    auto renderTone = [&](float deviationCents, float flexTune)
    {
        const float inputFrequency = targetFrequency *
            std::pow(2.0f, deviationCents / 1200.0f);
        std::vector<float> input(sampleCount);
        for (size_t i = 0; i < input.size(); ++i)
        {
            input[i] = 0.5f * std::sin(
                2.0f * pi * inputFrequency * static_cast<float>(i) /
                expectedSampleRate);
        }

        return AutoPitchCorrector::process(
            input,
            expectedSampleRate,
            cMajor,
            2048,
            512,
            50.0f,
            1000.0f,
            1.0f,
            true,
            true,
            flexTune);
    };

    const AutoPitchCorrectionResult smallWithoutFlex =
        renderTone(smallDeviationCents, 0.0f);
    const AutoPitchCorrectionResult smallWithFlex =
        renderTone(smallDeviationCents, 1.0f);
    const AutoPitchCorrectionResult transitionWithFlex =
        renderTone(transitionDeviationCents, 1.0f);
    const AutoPitchCorrectionResult largeWithoutFlex =
        renderTone(largeDeviationCents, 0.0f);
    const AutoPitchCorrectionResult largeWithFlex =
        renderTone(largeDeviationCents, 1.0f);
    const size_t steadyFrameIndex =
        static_cast<size_t>(expectedSampleRate * 0.5f / 512.0f);

    const auto hasSteadyVoicedFrame =
        [steadyFrameIndex](const AutoPitchCorrectionResult& result)
    {
        return result.samples.size() == sampleCount &&
            result.frameDiagnostics.size() > steadyFrameIndex &&
            result.frameDiagnostics[steadyFrameIndex].voiced;
    };
    if (!hasSteadyVoicedFrame(smallWithoutFlex) ||
        !hasSteadyVoicedFrame(smallWithFlex) ||
        !hasSteadyVoicedFrame(transitionWithFlex) ||
        !hasSteadyVoicedFrame(largeWithoutFlex) ||
        !hasSteadyVoicedFrame(largeWithFlex))
    {
        std::cout << "Flex Tune soft zone: FAIL, invalid voiced output\n";
        return false;
    }

    const float smallCorrectionWithoutFlex =
        smallWithoutFlex.frameDiagnostics[steadyFrameIndex]
            .smoothedCorrectionCents;
    const float smallCorrectionWithFlex =
        smallWithFlex.frameDiagnostics[steadyFrameIndex]
            .smoothedCorrectionCents;
    const float transitionCorrection =
        transitionWithFlex.frameDiagnostics[steadyFrameIndex]
            .smoothedCorrectionCents;
    const float largeCorrectionWithoutFlex =
        largeWithoutFlex.frameDiagnostics[steadyFrameIndex]
            .smoothedCorrectionCents;
    const float largeCorrectionWithFlex =
        largeWithFlex.frameDiagnostics[steadyFrameIndex]
            .smoothedCorrectionCents;

    const bool passed =
        std::abs(smallCorrectionWithoutFlex) > 5.0f &&
        std::abs(smallCorrectionWithFlex) < 1.0f &&
        std::abs(transitionCorrection) > 0.0f &&
        std::abs(transitionCorrection) <
            transitionDeviationCents * 0.75f &&
        std::abs(largeCorrectionWithoutFlex) > 60.0f &&
        std::abs(
            largeCorrectionWithFlex - largeCorrectionWithoutFlex) <
            0.1f;
    std::cout << "Flex Tune soft zone: small correction "
              << smallCorrectionWithoutFlex << " -> "
              << smallCorrectionWithFlex
              << " cents, transition correction "
              << transitionCorrection
              << " cents, large correction "
              << largeCorrectionWithoutFlex << " -> "
              << largeCorrectionWithFlex
              << " cents, "
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed;
}

bool checkNaturalVibratoPreservation()
{
    constexpr float targetFrequency = 261.6256f;
    constexpr float vibratoDepthCents = 20.0f;
    constexpr float vibratoRateHz = 5.0f;
    constexpr float durationSeconds = 2.0f;
    const size_t sampleCount = static_cast<size_t>(
        expectedSampleRate * durationSeconds);
    const size_t firstSteadyFrame =
        static_cast<size_t>(expectedSampleRate * 0.25f / 512.0f);
    const size_t lastSteadyFrame =
        static_cast<size_t>(expectedSampleRate * 1.75f / 512.0f);
    const Scale cMajor(60, ScaleType::Major);

    struct VibratoMetrics
    {
        double inputStandardDeviation = 0.0;
        double outputStandardDeviation = 0.0;
        size_t voicedFrames = 0;
    };

    auto measureVibrato = [&](float inputVibratoDepth, float naturalVibrato)
    {
        std::vector<float> input(sampleCount);
        double phase = 0.0;
        for (size_t i = 0; i < input.size(); ++i)
        {
            const float time =
                static_cast<float>(i) / expectedSampleRate;
            const float vibratoCents = inputVibratoDepth *
                std::sin(2.0f * pi * vibratoRateHz * time);
            const float inputFrequency = targetFrequency *
                std::pow(2.0f, vibratoCents / 1200.0f);
            input[i] = 0.5f * static_cast<float>(std::sin(phase));
            phase += 2.0 * pi * inputFrequency / expectedSampleRate;
        }

        const AutoPitchCorrectionResult result =
            AutoPitchCorrector::process(
                input,
                expectedSampleRate,
                cMajor,
                2048,
                512,
                50.0f,
                1000.0f,
                1.0f,
                true,
                true,
                0.0f,
                naturalVibrato);
        VibratoMetrics metrics;
        if (result.samples.size() != input.size() ||
            result.frameDiagnostics.size() <= lastSteadyFrame)
        {
            return metrics;
        }

        double inputSum = 0.0;
        double outputSum = 0.0;
        std::vector<double> inputDeviations;
        std::vector<double> outputDeviations;
        for (size_t i = firstSteadyFrame; i < lastSteadyFrame; ++i)
        {
            const PitchCorrectionFrameDiagnostics& frame =
                result.frameDiagnostics[i];
            if (!frame.voiced ||
                frame.targetFrequency <= 0.0f ||
                frame.trackedFrequency <= 0.0f)
            {
                continue;
            }

            const double inputDeviation =
                1200.0 * std::log2(
                    frame.trackedFrequency / frame.targetFrequency);
            const double outputDeviation =
                1200.0 * std::log2(
                    frame.trackedFrequency * frame.pitchRatio /
                    frame.targetFrequency);
            inputDeviations.push_back(inputDeviation);
            outputDeviations.push_back(outputDeviation);
            inputSum += inputDeviation;
            outputSum += outputDeviation;
        }

        metrics.voicedFrames = inputDeviations.size();
        if (metrics.voicedFrames == 0)
            return metrics;

        const double inputMean =
            inputSum / static_cast<double>(metrics.voicedFrames);
        const double outputMean =
            outputSum / static_cast<double>(metrics.voicedFrames);
        double inputVariance = 0.0;
        double outputVariance = 0.0;
        for (size_t i = 0; i < metrics.voicedFrames; ++i)
        {
            const double inputDifference =
                inputDeviations[i] - inputMean;
            const double outputDifference =
                outputDeviations[i] - outputMean;
            inputVariance += inputDifference * inputDifference;
            outputVariance += outputDifference * outputDifference;
        }

        metrics.inputStandardDeviation = std::sqrt(
            inputVariance / static_cast<double>(metrics.voicedFrames));
        metrics.outputStandardDeviation = std::sqrt(
            outputVariance / static_cast<double>(metrics.voicedFrames));
        return metrics;
    };

    const VibratoMetrics flattened =
        measureVibrato(vibratoDepthCents, 0.0f);
    const VibratoMetrics preserved =
        measureVibrato(vibratoDepthCents, 1.0f);
    const VibratoMetrics generatedFromNone =
        measureVibrato(0.0f, 1.0f);

    const bool passed =
        flattened.voicedFrames > 40 &&
        preserved.voicedFrames > 40 &&
        generatedFromNone.voicedFrames > 40 &&
        preserved.outputStandardDeviation >
            flattened.outputStandardDeviation * 2.0 + 2.0 &&
        preserved.outputStandardDeviation > 4.0 &&
        generatedFromNone.outputStandardDeviation < 5.0;

    std::cout << "Natural Vibrato: flattened output deviation "
              << flattened.outputStandardDeviation
              << " cents, preserved "
              << preserved.outputStandardDeviation
              << " cents from input "
              << preserved.inputStandardDeviation
              << ", clean-tone output "
              << generatedFromNone.outputStandardDeviation
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

    const PitchAnalysisResult gSharp5 = makeAnalysisFrame(
        830.6094f,
        0.98f,
        80,
        830.6094f);
    std::vector<PitchAnalysisResult> closeLargeTargetChange{
        gSharp5,
        makeAnalysisFrame(
            984.0f,
            0.98f,
            82,
            PitchToNote::midiNoteToFrequency(82)),
        makeAnalysisFrame(
            984.0f,
            0.98f,
            82,
            PitchToNote::midiNoteToFrequency(82))};
    const std::vector<TrackedPitchFrame> stableCloseLargeTargetChange =
        PitchFrameStabilizer::stabilize(
            closeLargeTargetChange,
            50.0f,
            1000.0f);
    const bool closerTargetPassed =
        stableCloseLargeTargetChange[1].targetNote.midi == 82;

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
        closerTargetPassed &&
        naturalVariationPassed;
    std::cout << "Pitch-frame stabilization (octave spike, short gap, "
                 "confidence, target hysteresis, natural F0 movement): "
              << (passed ? "PASS" : "FAIL")
              << " [octave=" << octavePassed
              << ", gap=" << gapPassed
              << ", confidence=" << confidencePassed
              << ", hysteresis=" << hysteresisPassed
              << ", octave-change=" << octaveHysteresisPassed
              << ", large-change=" << largeChangePassed
              << ", closer-target=" << closerTargetPassed
              << ", natural-F0=" << naturalVariationPassed << "]\n";
    return passed;
}
}

int main(int argc, char** argv)
{
    if (argc == 7 &&
        std::string(argv[1]) == "--phase-alignment-ab")
    {
        const int rootMidi = std::stoi(argv[4]);
        const std::string scaleName = argv[5];
        ScaleType scaleType;
        if (scaleName == "major")
            scaleType = ScaleType::Major;
        else if (scaleName == "minor")
            scaleType = ScaleType::Minor;
        else
        {
            std::cerr << "Scale must be major or minor\n";
            return 2;
        }

        return comparePhaseAlignmentOnInput(
            argv[2],
            argv[3],
            Scale(rootMidi, scaleType),
            std::stof(argv[6])) ? 0 : 1;
    }

    if ((argc == 7 || argc == 10) &&
        std::string(argv[1]) == "--compare-f0")
    {
        const double sourceStartSeconds = argc == 10
            ? std::stod(argv[7])
            : 0.0;
        const double sourceEndSeconds = argc == 10
            ? std::stod(argv[8])
            : -1.0;
        const size_t analysisHop = argc == 10
            ? static_cast<size_t>(std::stoull(argv[9]))
            : 4096;
        return compareF0AcrossRenders(
            argv[2],
            argv[3],
            argv[4],
            std::stod(argv[5]),
            std::stod(argv[6]),
            sourceStartSeconds,
            sourceEndSeconds,
            analysisHop) ? 0 : 1;
    }

    bool passed = true;
    passed = checkSineFrames("assets/sine_220.wav", 220.0f) && passed;
    passed = checkSineFrames("assets/sine_880.wav", 880.0f) && passed;
    passed = checkHarmonicRichToneKeepsFundamental() && passed;
    passed = checkCMajorANotes() && passed;
    passed = checkTargetSelectionUsesMeasuredPitch() && passed;
    passed = checkAutomaticCorrection() && passed;
    passed = checkFlexTuneSoftZone() && passed;
    passed = checkNaturalVibratoPreservation() && passed;
    passed = checkUnvoicedPassThrough() && passed;
    passed = checkInScaleToneIsUnchanged() && passed;
    passed = checkPitchFrameStabilizer() && passed;

    passed = checkPhaseAlignmentOnSteadyTone() && passed;
    passed = checkPhaseAlignmentOnChangingToneWithGap() && passed;
    if (argc == 3)
    {
        passed = comparePhaseAlignmentOnInput(
            argv[1],
            argv[2],
            Scale(60, ScaleType::Major),
            0.05f) && passed;
    }
    else if (argc != 1)
    {
        std::cerr << "Usage: PitchAnalysisDiagnostics "
                     "[input-mono.wav output-directory] | "
                     "--phase-alignment-ab input.wav output-directory "
                     "root-midi major|minor retune-speed\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
