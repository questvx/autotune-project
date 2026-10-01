#pragma once

#include "Note.h"
#include "../../../scale/Scale.h"

class TargetNote
{
public:
    static Note getTargetNote(const Note &detectedNote, const Scale &scale);
};