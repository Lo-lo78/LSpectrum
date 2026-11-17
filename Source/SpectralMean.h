// LSpectrum 4.5dB Analyzer — Copyright (C) 2026 Lo-lo and contributors.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
// Arithmetic mean of non-silent FFT-frame deviations (per independent channel).
// Zero valid frames => no measured spectrum; the UI reports "No signal".
namespace lspectrum
{
inline float meanDeviation(double sumOfValidDeviations, int validFrameCount) noexcept
{
    return validFrameCount > 0
        ? static_cast<float>(sumOfValidDeviations / validFrameCount)
        : 0.0f;
}
}
