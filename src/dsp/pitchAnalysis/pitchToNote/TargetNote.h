#pragma once

#include "Note.h"
#include "../../../scale/Scale.h"

class TargetNote
{
public:
    static Note getTargetNote(const Note &detectedNote, const Scale &scale);
    static Note getTargetNote(float detectedFrequency, const Scale &scale);
};