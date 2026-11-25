#include "AutoPitchCorrector.h"

#include "../frameGenerator/FrameGenerator.h"
#include "../pitchAnalysis/pitchAnalyzer/PitchAnalyzer.h"
#include "../pitchShifter/PitchMarks.h"
#include "../pitchShifter/PSOLAGrain.h"
#include "../pitchShifter/PSOLAProcessor.h"
#include "PitchCorrection.h"
#include "PitchFrameStabilizer.h"
#include "../retuneSpeed/RetuneSpeed.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr float minimumEffectiveCorrectionCents = 5.0f;
constexpr float maximumFlexTuneDeadZoneCents = 30.0f;
constexpr float maximumFlexTuneTransitionCents = 30.0f;

float applyFlexTune(float correctionCents, float flexTune)
{
    if (flexTune <= 0.0f)
        return correctionCents;

    const float deadZoneCents =
        maximumFlexTuneDeadZoneCents * flexTune;
    const float transitionCents =
        maximumFlexTuneTransitionCents * flexTune;
    const float correctionMagnitude = std::abs(correctionCents);

    if (correctionMagnitude <= deadZoneCents)
        return 0.0f;

    if (correctionMagnitude >= deadZoneCents + transitionCents)
        return correctionCents;

    const float transitionPosition =
        (correctionMagnitude - deadZoneCents) / transitionCents;
    const float correctionWeight =
        transitionPosition * transitionPosition *
        (3.0f - 2.0f * transitionPosition);
    return correctionCents * correctionWeight;
}

struct PhaseAlignmentResult
{
    int shiftSamples = 0;
    int searchLimitSamples = 0;
    float correlation = 0.0f;
    bool valid = false;
};

float phaseCorrelationAtShift(
    const std::vector<float>& frame,
    const std::vector<float>& accumulatedSamples,
    const std::vector<float>& windowSum,
    size_t frameStart,
    int maxShift,
    int shift)
{
    double crossProduct = 0.0;
    double frameEnergy = 0.0;
    double accumulatedEnergy = 0.0;

    for (size_t i = static_cast<size_t>(maxShift);
         i + static_cast<size_t>(maxShift) < frame.size();
         ++i)
    {
        const size_t outputIndex = frameStart + i;
        if (outputIndex >= accumulatedSamples.size() ||
            windowSum[outputIndex] <=
                std::numeric_limits<float>::epsilon())
        {
            continue;
        }

        const size_t frameIndex = static_cast<size_t>(
            (static_cast<long long>(i) + shift +
             static_cast<long long>(frame.size())) %
            static_cast<long long>(frame.size()));
        const double frameSample = frame[frameIndex];
        const double accumulatedSample =
            accumulatedSamples[outputIndex] / windowSum[outputIndex];
        crossProduct += frameSample * accumulatedSample;
        frameEnergy += frameSample * frameSample;
        accumulatedEnergy += accumulatedSample * accumulatedSample;
    }

    if (frameEnergy == 0.0 || accumulatedEnergy == 0.0)
        return -std::numeric_limits<float>::infinity();

    return static_cast<float>(
        crossProduct / std::sqrt(frameEnergy * accumulatedEnergy));
}

PhaseAlignmentResult findPhaseAlignment(
    const std::vector<float>& frame,
    const std::vector<float>& accumulatedSamples,
    const std::vector<float>& windowSum,
    size_t frameStart,
    float targetFrequency,
    float sampleRate)
{
    if (frame.empty() ||
        !std::isfinite(targetFrequency) ||
        targetFrequency <= 0.0f)
    {
        return {};
    }

    const int maxShift = std::min(
        static_cast<int>(frame.size() / 4),
        static_cast<int>(std::round(
            sampleRate / targetFrequency * 0.5f)));
    if (maxShift == 0)
        return {};

    PhaseAlignmentResult result;
    result.searchLimitSamples = maxShift;
    int bestShift = 0;
    double bestCorrelation = -std::numeric_limits<double>::infinity();
    for (int shift = -maxShift; shift <= maxShift; ++shift)
    {
        const float correlation = phaseCorrelationAtShift(
            frame,
            accumulatedSamples,
            windowSum,
            frameStart,
            maxShift,
            shift);
        if (!std::isfinite(correlation))
            continue;

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestShift = shift;
        }
    }

    if (std::isfinite(bestCorrelation))
    {
        result.shiftSamples = bestShift;
        result.correlation = static_cast<float>(bestCorrelation);
        result.valid = true;
    }

    return result;
}
}

