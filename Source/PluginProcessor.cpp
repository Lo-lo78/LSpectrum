#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace { constexpr double slope = 4.5; constexpr double loHz = 20., hiHz = 20000.; }
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
LSpectrumProcessor::LSpectrumProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& s: selected) s.store(true);
    for (auto& r: results) r.store(0.f);
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
bool LSpectrumProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void LSpectrumProcessor::prepareToPlay(double sr, int)
{
    sampleRateHz.store(sr > 0 ? sr : 44100.0);
    if (scanning.load()) cancelScan();
}
void LSpectrumProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = audio.getNumSamples();
    auto* left = audio.getWritePointer(0);
    auto* right = audio.getWritePointer(1);
    // The FIFO contains the incoming audio only: reference generators cannot bias the measurement.
    const bool active = scanning.load(std::memory_order_relaxed);
    const uint64_t target = scanTarget.load(std::memory_order_relaxed);
    uint64_t count = scanSamples.load(std::memory_order_relaxed);
    auto w = writeIndex.load(std::memory_order_relaxed);
    const auto workerRead = readIndex.load(std::memory_order_acquire);
    const float noiseLevel = noiseDb.load();
    const float toneLevel = toneDb.load();
    const float freq = toneFrequency.load();
    const bool tone = toneEnabled.load();
    const float nGain = noiseLevel <= -120 ? 0.f : std::pow(10.f, noiseLevel/20.f);
    const float tGain = std::pow(10.f, toneLevel/20.f);
    const double rate = sampleRateHz.load();
    const double phaseInc = juce::MathConstants<double>::twoPi * freq / rate;
    for (int i=0; i<n; ++i)
    {
        const float inL = left[i], inR = right[i];
        if (active && count < target && w-workerRead < ringSize-1)
        {
            ring[(size_t)(w & (ringSize-1))] = .5f*(inL+inR);
            ++w; ++count;
        }
        if (nGain != 0.f) { left[i] += noiseL.next()*nGain; right[i] += noiseR.next()*nGain; }
        if (tone)
        {
            const float s = (float)std::sin(tonePhase)*tGain;
            left[i] += s; right[i] += s;
            tonePhase += phaseInc;
            if (tonePhase >= juce::MathConstants<double>::twoPi) tonePhase -= juce::MathConstants<double>::twoPi;
        }
    }
    if (active)
    {
        writeIndex.store(w, std::memory_order_release);
        scanSamples.store(count, std::memory_order_release);
        if (count >= target) scanning.store(false);
        wake.notify_one();
    }
}
void LSpectrumProcessor::startScan()
{
    std::lock_guard<std::mutex> lock(scanMutex);
    scanning.store(false);
    scanGeneration.fetch_add(1);
    resultReady.store(false);
    readIndex.store(0); writeIndex.store(0); scanSamples.store(0);
    scanTarget.store((uint64_t)(sampleRateHz.load() * duration.load()));
    scanning.store(true, std::memory_order_release);
    wake.notify_one();
}
void LSpectrumProcessor::cancelScan()
{
    scanning.store(false);
    scanGeneration.fetch_add(1);
    wake.notify_one();
}
int LSpectrumProcessor::getRemainingSeconds() const
{
    const auto target=scanTarget.load(), seen=scanSamples.load();
    return scanning.load() ? (int)std::ceil((double)(target > seen ? target-seen : 0)/sampleRateHz.load()) : 0;
}
void LSpectrumProcessor::setDuration(int v) { duration.store(juce::jlimit(1, 600, v)); saveSettings(); }
void LSpectrumProcessor::setBandSelected(int b, bool v) { if (b>=0 && b<bands) { if (!v && selected[(size_t)b].load() && getSelectedCount()==1) return; selected[(size_t)b].store(v); saveSettings(); } }
void LSpectrumProcessor::selectAllBands() { for(auto& s:selected) s.store(true); saveSettings(); }
int LSpectrumProcessor::getSelectedCount() const { int n=0; for(const auto& s:selected) if(s.load()) ++n; return n; }
double LSpectrumProcessor::frequency(int b) { return loHz*std::pow(hiHz/loHz, ((double)b+.5)/bands); }
void LSpectrumProcessor::analyzeFrame(const std::array<float, fftSize>& frame, double sr, std::array<double,bands>& sums)
{
    double energy=0.;
    for (int i=0; i<fftSize; ++i) { energy += (double)frame[(size_t)i]*frame[(size_t)i]; fftWork[(size_t)i] = frame[(size_t)i]*(float)(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/(fftSize-1))); }
    if (energy/fftSize < 1e-14) { for(auto& s:sums) s+=-150.; return; }
    std::fill(fftWork.begin()+fftSize,fftWork.end(),0.f);
    fft.performRealOnlyForwardTransform(fftWork.data(), false);
    std::array<double,bands> db {};
    for(int b=0;b<bands;++b)
    {
        const auto low=loHz*std::pow(hiHz/loHz,(double)b/bands);
        const auto high=loHz*std::pow(hiHz/loHz,(double)(b+1)/bands);
        int k0=juce::jmax(1,(int)std::floor(low*fftSize/sr));
        int k1=juce::jmin(fftSize/2-1,(int)std::ceil(high*fftSize/sr));
        k0=juce::jmin(k0,fftSize/2-2); k1=juce::jmax(k0+1,k1);
        double mag=0.; int count=0;
        for(int k=k0;k<=k1;++k) { double re=fftWork[(size_t)2*k], im=fftWork[(size_t)2*k+1]; mag+=re*re+im*im; ++count; }
        db[(size_t)b] = 20.0*std::log10(std::max(1e-15,std::sqrt(mag/count)));
    }
    // Match the JSFX: align the theoretical -4.5 dB/oct curve at the closest band to 1 kHz.
    int pivot=0; for(int b=1;b<bands;++b) if(std::abs(frequency(b)-1000.)<std::abs(frequency(pivot)-1000.)) pivot=b;
    const double pivotDb=db[(size_t)pivot];
    for(int b=0;b<bands;++b)
    {
        const double target=pivotDb-slope*std::log2(frequency(b)/frequency(pivot));
        sums[(size_t)b] += db[(size_t)b]-target;
    }
}
void LSpectrumProcessor::workerLoop()
{
    std::array<float,fftSize> frame{};
    std::array<double,bands> sums{};
    uint64_t generation=scanGeneration.load();
    int frames=0, fill=0;
    for(;;)
    {
        if(quit.load()) return;
        const auto g=scanGeneration.load();
        if(g!=generation) { generation=g; sums.fill(0.); frames=0; fill=0; }
        uint64_t r=readIndex.load(std::memory_order_relaxed);
        const auto w=writeIndex.load(std::memory_order_acquire);
        if(w-r>ringSize) { r=w-ringSize; fill=0; sums.fill(0.); frames=0; }
        while(r<w && generation==scanGeneration.load())
        {
            frame[(size_t)fill++]=ring[(size_t)(r++ & (ringSize-1))];
            if(fill==fftSize)
            {
                analyzeFrame(frame,sampleRateHz.load(),sums); ++frames; fill=0;
            }
        }
        readIndex.store(r,std::memory_order_release);
        if(generation==scanGeneration.load() && !scanning.load() && scanSamples.load()>=scanTarget.load() && (frames>0 || fill>0))
        {
            // Last incomplete frame: analyze zero padded only when no full FFT frame was collected.
            if(frames==0 && fill>0) { std::fill(frame.begin()+fill,frame.end(),0.f); analyzeFrame(frame,sampleRateHz.load(),sums); ++frames; }
            for(int b=0;b<bands;++b) results[(size_t)b].store((float)(sums[(size_t)b]/frames));
            resultReady.store(true);
            saveReport();
            frames=0; fill=0;
        }
        std::unique_lock<std::mutex> lock(wakeMutex);
        wake.wait_for(lock,std::chrono::milliseconds(15));
    }
}
juce::File LSpectrumProcessor::dataDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LSpectrum 4.5dB Analyzer");
}
juce::File LSpectrumProcessor::reportFile() const { return dataDirectory().getChildFile("spectrum_LastResult.txt"); }
juce::String LSpectrumProcessor::reportText() const
{
    juce::String t="LSpectrum 4.5dB Analyzer\nTarget: 4.5 dB/oct\nBand | Frequency | dB\n";
    for(int b=0;b<bands;++b) if(selected[(size_t)b].load())
        t+=juce::String(b+1)+" | "+juce::String(frequency(b),1)+" Hz | "+juce::String(results[(size_t)b].load(),2)+" dB\n";
    return t;
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
    root.setAttribute("ToneDb",(double)toneDb.load());
    for(int b=0;b<bands;++b) root.setAttribute("Band"+juce::String(b+1),selected[(size_t)b].load());
    root.writeTo(dir.getChildFile("settings.xml"));
}
void LSpectrumProcessor::loadSettings()
{
    auto xml=juce::XmlDocument::parse(dataDirectory().getChildFile("settings.xml"));
    if(!xml || !xml->hasTagName("LSpectrumSettings")) return;
    duration.store(juce::jlimit(1,600,xml->getIntAttribute("Duration",10)));
    noiseDb.store((float)xml->getDoubleAttribute("NoiseDb",-120));
    toneDb.store((float)xml->getDoubleAttribute("ToneDb",-12));
    for(int b=0;b<bands;++b) selected[(size_t)b].store(xml->getBoolAttribute("Band"+juce::String(b+1),true));
}
void LSpectrumProcessor::loadLastResult()
{
    const auto file=reportFile(); if(!file.existsAsFile()) return;
    for(auto line:file.loadFileAsString().splitIntoLines())
    {
        auto parts=juce::StringArray::fromTokens(line,"|","");
        if(parts.size()!=3) continue;
        const int b=parts[0].trim().getIntValue();
        if(b>=1 && b<=bands) results[(size_t)(b-1)].store(parts[2].getFloatValue());
    }
    resultReady.store(true);
}
void LSpectrumProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("LSpectrumState");
    xml.setAttribute("Duration",duration.load());
    xml.setAttribute("NoiseDb",(double)noiseDb.load());
    xml.setAttribute("ToneDb",(double)toneDb.load());
    xml.setAttribute("ToneFreq",(double)toneFrequency.load());
    xml.setAttribute("ToneOn",toneEnabled.load());
    for(int b=0;b<bands;++b) xml.setAttribute("Band"+juce::String(b+1),selected[(size_t)b].load());
    copyXmlToBinary(xml,dest);
}
void LSpectrumProcessor::setStateInformation(const void* data,int size)
{
    auto xml=getXmlFromBinary(data,size);
    if(!xml || !xml->hasTagName("LSpectrumState")) return;
    duration.store(juce::jlimit(1,600,xml->getIntAttribute("Duration",10)));
    noiseDb.store((float)xml->getDoubleAttribute("NoiseDb",-120));
    toneDb.store((float)xml->getDoubleAttribute("ToneDb",-12));
    toneFrequency.store((float)xml->getDoubleAttribute("ToneFreq",440));
    toneEnabled.store(xml->getBoolAttribute("ToneOn",false));
    for(int b=0;b<bands;++b) selected[(size_t)b].store(xml->getBoolAttribute("Band"+juce::String(b+1),true));
}
juce::AudioProcessorEditor* LSpectrumProcessor::createEditor() { return new LSpectrumEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LSpectrumProcessor(); }
