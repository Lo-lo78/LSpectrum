#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

class LSpectrumProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int bands = 32;
    static constexpr int fftSize = 32768;
    static constexpr int ringSize = 262144;
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
    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void startScan();
    void cancelScan();
    bool isScanning() const { return scanning.load(); }
    bool hasResult() const { return resultReady.load(); }
    int getRemainingSeconds() const;
    float getResult(int b) const { return results[(size_t)b].load(); }
    int getDuration() const { return duration.load(); }
    void setDuration(int seconds);
    bool getBandSelected(int b) const { return selected[(size_t)b].load(); }
    void setBandSelected(int b, bool v);
    void selectAllBands();
    int getSelectedCount() const;
    void setTone(bool v) { toneEnabled.store(v); }
    bool isToneOn() const { return toneEnabled.load(); }
    void setToneFrequency(float f) { toneFrequency.store(f); }
    void setToneLevel(float db) { toneDb.store(db); }
    float getToneLevel() const { return toneDb.load(); }
    void setNoiseLevel(float db) { noiseDb.store(db); }
    float getNoiseLevel() const { return noiseDb.load(); }
    static double frequency(int b);
    juce::File dataDirectory() const;
    juce::File reportFile() const;
    bool saveReport() const;
    void loadSettings();
    void saveSettings() const;
    void loadLastResult();
    juce::String reportText() const;
    std::atomic<bool> resultReady {false};
private:
    void workerLoop();
    void analyzeFrame(const std::array<float, fftSize>& frame, double sampleRate, std::array<double, bands>& sums);
    struct NoiseState { float brown = 0, b0 = 0, b1 = 0, b2 = 0, b3 = 0; uint32_t seed = 0xA341316C; float next(); };
    juce::dsp::FFT fft {15};
    std::array<float, fftSize * 2> fftWork {};
    std::array<float, ringSize> ring {};
    std::atomic<uint64_t> writeIndex {0}, readIndex {0};
    std::array<std::atomic<float>, bands> results {};
    std::array<std::atomic<bool>, bands> selected {};
    std::atomic<int> duration {10};
    std::atomic<bool> scanning {false}, quit {false};
    std::atomic<uint64_t> scanSamples {0}, scanTarget {441000};
    std::atomic<double> sampleRateHz {44100.0};
    std::atomic<float> noiseDb {-120}, toneDb {-12}, toneFrequency {440};
    std::atomic<bool> toneEnabled {false};
    NoiseState noiseL, noiseR;
    double tonePhase = 0.0;
    std::thread worker;
    std::condition_variable wake;
    std::mutex wakeMutex;
    std::mutex scanMutex;
    std::atomic<uint64_t> scanGeneration {0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSpectrumProcessor)
};
