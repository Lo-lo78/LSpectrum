#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace { constexpr double slope = 4.5, loHz = 20.0, hiHz = 20000.0; }

float LSpectrumProcessor::NoiseState::next()
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    const float white = (float)(seed / 4294967295.0 * 2.0 - 1.0);
    brown = juce::jlimit(-1.f, 1.f, brown + white * .02f);
    b0 = .99765f*b0 + white*.0990460f;
    b1 = .963f*b1 + white*.2965164f;
    b2 = .57f*b2 + white*1.0526913f;
    b3 = .0000001f*b3 + white*.0000002f;
    return .5f * (brown + (b0+b1+b2+b3)*.05f);
}

LSpectrumProcessor::LSpectrumProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& s : selected) s.store(true);
    for (auto& channel : results) for (auto& v : channel) v.store(0.f);
    noiseR.seed = 0x9e3779b9;
    loadSettings();
    loadLastResult();
    worker = std::thread([this] { workerLoop(); });
}
LSpectrumProcessor::~LSpectrumProcessor()
{
    quit.store(true); wake.notify_all();
    if (worker.joinable()) worker.join();
}
bool LSpectrumProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void LSpectrumProcessor::prepareSoloFilters(double rate)
{
    // 6th-order analog Butterworth section Qs, implemented as three
    // independent digital biquads for each HP and LP flank.
    constexpr std::array<double, 3> butterworthQ {0.5176380902050415,
                                                    0.7071067811865476,
                                                    1.9318516525781366};
    const double top = 0.475 * rate;
    const double maxLower = 0.455 * rate;
    const double bandRatio = std::pow(hiHz / loHz, 1.0 / bands);

    auto makeBiquad = [rate](double cutoff, double q, bool highPass) noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * cutoff / rate;
        const double cosW = std::cos(w), sinW = std::sin(w);
        const double alpha = sinW / (2.0 * q);
        const double a0 = 1.0 + alpha;
        const double num = highPass ? (1.0 + cosW) * 0.5 : (1.0 - cosW) * 0.5;
        BiquadCoefficients c;
        c.b0 = num / a0;
        c.b1 = (highPass ? -2.0 : 2.0) * num / a0;
        c.b2 = c.b0;
        c.a1 = (-2.0 * cosW) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    };

    for (int band = 0; band < bands; ++band)
    {
        const double lowerEdge = loHz * std::pow(bandRatio, band);
        const double upperEdge = lowerEdge * bandRatio;
        const double centre = std::sqrt(lowerEdge * upperEdge);
        // Above the available Nyquist range do not pretend to solo a lower
        // frequency: silence is the truthful result for an inaudible band.
        bandAudible[(size_t) band] = centre < top && lowerEdge < top;
        const double lower = juce::jlimit(5.0, maxLower, lowerEdge);
        const double upper = juce::jlimit(lower + 1.0, top, upperEdge);
        auto& stages = soloCoefficients[(size_t) band];
        for (int stage = 0; stage < 3; ++stage)
        {
            stages[(size_t) stage] = makeBiquad(lower, butterworthQ[(size_t) stage], true);
            stages[(size_t) stage + 3] = makeBiquad(upper, butterworthQ[(size_t) stage], false);
        }
    }
}

void LSpectrumProcessor::resetSoloBank(int bank) noexcept
{
    for (auto& channel : soloStates[(size_t) bank])
        for (auto& section : channel) section = {};
}

double LSpectrumProcessor::processSoloBand(int bank, int channel, int band, double x) noexcept
{
    if (band < 0 || band >= bands || !bandAudible[(size_t) band]) return 0.0;
    const auto& coeffs = soloCoefficients[(size_t) band];
    auto& states = soloStates[(size_t) bank][(size_t) channel];
    for (int stage = 0; stage < soloStages; ++stage)
        x = states[(size_t) stage].process(x, coeffs[(size_t) stage]);
    return x;
}

