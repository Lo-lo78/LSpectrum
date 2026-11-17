#pragma once
#include "PluginProcessor.h"
#include <functional>

// Intercept countdown keys BEFORE JUCE TextEditor/Button dispatches them to
// the host or to its normal keyboard-focus traverser.
class LSpectrumCountdownEdit final : public juce::TextEditor
{
public:
    std::function<bool(const juce::KeyPress&)> onCountdownKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (onCountdownKey && onCountdownKey(key)) return true;
        return juce::TextEditor::keyPressed(key);
    }
};

class LSpectrumCountdownButton final : public juce::TextButton
{
public:
    explicit LSpectrumCountdownButton(const juce::String& caption) : juce::TextButton(caption) {}
    std::function<bool(const juce::KeyPress&)> onCountdownKey;
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (onCountdownKey && onCountdownKey(key)) return true;
        return juce::TextButton::keyPressed(key);
    }
};

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
    void focusGained(FocusChangeType) override;
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
    void saveCountdown();
    void cancelCountdown();
    void finishCountdown();
    bool countdownKeyPressed(const juce::KeyPress&, int control);
    void moveCountdownFocus(int direction, int control);
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
    juce::Label status;
    juce::TextButton analyze{"Analyze (R)"}, duration{"Countdown (C)"}, selection{"Bands (B)"};
    juce::TextButton solo{"Solo band (Enter)"}, noise{"Noise (N)"};
    LSpectrumCountdownEdit countdownInput;
    LSpectrumCountdownButton countdownOk{"OK"};
    LSpectrumCountdownButton countdownCancel{"Cancel"};
    bool countdownActive=false;
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
    bool initialLocationPending=true;
    bool initialFocusRetryDone=false;
    juce::uint32 initialFocusRequestedMs=0;
    juce::uint32 scanRequestedMs=0;
    uint64_t previousCaptured=0;
    float previousNoise=-60.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LSpectrumEditor)
};
