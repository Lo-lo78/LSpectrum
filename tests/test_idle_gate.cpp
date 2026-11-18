// LSpectrum 4.5dB Analyzer — Copyright (C) 2026 Lo-lo and contributors.
// SPDX-License-Identifier: AGPL-3.0-only
#include "../Source/IdleGate.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

int main()
{
    using lspectrum::canPassThroughUntouched;
    // Inactive audio: existing input samples are left untouched, no DSP work.
    static_assert(canPassThroughUntouched(false, false, 0.0f, -120.0f));
    static_assert(!canPassThroughUntouched(true, false, 0.0f, -120.0f)); // R
    static_assert(!canPassThroughUntouched(false, true, 0.0f, -120.0f)); // Enter
    static_assert(!canPassThroughUntouched(false, false, 0.4f, -120.0f)); // solo tail
    static_assert(!canPassThroughUntouched(false, false, 0.0f, -60.0f)); // noise
    static_assert(canPassThroughUntouched(false, false, 0.0f, -121.0f));

    // Simulate the C++20 worker sleep/wake protocol, including shutdown:
    // one event must wake a sleeping analyzer exactly once, no timeout needed.
    std::atomic<unsigned> generation {0};
    std::atomic<bool> ready {false};
    std::atomic<bool> completed {false};
    std::thread worker([&]
    {
        const auto observed = generation.load(std::memory_order_acquire);
        ready.store(true, std::memory_order_release);
        ready.notify_one();
        generation.wait(observed, std::memory_order_acquire);
        completed.store(true, std::memory_order_release);
    });
    // Wait until the consumer has recorded the generation to be watched.
    // The notification may race with entering wait; the generation check
    // guarantees the event cannot be missed.
    ready.wait(false, std::memory_order_acquire);
    generation.fetch_add(1, std::memory_order_release);
    generation.notify_one();
    worker.join();
    assert(completed.load(std::memory_order_acquire));
    return 0;
}