void LSpectrumProcessor::prepareToPlay(double sr, int maxBlockSize)
{
    juce::ignoreUnused(maxBlockSize);
    sampleRateHz.store(sr > 0 ? sr : 44100.0);
    cancelScan();
    prepareSoloFilters(sampleRateHz.load());
    resetSoloBank(0);
    resetSoloBank(1);
    activeSoloBank = 0;
    activeSoloBand = -1;
    fadingSoloBand = -1;
    soloBandBlend = 1.f;
    lastSoloFrequency = -1.f;
    lastSoloEnabled = false;
    soloMix = 0.f;
}
void LSpectrumProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (audio.getNumChannels() < 2) return;
    const int n = audio.getNumSamples();
    float* const left = audio.getWritePointer(0);
    float* const right = audio.getWritePointer(1);
    const auto target = scanTarget.load(std::memory_order_relaxed);
    auto captured = scanSamples.load(std::memory_order_relaxed);
    auto w = writeIndex.load(std::memory_order_relaxed);
    const auto r = readIndex.load(std::memory_order_acquire);
    const float nDb = noiseDb.load();
    const float nGain = nDb <= -120.f ? 0.f : std::pow(10.f, nDb / 20.f);
    const double rate = juce::jmax(1.0, sampleRateHz.load());
    const bool solo = soloEnabled.load(std::memory_order_relaxed);
    const float requestedFrequency = soloFrequency.load(std::memory_order_relaxed);
    // Only recompute the band on a changed GUI selection. Filter coefficient
    // generation is NOT performed here: just switch precomputed band tables.
    if (requestedFrequency != lastSoloFrequency)
    {
        lastSoloFrequency = requestedFrequency;
        int best = 0;
        double closest = std::abs(frequency(0) - requestedFrequency);
        for (int b = 1; b < bands; ++b)
        {
            const double distance = std::abs(frequency(b) - requestedFrequency);
            if (distance < closest) { closest = distance; best = b; }
        }
        if (best != activeSoloBand)
        {
            fadingSoloBand = activeSoloBand;
            activeSoloBank = 1 - activeSoloBank;
            activeSoloBand = best;
            resetSoloBank(activeSoloBank);
            soloBandBlend = (fadingSoloBand < 0 ? 1.f : 0.f);
        }
    }
    if (solo && !lastSoloEnabled)
    {
        // No previous stale filter history when enabling solo.
        resetSoloBank(activeSoloBank);
        fadingSoloBand = -1;
        soloBandBlend = 1.f;
    }
    lastSoloEnabled = solo;
    const float soloStep = (float) (1.0 / (.010 * rate));
    const float bandStep = (float) (1.0 / (.025 * rate));
    bool wrote = false;
    bool overflow = false;
    // A fresh audio callback acknowledges the request at a buffer boundary.
    // This prevents the worker from resetting FIFO pointers while an old block
    // is still writing, even if R is pressed repeatedly.
    int requested = 1;
    scanState.compare_exchange_strong(requested, 5, std::memory_order_acq_rel);
    const bool capturingAtBlockStart = (scanState.load(std::memory_order_acquire) == 2);
    for (int i = 0; i < n; ++i)
    {
        const float inL = left[i], inR = right[i];
        const float referenceL = nGain > 0.f ? noiseL.next() * nGain : 0.f;
        const float referenceR = nGain > 0.f ? noiseR.next() * nGain : 0.f;
        const float auditionL = inL + referenceL;
        const float auditionR = inR + referenceR;

        // The audio thread only writes to the FIFO. FFT never runs here.
        // Keep the unprocessed incoming stereo signal, plus enabled reference noise.
        if (capturingAtBlockStart && captured < target)
        {
            if (w - r < ringSize - 1)
            {
                const auto at = (size_t) ((w & (ringSize - 1)) * 2);
                ring[at] = auditionL;
                ring[at + 1] = auditionR;
                ++w; ++captured; wrote = true;
            }
            else overflow = true;
        }

        // The listening features modify output only when deliberately enabled.
        if (solo) soloMix = juce::jmin(1.f, soloMix + soloStep);
        else      soloMix = juce::jmax(0.f, soloMix - soloStep);
        if (soloMix > 0.f && activeSoloBand >= 0)
        {
            double bandL = processSoloBand(activeSoloBank, 0, activeSoloBand, auditionL);
            double bandR = processSoloBand(activeSoloBank, 1, activeSoloBand, auditionR);
            if (soloBandBlend < 1.f && fadingSoloBand >= 0)
            {
                const int oldBank = 1 - activeSoloBank;
                const double oldL = processSoloBand(oldBank, 0, fadingSoloBand, auditionL);
                const double oldR = processSoloBand(oldBank, 1, fadingSoloBand, auditionR);
                bandL = oldL + (bandL - oldL) * soloBandBlend;
                bandR = oldR + (bandR - oldR) * soloBandBlend;
                soloBandBlend = juce::jmin(1.f, soloBandBlend + bandStep);
            }
            left[i]  = auditionL + (static_cast<float>(bandL) - auditionL) * soloMix;
            right[i] = auditionR + (static_cast<float>(bandR) - auditionR) * soloMix;
        }
        else
        {
            left[i] = auditionL;
            right[i] = auditionR;
        }
    }

    if (capturingAtBlockStart)
    {
        // Publish data BEFORE publishing a completed scan. The worker will drain
        // all captured samples before emitting results.
        writeIndex.store(w, std::memory_order_release);
        scanSamples.store(captured, std::memory_order_release);
        if (overflow || captured >= target)
        {
            scanEndIndex.store(w, std::memory_order_release);
            int expected = 2;
            scanState.compare_exchange_strong(expected, overflow ? 4 : 3,
                                              std::memory_order_acq_rel);
        }
        if (wrote || overflow) wake.notify_one();
    }
}

