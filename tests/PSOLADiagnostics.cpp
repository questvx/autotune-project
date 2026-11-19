#include "wavReader/WavReader.h"
#include "dsp/pitchShifter/PitchMarks.h"
#include "dsp/pitchShifter/PSOLAProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr float sampleRate = 44100.0f;
constexpr float pi = 3.14159265358979323846f;
constexpr size_t sampleCount = 44100;

struct TestResult
{
    bool passed = true;
    float measuredFrequency = 0.0f;
    float zeroCrossingEstimate = 0.0f;
    float minimumBlockRms = 0.0f;
    float medianBlockRms = 0.0f;
    double targetTonePower = 0.0;
    double inputTonePower = 0.0;
};

std::vector<float> loadSineFile(const std::string& path)
{
    WavReader reader;
    if (!reader.load(path))
    {
        std::cerr << "Could not load " << path << std::endl;
        return {};
    }

    if (reader.getSampleRate() != static_cast<unsigned int>(sampleRate) ||
        reader.getChannels() != 1)
    {
        std::cerr << "Unexpected format in " << path << std::endl;
        return {};
    }

    return reader.getSamples();
}

float estimateFrequency(
    const std::vector<float>& samples,
    float sampleRate)
{
    const size_t start = static_cast<size_t>(sampleRate * 0.1f);
    const size_t end = samples.size() - start;
    size_t crossingCount = 0;

    for (size_t i = start + 1; i < end; ++i)
    {
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f)
            ++crossingCount;
    }

    return static_cast<float>(crossingCount) * sampleRate /
        static_cast<float>(end - start);
}

double calculateTonePower(
    const std::vector<float>& samples,
    float frequency,
    float sampleRate)
{
    const size_t start = static_cast<size_t>(sampleRate * 0.1f);
    const size_t end = samples.size() - start;
    const double coefficient = 2.0 * std::cos(
        2.0 * static_cast<double>(pi) * frequency / sampleRate);
    double previous = 0.0;
    double previousPrevious = 0.0;

    for (size_t i = start; i < end; ++i)
    {
        const double current =
            samples[i] + coefficient * previous - previousPrevious;
        previousPrevious = previous;
        previous = current;
    }

    return previous * previous + previousPrevious * previousPrevious -
        coefficient * previous * previousPrevious;
}

std::vector<float> calculateBlockRms(
    const std::vector<float>& samples,
    float sampleRate)
{
    const size_t blockSize = static_cast<size_t>(sampleRate * 0.01f);
    const size_t start = static_cast<size_t>(sampleRate * 0.1f);
    const size_t end = samples.size() - start;
    std::vector<float> blockRms;

    for (size_t blockStart = start;
         blockStart + blockSize <= end;
         blockStart += blockSize)
    {
        double sumSquares = 0.0;
        for (size_t i = blockStart; i < blockStart + blockSize; ++i)
        {
            const double sample = samples[i];
            sumSquares += sample * sample;
        }

        blockRms.push_back(static_cast<float>(
            std::sqrt(sumSquares / static_cast<double>(blockSize))));
    }

    std::sort(blockRms.begin(), blockRms.end());
    return blockRms;
}

TestResult runCase(
    const std::vector<float>& input,
    float inputFrequency,
    float targetFrequency)
{
    const std::vector<int> pitchMarks = PitchMarks::detect(
        input, inputFrequency, sampleRate);
    const int period = static_cast<int>(
        std::round(sampleRate / inputFrequency));
    const float pitchRatio = targetFrequency / inputFrequency;

    const std::vector<float> output = PSOLAProcessor::process(
        input, pitchMarks, pitchRatio, period);

    TestResult result;
    result.passed = output.size() == input.size();
    if (!result.passed || output.empty())
        return result;

    result.targetTonePower = calculateTonePower(
        output, targetFrequency, sampleRate);
    result.inputTonePower = calculateTonePower(
        output, inputFrequency, sampleRate);
    result.zeroCrossingEstimate = estimateFrequency(output, sampleRate);
    result.measuredFrequency = 220.0f;
    double strongestTonePower = calculateTonePower(
        output, result.measuredFrequency, sampleRate);
    for (float frequency : {440.0f, 880.0f})
    {
        const double tonePower = calculateTonePower(
            output, frequency, sampleRate);
        if (tonePower > strongestTonePower)
        {
            strongestTonePower = tonePower;
            result.measuredFrequency = frequency;
        }
    }

    const std::vector<float> blockRms = calculateBlockRms(
        output, sampleRate);
    if (!blockRms.empty())
    {
        result.minimumBlockRms = blockRms.front();
        result.medianBlockRms = blockRms[blockRms.size() / 2];
    }

    const float frequencyTolerance = std::max(3.0f, targetFrequency * 0.05f);
    result.passed =
        std::isfinite(result.measuredFrequency) &&
        std::abs(result.measuredFrequency - targetFrequency) <=
            frequencyTolerance &&
        std::isfinite(result.zeroCrossingEstimate) &&
        std::abs(result.zeroCrossingEstimate - targetFrequency) <=
            frequencyTolerance;

    return result;
}

