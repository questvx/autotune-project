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

- CMake 3.22 or later.
- A C++ compiler with C++20 support.
- An input mono WAV file for processing.

## Build and run

The JUCE standalone interface is built with CMake. The first configure downloads the pinned JUCE 8.0.14 source with `FetchContent`, so it requires an internet connection.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target AutoTuneStandalone
.\build\AutoTuneStandalone_artefacts\Debug\AutoTune.exe
```

The standalone interface uses a compact tuner-style layout. It lets you choose an input WAV and output path, set the root note and major/minor scale, and adjust retune speed, Flex Tune, and Natural Vibrato. Flex Tune keeps small pitch deviations more natural: at 100%, correction is suppressed within 30 cents, fades in smoothly from 30 to 60 cents, and remains full above 60 cents. Natural Vibrato preserves a percentage of the detected input vibrato, where 0% flattens it and 100% preserves it fully. A waveform preview is shown for the selected audio. Processing runs in the background, and the current reader supports mono WAV files only.

Humanize, Input Type, Transpose, and Low Latency are shown as work in progress. They are disabled and do not affect the rendered audio yet.

The original `AutoTuneEngineering` command-line target remains available. Its input path and settings are configured in `src/main.cpp`.

## Tests

The `PSOLADiagnostics` CTest target checks pitch shifting on the included 220 Hz, 440 Hz, and 880 Hz sine WAV files, along with overlap-add coverage on a constant signal. `PitchAnalysisDiagnostics` checks pitch detection across the frames of the 220 Hz and 880 Hz WAV files, verifies scale-target selection from measured pitch, checks C#4-to-C4 correction and transparent pass-through, verifies Flex Tune behavior for small, transitional, and large pitch deviations, checks natural vibrato preservation and no-vibrato pass-through, exercises frame stabilization, and diagnoses phase alignment on a steady sine and a changing-pitch signal with a short gap. In VS Code, configure the project with CMake Tools, build the desired diagnostics target, then run its test from the Testing view.

To render a mono vocal twice with identical pitch analysis and correction settings, once with phase alignment and once without it, run from the repository root:

```powershell
.\build\Debug\PitchAnalysisDiagnostics.exe assets\rainbow2.wav build\phase-comparison
```

The command creates the output directory if needed and writes `<input>_phase_alignment_on.wav` and `<input>_phase_alignment_off.wav`; choose a new output directory if those files already exist. It also reports phase-correlation, search-boundary, unwrapped-shift, and render-difference metrics. These diagnostics do not replace subjective listening or measure real-time performance.

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
