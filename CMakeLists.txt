cmake_minimum_required(VERSION 3.20)

project(AutoTuneEngineering)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    AutoTuneEngineering
    src/main.cpp
    src/WavReader/WavReader.cpp
    src/dsp/frameGenerator/FrameGenerator.cpp
    src/dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.cpp
    src/dsp/pitchAnalysis/pitchDetector/PitchDetector.cpp
    src/dsp/pitchAnalysis/pitchToNote/PitchToNote.cpp
    src/dsp/pitchAnalysis/cents/Cents.cpp
)