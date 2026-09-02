// PitchToNote.cpp

#include "PitchToNote.h"
#include <cmath>

const char *noteNames[] =
    {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"};

Note PitchToNote::frequencyToMidiNote(float frequency)
{
    if (frequency <= 0.0f)
        return {-1, "Invalid", 0.0f};
    int midiNote = static_cast<int>(
        std::round(69 + 12 * std::log2(frequency / 440.0f)));

    return {
        midiNote,
        noteNames[midiNote % 12],
        midiNoteToFrequency(midiNote)};
}

float PitchToNote::midiNoteToFrequency(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return 0.0f;
        
    return 440.0f * std::pow(
        2,
        (midiNote - 69) / 12.0f
    );
}