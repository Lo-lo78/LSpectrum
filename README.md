# LSpectrum 4.5dB Analyzer — VST3 0.99.1

Free and open-source, NVDA-accessible stereo spectrum analyzer for REAPER.

**First public release — 18 November 2026**  
**Author:** Lo-lo  
**Repository:** https://github.com/Lo-lo78/LSpectrum  
**License:** GNU AGPLv3 only (`LICENSE`). Framework: JUCE 8.0.15 (AGPLv3 route).

## Features

- 32 logarithmic bands (about 20 Hz–20 kHz), comparing L and R independently
  with a 4.5 dB/octave reference anchored near 1 kHz.
- Real-time stereo audio capture and 32,768-point FFT analysis on a worker
  thread; default 10-second countdown, adjustable from 1 to 600 seconds.
- Silent FFT frames are ignored per channel; all-silent scans or silent
  individual channels show **No signal**, not an artificial -150 dB average.
- Stereo 6th-order Butterworth bandpass on each flank, about 36 dB/octave
  below and above the selected band, with 25-ms band transition.
- Bandpass solo level from -120 to +24 dB, default 0 dB, no extra makeup gain.
- Reference noise, selected-band view, copy/open text report.
- NVDA / Windows UI Automation accessibility, cyclic countdown edit–OK–Cancel,
  editable duration, remembered band/column and an on-demand About (Alt+A).
- Per-instance project state; shared latest-report TXT and defaults in
  `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\`.
- Pass-through stereo audio remains unchanged when solo/noise are disabled.

**Important:** readings are **spectral deviations from the slope**, not
absolute dBFS. Two channels with an identical spectral shape but different
absolute gain may yield similar readings. No sine oscillator or phase rotation.

## Manuals

- [Guida italiana](docs/Guida_Italiano.md)
- [English guide](docs/English_Guide.md)
- [Quick reference (IT)](GUIDA_RAPIDA.txt)
- [About](ABOUT.txt)
- [Release notes (IT)](RELEASE_NOTES_IT.md)
- [Release notes (EN)](RELEASE_NOTES_EN.md)

## Build with GitHub Actions

The project requires **Windows x64** and is configured for **Visual Studio 2022**
via `windows-2022`. The workflow downloads **JUCE 8.0.15** with CMake
`FetchContent`, builds the VST3, runs the small C++ averaging test, and creates
`LSpectrum.0.99.1.VST3.zip` containing the `.vst3` directory and documentation.

1. Copy all files into the root of your local `LSpectrum` repository, keeping
   the `.git` folder intact. Include `.github/workflows/build-windows.yml`.
2. Run:

   ```cmd
   git add . && git commit -m "Release LSpectrum 0.99.1" && git push
   ```

3. Wait for the Windows workflow to complete, download its Actions artifact,
   extract the inner `LSpectrum.0.99.1.VST3.zip` and attach that ZIP to a new
   GitHub Release with tag **`v0.99.1`**.
4. Install the resulting `.vst3` directory in the normal VST3 location (often
   `C:\Program Files\Common Files\VST3`), then rescan in REAPER if needed.

You can also configure and build locally with CMake and JUCE 8.0.15.

**Not yet verified here:** a complete Windows VST3 build and actual screen-reader
behavior. Check GitHub Actions and perform a short REAPER/NVDA smoke test before
marking the release as published.

## Source and licensing

The `LICENSE` file contains the complete GNU Affero General Public License v3.
Source code for the VST3 is available in this repository. JUCE 8.0.15 is
included as a fetched build dependency and licensed under its AGPLv3 option.
Read [COPYRIGHT.md](COPYRIGHT.md) for attribution and third-party licensing.
