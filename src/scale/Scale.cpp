#pragma once

enum class ScaleType
{
    Major,
    Minor
};

class Scale
{
public:
    Scale(int rootNote, ScaleType type);

    bool isNoteAllowed(int midiNote) const;

private:
    int rootNote;
    ScaleType type;
};