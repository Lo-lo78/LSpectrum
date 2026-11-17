#include "PluginEditor.h"
#include "ScreenReaderAnnouncer.h"
#include <cmath>
#include <cwchar>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
    constexpr int headerHeight=25, lineHeight=18;
    const char* columnLabels[4]={"Band","Frequency (Hz)","Left diff (dB)","Right diff (dB)"};
}
// REAPER-specific Windows GUI bridge: dispatch the documented main-section
// transport actions, rather than injecting keystrokes into the window tree.
// All calls happen in response to a UI key press, never on the audio thread.
namespace
{
#if JUCE_WINDOWS
    HWND findReaperWindowInThisProcess()
    {
        HWND found = nullptr;
        EnumWindows([](HWND hwnd, LPARAM userdata) -> BOOL
        {
            DWORD ownerPid = 0;
            GetWindowThreadProcessId(hwnd, &ownerPid);
            if (ownerPid != GetCurrentProcessId()) return TRUE;
            wchar_t cls[128] {};
            GetClassNameW(hwnd, cls, 128);
            if (std::wcscmp(cls, L"REAPERwnd") != 0) return TRUE;
            *reinterpret_cast<HWND*>(userdata) = hwnd;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&found));
        return found;
    }
#endif
}

bool LSpectrumEditor::sendReaperTransportCommand(bool playPause)
{
#if JUCE_WINDOWS
    if (auto* hwnd = findReaperWindowInThisProcess())
    {
        // 40044 = Transport: Play/stop; 40073 = Transport: Play/pause.
        // Post to REAPER's own top-level window, never to the VST child.
        if (PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(playPause ? 40073 : 40044, 0), 0))
            return true;
    }
#endif
    announce("REAPER transport unavailable from this plugin window");
    return false;
}

