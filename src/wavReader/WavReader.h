//WavReader.h
#pragma once
#include <filesystem>

class WavReader
{
    public:
        bool load(const std::filesystem::path& path);
};