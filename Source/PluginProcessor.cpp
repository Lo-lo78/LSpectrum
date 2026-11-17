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
    for (auto& channel : amplitudes) for (auto& v : channel) v.store(0.f);
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
void LSpectrumProcessor::prepareToPlay(double sr, int maxBlockSize)
{
    sampleRateHz.store(sr > 0 ? sr : 44100.0);
    cancelScan();
    juce::dsp::ProcessSpec spec {sampleRateHz.load(), (juce::uint32)juce::jmax(1,maxBlockSize), 1};
    soloL.prepare(spec); soloR.prepare(spec);
    soloL.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    soloR.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    soloL.setResonance(4.f); soloR.setResonance(4.f);
    soloL.reset(); soloR.reset(); lastSoloCutoff=0.f; soloMix=0.f;
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
    const float nDb = noiseDb.load(), tDb = toneDb.load();
    const float nGain = nDb <= -120.f ? 0.f : std::pow(10.f, nDb / 20.f);
    const float tGain = std::pow(10.f, tDb / 20.f);
    const bool tone = toneEnabled.load();
    // One oscillator, two independent channel gains. Keeping one phase avoids
    // inventing an L/R phase offset just to represent the stereo level ratio.
    const float balanceL = toneScaleL.load(), balanceR = toneScaleR.load();
    const double rate = juce::jmax(1.0, sampleRateHz.load());
    const double phaseInc = juce::MathConstants<double>::twoPi * toneFrequency.load() / rate;
    const float toneSmoothing = (float) (1.0 - std::exp(-1.0 / (0.012 * rate)));
    const bool solo = soloEnabled.load();
    const float cutoff = juce::jlimit(20.f, (float) juce::jmax(30.0, rate * .4), soloFrequency.load());
    if (std::abs(cutoff - lastSoloCutoff) > .01f)
    {
        soloL.setCutoffFrequency(cutoff); soloR.setCutoffFrequency(cutoff);
        lastSoloCutoff = cutoff;
    }
    const float soloStep = (float) (1.0 / (.005 * rate));
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

        // The audio thread only writes to the FIFO. FFT never runs here.
        // Keep the unprocessed incoming stereo signal, plus enabled reference noise.
        if (capturingAtBlockStart && captured < target)
        {
            if (w - r < ringSize - 1)
            {
                const auto at = (size_t) ((w & (ringSize - 1)) * 2);
                ring[at] = inL + referenceL;
                ring[at + 1] = inR + referenceR;
                ++w; ++captured; wrote = true;
            }
            else overflow = true;
        }

        // The listening features modify output only when deliberately enabled.
        if (solo) soloMix = juce::jmin(1.f, soloMix + soloStep);
        else      soloMix = juce::jmax(0.f, soloMix - soloStep);
        if (soloMix > 0.f)
        {
            const float bandL = soloL.processSample(0, inL);
            const float bandR = soloR.processSample(0, inR);
            left[i]  = inL + (bandL - inL) * soloMix;
            right[i] = inR + (bandR - inR) * soloMix;
        }
        if (nGain > 0.f) { left[i] += referenceL; right[i] += referenceR; }
        // Smooth the L/R gains as the selected band changes; avoid clicks
        // when pressing Enter or moving through bands during sine audition.
        toneSmoothedL += (balanceL - toneSmoothedL) * toneSmoothing;
        toneSmoothedR += (balanceR - toneSmoothedR) * toneSmoothing;
        toneFade += ((tone ? 1.f : 0.f) - toneFade) * toneSmoothing;
        if (!tone && toneFade < 1.e-5f) toneFade = 0.f;
        if (tone || toneFade > 0.f)
        {
            const float sine = (float) std::sin(tonePhase) * tGain * toneFade;
            left[i] += sine * toneSmoothedL;
            right[i] += sine * toneSmoothedR;
            tonePhase += phaseInc;
            if (tonePhase >= juce::MathConstants<double>::twoPi)
                tonePhase -= juce::MathConstants<double>::twoPi;
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
void LSpectrumProcessor::setToneStereoBand(int band)
{
    if (band < 0 || band >= bands) return;

    // On first launch, before any scan, keep the sine audible in both ears.
    // A completed scan provides TRUE per-channel FFT magnitudes, not the
    // displayed deviations from the 4.5 dB/oct reference curve.
    if (!toneLevelsValid.load(std::memory_order_acquire))
    {
        toneScaleL.store(1.f); toneScaleR.store(1.f);
        return;
    }
    if (!signalDetected.load())
    {
        toneScaleL.store(0.f); toneScaleR.store(0.f);
        return;
    }
    const float left = juce::jmax(0.f, amplitudes[0][(size_t)band].load());
    const float right = juce::jmax(0.f, amplitudes[1][(size_t)band].load());
    const float maxAmp = juce::jmax(left, right);
    // Silence should not be normalised to a full-volume sine.
    if (maxAmp <= 1.0e-6f || !std::isfinite(maxAmp))
    {
        toneScaleL.store(0.f); toneScaleR.store(0.f);
        return;
    }
    // Equivalent to gain = 10^((channelDb - louderChannelDb)/20).
    // This preserves the measured stereo level difference without
    // changing the global reference sine level or the relative phase.
    toneScaleL.store(juce::jlimit(0.f, 1.f, left / maxAmp));
    toneScaleR.store(juce::jlimit(0.f, 1.f, right / maxAmp));
}
double LSpectrumProcessor::frequency(int b)
{
    return loHz*std::pow(hiHz/loHz,((double)b+.5)/bands);
}
bool LSpectrumProcessor::analyzeFrame(const std::array<float,fftSize>& frame, double sr,
                                      std::array<double,bands>& deviations,
                                      std::array<double,bands>& absoluteLevels)
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
        for(auto& v:absoluteLevels) v+=-150.;
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
        absoluteLevels[(size_t)b]+=db[(size_t)b];
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
    std::array<std::array<double, bands>, 2> absolute {};
    uint64_t r = 0;
    int nFrames = 0, fill = 0;
    bool foundSignal = false;
    auto reset = [&]
    {
        for (auto& v : sums) v.fill(0.);
        for (auto& v : absolute) v.fill(0.);
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
                                                        sums[(size_t) ch], absolute[(size_t) ch]);
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
                                                        sums[(size_t) ch], absolute[(size_t) ch]);
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
                                amplitudes[(size_t) ch][(size_t) b].store(
                                    (float) std::pow(10., (absolute[(size_t) ch][(size_t) b] / nFrames) / 20.));
                            }
                        signalDetected.store(foundSignal);
                        toneLevelsValid.store(true, std::memory_order_release);
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
juce::File LSpectrumProcessor::levelsFile() const
{
    return dataDirectory().getChildFile("spectrum_LastLevels.txt");
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
    // The public report remains a four-column table. This companion file
    // stores the absolute measured stereo magnitudes for sine panorama
    // restoration after reopening REAPER, without altering displayed dB data.
    juce::String levels = "Band | Left magnitude | Right magnitude\n";
    for (int b = 0; b < bands; ++b)
        levels += juce::String(b + 1) + " | "
                + juce::String((double)amplitudes[0][(size_t)b].load(), 9) + " | "
                + juce::String((double)amplitudes[1][(size_t)b].load(), 9) + "\n";
    const bool okReport = reportFile().replaceWithText(reportText());
    const bool okLevels = levelsFile().replaceWithText(levels);
    return okReport && okLevels;
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
    // TEST4 report files do not have these levels: retain the original
    // centred sine in that case until a fresh scan is completed.
    int restored = 0;
    if (loaded > 0 && levelsFile().existsAsFile())
        for (const auto& line : juce::StringArray::fromLines(levelsFile().loadFileAsString()))
        {
            const auto parts = juce::StringArray::fromTokens(line, "|", "");
            if (parts.size() != 3) continue;
            const int band = parts[0].trim().getIntValue();
            if (band < 1 || band > bands) continue;
            const double left = parts[1].trim().getDoubleValue();
            const double right = parts[2].trim().getDoubleValue();
            if (!std::isfinite(left) || !std::isfinite(right) || left < 0. || right < 0.) continue;
            amplitudes[0][(size_t)(band - 1)].store((float)left);
            amplitudes[1][(size_t)(band - 1)].store((float)right);
            ++restored;
        }
    toneLevelsValid.store(restored == bands, std::memory_order_release);
}
void LSpectrumProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("LSpectrumState");
    xml.setAttribute("Duration",duration.load());
    xml.setAttribute("EditorBand", editorBand.load());
    xml.setAttribute("EditorColumn", editorColumn.load());
    xml.setAttribute("EditorSelectionMode", editorSelectionMode.load());
    xml.setAttribute("NoiseDb",(double)noiseDb.load());
    xml.setAttribute("ToneDb",(double)toneDb.load());
    xml.setAttribute("StereoSineVersion",1); // TEST5: independent L/R gains
    xml.setAttribute("ToneFreq",(double)toneFrequency.load());
    xml.setAttribute("ToneOn",toneEnabled.load());
    xml.setAttribute("SoloOn",soloEnabled.load());
    xml.setAttribute("SoloFreq",(double)soloFrequency.load());
    xml.setAttribute("HasResult",resultReady.load());
    xml.setAttribute("HasToneLevels",toneLevelsValid.load());
    xml.setAttribute("HadSignal",signalDetected.load());
    for(int b=0;b<bands;++b)
    {
        xml.setAttribute("Band"+juce::String(b+1),selected[(size_t)b].load());
        xml.setAttribute("Left"+juce::String(b+1),(double)getResult(b,0));
        xml.setAttribute("Right"+juce::String(b+1),(double)getResult(b,1));
        xml.setAttribute("ToneLeft"+juce::String(b+1),(double)getBandAmplitude(b,0));
        xml.setAttribute("ToneRight"+juce::String(b+1),(double)getBandAmplitude(b,1));
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
    // TEST4 changed the global sine level when navigating a different band.
    // That old value was transient, not a user setting: reset to -24 dB
    // on old projects to avoid a nearly inaudible TEST5 sine.
    const bool hasStereoSine = xml->getIntAttribute("StereoSineVersion",0) >= 1;
    toneDb.store(hasStereoSine
        ? juce::jlimit(-60.f, 0.f, (float)xml->getDoubleAttribute("ToneDb",-24.))
        : -24.f);
    toneFrequency.store((float)xml->getDoubleAttribute("ToneFreq",440.));
    toneEnabled.store(xml->getBoolAttribute("ToneOn",false));
    soloEnabled.store(xml->getBoolAttribute("SoloOn",false));
    soloFrequency.store((float)xml->getDoubleAttribute("SoloFreq",440.));
    for(int b=0;b<bands;++b)
    {
        selected[(size_t)b].store(xml->getBoolAttribute("Band"+juce::String(b+1),true));
        results[0][(size_t)b].store((float)xml->getDoubleAttribute("Left"+juce::String(b+1),0.));
        results[1][(size_t)b].store((float)xml->getDoubleAttribute("Right"+juce::String(b+1),0.));
        amplitudes[0][(size_t)b].store((float)xml->getDoubleAttribute("ToneLeft"+juce::String(b+1),0.));
        amplitudes[1][(size_t)b].store((float)xml->getDoubleAttribute("ToneRight"+juce::String(b+1),0.));
    }
    if(getSelectedCount()==0) for(auto& s:selected) s.store(true);
    resultReady.store(xml->getBoolAttribute("HasResult",false));
    toneLevelsValid.store(xml->getBoolAttribute("HasToneLevels",false), std::memory_order_release);
    signalDetected.store(xml->getBoolAttribute("HadSignal",resultReady.load()));
}
juce::AudioProcessorEditor* LSpectrumProcessor::createEditor() {return new LSpectrumEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new LSpectrumProcessor();}
