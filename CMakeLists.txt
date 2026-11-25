cmake_minimum_required(VERSION 3.22)

project(AutoTuneEngineering)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AUTO_TUNE_CORE_SOURCES
    src/WavReader/WavReader.cpp
    src/dsp/frameGenerator/FrameGenerator.cpp
    src/dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.cpp
    src/dsp/pitchAnalysis/pitchDetector/PitchDetector.cpp
    src/dsp/pitchAnalysis/pitchToNote/PitchToNote.cpp
    src/dsp/pitchAnalysis/pitchToNote/TargetNote.cpp
    src/dsp/pitchAnalysis/cents/Cents.cpp
    src/dsp/pitchCorrection/AutoPitchCorrector.cpp
    src/dsp/pitchCorrection/PitchCorrection.cpp
    src/dsp/pitchCorrection/PitchFrameStabilizer.cpp
    src/dsp/retuneSpeed/RetuneSpeed.cpp
    src/scale/Scale.cpp
    src/dsp/pitchShifter/PitchMarks.cpp
    src/dsp/pitchShifter/PSOLAGrain.cpp
    src/dsp/pitchShifter/PSOLAProcessor.cpp
)

add_executable(AutoTuneEngineering src/main.cpp ${AUTO_TUNE_CORE_SOURCES})

include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(
    AutoTuneStandalone
    PRODUCT_NAME "AutoTune"
    VERSION "0.1.0"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(AutoTuneStandalone)

target_sources(
    AutoTuneStandalone
    PRIVATE
        src/gui/Main.cpp
        src/gui/MainComponent.cpp
        ${AUTO_TUNE_CORE_SOURCES}
)

target_include_directories(AutoTuneStandalone PRIVATE src)

target_compile_definitions(
    AutoTuneStandalone
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AutoTuneStandalone,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AutoTuneStandalone,JUCE_VERSION>"
)

target_link_libraries(
    AutoTuneStandalone
    PRIVATE
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

include(CTest)

if(BUILD_TESTING)
    add_executable(
        PSOLADiagnostics
        tests/PSOLADiagnostics.cpp
        src/wavReader/WavReader.cpp
        src/dsp/pitchShifter/PitchMarks.cpp
        src/dsp/pitchShifter/PSOLAGrain.cpp
        src/dsp/pitchShifter/PSOLAProcessor.cpp
    )

    target_include_directories(PSOLADiagnostics PRIVATE src)
    add_test(NAME PSOLADiagnostics COMMAND PSOLADiagnostics)
    set_tests_properties(
        PSOLADiagnostics
        PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    add_executable(
        PitchAnalysisDiagnostics
        tests/PitchAnalysisDiagnostics.cpp
        src/wavReader/WavReader.cpp
        src/dsp/frameGenerator/FrameGenerator.cpp
        src/dsp/pitchAnalysis/pitchAnalyzer/PitchAnalyzer.cpp
        src/dsp/pitchAnalysis/pitchDetector/PitchDetector.cpp
        src/dsp/pitchAnalysis/pitchToNote/PitchToNote.cpp
        src/dsp/pitchAnalysis/pitchToNote/TargetNote.cpp
        src/dsp/pitchAnalysis/cents/Cents.cpp
        src/dsp/pitchCorrection/AutoPitchCorrector.cpp
        src/dsp/pitchCorrection/PitchCorrection.cpp
        src/dsp/pitchCorrection/PitchFrameStabilizer.cpp
        src/dsp/retuneSpeed/RetuneSpeed.cpp
        src/dsp/pitchShifter/PitchMarks.cpp
        src/dsp/pitchShifter/PSOLAGrain.cpp
        src/dsp/pitchShifter/PSOLAProcessor.cpp
        src/scale/Scale.cpp
    )

    target_include_directories(PitchAnalysisDiagnostics PRIVATE src)
    add_test(NAME PitchAnalysisDiagnostics COMMAND PitchAnalysisDiagnostics)
    set_tests_properties(
        PitchAnalysisDiagnostics
        PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()