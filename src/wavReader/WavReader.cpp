//WavReader.cpp
#include "WavReader.h"
#include <filesystem>
#include <iostream>

using namespace std;

bool WavReader::load(const filesystem::path& path)
{
   cout << "Current path(wav reader): "
          << std::filesystem::current_path()
          << "\n";

    if(!filesystem::exists(path))
    {
        cerr << "Error: File does not exist." << std::endl;
        return false;
    }

    cout << "File exists. Proceeding with loading..." << std::endl;

    return true;
}