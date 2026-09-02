//PitchToNote.h

#pragma once
#include <cmath>
#include "Note.h"

class PitchToNote
{
public:
    static Note frequencyToNote(float frequency);
    static float midiNoteToFrequency(int midiNote);
};