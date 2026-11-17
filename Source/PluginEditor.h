#pragma once
#include "PluginProcessor.h"

class LSpectrumEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LSpectrumEditor(LSpectrumProcessor&);
    ~LSpectrumEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
private:
    void timerCallback() override;
    void announce(const juce::String& text);
    void announceCell();
    void navigate(int dRow, int dCol);
    void updatePreview();
    void toggleSelectionMode();
    void toggleCurrentBand();
    void refreshText();
    juce::String lineText() const;
    juce::String cellText() const;
    juce::String currentBandLabel() const;
    int bandAtRow() const;
    int totalRows() const;
    void copy(const juce::String&, const juce::String& announcement);
    LSpectrumProcessor& processor;
    juce::Label status, guide, tableText;
    juce::TextButton analyze{"Analyze (R)"}, duration{"Duration (D)"}, selection{"Select bands (B)"}, noise{"Noise level (+/-)"}, tone{"Tone (Enter)"}, copyReport{"Copy report (Ctrl+C)"};
    int row=0,col=0;
    bool selectionMode=false;
    int tabStreak=0, lastSecond=-1;
    juce::uint32 lastTab=0, lastDigit=0;
    int digitLast=0;
    bool lastResultStatus=false;
    std::array<int,LSpectrumProcessor::bands> order {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSpectrumEditor)
};
