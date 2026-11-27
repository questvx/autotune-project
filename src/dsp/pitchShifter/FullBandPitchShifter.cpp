#include "FullBandPitchShifter.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
constexpr size_t fftOrder = 11;
constexpr size_t fftSize = 1u << fftOrder;
constexpr size_t analysisHop = fftSize / 4;
constexpr size_t binCount = fftSize / 2 + 1;
constexpr size_t outputPadding = fftSize;
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

float wrapPhase(float phase)
{
    return std::remainder(phase, twoPi);
}

float hannWindow(size_t index)
{
    const float position =
        (static_cast<float>(index) + 0.5f) /
        static_cast<float>(fftSize);
    return 0.5f * (1.0f - std::cos(twoPi * position));
}
}

std::vector<float> FullBandPitchShifter::processInterleaved(
    const std::vector<float>& interleavedSamples,
    unsigned int channelCount,
    int semitones)
{
    if ((channelCount != 1 && channelCount != 2) ||
        interleavedSamples.empty() ||
        interleavedSamples.size() % channelCount != 0 ||
        semitones < -12 ||
        semitones > 12)
    {
        return {};
    }

    for (const float sample : interleavedSamples)
    {
        if (!std::isfinite(sample))
            return {};
    }

    if (semitones == 0)
        return interleavedSamples;

    const size_t sampleFrameCount =
        interleavedSamples.size() / channelCount;
    const double pitchRatio = std::pow(
        2.0,
        static_cast<double>(semitones) / 12.0);
    const double stretchedLengthValue = std::ceil(
        static_cast<double>(sampleFrameCount) * pitchRatio);
    const size_t maximumSafeSize =
        std::numeric_limits<size_t>::max() -
        2 * outputPadding -
        fftSize;
    if (!std::isfinite(stretchedLengthValue) ||
        stretchedLengthValue < 1.0 ||
        stretchedLengthValue > static_cast<double>(maximumSafeSize))
    {
        return {};
    }

    const size_t stretchedLength =
        static_cast<size_t>(stretchedLengthValue);
    const size_t frameCount =
        sampleFrameCount / analysisHop +
        (sampleFrameCount % analysisHop == 0 ? 1 : 2);
    const size_t outputBufferSize =
        stretchedLength + 2 * outputPadding + fftSize;
    const size_t referenceChannel = [&]
    {
        if (channelCount == 1)
            return size_t{0};

        double channelEnergies[2]{0.0, 0.0};
        for (size_t frame = 0; frame < sampleFrameCount; ++frame)
        {
            for (size_t channel = 0; channel < 2; ++channel)
            {
                const double sample =
                    interleavedSamples[frame * 2 + channel];
                channelEnergies[channel] += sample * sample;
            }
        }
        return channelEnergies[1] > channelEnergies[0]
            ? size_t{1}
            : size_t{0};
    }();

    juce::dsp::FFT fft(static_cast<int>(fftOrder));
    std::vector<std::vector<float>> fftBuffers(
        channelCount,
        std::vector<float>(fftSize * 2, 0.0f));
    std::vector<std::vector<float>> magnitudes(
        channelCount,
        std::vector<float>(binCount, 0.0f));
    std::vector<std::vector<float>> phases(
        channelCount,
        std::vector<float>(binCount, 0.0f));
    std::vector<float> previousReferencePhase(binCount, 0.0f);
    std::vector<float> synthesisReferencePhase(binCount, 0.0f);
    std::vector<std::vector<float>> output(
        channelCount,
        std::vector<float>(outputBufferSize, 0.0f));
    std::vector<float> windowSum(outputBufferSize, 0.0f);

    for (size_t frame = 0; frame < frameCount; ++frame)
    {
        const long long inputCentre =
            static_cast<long long>(frame * analysisHop);
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            std::vector<float>& fftBuffer = fftBuffers[channel];
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);

            for (size_t i = 0; i < fftSize; ++i)
            {
                const long long inputIndex =
                    inputCentre + static_cast<long long>(i) -
                    static_cast<long long>(fftSize / 2);
                if (inputIndex >= 0 &&
                    static_cast<size_t>(inputIndex) < sampleFrameCount)
                {
                    fftBuffer[i] =
                        interleavedSamples[
                            static_cast<size_t>(inputIndex) * channelCount +
                            channel] *
                        hannWindow(i);
                }
            }

            fft.performRealOnlyForwardTransform(fftBuffer.data(), true);
            for (size_t bin = 0; bin < binCount; ++bin)
            {
                const float real = fftBuffer[bin * 2];
                const float imaginary = fftBuffer[bin * 2 + 1];
                magnitudes[channel][bin] =
                    std::hypot(real, imaginary);
                phases[channel][bin] = std::atan2(imaginary, real);
            }
        }

        const std::vector<float>& referencePhase =
            phases[referenceChannel];
        if (frame == 0)
        {
            synthesisReferencePhase = referencePhase;
        }
        else
        {
            for (size_t bin = 0; bin < binCount; ++bin)
            {
                const float expectedAdvance =
                    twoPi * static_cast<float>(bin * analysisHop) /
                    static_cast<float>(fftSize);
                const float phaseDeviation = wrapPhase(
                    referencePhase[bin] -
                    previousReferencePhase[bin] -
                    expectedAdvance);
                synthesisReferencePhase[bin] +=
                    (expectedAdvance + phaseDeviation) *
                    static_cast<float>(pitchRatio);
            }
        }

        const double outputCentreValue =
            static_cast<double>(frame * analysisHop) * pitchRatio;
        if (!std::isfinite(outputCentreValue) ||
            outputCentreValue >
                static_cast<double>(outputBufferSize - outputPadding))
        {
            return {};
        }
        const size_t outputCentre = outputPadding +
            static_cast<size_t>(std::llround(outputCentreValue));
        const size_t outputStart = outputCentre - fftSize / 2;

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            std::vector<float>& fftBuffer = fftBuffers[channel];
            for (size_t bin = 1; bin + 1 < binCount; ++bin)
            {
                const float relativePhase = wrapPhase(
                    phases[channel][bin] - referencePhase[bin]);
                const float outputPhase =
                    synthesisReferencePhase[bin] + relativePhase;
                fftBuffer[bin * 2] =
                    magnitudes[channel][bin] * std::cos(outputPhase);
                fftBuffer[bin * 2 + 1] =
                    magnitudes[channel][bin] * std::sin(outputPhase);
            }
            fftBuffer[1] = 0.0f;
            fftBuffer[fftSize + 1] = 0.0f;
            fft.performRealOnlyInverseTransform(fftBuffer.data());

            for (size_t i = 0; i < fftSize; ++i)
            {
                const size_t outputIndex = outputStart + i;
                const float window = hannWindow(i);
                output[channel][outputIndex] +=
                    fftBuffer[i] * window;
                windowSum[outputIndex] += window * window;
            }
        }

        previousReferencePhase = referencePhase;
    }

    std::vector<std::vector<float>> stretched(
        channelCount,
        std::vector<float>(stretchedLength, 0.0f));
    for (size_t frame = 0; frame < stretchedLength; ++frame)
    {
        const size_t outputIndex = outputPadding + frame;
        if (windowSum[outputIndex] <=
            std::numeric_limits<float>::epsilon())
        {
            return {};
        }

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            stretched[channel][frame] =
                output[channel][outputIndex] / windowSum[outputIndex];
        }
    }

    std::vector<float> shifted(interleavedSamples.size(), 0.0f);
    for (size_t frame = 0; frame < sampleFrameCount; ++frame)
    {
        const double sourcePosition =
            static_cast<double>(frame) * pitchRatio;
        const size_t leftIndex = static_cast<size_t>(sourcePosition);
        const size_t rightIndex =
            std::min(leftIndex + 1, stretchedLength - 1);
        const float fraction = static_cast<float>(
            sourcePosition - static_cast<double>(leftIndex));
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            const float left = stretched[channel][leftIndex];
            const float right = stretched[channel][rightIndex];
            shifted[frame * channelCount + channel] =
                left + (right - left) * fraction;
        }
    }

    return shifted;
}
