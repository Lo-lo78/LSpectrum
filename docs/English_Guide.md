# LSpectrum 4.5dB Analyzer — English user guide

**Version:** 0.99.1  
**Release date:** 18 November 2026  
**Author:** Lo-lo  
**Repository:** https://github.com/Lo-lo78/LSpectrum

## 1. Overview

LSpectrum is an accessible, stereo VST3 analyzer for REAPER. It measures
**32 logarithmically spaced frequency bands** from about 20 Hz to 20 kHz and
compares their spectral levels against a **4.5 dB/octave downward reference
slope**, anchored near 1 kHz.

The reported Left and Right values are **deviations from the reference**, not
absolute dBFS measurements. Each channel is normalized to its own 1-kHz-area
reference. Two equally shaped signals with a 6 dB difference in overall volume
may therefore show approximately the same numbers. Use an absolute level meter
for gain comparisons.

This VST3 replaces the original JSFX + Lua combination. It analyzes the audio
received wherever it is inserted in the FX chain, on a track or the master.
It does not automatically create, relocate or remove other FX.

When noise and bandpass solo are off, the stereo input is passed through
unchanged. With bandpass on, the true input content of the selected frequency
band is auditioned rather than an artificial sine wave. The optional noise
reference is added to both channels and will also enter the analysis.

## 2. Run a scan

1. Insert LSpectrum on the track/master you wish to inspect.
2. Start REAPER playback with **Space**; **Ctrl+Space** is Play/Pause.
3. Press **R** to start a fresh scan, defaulting to **10 seconds of received
   audio**, rather than elapsed wall-clock time when the host is stopped.
4. Wait for completion and inspect the four-column results table.

An FFT of **32,768 samples** is processed on a separate worker thread.
Silent FFT frames are **excluded from the average for each channel
independently**. A completely silent scan or silent L/R channel displays
**No signal** rather than a misleading numeric 0 dB.

## 3. Accessible table

The four columns are **Band**, **Frequency**, **Left dB** and **Right dB**.
Use Up/Down for rows, Left/Right for columns, Home/End for the first/last
row, and Page Up/Down in steps of eight rows.

**1, 2, 3, 4** announce the respective column; quickly press twice to copy
that value. **Tab** once repeats the cell, twice speaks the full row, and
three times copies the row. **Ctrl+C** copies the complete report.

The current band, column and band-selection mode are saved with the REAPER
project and restored when reopening the FX editor. NVDA should announce the
current cell directly, without an introductory message. **Escape in the
table** closes the FX editor window where permitted by the REAPER host; it
does not remove the plugin from the project.

## 4. Countdown and band selection

Press **C** to focus an edit control named **countdown** (1–600 seconds).
Inside the countdown group, **Tab** loops through **countdown → OK → Cancel →
countdown**, and **Shift+Tab** travels backwards. **Enter** in the edit field
or on OK saves; Enter on Cancel or **Escape** discards changes. In both cases,
focus returns to the table without closing the VST3. Arrow keys on OK and
Cancel are consumed inside the plugin, not forwarded to the host FX chain.

Press **B** to select bands. In that mode:
- **Shift+Space** toggles the current band;
- **X** keeps only the current band selected;
- **Delete** restores all 32 bands;
- **B** returns to the result view with the selected-band filter applied.

At least one band stays selected.

## 5. Bandpass audition and noise

**Enter in the table** toggles the stereo bandpass audition for the current
band. Each flank is a **sixth-order Butterworth filter** (high-pass and low-pass),
giving approximately **36 dB/octave per flank**; this is not an infinitely sharp
brick-wall filter. Band changes use a short crossfade.

**Ctrl+Up / Ctrl+Down** adjusts bandpass level in **1 dB** steps from **−120 to
+24 dB**, default **0 dB with no hidden makeup gain**. It does not affect the
spectrum measurement or unfiltered audio. Positive gain can cause peaks above
0 dBFS; no output limiter is used.

**N** toggles reference noise; **+ / −** changes its level by 5 dB. The sine
oscillator from the old JSFX has been removed.

## 6. Keyboard reference

| Key | Action |
|---|---|
| Arrow keys / Home / End / Page Up / Page Down | Navigate the table |
| Space / Ctrl+Space | REAPER Play/Stop / Play/Pause (Windows/REAPER) |
| R | Start new analysis |
| C | Countdown edit, OK, Cancel |
| B / Shift+Space / X / Delete | Select frequency bands |
| Enter (table) | Toggle bandpass solo |
| Ctrl+Up / Ctrl+Down | Bandpass audition gain |
| N / + / − | Toggle/adjust noise |
| 1 / 2 / 3 / 4 | Announce columns; double press copies |
| Ctrl+C | Copy full report |
| O or P | Open the saved report |
| F1 | Spoken keyboard help on request |
| Alt+A | About panel (read-only accessible text) |
| Escape (table) | Close the FX editor without removing the plugin |

Within About, Tab switches between the read-only text and OK. Escape or Enter
closes About and returns to the same table cell.

## 7. Saving, files, installation

Per-instance settings and results are stored in the REAPER project state.
Global defaults and the **most recently exported report** are written to:

`%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\`

as `settings.xml` and `spectrum_LastResult.txt`. Multiple instances keep
separate results in their project state, but the exported TXT file is shared
and overwritten by the last completed scan.

Copy the distributed `.vst3` folder to your VST3 installation directory,
usually `C:\Program Files\Common Files\VST3` on Windows, and rescan
plugins in REAPER if needed. The standalone VST3 no longer requires the old
JSFX, Lua, OSARA or SWS to analyze audio. Keyboard accessibility is designed
for NVDA through Windows UI Automation.

## 8. Accuracy and licensing

The instrument is mainly designed for music and broad-spectrum signals.
Single sine waves can leak energy into neighboring FFT bins because the
original JSFX's non-windowed FFT behavior was retained. Near Nyquist, the
highest bands can be only partly represented at low sample rates.

Distributed as open-source software under **GNU AGPLv3 only**, with the
complete license in `LICENSE`. Built with JUCE 8.0.15 through its AGPLv3
licensing route. Provided without warranty.
