//PitchAnalyzer.cpp
#include "PitchAnalyzer.h"
#include "../pitchToNote/PitchToNote.h"
#include "../cents/Cents.h"

PitchAnalysisResult PitchAnalyzer::analyze(
    const AudioFrame& frame,
    float sampleRate,
    float minFrequency,
    float maxFrequency)
{
    PitchAnalysisResult result;

    // 1. Detect pitch
    result.pitchResult = pitchDetector.detectPitch(
        frame,
        sampleRate,
        minFrequency,
        maxFrequency
    );

    // 2. Convert detected frequency to note
    result.note = PitchToNote::frequencyToNote(
        result.pitchResult.frequency
    );

    // If pitch detection failed
    if (result.pitchResult.frequency <= 0.0f ||
        result.note.midi < 0)
    {
        return result;
    }

    // 3. Target frequency
    result.targetFrequency = result.note.frequency;

    // 4. Calculate cents deviation
    result.cents = Cents::calculate(
        result.pitchResult.frequency,
        result.targetFrequency
    );

    return result;
}