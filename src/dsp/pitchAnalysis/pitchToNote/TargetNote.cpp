#include "TargetNote.h"
#include "PitchToNote.h"
#include <cmath>
#include <limits>

Note TargetNote::getTargetNote(
    const Note& detectedNote,
    const Scale& scale)
{
    if (detectedNote.midi < 0 || detectedNote.midi > 127)
        return {-1, "Invalid", 0.0f};

    return getTargetNote(detectedNote.frequency, scale);
}

Note TargetNote::getTargetNote(
    float detectedFrequency,
    const Scale& scale)
{
    if (!std::isfinite(detectedFrequency) || detectedFrequency <= 0.0f)
        return {-1, "Invalid", 0.0f};

    const Note detectedNote =
        PitchToNote::frequencyToNote(detectedFrequency);
    if (detectedNote.midi < 0 || detectedNote.midi > 127)
        return {-1, "Invalid", 0.0f};

    int targetMidi = -1;
    float minimumDistanceCents = std::numeric_limits<float>::infinity();
    constexpr float tieToleranceCents = 0.5f;
    for (int midiNote = 0; midiNote <= 127; ++midiNote)
    {
        if (!scale.isNoteAllowed(midiNote))
            continue;

        const float candidateFrequency =
            PitchToNote::midiNoteToFrequency(midiNote);
        const float distanceCents = std::abs(
            1200.0f * std::log2(
                detectedFrequency / candidateFrequency));
        if (distanceCents + tieToleranceCents < minimumDistanceCents)
        {
            minimumDistanceCents = distanceCents;
            targetMidi = midiNote;
        }
    }

    if (targetMidi < 0)
        return {-1, "Invalid", 0.0f};

    return PitchToNote::frequencyToNote(
        PitchToNote::midiNoteToFrequency(targetMidi));
}