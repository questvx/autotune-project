#include "AutoPitchCorrector.h"

#include "../frameGenerator/FrameGenerator.h"
#include "../pitchAnalysis/pitchAnalyzer/PitchAnalyzer.h"
#include "../pitchShifter/PitchMarks.h"
#include "../pitchShifter/PSOLAGrain.h"
#include "../pitchShifter/PSOLAProcessor.h"
#include "PitchCorrection.h"
#include "../retuneSpeed/RetuneSpeed.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr float minimumEffectiveCorrectionCents = 5.0f;

int findPhaseAlignment(
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
        return 0;
    }

    const int maxShift = std::min(
        static_cast<int>(frame.size() / 4),
        static_cast<int>(std::round(
            sampleRate / targetFrequency * 0.5f)));
    if (maxShift == 0)
        return 0;

    int bestShift = 0;
    double bestCorrelation = -std::numeric_limits<double>::infinity();
    for (int shift = -maxShift; shift <= maxShift; ++shift)
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
            continue;

        const double correlation = crossProduct /
            std::sqrt(frameEnergy * accumulatedEnergy);
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestShift = shift;
        }
    }

    return bestShift;
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
    float retuneSpeed)
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
        retuneSpeed > 1.0f)
    {
        return result;
    }

    FrameGenerator frameGenerator;
    PitchAnalyzer pitchAnalyzer;
    RetuneSpeed correctionSmoother(retuneSpeed);
    const std::vector<AudioFrame> frames =
        frameGenerator.generateFrames(samples, frameSize, hopSize);
    if (frames.empty())
        return result;

    result.samples.assign(samples.size(), 0.0f);
    result.frameAnalysis.reserve(frames.size());
    std::vector<float> windowSum(samples.size(), 0.0f);

    for (size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex)
    {
        const AudioFrame& frame = frames[frameIndex];
        PitchAnalysisResult analysis = pitchAnalyzer.analyze(
            frame,
            sampleRate,
            minFrequency,
            maxFrequency,
            scale);
        result.frameAnalysis.push_back(analysis);

        const bool voiced =
            analysis.pitchResult.frequency > 0.0f &&
            analysis.targetFrequency > 0.0f;
        const float targetCorrection = voiced
            ? PitchCorrection::calculate(analysis.cents)
            : 0.0f;
        const float smoothedCorrection =
            correctionSmoother.process(targetCorrection);
        const bool needsPitchShift = voiced &&
            std::abs(smoothedCorrection) >
                minimumEffectiveCorrectionCents;

        std::vector<float> correctedFrame;
        if (needsPitchShift)
        {
            const float ratio = std::pow(
                2.0f, smoothedCorrection / 1200.0f);
            const float periodValue =
                sampleRate / analysis.pitchResult.frequency;
            if (!std::isfinite(ratio) ||
                !std::isfinite(periodValue) ||
                periodValue < 1.0f ||
                periodValue > static_cast<float>(
                    std::numeric_limits<int>::max()))
            {
                return {};
            }

            const int period = static_cast<int>(std::round(periodValue));
            const std::vector<int> pitchMarks = PitchMarks::detect(
                frame.frameSamples,
                analysis.pitchResult.frequency,
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
        const int phaseShift = needsPitchShift
            ? findPhaseAlignment(
                correctedFrame,
                result.samples,
                windowSum,
                frameStart,
                analysis.targetFrequency,
                sampleRate)
            : 0;
        for (size_t i = 0; i < frameSize; ++i)
        {
            const size_t outputIndex = frameStart + i;
            if (outputIndex >= samples.size())
                break;

            const size_t shiftedIndex = static_cast<size_t>(
                (static_cast<long long>(i) + phaseShift +
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
