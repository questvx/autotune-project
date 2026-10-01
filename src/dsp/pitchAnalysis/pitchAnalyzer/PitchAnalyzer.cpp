// PitchAnalyzer.cpp
#include "PitchAnalyzer.h"
#include "../pitchToNote/PitchToNote.h"
#include "../cents/Cents.h"
#include "../pitchToNote/TargetNote.h"

PitchAnalysisResult PitchAnalyzer::analyze(
    const AudioFrame& frame,
    float sampleRate,
    float minFrequency,
    float maxFrequency,
    const Scale& scale)
{
    PitchAnalysisResult result;
    result.note = {-1, "Invalid", 0.0f};
    result.targetNote = {-1, "Invalid", 0.0f};

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

    // 3. Find target note based on scale
    result.targetNote = TargetNote::getTargetNote(
        result.note,
        scale
    );

    // 4. Target frequency
    result.targetFrequency = result.targetNote.frequency;

    // 5. Calculate cents deviation
    result.cents = Cents::calculate(
        result.pitchResult.frequency,
        result.targetFrequency
    );

    return result;
}