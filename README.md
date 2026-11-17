# LSpectrum 4.5dB Analyzer — JUCE VST3 TEST1

Migration from the supplied JSFX + Lua sources. JUCE 8.0.15 (fetched by CMake), VST3 stereo in/out, 32 log bands, -4.5 dB/oct theoretical curve with a 1 kHz pivot, FFT 32768 in a background worker, 10-second scan as default. Signal is pass-through; noise and sine generators are off by default. No REAPER Lua API usage.

## Build

Windows: run `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`, then `cmake --build build --config Release`. GitHub Actions workflow builds the VST3 artifact automatically. Requires network to fetch JUCE.

## Keyboard (focus must be on plug-in surface)

R scan; D duration; B selection; Space toggle selected band; Delete select all; arrows rows/columns; Home/End, Page Up/Down; 1/2/3 spoken column, double-tap copies; Tab once cell, twice row, three times copy row; Enter toggle tone; + or - noise by 5 dB; Ctrl+C copy report; O open result; Escape cancel running scan/exit band selection.

## Files

`Documents/LSpectrum 4.5dB Analyzer/settings.xml` for duration and band selection and `spectrum_LastResult.txt` for the latest analysis. Project-specific state is also stored in the host chunk.

## Known TEST1 limitations

- Needs compiling and NVDA/OSARA keyboard testing inside REAPER; not yet validated as a binary.
- The FFT worker processes non-overlapping 32768-sample frames like the JSFX. Analysis time uses accumulated audio samples: if the DAW is stopped or no processing occurs, the countdown does not advance.
- This version does not deliberately control REAPER playback; start transport from REAPER itself.
- The generated noise and sine intentionally go to the output and are disabled by default.
