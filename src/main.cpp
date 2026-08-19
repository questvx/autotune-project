// main.cpp
#include <iostream>
#include <filesystem>
#include "wavReader/WavReader.h"

using namespace std;

// Function to get the absolute path of the executable
static filesystem::path getExecutablePath(const char *argv0)
{
    filesystem::path exePath(argv0);
    if (exePath.is_relative())
    {
        exePath = filesystem::current_path() / exePath;
    }
    return filesystem::weakly_canonical(exePath);
}

static filesystem::path getAssetPath(const filesystem::path &exePath)
{
    return exePath.parent_path() / ".." / ".." / "assets" / "sine_440.wav";
}


// ----- Main function -----
int main(int argc, char **argv)
{
    WavReader reader;
    filesystem::path exePath = getExecutablePath(argv[0]);
    filesystem::path assetPath = getAssetPath(exePath);

    cout << "Executable path: " << exePath << "\n";
    cout << "Asset path: " << assetPath << "\n";
    cout << "Launching AutoTune..." << endl;

    if (!reader.load(assetPath))
    {
        return 1;
    }

    return 0;
}