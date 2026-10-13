# AutoTune

**An experimental C++20 audio processor** developed as an engineering project. Its goal is to analyze audio pitch and correct intonation by matching detected notes to a selected musical scale.

The project demonstrates practical digital signal processing (DSP), audio data handling, and a modular C++ application structure. It is a command-line prototype, not yet a complete or real-time replacement for commercial autotune software.

## Features

- Reads mono WAV files and converts audio samples to floating-point values.
- Splits audio into overlapping frames.
- Estimates fundamental frequency using normalized autocorrelation.
- Converts frequencies to MIDI notes and finds the nearest allowed note in a major or minor scale.
- Calculates the deviation between detected and target pitches in cents.
- Experiments with pitch-period marking and pitch shifting using PSOLA (Pitch-Synchronous Overlap-Add) with a Hann window.
- Writes the processed signal to a WAV file.

## Tech stack

- **C++20** — application and DSP algorithm implementation.
- **CMake 3.20+** — project configuration and builds.
- **dr_wav 0.14.6** — bundled single-header library for WAV reading and writing; no separate installation is required.
- **Visual Studio 2022** — the example project configuration on Windows. Other compilers with C++20 support can also be used.

## Requirements

- CMake 3.20 or later.
- A C++ compiler with C++20 support.
- The example audio file `assets/guitar_h.wav` (included in the repository).

## Build and run

Run these commands in PowerShell from the project root. The following example uses the Visual Studio 2022 generator:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\AutoTuneEngineering.exe
```

The application locates the input file `assets/guitar_h.wav` relative to the executable. Run it from the repository root to place the generated `output.wav` there. For a Release build, use `--config Release` and run `.\build\Release\AutoTuneEngineering.exe`.

## Current implementation

`main.cpp` loads the example audio file, splits it into frames of 2048 samples with a hop size of 512 samples, and analyzes pitch in the 50–1000 Hz range. The analysis reports values including frequency, correlation, detected note, target note, and deviation in cents. The example is currently configured to use C major.

In the current version, analysis and pitch shifting are still partly separate demonstration paths: the PSOLA input and target frequencies are hard-coded test values rather than being derived automatically from the analysis result. The program does not yet accept an input path or settings as command-line arguments.

## Project structure

```text
src/
  main.cpp                         Entry point and example processing flow
  wavReader/                       WAV sample reading
  dsp/
    frameGenerator/                Audio framing
    pitchAnalysis/                 Pitch, note, and deviation analysis
    pitchShifter/                  Pitch-period marking and experimental PSOLA
    pitchCorrection/               Pitch correction module
    retuneSpeed/                   Correction speed module
  scale/                           Musical scale definitions
assets/                            Example WAV recordings
external/dr_wav.h                  Bundled WAV library
```

## Limitations and next steps

- The application currently reads mono audio and uses the hard-coded file `assets/guitar_h.wav`.
- Analysis settings, scale, and test pitch-shifting values are configured in the source code.
- PSOLA is experimental and is not yet integrated with the automatically analyzed notes.
- There is no graphical interface, real-time audio processing, or automated test suite yet.

Potential next steps include connecting pitch analysis to correction, adding CLI configuration, expanding audio input support, and writing tests for the DSP algorithms.