LSpectrumEditor::LSpectrumEditor(LSpectrumProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(810,790);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    // Keep the accessible label tied to the CURRENT cell, never to an
    // introductory description that NVDA repeats at each editor opening.
    setDescription({});
    selectionMode = processor.getEditorSelectionMode();
    col = juce::jlimit(0, 3, processor.getEditorColumn());
    const int savedBand = juce::jlimit(0, LSpectrumProcessor::bands-1, processor.getEditorBand());
    if (selectionMode) row = savedBand;
    else
    {
        int visibleIndex = 0;
        int nearestRow = 0;
        int nearestDistance = LSpectrumProcessor::bands;
        for (int band=0; band<LSpectrumProcessor::bands; ++band)
            if (processor.getBandSelected(band))
            {
                const int distance = std::abs(band - savedBand);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearestRow = visibleIndex;
                }
                ++visibleIndex;
            }
        row = nearestRow;
    }
    // The plugin's product name stays unchanged for the host, but the editor
    // itself exposes the CURRENT CELL, not "LSpectrum 4.5dB Analyzer".
    setName(cellText());
    setTitle(cellText());
    initialFocusRequestedMs = juce::Time::getMillisecondCounter();
    // No introductory text for NVDA/REAPER when landing in the FX chain.
    // Progress status is drawn for sighted users, but is not part of the
    // accessibility tree; only the current table cell is announced on entry.
    addAndMakeVisible(status);
    status.setAccessible(false);
    status.setJustificationType(juce::Justification::centredLeft);
    status.setColour(juce::Label::textColourId, juce::Colours::white);
    status.setText({}, juce::dontSendNotification);
    lastResultStatus=p.hasResult();
    for(auto* button : {&analyze,&duration,&selection,&solo,&noise,&countdownSave,&copyReport,&openReport,&closeButton})
    {
        addAndMakeVisible(*button);
        button->setMouseClickGrabsKeyboardFocus(false);
        button->setWantsKeyboardFocus(false);
        button->addKeyListener(this);
    }
    // Save is the only control in the top toolbar that must also be
    // keyboard-reachable from the countdown edit with Tab.
    countdownSave.setWantsKeyboardFocus(true);
    analyze.onClick=[this]{startAnalysis();grabKeyboardFocus();};
    duration.onClick=[this]{changeDuration();};
    selection.onClick=[this]{toggleSelectionMode();grabKeyboardFocus();};
    solo.onClick=[this]{toggleSolo();grabKeyboardFocus();};
    noise.onClick=[this]{toggleNoise();grabKeyboardFocus();};
    // The countdown is edited directly in the VST window. No AlertWindow,
    // duplicate Save announcements or focus transfer to a native modal.
    addAndMakeVisible(countdownInput);
    countdownInput.setName("countdown");
    countdownInput.setDescription({});
    countdownInput.setInputRestrictions(3, "0123456789");
    countdownInput.setText(juce::String(processor.getDuration()), false);
    countdownInput.addKeyListener(this);
    countdownInput.onReturnKey=[this]{saveCountdown();};
    countdownInput.onEscapeKey=[this]{requestClose();};
    countdownSave.onClick=[this]{saveCountdown();};
    copyReport.onClick=[this]
    {
        if(!processor.hasResult()) announce("No analysis results yet");
        else copy(processor.reportText(),"Full report copied to clipboard");
        grabKeyboardFocus();
    };
    openReport.onClick=[this]
    {
        if(processor.reportFile().existsAsFile() && processor.reportFile().startAsProcess())
            announce("Opening saved report");
        else announce("No saved report found");
        grabKeyboardFocus();
    };
    closeButton.onClick=[this]{requestClose();};
    status.addKeyListener(this);
    updatePreview(); refreshScreen(); rememberPosition(); startTimerHz(8);
}
LSpectrumEditor::~LSpectrumEditor()
{
    rememberPosition();
}
void LSpectrumEditor::visibilityChanged()
{
    if (isShowing())
    {
        // Do not queue a one-shot announcement before UI Automation is ready:
        // on the FIRST editor opening the provider may not yet exist.
        setName(cellText());
        setTitle(cellText());
        // The manual notification fixes the FIRST visit, when Windows UIA
        // has not necessarily announced the accessible cell name yet.
        // On subsequent visits REAPER/NVDA announces that focus name itself;
        // do not add another UiaRaiseNotificationEvent for the same cell.
        initialLocationAnnounced = !processor.needsFirstEditorCellAnnouncement();
        initialLocationPending = !initialLocationAnnounced;
        initialFocusRetryDone = false;
        initialFocusRequestedMs = juce::Time::getMillisecondCounter();
        grabKeyboardFocus();
    }
    else
    {
        initialLocationPending = false;
        initialLocationAnnounced = false;
    }
}
void LSpectrumEditor::focusGained(FocusChangeType)
{
    if (isShowing() && !initialLocationAnnounced
        && processor.needsFirstEditorCellAnnouncement())
    {
        initialLocationPending = true;
        initialFocusRequestedMs = juce::Time::getMillisecondCounter();
    }
}
void LSpectrumEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff121820));
    graphics.setColour(juce::Colours::white);
    graphics.setFont(juce::FontOptions(22.f));
    // No static product title in the editor: show the current location.
    graphics.drawText(cellText(),12,8,786,32,juce::Justification::centredLeft);
    graphics.setFont(juce::FontOptions(14.f));
    graphics.setColour(juce::Colour(0xffc0cbd5));
    const float soloDb = processor.getSoloGainDb();
    const juce::String soloLevel = juce::String(soloDb >= 0.f ? "+" : "") + juce::String(soloDb, 0);
    graphics.drawText("32 bands | 4.5 dB/oct | Bandpass level " + soloLevel + " dB",12,39,786,22,juce::Justification::centredLeft);
    graphics.setColour(juce::Colour(0xff273645));
    graphics.fillRoundedRectangle(tableRect.toFloat(),3.f);
    const int widths[4]={80,185,245,260};
    int x=tableRect.getX();
    graphics.setFont(juce::FontOptions(15.f));
    graphics.setColour(juce::Colours::white);
    for(int c=0;c<4;++c)
    {
        graphics.drawText(columnLabels[c],x+8,tableRect.getY()+2,widths[c]-12,headerHeight-4,juce::Justification::centredLeft);
        x+=widths[c];
    }
    graphics.setFont(juce::FontOptions(14.f));
    for(int r=0;r<totalRows();++r)
    {
        const int top=tableRect.getY()+headerHeight+r*lineHeight;
        if(top+lineHeight>tableRect.getBottom()) break;
        if(r==row)
        {
            graphics.setColour(juce::Colour(0xff3c6280));
            graphics.fillRect(tableRect.getX()+2,top,tableRect.getWidth()-4,lineHeight);
            int before=0;
            for(int c=0;c<col;++c) before+=widths[c];
            graphics.setColour(juce::Colour(0xffffd166));
            graphics.drawRect(tableRect.getX()+before+2,top+1,widths[col]-4,lineHeight-2,1);
        }
        else if((r%2)==0)
        {
            graphics.setColour(juce::Colour(0xff20303d));
            graphics.fillRect(tableRect.getX()+2,top,tableRect.getWidth()-4,lineHeight);
        }
        x=tableRect.getX();
        const int band=selectionMode ? r : [&]()->int
        {
            int i=0;
            for(int b=0;b<LSpectrumProcessor::bands;++b)
                if(processor.getBandSelected(b) && i++==r) return b;
            return 0;
        }();
        for(int c=0;c<4;++c)
        {
            graphics.setColour(juce::Colours::white);
            juce::String value;
            if(c==0) value=juce::String(band+1)+(selectionMode?(processor.getBandSelected(band)?" *":""):"");
            if(c==1) value=juce::String(LSpectrumProcessor::frequency(band),1);
            if(c==2) value=selectionMode?(processor.getBandSelected(band)?"selected":"not selected")
                :(processor.hasResult()?juce::String(processor.getResult(band,0),2):"--");
            if(c==3) value=selectionMode?"":(processor.hasResult()?juce::String(processor.getResult(band,1),2):"--");
            graphics.drawText(value,x+8,top+1,widths[c]-12,lineHeight-2,juce::Justification::centredLeft);
            x+=widths[c];
        }
    }
    graphics.setColour(juce::Colour(0xff6d8496));
    graphics.drawRect(tableRect,1);
}
void LSpectrumEditor::resized()
{
    status.setBounds(12,64,786,21);
    analyze.setBounds(12,88,137,33);
    duration.setBounds(157,88,137,33);
    selection.setBounds(302,88,137,33);
    solo.setBounds(447,88,170,33);
    noise.setBounds(625,88,173,33);
    countdownInput.setBounds(12,128,92,31);
    countdownSave.setBounds(110,128,100,31);
    copyReport.setBounds(218,128,165,31);
    openReport.setBounds(391,128,165,31);
    closeButton.setBounds(564,128,160,31);
    tableRect=juce::Rectangle<int>(12,169,786,headerHeight+LSpectrumProcessor::bands*lineHeight+2);
}
void LSpectrumEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto point=event.getPosition();
    if(!tableRect.contains(point)) {grabKeyboardFocus();return;}
    const int newRow=(point.y-tableRect.getY()-headerHeight)/lineHeight;
    if(point.y<tableRect.getY()+headerHeight || newRow<0 || newRow>=totalRows()) return;
    int x=point.x-tableRect.getX();
    const int widths[4]={80,185,245,260};
    int newCol=0;
    for(int c=0;c<3;++c) {if(x<widths[c]) break; x-=widths[c];++newCol;}
    row=newRow;col=newCol;
    updatePreview(); rememberPosition(); refreshScreen(); grabKeyboardFocus(); announceCell();
    if(selectionMode && event.getNumberOfClicks()>1) toggleCurrentBand();
}
void LSpectrumEditor::announce(const juce::String& message)
{
    lspectrum::announceToActiveScreenReader(*this,message,true);
}
int LSpectrumEditor::totalRows() const
{
    return selectionMode ? LSpectrumProcessor::bands : juce::jmax(1,processor.getSelectedCount());
}
int LSpectrumEditor::bandAtRow() const
{
    if(selectionMode) return juce::jlimit(0,LSpectrumProcessor::bands-1,row);
    int visible=0;
    for(int band=0;band<LSpectrumProcessor::bands;++band)
        if(processor.getBandSelected(band) && visible++==row) return band;
    return 0;
}
juce::String LSpectrumEditor::currentBandLabel() const
{
    return juce::String(LSpectrumProcessor::frequency(bandAtRow()),1)+" hertz";
}
juce::String LSpectrumEditor::fieldText(int column) const
{
    const int band=bandAtRow();
    if(column==0) return "Band "+juce::String(band+1);
    if(column==1) return currentBandLabel();
    if(selectionMode) return processor.getBandSelected(band)?"Selected":"Not selected";
    if(!processor.hasResult()) return "No results yet";
    return juce::String(column==2?"Left, ":"Right, ")+juce::String(processor.getResult(band,column-2),2)+" dB";
}
juce::String LSpectrumEditor::cellText() const
{
    if(selectionMode && col==0) return fieldText(0)+", "+fieldText(2);
    return fieldText(col);
}
juce::String LSpectrumEditor::rowText() const
{
    const int band=bandAtRow();
    juce::String text="Band "+juce::String(band+1)+", "+currentBandLabel();
    if(selectionMode) return text+", "+(processor.getBandSelected(band)?"selected":"not selected");
    if(!processor.hasResult()) return text+", no results yet";
    return text+", left "+juce::String(processor.getResult(band,0),2)
        +" dB, right "+juce::String(processor.getResult(band,1),2)+" dB";
}
void LSpectrumEditor::rememberPosition()
{
    processor.rememberEditorPosition(bandAtRow(), col, selectionMode);
}
void LSpectrumEditor::announceCell()
{
    // Only mark the entry as complete after JUCE has exposed a valid Windows
    // accessibility provider and the UIA notification has been accepted.
    if (lspectrum::announceToActiveScreenReader(*this, cellText(), true))
    {
        // This explicit notification is required only once per VST instance.
        // On re-entry the native accessibility focus provides the location.
        processor.markFirstEditorCellAnnounced();
        initialLocationAnnounced = true;
        initialLocationPending = false;
    }
}
void LSpectrumEditor::navigate(int dRow,int dCol)
{
    const int oldRow=row,oldCol=col;
    row=juce::jlimit(0,totalRows()-1,row+dRow);
    col=juce::jlimit(0,3,col+dCol);
    if(oldRow!=row || oldCol!=col)
    {
        updatePreview(); rememberPosition(); refreshScreen(); announceCell();
    }
}
void LSpectrumEditor::updatePreview()
{
    const int band=bandAtRow();
    const auto frequency=(float)LSpectrumProcessor::frequency(band);
    processor.setSoloFrequency(frequency);
}
void LSpectrumEditor::toggleSelectionMode()
{
    const int previousBand=bandAtRow();
    selectionMode=!selectionMode;
    if(selectionMode) row=previousBand;
    else
    {
        int visible=0; row=totalRows()-1;
        for(int band=0;band<LSpectrumProcessor::bands;++band)
            if(processor.getBandSelected(band)) {if(band>=previousBand) {row=visible;break;} ++visible;}
        row=juce::jlimit(0,totalRows()-1,row);
    }
    col=0;
    rememberPosition(); refreshScreen(); updatePreview();
    announce((selectionMode ? "Band selection. " : "Results. ") + cellText());
}
void LSpectrumEditor::toggleCurrentBand()
{
    if(!selectionMode) return;
    const int band=bandAtRow();
    if(processor.getBandSelected(band) && processor.getSelectedCount()==1)
    {announce("At least one band must remain selected");return;}
    processor.setBandSelected(band,!processor.getBandSelected(band));
    rememberPosition(); announceCell(); refreshScreen();
}
void LSpectrumEditor::refreshScreen()
{
    selection.setButtonText(selectionMode?"Results (B)":"Bands (B)");
    solo.setButtonText(processor.isSoloOn()?"Solo ON (Enter)":"Solo band (Enter)");
    noise.setButtonText(processor.getNoiseLevel()<=-120.f?"Noise OFF (N)":"Noise ON (N)");
    repaint(); // Also refresh the visible bandpass gain at the top.
}
void LSpectrumEditor::startAnalysis()
{
    processor.startScan();
    lastSecond=-1;lastResultStatus=false;
    waitingAnnounced=false;captureBegan=false;errorAnnounced=false;
    previousCaptured=0;
    scanRequestedMs=juce::Time::getMillisecondCounter();
    status.setText("Preparing new stereo scan...", juce::dontSendNotification);
    announce("Analyzing: " + juce::String(processor.getDuration()) + " seconds");
    refreshScreen();
}
void LSpectrumEditor::changeDuration()
{
    // C focuses the existing inline edit control; NVDA speaks its own
    // accessible name ("countdown") and value. No explicit announcements.
    countdownInput.setText(juce::String(processor.getDuration()), false);
    countdownInput.grabKeyboardFocus();
    countdownInput.selectAll();
}
void LSpectrumEditor::saveCountdown()
{
    const auto value=countdownInput.getText().trim();
    // Use a strict range check. The plugin's duration is always 1..600 s.
    const int n=value.getIntValue();
    if(value.isEmpty() || !value.containsOnly("0123456789") || n < 1 || n > 600)
    {
        countdownInput.grabKeyboardFocus();
        countdownInput.selectAll();
        announce("Countdown must be between 1 and 600 seconds");
        return;
    }
    processor.setDuration(n);
    countdownInput.setText(juce::String(processor.getDuration()), false);
    status.setText("Countdown: " + juce::String(processor.getDuration()) + " seconds", juce::dontSendNotification);
    grabKeyboardFocus();
}
void LSpectrumEditor::toggleSolo()
{
    processor.setSolo(!processor.isSoloOn());
    updatePreview(); refreshScreen();
    announce(processor.isSoloOn()?"Bandpass solo on, "+currentBandLabel():"Bandpass solo off");
}
void LSpectrumEditor::toggleNoise()
{
    const float now=processor.getNoiseLevel();
    if(now<=-120.f) processor.setNoiseLevel(previousNoise);
    else {previousNoise=now;processor.setNoiseLevel(-120.f);}
    refreshScreen();
    if(processor.getNoiseLevel()<=-120.f) announce("Noise off");
    else announce("Noise on, "+juce::String(processor.getNoiseLevel(),0)+" dB");
}
void LSpectrumEditor::copy(const juce::String& text,const juce::String& result)
{
    juce::SystemClipboard::copyTextToClipboard(text);
    announce(result);
}
void LSpectrumEditor::timerCallback()
{
    // First-open/re-open NVDA entry: only try after JUCE's peer and keyboard
    // focus are established, and retry silently if the native UIA provider
    // has not been created yet. No introduction or help text is ever spoken.
    if (initialLocationPending && processor.needsFirstEditorCellAnnouncement()
        && isShowing() && getPeer() != nullptr)
    {
        const auto elapsed = juce::Time::getMillisecondCounter() - initialFocusRequestedMs;
        // Try one delayed focus acquisition only if REAPER has not yet given
        // keyboard focus to our editor. Never steal it from an input/button.
        if (elapsed >= 200 && !initialFocusRetryDone)
        {
            initialFocusRetryDone = true;
            if (!hasKeyboardFocus(true)) grabKeyboardFocus();
        }
        if (elapsed >= 250 && hasKeyboardFocus(true)
            && !countdownInput.hasKeyboardFocus(true))
            announceCell();
    }
    const int state=processor.getScanState();
    if (processor.hasScanError())
    {
        if (!errorAnnounced)
        {
            errorAnnounced=true;
            status.setText("Analysis stopped: audio FIFO overrun",juce::dontSendNotification);
            announce("Analysis error: FFT thread could not keep up with incoming audio. Please restart R.");
        }
        return;
    }
    if (processor.isScanning())
    {
        const uint64_t captured=processor.getCapturedSamples();
        const auto now=juce::Time::getMillisecondCounter();
        if (captured==0 && state!=3)
        {
            status.setText("Waiting for incoming audio from REAPER...",juce::dontSendNotification);
            if(!waitingAnnounced && now-scanRequestedMs>1500)
            {
                waitingAnnounced=true;
                announce("Waiting for REAPER audio. Press Space to play.");
            }
        }
        else if(state==3)
        {
            status.setText("Audio captured. Finishing FFT...",juce::dontSendNotification);
        }
        else
        {
            if(!captureBegan)
            {
                captureBegan=true;
                // Countdown below is the only capture announcement.
            }
            const int seconds=processor.getRemainingSeconds();
            status.setText("Analyzing stereo, "+juce::String(seconds)
                            +" seconds remaining; samples "
                            +juce::String((juce::int64)captured),juce::dontSendNotification);
            if(seconds!=lastSecond) {lastSecond=seconds;announce(juce::String(seconds));}
        }
        previousCaptured=captured;
    }
    else if(processor.hasResult() && !lastResultStatus)
    {
        lastResultStatus=true;
        const bool good=processor.hadSignal();
        status.setText(good ? "Analysis complete. Stereo result ready."
                            : "Analysis complete: silence or no significant audio.",
                       juce::dontSendNotification);
        updatePreview();refreshScreen();
        announce((good ? "Analysis complete. " : "Analysis complete, silence. ") + cellText());
    }
}