AutoPitchCorrectionResult AutoPitchCorrector::process(
    const std::vector<float>& samples,
    float sampleRate,
    const Scale& scale,
    size_t frameSize,
    size_t hopSize,
    float minFrequency,
    float maxFrequency,
    float retuneSpeed,
    bool phaseAlignmentEnabled,
    bool phaseContinuityEnabled,
    float flexTune,
    float naturalVibrato)
{
    AutoPitchCorrectionResult result;
    if (samples.empty() ||
        !std::isfinite(sampleRate) ||
        sampleRate <= 0.0f ||
        frameSize == 0 ||
        hopSize == 0 ||
        hopSize > frameSize ||
        !std::isfinite(minFrequency) ||
        !std::isfinite(maxFrequency) ||
        minFrequency <= 0.0f ||
        maxFrequency <= minFrequency ||
        !std::isfinite(retuneSpeed) ||
        retuneSpeed <= 0.0f ||
        retuneSpeed > 1.0f ||
        !std::isfinite(flexTune) ||
        flexTune < 0.0f ||
        flexTune > 1.0f ||
        !std::isfinite(naturalVibrato) ||
        naturalVibrato < 0.0f ||
        naturalVibrato > 1.0f)
    {
        return result;
    }

    FrameGenerator frameGenerator;
    PitchAnalyzer pitchAnalyzer;
    const std::vector<AudioFrame> frames =
        frameGenerator.generateFrames(samples, frameSize, hopSize);
    if (frames.empty())
        return result;

    result.frameAnalysis.reserve(frames.size());
    for (const AudioFrame& frame : frames)
    {
        result.frameAnalysis.push_back(pitchAnalyzer.analyze(
            frame,
            sampleRate,
            minFrequency,
            maxFrequency,
            scale));
    }

    const std::vector<TrackedPitchFrame> trackedFrames =
        PitchFrameStabilizer::stabilize(
            result.frameAnalysis,
            minFrequency,
            maxFrequency);
    result.samples.assign(samples.size(), 0.0f);
    result.frameDiagnostics.reserve(frames.size());
    std::vector<float> windowSum(samples.size(), 0.0f);
    RetuneSpeed correctionSmoother(retuneSpeed);
    bool hasPreviousUnwrappedPhaseShift = false;
    float previousUnwrappedPhaseShift = 0.0f;
    bool hasVibratoBaseline = false;
    int vibratoTargetMidi = -1;
    float vibratoBaselineCents = 0.0f;
    const float vibratoBaselineAlpha = 1.0f - std::exp(
        -2.0f * static_cast<float>(std::acos(-1.0)) * 2.0f *
        static_cast<float>(hopSize) / sampleRate);

    for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex)
    {
        const AudioFrame& frame = frames[frameIndex];
        const PitchAnalysisResult& analysis =
            result.frameAnalysis[frameIndex];
        const TrackedPitchFrame& tracked = trackedFrames[frameIndex];

        PitchCorrectionFrameDiagnostics diagnostics;
        diagnostics.trackedFrequency = tracked.frequency;
        diagnostics.confidence = analysis.pitchResult.correlation;
        diagnostics.targetMidi = tracked.targetNote.midi;
        diagnostics.targetFrequency = tracked.targetFrequency;
        diagnostics.smoothedTargetFrequency = tracked.targetFrequency;
        diagnostics.voiced = tracked.voiced;

        const float targetCorrection = tracked.voiced
            ? PitchCorrection::calculate(
                1200.0f * std::log2(
                    tracked.frequency / tracked.targetFrequency))
            : 0.0f;
        diagnostics.requestedCorrectionCents = targetCorrection;

        float preservedVibratoCents = 0.0f;
        if (tracked.voiced &&
            naturalVibrato > 0.0f &&
            tracked.targetFrequency > 0.0f)
        {
            const float pitchDeviationCents =
                1200.0f * std::log2(
                    tracked.frequency / tracked.targetFrequency);
            if (!hasVibratoBaseline ||
                vibratoTargetMidi != tracked.targetNote.midi)
            {
                vibratoBaselineCents = pitchDeviationCents;
                vibratoTargetMidi = tracked.targetNote.midi;
                hasVibratoBaseline = true;
            }
            else
            {
                vibratoBaselineCents += vibratoBaselineAlpha *
                    (pitchDeviationCents - vibratoBaselineCents);
                preservedVibratoCents =
                    (pitchDeviationCents - vibratoBaselineCents) *
                    naturalVibrato;
            }
        }
        else
        {
            hasVibratoBaseline = false;
            vibratoTargetMidi = -1;
        }
        diagnostics.preservedVibratoCents = preservedVibratoCents;

        const float vibratoAwareCorrection =
            targetCorrection + preservedVibratoCents;
        const float smoothedCorrection =
            correctionSmoother.process(
                applyFlexTune(vibratoAwareCorrection, flexTune));
        diagnostics.smoothedCorrectionCents = smoothedCorrection;

        const float ratio = std::pow(
            2.0f,
            smoothedCorrection / 1200.0f);
        if (!std::isfinite(ratio) || ratio <= 0.0f)
            return {};

        const bool needsPitchShift = tracked.voiced &&
            std::abs(smoothedCorrection) >
                minimumEffectiveCorrectionCents;
        std::vector<float> correctedFrame;
        if (needsPitchShift)
        {
            const float periodValue =
                sampleRate / tracked.frequency;
            if (!std::isfinite(periodValue) ||
                periodValue < 1.0f ||
                periodValue > static_cast<float>(
                    std::numeric_limits<int>::max()))
            {
                return {};
            }

            const int period = static_cast<int>(std::round(periodValue));
            const std::vector<int> pitchMarks = PitchMarks::detect(
                frame.frameSamples,
                tracked.frequency,
                sampleRate);
            correctedFrame = PSOLAProcessor::process(
                frame.frameSamples,
                pitchMarks,
                ratio,
                period);
            if (correctedFrame.size() != frame.frameSamples.size())
                return {};
        }
        else
        {
            correctedFrame = frame.frameSamples;
        }

        const size_t frameStart = frameIndex * hopSize;
        PhaseAlignmentResult phaseAlignment =
            needsPitchShift && phaseAlignmentEnabled
            ? findPhaseAlignment(
                correctedFrame,
                result.samples,
                windowSum,
                frameStart,
                tracked.targetFrequency,
                sampleRate)
            : PhaseAlignmentResult{};

        if (phaseContinuityEnabled &&
            needsPitchShift &&
            phaseAlignmentEnabled &&
            phaseAlignment.valid)
        {
            if (hasPreviousUnwrappedPhaseShift)
            {
                constexpr float lowCorrelation = 0.85f;
                constexpr float fullCorrelation = 0.98f;
                const float measuredWeight = std::max(
                    0.0f,
                    std::min(
                        1.0f,
                        (phaseAlignment.correlation - lowCorrelation) /
                            (fullCorrelation - lowCorrelation)));
                const float outputFrequency = tracked.frequency * ratio;
                const float phasePeriod = sampleRate / outputFrequency;
                const float expectedShift =
                    previousUnwrappedPhaseShift +
                    static_cast<float>(hopSize) *
                        (1.0f - 1.0f / ratio);
                const float measuredShift =
                    static_cast<float>(phaseAlignment.shiftSamples) +
                    std::round(
                        (expectedShift -
                         static_cast<float>(phaseAlignment.shiftSamples)) /
                        phasePeriod) *
                        phasePeriod;
                const float regularizedShift =
                    expectedShift +
                    measuredWeight * (measuredShift - expectedShift);
                const int periodSamples =
                    static_cast<int>(std::round(phasePeriod));
                int wrappedShift = static_cast<int>(std::round(
                    regularizedShift -
                    std::round(regularizedShift / phasePeriod) *
                        phasePeriod));
                while (wrappedShift > phaseAlignment.searchLimitSamples)
                    wrappedShift -= periodSamples;
                while (wrappedShift < -phaseAlignment.searchLimitSamples)
                    wrappedShift += periodSamples;
                phaseAlignment.shiftSamples = wrappedShift;
                phaseAlignment.correlation = phaseCorrelationAtShift(
                    correctedFrame,
                    result.samples,
                    windowSum,
                    frameStart,
                    phaseAlignment.searchLimitSamples,
                    phaseAlignment.shiftSamples);
                previousUnwrappedPhaseShift =
                    static_cast<float>(phaseAlignment.shiftSamples) +
                    std::round(
                        (regularizedShift -
                         static_cast<float>(phaseAlignment.shiftSamples)) /
                        phasePeriod) *
                        phasePeriod;
            }
            else
            {
                previousUnwrappedPhaseShift =
                    static_cast<float>(phaseAlignment.shiftSamples);
            }
            hasPreviousUnwrappedPhaseShift = true;
        }
        else
        {
            hasPreviousUnwrappedPhaseShift = false;
        }

        diagnostics.pitchShiftApplied = needsPitchShift;
        diagnostics.pitchRatio = needsPitchShift ? ratio : 1.0f;
        diagnostics.phaseShiftSamples = phaseAlignment.shiftSamples;
        diagnostics.phaseShiftSearchLimitSamples =
            phaseAlignment.searchLimitSamples;
        diagnostics.phaseAlignmentCorrelation = phaseAlignment.correlation;
        diagnostics.phaseAlignmentValid = phaseAlignment.valid;
        diagnostics.phaseShiftAtSearchLimit = phaseAlignment.valid &&
            std::abs(phaseAlignment.shiftSamples) ==
                phaseAlignment.searchLimitSamples;
        result.frameDiagnostics.push_back(diagnostics);

        for (size_t i = 0; i < frameSize; ++i)
        {
            const size_t outputIndex = frameStart + i;
            if (outputIndex >= samples.size())
                break;

            const size_t shiftedIndex = static_cast<size_t>(
                (static_cast<long long>(i) + phaseAlignment.shiftSamples +
                 static_cast<long long>(frameSize)) %
                static_cast<long long>(frameSize));
            const float window = PSOLAGrain::hannWindow(i, frameSize);
            result.samples[outputIndex] +=
                correctedFrame[shiftedIndex] * window;
            windowSum[outputIndex] += window;
        }
    }

    for (size_t i = 0; i < result.samples.size(); ++i)
    {
        if (windowSum[i] > std::numeric_limits<float>::epsilon())
            result.samples[i] /= windowSum[i];
        else
            return {};
    }

    return result;
}
