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
- Automatically corrects voiced audio frame by frame toward the selected scale, with smoothed retuning and overlap-add.
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

## Tests

The `PSOLADiagnostics` CTest target checks pitch shifting on the included 220 Hz, 440 Hz, and 880 Hz sine WAV files, along with overlap-add coverage on a constant signal. `PitchAnalysisDiagnostics` checks pitch detection across the frames of the 220 Hz and 880 Hz WAV files, verifies that A notes are accepted in C major, checks scale-target choice from measured pitch on either side of chromatic midpoints, checks C#4-to-C4 correction, and verifies transparent pass-through for both unvoiced silence and an in-scale 440 Hz sine. It also exercises stabilization against a one-frame octave error, a short unvoiced gap, low-confidence detection, target hysteresis and pitch-leap recovery, and preservation of valid frame-to-frame F0 movement. In VS Code, configure the project with CMake Tools, build the desired diagnostics target, then run its test from the Testing view.

These are focused DSP diagnostics; they do not measure pitch-shifting quality on vocals or real-time performance.

## Current implementation

`main.cpp` loads the WAV path configured in `getAssetPath`, splits it into frames of 2048 samples with a hop size of 512 samples, and analyzes pitch in the 50–1000 Hz range. It targets C major, stabilizes isolated frame-level pitch/target glitches, requires two consistent voiced frames before switching nearby target notes, immediately drops stale targets after large pitch leaps, and bridges one-frame voiced gaps. Each frame is pitch-shifted with PSOLA and Hann overlap-add; the retune speed controls how quickly the correction reaches the selected scale pitch. The per-frame report includes raw and tracked F0, confidence, stable target MIDI, requested and smoothed correction, applied pitch ratio, and phase alignment.

The example's input path, scale, analysis range, and retune speed are configured in the source code. The program does not yet accept an input path or settings as command-line arguments.

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

- The application currently reads mono audio and uses the hard-coded files from `assets` folder.
- Analysis settings, scale, and test pitch-shifting values are configured in the source code.
- PSOLA and per-frame correction are experimental; vocal quality and transitions between changing pitches have not yet been validated.
- There is no graphical interface or real-time audio processing yet. The automatic-correction diagnostic currently uses a synthetic sine signal.

Potential next steps include adding CLI configuration, expanding audio input support, and adding diagnostics for variable-pitch and vocal signals.