void LSpectrumEditor::requestClose()
{
    // The plugin editor is owned by REAPER, so we must ask the HOST window
    // to close. Never delete the AudioProcessorEditor ourselves.
    auto safe=juce::Component::SafePointer<LSpectrumEditor>(this);
    juce::MessageManager::callAsync([safe] { if (safe) safe->closeHostWindow(); });
}

void LSpectrumEditor::closeHostWindow()
{
#if JUCE_WINDOWS
    if (auto* peer=getPeer())
    {
        const auto native=reinterpret_cast<HWND>(peer->getNativeHandle());
        if (native!=nullptr && IsWindow(native))
        {
            // Only close a recognisable floating FX window. Posting WM_CLOSE
            // to REAPER's main window would terminate the whole application!
            const auto root=GetAncestor(native,GA_ROOT);
            if (root!=nullptr && root!=native)
            {
                wchar_t title[512]{};
                GetWindowTextW(root,title,512);
                const juce::String name(title);
                const bool isFloatingFx=name.containsIgnoreCase("FX:")
                    || name.containsIgnoreCase("LSpectrum")
                    || name.containsIgnoreCase("VST3:");
                wchar_t windowClass[256]{};
                GetClassNameW(root, windowClass, 256);
                const juce::String className(windowClass);
                const bool isMainReaper=className.equalsIgnoreCase("REAPERwnd")
                    || name.containsIgnoreCase(" - REAPER")
                    || name.startsWithIgnoreCase("REAPER ");
                if (isFloatingFx && !isMainReaper)
                {
                    PostMessageW(root,WM_CLOSE,0,0);
                    return;
                }
            }
            // For an embedded/docked FX chain, hand Esc back to REAPER.
            // VST3 itself has no cross-host editor-close API.
            if (auto* parent=GetParent(native))
            {
                SetFocus(parent);
                PostMessageW(parent,WM_KEYDOWN,VK_ESCAPE,0);
                PostMessageW(parent,WM_KEYUP,VK_ESCAPE,0);
                return;
            }
        }
    }
#endif
    announce("REAPER owns this window. Close the embedded FX panel using REAPER's command.");
}

