// LSpectrum 4.5dB Analyzer — Copyright (C) 2026 Lo-lo and contributors.
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace lspectrum
{
    // In this state the AudioBuffer already contains the host's stereo input.
    // Returning immediately preserves both channels sample-for-sample, with
    // no FFT, FIFO, noise, filtering, per-sample gain or signal copying.
    constexpr bool canPassThroughUntouched(bool collectingSamples,
                                            bool soloRequested,
                                            float soloCrossfade,
                                            float noiseLevelDb) noexcept
    {
        return !collectingSamples && !soloRequested
            && soloCrossfade <= 0.0f && noiseLevelDb <= -120.0f;
    }
}
