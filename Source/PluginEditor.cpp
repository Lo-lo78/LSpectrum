#include "PluginEditor.h"
#include "ScreenReaderAnnouncer.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
    constexpr int headerHeight=25, lineHeight=18;
    const char* columnLabels[4]={"Band","Frequency (Hz)","Left diff (dB)","Right diff (dB)"};
}
LSpectrumEditor::LSpectrumEditor(LSpectrumProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(810,830);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setTitle("LSpectrum 4.5dB Analyzer");
    setDescription("Accessible stereo spectrum analyzer. Use arrow keys for the table. F1 for help.");
    for(auto* label : {&status,&guide})
    {
        addAndMakeVisible(*label);
        label->setJustificationType(juce::Justification::centredLeft);
        label->setColour(juce::Label::textColourId,juce::Colours::white);
    }
    guide.setText("Arrows: table   B: choose bands   Space: select   R: analyze   S: solo   Enter: sine   F1: help",juce::dontSendNotification);
    status.setText(p.hasResult() ? "Previous result available. Press R for a new scan."
                                 : "Ready. Start playback in REAPER, then press R to analyze " + juce::String(p.getDuration()) + " seconds.",juce::dontSendNotification);
    lastResultStatus=p.hasResult();
    for(auto* button : {&analyze,&duration,&selection,&solo,&tone,&noise,&copyReport,&openReport,&closeButton})
    {
        addAndMakeVisible(*button);
        button->setMouseClickGrabsKeyboardFocus(false);
        button->setWantsKeyboardFocus(false);
        button->addKeyListener(this);
    }
    analyze.onClick=[this]{startAnalysis();grabKeyboardFocus();};
    duration.onClick=[this]{changeDuration();};
    selection.onClick=[this]{toggleSelectionMode();grabKeyboardFocus();};
    solo.onClick=[this]{toggleSolo();grabKeyboardFocus();};
    tone.onClick=[this]{toggleTone();grabKeyboardFocus();};
    noise.onClick=[this]{toggleNoise();grabKeyboardFocus();};
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
    guide.addKeyListener(this);
    updatePreview(); refreshScreen(); startTimerHz(8);
}
void LSpectrumEditor::visibilityChanged()
{
    if(isShowing()) grabKeyboardFocus();
}
void LSpectrumEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff121820));
    graphics.setColour(juce::Colours::white);
    graphics.setFont(juce::FontOptions(22.f));
    graphics.drawText("LSpectrum 4.5dB Analyzer",12,8,786,32,juce::Justification::centredLeft);
    graphics.setFont(juce::FontOptions(14.f));
    graphics.setColour(juce::Colour(0xffc0cbd5));
    graphics.drawText("32 log bands | 4.5 dB/oct reference | Stereo input/output",12,39,786,22,juce::Justification::centredLeft);
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
    status.setBounds(12,68,786,27);
    guide.setBounds(12,100,786,26);
    analyze.setBounds(12,140,137,33);
    duration.setBounds(157,140,137,33);
    selection.setBounds(302,140,137,33);
    solo.setBounds(447,140,137,33);
    tone.setBounds(592,140,206,33);
    noise.setBounds(12,178,137,31);
    copyReport.setBounds(157,178,165,31);
    openReport.setBounds(330,178,165,31);
    closeButton.setBounds(503,178,160,31);
    tableRect=juce::Rectangle<int>(12,219,786,headerHeight+LSpectrumProcessor::bands*lineHeight+2);
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
    updatePreview(); refreshScreen(); grabKeyboardFocus(); announceCell();
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
    return juce::String(column==2?"Left, ":"Right, ")+juce::String(processor.getResult(band,column-2),2)+" dB difference";
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
void LSpectrumEditor::announceCell() {announce(cellText());}
void LSpectrumEditor::navigate(int dRow,int dCol)
{
    const int oldRow=row,oldCol=col;
    row=juce::jlimit(0,totalRows()-1,row+dRow);
    col=juce::jlimit(0,3,col+dCol);
    if(oldRow!=row || oldCol!=col)
    {
        updatePreview(); refreshScreen(); announceCell();
    }
}
void LSpectrumEditor::updatePreview()
{
    const int band=bandAtRow();
    const auto frequency=(float)LSpectrumProcessor::frequency(band);
    processor.setToneFrequency(frequency);
    processor.setSoloFrequency(frequency);
    processor.setToneStereoBand(band);
    const float dev=.5f*(processor.getResult(band,0)+processor.getResult(band,1));
    processor.setToneLevel(juce::jlimit(-60.f,-18.f,-24.f+dev));
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
    refreshScreen(); updatePreview();
    announce(selectionMode?"Band selection mode. Space to toggle band":"Result table mode");
    announceCell();
}
void LSpectrumEditor::toggleCurrentBand()
{
    if(!selectionMode) return;
    const int band=bandAtRow();
    if(processor.getBandSelected(band) && processor.getSelectedCount()==1)
    {announce("At least one band must remain selected");return;}
    processor.setBandSelected(band,!processor.getBandSelected(band));
    announceCell(); refreshScreen();
}
void LSpectrumEditor::refreshScreen()
{
    selection.setButtonText(selectionMode?"Results (B)":"Bands (B)");
    solo.setButtonText(processor.isSoloOn()?"Solo ON (S)":"Solo band (S)");
    tone.setButtonText(processor.isToneOn()?"Sine ON (Enter)":"Sine (Enter)");
    noise.setButtonText(processor.getNoiseLevel()<=-120.f?"Noise OFF (N)":"Noise ON (N)");
    repaint(tableRect);
}
void LSpectrumEditor::startAnalysis()
{
    processor.startScan();
    lastSecond=-1;lastResultStatus=false;
    waitingAnnounced=false;captureBegan=false;errorAnnounced=false;
    previousCaptured=0;
    scanRequestedMs=juce::Time::getMillisecondCounter();
    status.setText("Preparing new stereo scan...", juce::dontSendNotification);
    announce("New stereo scan requested. " + juce::String(processor.getDuration())
             + " seconds. The VST analyzes incoming audio; start playback in REAPER if stopped.");
    refreshScreen();
}
void LSpectrumEditor::changeDuration()
{
    auto* dialog=new juce::AlertWindow("Analysis duration","Seconds (1 to 600)",juce::MessageBoxIconType::QuestionIcon);
    dialog->addTextEditor("seconds",juce::String(processor.getDuration()),"Seconds");
    dialog->addButton("Save",1);
    dialog->addButton("Cancel",0);
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<LSpectrumEditor>(this),dialog](int result)
        {
            if(safe && result==1)
            {
                const auto input=dialog->getTextEditorContents("seconds").trim();
                if(input.containsOnly("0123456789") && input.isNotEmpty())
                {
                    safe->processor.setDuration(input.getIntValue());
                    safe->announce("Duration: "+juce::String(safe->processor.getDuration())+" seconds");
                }
                else safe->announce("Invalid duration. No change made.");
            }
            delete dialog;
            if(safe) safe->grabKeyboardFocus();
        }),false);
}
void LSpectrumEditor::toggleTone()
{
    processor.setTone(!processor.isToneOn());
    updatePreview(); refreshScreen();
    announce(processor.isToneOn()?"Sine on, "+currentBandLabel():"Sine off");
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
                announce("Waiting for incoming audio. Start playback in REAPER, or check the input routing.");
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
                announce("Audio capture active");
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
        announce((good ? "Stereo analysis complete. "
                        : "Analysis complete, but input was silent. ")
                 +juce::String(processor.getSelectedCount())+" selected bands. "+rowText());
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
    // Esc never cancels a measurement or returns to selection mode: always EXIT.
    if(k==juce::KeyPress::escapeKey) {requestClose();return true;}
    if(mods.isCtrlDown() && (k=='c'||k=='C'))
    {
        if(processor.hasResult()) copy(processor.reportText(),"Full report copied");
        else announce("No result to copy");
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
    if(k=='d'||k=='D'){changeDuration();return true;}
    if(k=='b'||k=='B'){toggleSelectionMode();return true;}
    if(k=='s'||k=='S'){toggleSolo();return true;}
    if(k=='n'||k=='N'){toggleNoise();return true;}
    if(k==juce::KeyPress::returnKey){toggleTone();return true;}
    if(k==' ')
    {
        if(selectionMode)toggleCurrentBand();
        else announce("Press B to choose bands. Start and stop playback with REAPER transport.");
        return true;
    }
    if(k=='x'||k=='X')
    {
        if(selectionMode)
        {
            processor.selectOnlyBand(bandAtRow());
            refreshScreen();announce("Only current band selected. "+rowText());
        }
        else announce("Press B to enter band selection mode first");
        return true;
    }
    if(k==juce::KeyPress::deleteKey)
    {
        processor.selectAllBands();
        if(!selectionMode)row=0;
        refreshScreen();updatePreview();announce("All 32 bands restored");return true;
    }
    if(k=='+'||k=='-')
    {
        const float value=processor.getNoiseLevel()+(k=='+'?5.f:-5.f);
        processor.setNoiseLevel(juce::jlimit(-120.f,0.f,value));
        if(processor.getNoiseLevel()>-120.f)previousNoise=processor.getNoiseLevel();
        refreshScreen();
        announce("Noise level "+juce::String(processor.getNoiseLevel(),0)+" dB");return true;
    }
    if(k=='o'||k=='O'){openReport.triggerClick();return true;}
    if(k=='t'||k=='T')
    {
        announce("4.5 dB per octave. "+juce::String(processor.getSelectedCount())+" bands active. Duration "+juce::String(processor.getDuration())+" seconds.");
        return true;
    }
    if(k==juce::KeyPress::F1Key || k=='?')
    {
        announce("Arrows move through rows and four columns. R scans. D sets seconds. B edits band selection, Space toggles, X selects only the current band. Delete restores all. S solos the current band. Enter toggles sine. N toggles noise, plus and minus adjust its level. Numbers 1 to 4 read columns. Control C copies the report. Escape closes the plugin editor.");
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