bool LSpectrumEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return handleKeyPress(key);
}
bool LSpectrumEditor::keyPressed(const juce::KeyPress& key)
{
    return handleKeyPress(key);
}

bool LSpectrumEditor::handleKeyPress(const juce::KeyPress& key)
{
    const int k=key.getKeyCode();
    const auto mods=key.getModifiers();
    // Never treat arrows typed in the countdown field as table shortcuts.
    // Escape is the ONLY key that closes the plugin, from every focus context.
    if(k==juce::KeyPress::escapeKey) {requestClose();return true;}
    if(countdownInput.hasKeyboardFocus(true) || countdownSave.hasKeyboardFocus(true)) return false;
    // REAPER transport is handled by action IDs on the GUI thread, not by
    // sending synthetic keypresses or touching the audio callback.
    if (k==' ' && mods.isCtrlDown())
    {
        sendReaperTransportCommand(true);
        return true;
    }
    if (k==' ' && mods.isShiftDown())
    {
        if (selectionMode) toggleCurrentBand();
        else announce("Press B to select bands");
        return true;
    }
    if (k==' ')
    {
        sendReaperTransportCommand(false);
        return true;
    }
    if(mods.isCtrlDown() && (k=='c'||k=='C'))
    {
        if(processor.hasResult()) copy(processor.reportText(),"Full report copied");
        else announce("No result to copy");
        return true;
    }
    // Dedicated bandpass audition gain; Ctrl+arrows must never move the cursor.
    if (mods.isCtrlDown() && !mods.isAltDown() && !mods.isShiftDown()
        && (k == juce::KeyPress::upKey || k == juce::KeyPress::downKey))
    {
        const float oldDb = processor.getSoloGainDb();
        const float wanted = oldDb + (k == juce::KeyPress::upKey ? 1.f : -1.f);
        const float newDb = juce::jlimit(LSpectrumProcessor::soloGainMinimumDb,
                                        LSpectrumProcessor::soloGainMaximumDb, wanted);
        if (newDb != oldDb) processor.setSoloGainDb(newDb);
        announce("Bandpass level " + juce::String(newDb >= 0.f ? "+" : "")
                 + juce::String(newDb, 0) + " dB");
        refreshScreen();
        return true;
    }
    if(k==juce::KeyPress::upKey){navigate(-1,0);return true;}
    if(k==juce::KeyPress::downKey){navigate(1,0);return true;}
    if(k==juce::KeyPress::leftKey){navigate(0,-1);return true;}
    if(k==juce::KeyPress::rightKey){navigate(0,1);return true;}
    if(k==juce::KeyPress::homeKey){navigate(-row,0);return true;}
    if(k==juce::KeyPress::endKey){navigate(totalRows()-1-row,0);return true;}
    if(k==juce::KeyPress::pageUpKey){navigate(-8,0);return true;}
    if(k==juce::KeyPress::pageDownKey){navigate(8,0);return true;}
    if(k=='r'||k=='R'){startAnalysis();return true;}
    if((k=='c'||k=='C') && !mods.isCtrlDown() && !mods.isAltDown()){changeDuration();return true;}
    if(k=='b'||k=='B'){toggleSelectionMode();return true;}
    if(k=='n'||k=='N'){toggleNoise();return true;}
    if(k==juce::KeyPress::returnKey){toggleSolo();return true;}
    if(k=='x'||k=='X')
    {
        if(selectionMode)
        {
            processor.selectOnlyBand(bandAtRow());
            rememberPosition();refreshScreen();announce("Only current band selected. "+rowText());
        }
        else announce("Press B to enter band selection mode first");
        return true;
    }
    if(k==juce::KeyPress::deleteKey)
    {
        processor.selectAllBands();
        if(!selectionMode)row=0;
        rememberPosition();refreshScreen();updatePreview();announce("All 32 bands restored");return true;
    }
    if(k=='+'||k=='-')
    {
        const float value=processor.getNoiseLevel()+(k=='+'?5.f:-5.f);
        processor.setNoiseLevel(juce::jlimit(-120.f,0.f,value));
        if(processor.getNoiseLevel()>-120.f)previousNoise=processor.getNoiseLevel();
        refreshScreen();
        announce("Noise level "+juce::String(processor.getNoiseLevel(),0)+" dB");return true;
    }
    if(k=='o'||k=='O'||k=='p'||k=='P'){openReport.triggerClick();return true;}
    if(k=='t'||k=='T'||k=='\\')
    {
        announce("4.5 dB per octave. "+juce::String(processor.getSelectedCount())+" bands active. Duration "+juce::String(processor.getDuration())+" seconds.");
        return true;
    }
    if(k==juce::KeyPress::F1Key || k=='?')
    {
        announce("Arrows navigate the four columns. Control Up and Down: bandpass level, minus 120 to plus 24 dB. Space: REAPER play stop. Control Space: play pause. R: analyze. C: countdown edit. B: band selection. Shift Space: select band. X: only this band. Delete: all bands. Enter: bandpass solo. N: noise. Plus and minus: noise level. 1 to 4: read columns. Control C: copy report. O or P: open report. Escape: close plugin window.");
        return true;
    }
    if(k==juce::KeyPress::tabKey)
    {
        const auto now=juce::Time::getMillisecondCounter();
        tabStreak=(now-lastTab<400)?tabStreak+1:1;
        if(tabStreak>3)tabStreak=1;
        lastTab=now;
        if(tabStreak==1)announceCell();
        else if(tabStreak==2)announce(rowText());
        else {copy(rowText(),"Row copied");tabStreak=0;}
        return true;
    }
    if(k>='1' && k<='4')
    {
        const auto now=juce::Time::getMillisecondCounter();
        const int which=k-'1';
        const auto value=fieldText(which);
        if(which+1==digitLast && now-lastDigit<300)copy(value,"Value copied");
        else announce(value);
        digitLast=which+1;lastDigit=now;return true;
    }
    return false;
}
