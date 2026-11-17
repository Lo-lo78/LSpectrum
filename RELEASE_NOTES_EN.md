# LSpectrum 4.5dB Analyzer 0.99.1

First public release of the standalone C++/JUCE stereo VST3 for REAPER.

- Migrated the original JSFX/Lua analyzer into one self-contained plugin.
- 32 logarithmic bands and a 4.5 dB/octave spectral reference.
- Separate L/R FFT analysis on a worker thread, 1–600 s countdown.
- Fixed silent-frame averaging independently for each channel; **No signal**
  is shown instead of misleading 0 dB values on silent channels.
- Stereo 36 dB/octave-per-flank bandpass audition, −120 to +24 dB gain,
  no hidden boost at the 0 dB setting.
- Noise reference, selected-band view, clipboard and saved report.
- NVDA-oriented editor, cyclic countdown OK/Cancel, accessible Alt+A About.
- Per-project settings/results; global configuration under Roaming/Lo-lo.
- Open source under GNU AGPLv3; Italian and English manuals included.

Compatibility: Windows x64 VST3, designed for REAPER and NVDA.
Band values are spectral deviations from the reference, not absolute dBFS.
