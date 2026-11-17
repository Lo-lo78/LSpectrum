#pragma once
#include "PluginProcessor.h"

class LSpectrumEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::KeyListener
{
public:
    explicit LSpectrumEditor(LSpectrumProcessor&);
    ~LSpectrumEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
private:
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    bool handleKeyPress(const juce::KeyPress&);
    void requestClose();
    void closeHostWindow();
    void timerCallback() override;
    void announce(const juce::String&);
    void announceCell();
    void rememberPosition();
    bool sendReaperTransportCommand(bool playPause);
    void navigate(int dRow,int dCol);
    void updatePreview();
    void toggleSelectionMode();
    void toggleCurrentBand();
    void refreshScreen();
    void startAnalysis();
    void changeDuration();
    void toggleTone();
    void toggleSolo();
    void toggleNoise();
    void copy(const juce::String&,const juce::String&);
    juce::String rowText() const;
    juce::String cellText() const;
    juce::String fieldText(int column) const;
    juce::String currentBandLabel() const;
    int bandAtRow() const;
    int totalRows() const;
    LSpectrumProcessor& processor;
    juce::Label status, guide;
    juce::TextButton analyze{"Analyze (R)"}, duration{"Duration (D)"}, selection{"Bands (B)"};
    juce::TextButton solo{"Solo band (S)"}, tone{"Sine (Enter)"}, noise{"Noise (N)"};
    juce::TextButton copyReport{"Copy report"}, openReport{"Open report"}, closeButton{"Close (Esc)"};
    juce::Rectangle<int> tableRect;
    int row=0,col=0;
    bool selectionMode=false;
    int tabStreak=0,lastSecond=-1;
    juce::uint32 lastTab=0,lastDigit=0;
    int digitLast=0;
    bool lastResultStatus=false;
    bool waitingAnnounced=false, captureBegan=false, errorAnnounced=false;
    bool initialLocationAnnounced=false;
    juce::uint32 scanRequestedMs=0;
    uint64_t previousCaptured=0;
    float previousNoise=-60.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSpectrumEditor)
};
