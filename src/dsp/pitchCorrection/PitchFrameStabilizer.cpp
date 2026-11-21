#include "PitchFrameStabilizer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr float minimumPitchConfidence = 0.75f;
}

std::vector<TrackedPitchFrame> PitchFrameStabilizer::stabilize(
    const std::vector<PitchAnalysisResult>& analysis,
    float minFrequency,
    float maxFrequency)
{
    std::vector<TrackedPitchFrame> rawFrames;
    rawFrames.reserve(analysis.size());

    for (const PitchAnalysisResult& frame : analysis)
    {
        TrackedPitchFrame tracked;
        tracked.confidence = frame.pitchResult.correlation;
        if (std::isfinite(frame.pitchResult.frequency) &&
            frame.pitchResult.frequency >= minFrequency &&
            frame.pitchResult.frequency <= maxFrequency &&
            frame.pitchResult.correlation >= minimumPitchConfidence &&
            frame.targetNote.midi >= 0 &&
            std::isfinite(frame.targetFrequency) &&
            frame.targetFrequency > 0.0f)
        {
            tracked.voiced = true;
            tracked.frequency = frame.pitchResult.frequency;
            tracked.targetNote = frame.targetNote;
            tracked.targetFrequency = frame.targetFrequency;
        }
        rawFrames.push_back(tracked);
    }

    std::vector<TrackedPitchFrame> stabilizedFrames = rawFrames;
    for (size_t i = 1; i + 1 < rawFrames.size(); ++i)
    {
        const TrackedPitchFrame& previous = rawFrames[i - 1];
        const TrackedPitchFrame& current = rawFrames[i];
        const TrackedPitchFrame& next = rawFrames[i + 1];
        if (!previous.voiced ||
            !next.voiced ||
            previous.targetNote.midi != next.targetNote.midi)
        {
            continue;
        }

        const float neighborFrequency = std::sqrt(
            previous.frequency * next.frequency);
        const float neighborDeviation = std::abs(
            1200.0f * std::log2(previous.frequency / next.frequency));
        if (neighborDeviation > 100.0f)
            continue;

        if (!current.voiced)
        {
            stabilizedFrames[i] = previous;
            stabilizedFrames[i].frequency = neighborFrequency;
            stabilizedFrames[i].confidence = std::min(
                previous.confidence,
                next.confidence);
            continue;
        }

        const float currentDeviation = std::abs(
            1200.0f * std::log2(
                current.frequency / neighborFrequency));
        const bool isolatedTargetFlip =
            current.targetNote.midi != previous.targetNote.midi &&
            currentDeviation <= 150.0f;
        const bool octaveOutlier = currentDeviation >= 600.0f;
        if (isolatedTargetFlip || octaveOutlier)
        {
            stabilizedFrames[i].targetNote = previous.targetNote;
            stabilizedFrames[i].targetFrequency =
                previous.targetFrequency;
            if (octaveOutlier)
                stabilizedFrames[i].frequency = neighborFrequency;
        }
    }

    int acceptedTargetMidi = -1;
    float acceptedTargetFrequency = 0.0f;
    int candidateTargetMidi = -1;
    float candidateTargetFrequency = 0.0f;
    size_t candidateFrameCount = 0;
    size_t unvoicedFrameCount = 0;
    for (TrackedPitchFrame& frame : stabilizedFrames)
    {
        if (!frame.voiced)
        {
            ++unvoicedFrameCount;
            candidateTargetMidi = -1;
            candidateFrameCount = 0;
            if (unvoicedFrameCount > 2)
            {
                acceptedTargetMidi = -1;
                acceptedTargetFrequency = 0.0f;
            }
            continue;
        }

        unvoicedFrameCount = 0;
        if (acceptedTargetMidi < 0)
        {
            acceptedTargetMidi = frame.targetNote.midi;
            acceptedTargetFrequency = frame.targetFrequency;
            candidateTargetMidi = -1;
            candidateFrameCount = 0;
            continue;
        }

        if (frame.targetNote.midi == acceptedTargetMidi)
        {
            candidateTargetMidi = -1;
            candidateFrameCount = 0;
            continue;
        }

        const float acceptedTargetDeviationCents = std::abs(
            1200.0f * std::log2(
                frame.frequency / acceptedTargetFrequency));
        const float currentTargetDeviationCents = std::abs(
            1200.0f * std::log2(
                frame.frequency / frame.targetFrequency));
        if (acceptedTargetDeviationCents -
                currentTargetDeviationCents >= 250.0f)
        {
            acceptedTargetMidi = frame.targetNote.midi;
            acceptedTargetFrequency = frame.targetFrequency;
            candidateTargetMidi = -1;
            candidateFrameCount = 0;
            continue;
        }

        if (frame.targetNote.midi == candidateTargetMidi)
        {
            ++candidateFrameCount;
        }
        else
        {
            candidateTargetMidi = frame.targetNote.midi;
            candidateTargetFrequency = frame.targetFrequency;
            candidateFrameCount = 1;
        }

        if (candidateFrameCount >= 2)
        {
            acceptedTargetMidi = candidateTargetMidi;
            acceptedTargetFrequency = candidateTargetFrequency;
            candidateTargetMidi = -1;
            candidateFrameCount = 0;
        }
        else
        {
            frame.targetNote.midi = acceptedTargetMidi;
            frame.targetFrequency = acceptedTargetFrequency;
        }
    }

    return stabilizedFrames;
}