bool runAndReport(
    const std::vector<float>& input,
    float inputFrequency,
    float targetFrequency)
{
    std::cerr << "Running " << inputFrequency << " Hz -> "
              << targetFrequency << " Hz" << std::endl;
    const TestResult result = runCase(input, inputFrequency, targetFrequency);
    const float levelRatio = result.medianBlockRms > 0.0f
        ? result.minimumBlockRms / result.medianBlockRms
        : 0.0f;
    const bool passed = result.passed && levelRatio >= 0.7f;
    const double tonePowerRatio = result.inputTonePower > 0.0
        ? result.targetTonePower / result.inputTonePower
        : 0.0;

    std::cout << inputFrequency << " Hz -> " << targetFrequency << " Hz: "
              << (passed ? "PASS" : "FAIL")
              << ", measured=" << result.measuredFrequency << " Hz"
              << ", zero-crossing estimate=" << result.zeroCrossingEstimate
              << " Hz"
              << ", target/input tone power=" << tonePowerRatio
              << ", min/median 10 ms RMS=" << levelRatio << std::endl;

    return passed;
}

bool runCoverageDiagnostic()
{
    constexpr int period = 100;
    constexpr float inputLevel = 0.5f;
    std::vector<float> input(sampleCount, inputLevel);
    std::vector<int> pitchMarks;
    for (int mark = 0; mark < static_cast<int>(sampleCount); mark += period)
        pitchMarks.push_back(mark);

    const std::vector<float> output = PSOLAProcessor::process(
        input, pitchMarks, 0.5f, period);
    if (output.size() != input.size())
        return false;

    const size_t margin = static_cast<size_t>(sampleRate * 0.02f);
    const auto interiorBegin = output.begin() +
        static_cast<std::ptrdiff_t>(margin);
    const auto interiorEnd = output.end() -
        static_cast<std::ptrdiff_t>(margin);
    const float minimum = *std::min_element(interiorBegin, interiorEnd);
    const bool passed = minimum >= inputLevel * 0.99f;

    std::cout << "Constant signal, ratio=0.5: "
              << (passed ? "PASS" : "FAIL")
              << ", minimum interior sample=" << minimum << std::endl;
    return passed;
}
}

int main()
{
    const std::vector<float> sine220 =
        loadSineFile("assets/sine_220.wav");
    const std::vector<float> sine440 =
        loadSineFile("assets/sine_440.wav");
    const std::vector<float> sine880 =
        loadSineFile("assets/sine_880.wav");

    if (sine220.empty() || sine440.empty() || sine880.empty())
        return 1;

    bool pitchCasesPassed = true;

    pitchCasesPassed = runAndReport(sine220, 220.0f, 220.0f) &&
        pitchCasesPassed;
    pitchCasesPassed = runAndReport(sine440, 440.0f, 440.0f) &&
        pitchCasesPassed;
    pitchCasesPassed = runAndReport(sine880, 880.0f, 880.0f) &&
        pitchCasesPassed;

    pitchCasesPassed = runAndReport(sine220, 220.0f, 440.0f) &&
        pitchCasesPassed;
    pitchCasesPassed = runAndReport(sine440, 440.0f, 220.0f) &&
        pitchCasesPassed;
    pitchCasesPassed = runAndReport(sine440, 440.0f, 880.0f) &&
        pitchCasesPassed;
    pitchCasesPassed = runAndReport(sine880, 880.0f, 440.0f) &&
        pitchCasesPassed;

    const bool coveragePassed = runCoverageDiagnostic();
    return pitchCasesPassed && coveragePassed ? 0 : 1;
}
