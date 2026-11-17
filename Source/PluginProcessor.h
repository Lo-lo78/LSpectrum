#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <cstdint>

class LSpectrumProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int bands = 32;
    static constexpr int fftSize = 32768;
    static constexpr int ringSize = 262144; // stereo sample frames, power of two

    LSpectrumProcessor();
    ~LSpectrumProcessor() override;
    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LSpectrum 4.5dB Analyzer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void startScan();
    void cancelScan();
    // 0 idle, 1 requested, 2 capturing, 3 processing, 4 overflow, 5 audio acknowledged request.
    int getScanState() const { return scanState.load(std::memory_order_acquire); }
    bool isScanning() const { auto s = getScanState(); return s == 1 || s == 2 || s == 3 || s == 5; }
    bool hasScanError() const { return getScanState() == 4; }
    bool hasResult() const { return resultReady.load(std::memory_order_acquire); }
    bool hadSignal() const { return signalDetected.load(); }
    uint64_t getCapturedSamples() const { return scanSamples.load(); }
    int getRemainingSeconds() const;
    float getResult(int b, int channel) const { return results[(size_t)channel][(size_t)b].load(); }
    int getDuration() const { return duration.load(); }
    void setDuration(int seconds);
    bool getBandSelected(int b) const { return selected[(size_t)b].load(); }
    void setBandSelected(int b, bool value);
    void selectAllBands();
    void selectOnlyBand(int band);
    int getSelectedCount() const;

    // Editor position belongs to this plugin instance and is saved with the DAW project.
    void rememberEditorPosition(int band, int column, bool selectionMode) noexcept;
    int getEditorBand() const noexcept { return editorBand.load(std::memory_order_acquire); }
    int getEditorColumn() const noexcept { return editorColumn.load(std::memory_order_acquire); }
    bool getEditorSelectionMode() const noexcept { return editorSelectionMode.load(std::memory_order_acquire); }

    void setNoiseLevel(float db);
    float getNoiseLevel() const { return noiseDb.load(); }
    void setSolo(bool enabled) { soloEnabled.store(enabled); }
    bool isSoloOn() const { return soloEnabled.load(); }
    void setSoloFrequency(float f) { soloFrequency.store(f); }
    // Level of the audition bandpass ONLY, not the main input or FFT.
    static constexpr float soloGainMinimumDb = -120.f;
    static constexpr float soloGainMaximumDb = 24.f;
    void setSoloGainDb(float db);
    float getSoloGainDb() const { return soloGainDb.load(std::memory_order_relaxed); }

    static double frequency(int b);
    juce::File dataDirectory() const;
    juce::File reportFile() const;
    bool saveReport() const;
    void loadSettings();
    void saveSettings() const;
    void loadLastResult();
    juce::String reportText() const;

private:
    void workerLoop();
    bool analyzeFrame(const std::array<float, fftSize>& frame, double sampleRate,
                      std::array<double, bands>& deviations);
    struct NoiseState
    {
        float brown=0, b0=0, b1=0, b2=0, b3=0;
        uint32_t seed=0xA341316C;
        float next();
    };

    juce::dsp::FFT fft {15};
    std::array<float, fftSize * 2> fftWork {};
    std::array<float, ringSize * 2> ring {}; // L,R interleaved; no allocations on audio thread
    std::atomic<uint64_t> writeIndex {0}, readIndex {0}, scanEndIndex {0};
    std::array<std::array<std::atomic<float>, bands>, 2> results {};
    std::array<std::atomic<bool>, bands> selected {};
    std::atomic<int> duration {10};
    std::atomic<int> editorBand {0}, editorColumn {0};
    std::atomic<bool> editorSelectionMode {false};
    std::atomic<bool> resultReady {false}, quit {false}, signalDetected {false};
    std::atomic<int> scanState {0};
    std::atomic<uint64_t> scanSamples {0}, scanTarget {441000};
    std::atomic<double> sampleRateHz {44100.0};
    std::atomic<float> noiseDb {-120.f};
    std::atomic<float> soloFrequency {440.f};
    std::atomic<bool> soloEnabled {false};
    std::atomic<float> soloGainDb {0.f}; // Default 0 dB, range -120 to +24 dB.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> soloGainSmoother;
    NoiseState noiseL, noiseR;
    // 36 dB/oct on EACH flank: 6th-order Butterworth high-pass +
    // 6th-order Butterworth low-pass (3 biquads for each edge).
    // Coefficients are calculated only in prepareToPlay, never on audio thread.
    struct BiquadCoefficients
    {
        double b0=1., b1=0., b2=0., a1=0., a2=0.;
    };
    struct BiquadState
    {
        double z1=0., z2=0.;
        double process(double x, const BiquadCoefficients& c) noexcept
        {
            const double y = c.b0*x + z1;
            z1 = c.b1*x - c.a1*y + z2;
            z2 = c.b2*x - c.a2*y;
            return y;
        }
    };
    static constexpr int soloStages = 6;
    using StageCoefficients = std::array<BiquadCoefficients, soloStages>;
    using StageStates = std::array<BiquadState, soloStages>;
    std::array<StageCoefficients, bands> soloCoefficients {};
    std::array<bool, bands> bandAudible {};
    // Two banks allow a short click-free crossfade when moving to another band.
    std::array<std::array<StageStates, 2>, 2> soloStates {};
    int activeSoloBank = 0, activeSoloBand = -1, fadingSoloBand = -1;
    float soloBandBlend = 1.f, soloMix = 0.f, lastSoloFrequency = -1.f;
    bool lastSoloEnabled = false;
    void prepareSoloFilters(double sampleRate);
    void resetSoloBank(int bank) noexcept;
    double processSoloBand(int bank, int channel, int band, double x) noexcept;
    std::thread worker;
    std::condition_variable wake;
    std::mutex wakeMutex;
    std::mutex requestMutex; // GUI/state requests only; never acquired by audio callback
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSpectrumProcessor)
};
