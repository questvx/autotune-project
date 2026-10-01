#include "Scale.h"

Scale::Scale(int rootNote, ScaleType type)
    : rootNote(rootNote), type(type)
{
}

bool Scale::isNoteAllowed(int midiNote) const
{
    int noteClass = midiNote % 12;
    int relativeNote = (noteClass - rootNote + 12) % 12;

    if (type == ScaleType::Major)
    {
        return relativeNote == 0 ||
               relativeNote == 2 ||
               relativeNote == 4 ||
               relativeNote == 5 ||
               relativeNote == 7 ||
               relativeNote == 9 ||
               relativeNote == 11;
    }

    if (type == ScaleType::Minor)
    {
        return relativeNote == 0 ||
               relativeNote == 2 ||
               relativeNote == 3 ||
               relativeNote == 5 ||
               relativeNote == 7 ||
               relativeNote == 8 ||
               relativeNote == 10;
    }

    return false;
}