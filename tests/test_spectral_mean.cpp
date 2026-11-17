// SPDX-License-Identifier: AGPL-3.0-only
#include "../Source/SpectralMean.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    // Five valid 0-dB deviations followed by five SILENT frames:
    // the correct spectrum is still 0 dB, never -75 dB.
    assert(std::abs(lspectrum::meanDeviation(0.0, 5)) < 1.e-9);
    // Left: 3 measured frames at +2 dB, right: 1 measured frame at -6 dB.
    assert(std::abs(lspectrum::meanDeviation(6.0, 3) - 2.0f) < 1.e-6f);
    assert(std::abs(lspectrum::meanDeviation(-6.0, 1) + 6.0f) < 1.e-6f);
    // Entirely silent channel has zero VALID measurements; flag checked by caller.
    assert(lspectrum::meanDeviation(0.0, 0) == 0.f);
    // A lone valid FFT among many silent FFTs is not diluted.
    assert(std::abs(lspectrum::meanDeviation(-3.5, 1) + 3.5f) < 1.e-6f);
    std::cout << "Spectral mean checks passed\n";
}