void LSpectrumProcessor::startScan()
{
    // The worker arms a new scan. GUI never rewinds FIFO pointers while the
    // audio thread is writing: that was the previous restart race.
    std::lock_guard<std::mutex> lock(requestMutex);
    resultReady.store(false, std::memory_order_release);
    signalDetected.store(false);
    scanState.store(1, std::memory_order_release);
    wake.notify_one();
}
void LSpectrumProcessor::cancelScan()
{
    std::lock_guard<std::mutex> lock(requestMutex);
    scanState.store(0, std::memory_order_release);
    wake.notify_one();
}
int LSpectrumProcessor::getRemainingSeconds() const
{
    const auto target = scanTarget.load(), seen = scanSamples.load();
    const double rate = juce::jmax(1.0, sampleRateHz.load());
    return isScanning() ? (int) std::ceil((double) (target > seen ? target - seen : 0) / rate) : 0;
}
void LSpectrumProcessor::setDuration(int seconds)
{
    duration.store(juce::jlimit(1,600,seconds)); saveSettings();
}
void LSpectrumProcessor::setBandSelected(int band,bool value)
{
    if(band<0 || band>=bands) return;
    if(!value && selected[(size_t)band].load() && getSelectedCount()==1) return;
    selected[(size_t)band].store(value); saveSettings();
}
void LSpectrumProcessor::selectAllBands()
{
    for(auto& s:selected) s.store(true);
    saveSettings();
}
void LSpectrumProcessor::selectOnlyBand(int band)
{
    if(band<0 || band>=bands) return;
    for(int b=0;b<bands;++b) selected[(size_t)b].store(b==band);
    saveSettings();
}
int LSpectrumProcessor::getSelectedCount() const
{
    int n=0; for(const auto& s:selected) if(s.load()) ++n; return n;
}
void LSpectrumProcessor::setNoiseLevel(float db)
{
    noiseDb.store(juce::jlimit(-120.f,0.f,db)); saveSettings();
}
void LSpectrumProcessor::rememberEditorPosition(int band, int column, bool selecting) noexcept
{
    editorBand.store(juce::jlimit(0,bands-1,band), std::memory_order_release);
    editorColumn.store(juce::jlimit(0,3,column), std::memory_order_release);
    editorSelectionMode.store(selecting, std::memory_order_release);
}
double LSpectrumProcessor::frequency(int b)
{
    return loHz*std::pow(hiHz/loHz,((double)b+.5)/bands);
}
bool LSpectrumProcessor::analyzeFrame(const std::array<float,fftSize>& frame, double sr,
                                      std::array<double,bands>& deviations)
{
    double energy=0.;
    for(int i=0;i<fftSize;++i)
    {
        const float sample=frame[(size_t)i];
        energy+=(double)sample*sample;
        fftWork[(size_t)i]=sample; // original JSFX computes an unwindowed FFT
    }
    if(energy/fftSize<1.e-14)
    {
        for(auto& v:deviations) v+=-150.;
        return false;
    }
    std::fill(fftWork.begin()+fftSize,fftWork.end(),0.f);
    fft.performRealOnlyForwardTransform(fftWork.data(),false);
    std::array<double,bands> db{};
    for(int b=0;b<bands;++b)
    {
        const auto low=loHz*std::pow(hiHz/loHz,(double)b/bands);
        const auto high=loHz*std::pow(hiHz/loHz,(double)(b+1)/bands);
        int k0=juce::jmax(1,(int)std::floor(low*fftSize/sr));
        int k1=juce::jmin(fftSize/2-1,(int)std::ceil(high*fftSize/sr));
        k0=juce::jmin(k0,fftSize/2-2); k1=juce::jmax(k0+1,k1);
        double mag=0.; int count=0;
        for(int k=k0;k<=k1;++k)
        {
            const double re=fftWork[(size_t)2*k],im=fftWork[(size_t)2*k+1];
            mag+=re*re+im*im; ++count;
        }
        db[(size_t)b]=20.0*std::log10(std::max(1.e-15,std::sqrt(mag/count)));
    }
    int pivot=0;
    for(int b=1;b<bands;++b)
        if(std::abs(frequency(b)-1000.)<std::abs(frequency(pivot)-1000.)) pivot=b;
    for(int b=0;b<bands;++b)
    {
        const double target=db[(size_t)pivot]-slope*std::log2(frequency(b)/frequency(pivot));
        deviations[(size_t)b]+=db[(size_t)b]-target;
    }
    return true;
}
void LSpectrumProcessor::workerLoop()
{
    std::array<std::array<float, fftSize>, 2> frames {};
    std::array<std::array<double, bands>, 2> sums {};
    uint64_t r = 0;
    int nFrames = 0, fill = 0;
    bool foundSignal = false;
    auto reset = [&]
    {
        for (auto& v : sums) v.fill(0.);
        nFrames = 0; fill = 0; foundSignal = false;
    };
    for (;;)
    {
        if (quit.load(std::memory_order_acquire)) return;
        const int state = scanState.load(std::memory_order_acquire);
        if (state == 5)
        {
            // A new scan begins only after the worker has discarded old data.
            // The producer cannot fill the FIFO until state becomes 2.
            reset();
            r = writeIndex.load(std::memory_order_acquire);
            readIndex.store(r, std::memory_order_release);
            scanSamples.store(0, std::memory_order_release);
            scanTarget.store(juce::jmax<uint64_t>(1, (uint64_t)
                    std::ceil(sampleRateHz.load() * (double) duration.load())));
            int expected = 5;
            scanState.compare_exchange_strong(expected, 2, std::memory_order_acq_rel);
        }
        else if (state == 2 || state == 3)
        {
            auto w = writeIndex.load(std::memory_order_acquire);
            // While capturing, an unexpected overrun is a failed scan, not a
            // partial or misleading result.
            if (w - r > ringSize)
            {
                int expected = 2;
                scanState.compare_exchange_strong(expected, 4);
            }
            else
            {
                while (r < w && scanState.load(std::memory_order_acquire) >= 2
                               && scanState.load(std::memory_order_acquire) <= 3)
                {
                    const auto at = (size_t) ((r & (ringSize - 1)) * 2);
                    frames[0][(size_t) fill] = ring[at];
                    frames[1][(size_t) fill] = ring[at + 1];
                    ++r; ++fill;
                    if (fill == fftSize)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            foundSignal |= analyzeFrame(frames[(size_t) ch], sampleRateHz.load(),
                                                        sums[(size_t) ch]);
                        ++nFrames; fill = 0;
                    }
                }
                readIndex.store(r, std::memory_order_release);
                // Only finish after the entire captured FIFO has been drained.
                if (scanState.load(std::memory_order_acquire) == 3
                    && r >= scanEndIndex.load(std::memory_order_acquire))
                {
                    if (fill > 0)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                        {
                            std::fill(frames[(size_t) ch].begin() + fill,
                                      frames[(size_t) ch].end(), 0.f);
                            foundSignal |= analyzeFrame(frames[(size_t) ch], sampleRateHz.load(),
                                                        sums[(size_t) ch]);
                        }
                        ++nFrames; fill = 0;
                    }
                    if (nFrames > 0)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            for (int b = 0; b < bands; ++b)
                            {
                                results[(size_t) ch][(size_t) b].store(
                                    (float) (sums[(size_t) ch][(size_t) b] / nFrames));
                            }
                        signalDetected.store(foundSignal);
                        // Do not publish an old result over a newly requested scan.
                        {
                            std::lock_guard<std::mutex> guard(requestMutex);
                            int expected = 3;
                            if (scanState.compare_exchange_strong(expected, 0, std::memory_order_acq_rel))
                            {
                                resultReady.store(true, std::memory_order_release);
                                saveReport();
                            }
                        }
                    }
                    reset();
                }
            }
        }
        std::unique_lock<std::mutex> lock(wakeMutex);
        wake.wait_for(lock, std::chrono::milliseconds(15));
    }
}
juce::File LSpectrumProcessor::dataDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Lo-lo").getChildFile("LSpectrum 4.5dB Analyzer");
}
juce::File LSpectrumProcessor::reportFile() const
{
    return dataDirectory().getChildFile("spectrum_LastResult.txt");
}
juce::String LSpectrumProcessor::reportText() const
{
    juce::String text="LSpectrum 4.5dB Analyzer\nReference: 4.5 dB/oct\n";
    text += "Captured stereo samples: " + juce::String((juce::int64) scanSamples.load()) + "\n";
    text += "Sample rate: " + juce::String(sampleRateHz.load(), 0) + " Hz\n";
    text += "Band | Frequency | Left diff dB | Right diff dB\n";
    for(int b=0;b<bands;++b) if(selected[(size_t)b].load())
        text+=juce::String(b+1)+" | "+juce::String(frequency(b),1)+" Hz | "
            +juce::String(getResult(b,0),2)+" | "+juce::String(getResult(b,1),2)+"\n";
    return text;
}
bool LSpectrumProcessor::saveReport() const
{
    auto dir=dataDirectory(); if(dir.createDirectory().failed()) return false;
    return reportFile().replaceWithText(reportText());
}
void LSpectrumProcessor::saveSettings() const
{
    auto dir=dataDirectory(); if(dir.createDirectory().failed()) return;
    juce::XmlElement root("LSpectrumSettings");
    root.setAttribute("Duration",duration.load());
    root.setAttribute("NoiseDb",(double)noiseDb.load());
    for(int b=0;b<bands;++b) root.setAttribute("Band"+juce::String(b+1),selected[(size_t)b].load());
    root.writeTo(dir.getChildFile("settings.xml"));
}
void LSpectrumProcessor::loadSettings()
{
    auto xml=juce::XmlDocument::parse(dataDirectory().getChildFile("settings.xml"));
    if(!xml || !xml->hasTagName("LSpectrumSettings")) return;
    duration.store(juce::jlimit(1,600,xml->getIntAttribute("Duration",10)));
    editorBand.store(juce::jlimit(0,bands-1,xml->getIntAttribute("EditorBand",0)));
    editorColumn.store(juce::jlimit(0,3,xml->getIntAttribute("EditorColumn",0)));
    editorSelectionMode.store(xml->getBoolAttribute("EditorSelectionMode", false));
    noiseDb.store((float)xml->getDoubleAttribute("NoiseDb",-120.));
    for(int b=0;b<bands;++b) selected[(size_t)b].store(xml->getBoolAttribute("Band"+juce::String(b+1),true));
    if(getSelectedCount()==0) for(auto& s:selected) s.store(true);
}
void LSpectrumProcessor::loadLastResult()
{
    const auto file=reportFile(); if(!file.existsAsFile()) return;
    int loaded=0;
    for (const auto& line : juce::StringArray::fromLines(file.loadFileAsString()))
    {
        auto parts=juce::StringArray::fromTokens(line,"|","");
        if(parts.size()!=4 && parts.size()!=3) continue;
        const int band=parts[0].trim().getIntValue();
        if(band<1 || band>bands) continue;
        const float left=parts[2].getFloatValue();
        const float right=(parts.size()==4 ? parts[3].getFloatValue() : left);
        results[0][(size_t)(band-1)].store(left);
        results[1][(size_t)(band-1)].store(right);
        ++loaded;
    }
    resultReady.store(loaded>0);
    signalDetected.store(loaded>0);
}
void LSpectrumProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("LSpectrumState");
    xml.setAttribute("Duration",duration.load());
    xml.setAttribute("EditorBand", editorBand.load());
    xml.setAttribute("EditorColumn", editorColumn.load());
    xml.setAttribute("EditorSelectionMode", editorSelectionMode.load());
    xml.setAttribute("NoiseDb",(double)noiseDb.load());
    xml.setAttribute("SoloOn",soloEnabled.load());
    xml.setAttribute("SoloFreq",(double)soloFrequency.load());
    xml.setAttribute("HasResult",resultReady.load());
    xml.setAttribute("HadSignal",signalDetected.load());
    for(int b=0;b<bands;++b)
    {
        xml.setAttribute("Band"+juce::String(b+1),selected[(size_t)b].load());
        xml.setAttribute("Left"+juce::String(b+1),(double)getResult(b,0));
        xml.setAttribute("Right"+juce::String(b+1),(double)getResult(b,1));
    }
    copyXmlToBinary(xml,dest);
}
void LSpectrumProcessor::setStateInformation(const void* data,int size)
{
    auto xml=getXmlFromBinary(data,size);
    if(!xml || !xml->hasTagName("LSpectrumState")) return;
    duration.store(juce::jlimit(1,600,xml->getIntAttribute("Duration",10)));
    editorBand.store(juce::jlimit(0,bands-1,xml->getIntAttribute("EditorBand",0)));
    editorColumn.store(juce::jlimit(0,3,xml->getIntAttribute("EditorColumn",0)));
    editorSelectionMode.store(xml->getBoolAttribute("EditorSelectionMode", false));
    noiseDb.store((float)xml->getDoubleAttribute("NoiseDb",-120.));
    // Legacy TEST5 ToneOn/ToneDb/ToneFreq fields are intentionally ignored.
    // This version does not generate a sine wave, even in saved projects.
    soloEnabled.store(xml->getBoolAttribute("SoloOn",false));
    soloFrequency.store((float)xml->getDoubleAttribute("SoloFreq",440.));
    for(int b=0;b<bands;++b)
    {
        selected[(size_t)b].store(xml->getBoolAttribute("Band"+juce::String(b+1),true));
        results[0][(size_t)b].store((float)xml->getDoubleAttribute("Left"+juce::String(b+1),0.));
        results[1][(size_t)b].store((float)xml->getDoubleAttribute("Right"+juce::String(b+1),0.));
    }
    if(getSelectedCount()==0) for(auto& s:selected) s.store(true);
    resultReady.store(xml->getBoolAttribute("HasResult",false));
    signalDetected.store(xml->getBoolAttribute("HadSignal",resultReady.load()));
}
juce::AudioProcessorEditor* LSpectrumProcessor::createEditor() {return new LSpectrumEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new LSpectrumProcessor();}
