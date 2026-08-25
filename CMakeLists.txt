cmake_minimum_required(VERSION 3.20)

project(AutoTuneEngineering)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    AutoTuneEngineering
    src/main.cpp
    src/WavReader/WavReader.cpp
    src/dsp/FrameGenerator.cpp
)