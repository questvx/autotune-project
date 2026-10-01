#include "TargetNote.h"
#include "PitchToNote.h"
#include <cmath>

Note TargetNote::getTargetNote(const Note &detectedNote, const Scale &scale){
    if (detectedNote.midi < 0 || detectedNote.midi > 127) {
        return { -1, "Invalid", 0.0f }; // Return an invalid note if the detected note is out of range
    }
    if (scale.isNoteAllowed(detectedNote.midi)) {
        return detectedNote; // Return the detected note if it's allowed in the scale
    }

    int upperNote = detectedNote.midi + 1;
    int lowerNote = detectedNote.midi - 1;

    while (lowerNote >= 0 && !scale.isNoteAllowed(lowerNote))
        lowerNote--;

    while (upperNote <= 127 && !scale.isNoteAllowed(upperNote))
        upperNote++;

    int distanceToLower = detectedNote.midi - lowerNote;
    int distanceToUpper = upperNote - detectedNote.midi;

    int targetMidi = (distanceToLower <= distanceToUpper) ? lowerNote : upperNote;
    
    float targetFrequency = PitchToNote::midiNoteToFrequency(targetMidi);

    return PitchToNote::frequencyToNote(targetFrequency);
}