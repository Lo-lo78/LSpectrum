#include "PluginEditor.h"
#include "ScreenReaderAnnouncer.h"

LSpectrumEditor::LSpectrumEditor(LSpectrumProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setSize(760,760);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    for(auto* l:{&status,&guide,&tableText}) { addAndMakeVisible(*l); l->setJustificationType(juce::Justification::topLeft); }
    status.setText("LSpectrum 4.5dB Analyzer. Press R to analyze.",juce::dontSendNotification);
    guide.setText("Arrows: navigate rows/columns | B: select bands | Space: toggle | Delete: all | Enter: tone | R: analyze | D: duration | +/-: noise | 1,2,3: columns | Ctrl+C: copy",juce::dontSendNotification);
    tableText.setJustificationType(juce::Justification::topLeft);
    for(auto* b:{&analyze,&duration,&selection,&noise,&tone,&copyReport}) addAndMakeVisible(*b);
    analyze.onClick=[this] { processor.startScan(); lastSecond=-1; lastResultStatus=false; announce("Analysis started. " +juce::String(processor.getDuration())+" seconds"); refreshText(); grabKeyboardFocus(); };
    duration.onClick=[this] { auto* dlg=new juce::AlertWindow("Analysis duration","Seconds (1 to 600)",juce::MessageBoxIconType::QuestionIcon); dlg->addTextEditor("seconds",juce::String(processor.getDuration()),"Seconds"); dlg->addButton("Save",1); dlg->addButton("Cancel",0); dlg->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<LSpectrumEditor>(this),dlg](int result) { if(safe && result==1) { safe->processor.setDuration(dlg->getTextEditorContents("seconds").getIntValue()); safe->announce("Duration "+juce::String(safe->processor.getDuration())+" seconds"); safe->grabKeyboardFocus(); } delete dlg; }),false); };
    selection.onClick=[this] { toggleSelectionMode(); grabKeyboardFocus(); };
    noise.onClick=[this] { auto v=processor.getNoiseLevel(); processor.setNoiseLevel(v<=-120.f ? -60.f : -120.f); announce("Noise level "+juce::String(processor.getNoiseLevel(),0)+" dB"); grabKeyboardFocus(); };
    tone.onClick=[this] { processor.setTone(!processor.isToneOn()); updatePreview(); announce(processor.isToneOn()?"Tone on, "+currentBandLabel():"Tone off"); grabKeyboardFocus(); };
    copyReport.onClick=[this] { copy(processor.reportText(),"Report copied to clipboard"); grabKeyboardFocus(); };
    refreshText();
    startTimerHz(10);
}
void LSpectrumEditor::visibilityChanged() { if(isShowing()) grabKeyboardFocus(); }
void LSpectrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.f));
    g.drawText("LSpectrum 4.5dB Analyzer",12,10,730,30,juce::Justification::left);
}
void LSpectrumEditor::resized()
{
    status.setBounds(12,48,735,35);
    guide.setBounds(12,86,735,64);
    analyze.setBounds(12,155,115,34); duration.setBounds(135,155,115,34);
    selection.setBounds(258,155,145,34); noise.setBounds(410,155,110,34);
    tone.setBounds(528,155,100,34); copyReport.setBounds(636,155,112,34);
    tableText.setBounds(12,200,736,545);
}
void LSpectrumEditor::announce(const juce::String& s) { lspectrum::announceToActiveScreenReader(*this,s,true); }
int LSpectrumEditor::totalRows() const
{
    if(selectionMode) return LSpectrumProcessor::bands;
    return juce::jmax(1,processor.getSelectedCount());
}
int LSpectrumEditor::bandAtRow() const
{
    if(selectionMode) return juce::jlimit(0,LSpectrumProcessor::bands-1,row);
    int i=0;
    for(int b=0;b<LSpectrumProcessor::bands;++b) if(processor.getBandSelected(b))
    {
        if(i++==row) return b;
    }
    return 0;
}
juce::String LSpectrumEditor::currentBandLabel() const
{
    const int b=bandAtRow();
    return juce::String(LSpectrumProcessor::frequency(b),1)+" hertz";
}
juce::String LSpectrumEditor::cellText() const
{
    const int b=bandAtRow();
    if(col==0) return "Band "+juce::String(b+1)+(selectionMode?(processor.getBandSelected(b)?", selected":", unselected"):"");
    if(col==1) return currentBandLabel();
    return selectionMode ? (processor.getBandSelected(b)?"Selected":"Unselected") : juce::String(processor.getResult(b),2)+" dB";
}
juce::String LSpectrumEditor::lineText() const
{
    const int b=bandAtRow();
    return "Band "+juce::String(b+1)+", "+currentBandLabel()+", "+(selectionMode?(processor.getBandSelected(b)?"selected":"unselected"):juce::String(processor.getResult(b),2)+" dB");
}
void LSpectrumEditor::announceCell() { announce(cellText()); }
void LSpectrumEditor::navigate(int dr,int dc)
{
    const int oldrow=row,oldcol=col;
    row=juce::jlimit(0,totalRows()-1,row+dr);
    col=juce::jlimit(0,2,col+dc);
    if(oldrow!=row || oldcol!=col) { updatePreview(); announceCell(); refreshText(); }
}
void LSpectrumEditor::updatePreview()
{
    processor.setToneFrequency((float)LSpectrumProcessor::frequency(bandAtRow()));
    // Similar to the Lua: preview level follows the measured deviation, capped at 0 dB.
    processor.setToneLevel(juce::jlimit(-60.f,0.f,-24.f+processor.getResult(bandAtRow())));
}
void LSpectrumEditor::toggleSelectionMode()
{
    const int old=bandAtRow();
    selectionMode=!selectionMode;
    if(selectionMode) row=old;
    else { row=0; for(int b=0,i=0;b<LSpectrumProcessor::bands;++b) if(processor.getBandSelected(b)) { if(b==old) { row=i; break; } ++i; } }
    col=0;
    announce(selectionMode?"Band selection mode":"Result table mode");
    announceCell(); refreshText();
}
void LSpectrumEditor::toggleCurrentBand()
{
    if(!selectionMode) return;
    const int b=bandAtRow();
    if(processor.getBandSelected(b) && processor.getSelectedCount()==1) { announce("At least one band must remain selected"); return; }
    processor.setBandSelected(b,!processor.getBandSelected(b));
    announceCell(); refreshText();
}
void LSpectrumEditor::copy(const juce::String& s,const juce::String& msg)
{
    juce::SystemClipboard::copyTextToClipboard(s); announce(msg);
}
void LSpectrumEditor::refreshText()
{
    juce::String t="Band       Frequency      Difference\n";
    if(selectionMode)
    {
        for(int b=0;b<LSpectrumProcessor::bands;++b) t+=(b==row?"> ":"  ")+juce::String(b+1).paddedLeft(' ',2)+"          "+juce::String(LSpectrumProcessor::frequency(b),1).paddedLeft(' ',8)+" Hz        "+(processor.getBandSelected(b)?"Selected":"Unselected")+"\n";
    }
    else
    {
        int i=0;
        for(int b=0;b<LSpectrumProcessor::bands;++b) if(processor.getBandSelected(b)) t+=(i++==row?"> ":"  ")+juce::String(b+1).paddedLeft(' ',2)+"          "+juce::String(LSpectrumProcessor::frequency(b),1).paddedLeft(' ',8)+" Hz       "+juce::String(processor.getResult(b),2).paddedLeft(' ',8)+" dB\n";
    }
    tableText.setText(t,juce::dontSendNotification);
}
void LSpectrumEditor::timerCallback()
{
    if(processor.isScanning())
    {
        int sec=processor.getRemainingSeconds();
        status.setText("Analyzing: "+juce::String(sec)+" seconds remaining",juce::dontSendNotification);
        if(sec!=lastSecond) { lastSecond=sec; announce(juce::String(sec)); }
    }
    else if(processor.hasResult() && !lastResultStatus)
    {
        lastResultStatus=true;
        status.setText("Analysis complete. "+juce::String(processor.getSelectedCount())+" bands active.",juce::dontSendNotification);
        refreshText();
        announce("Analysis complete. "+juce::String(processor.getSelectedCount())+" bands active. "+lineText());
    }
}
bool LSpectrumEditor::keyPressed(const juce::KeyPress& key)
{
    const int k=key.getKeyCode();
    const auto mods=key.getModifiers();
    if(mods.isCtrlDown() && (k=='c'||k=='C')) { copy(processor.reportText(),"Report copied"); return true; }
    if(k==juce::KeyPress::upKey) { navigate(-1,0); return true; }
    if(k==juce::KeyPress::downKey) { navigate(1,0); return true; }
    if(k==juce::KeyPress::leftKey) { navigate(0,-1); return true; }
    if(k==juce::KeyPress::rightKey) { navigate(0,1); return true; }
    if(k==juce::KeyPress::homeKey) { navigate(-row,0); return true; }
    if(k==juce::KeyPress::endKey) { navigate(totalRows()-1-row,0); return true; }
    if(k==juce::KeyPress::pageUpKey) { navigate(-8,0); return true; }
    if(k==juce::KeyPress::pageDownKey) { navigate(8,0); return true; }
    if(k=='r'||k=='R') { analyze.triggerClick(); return true; }
    if(k=='d'||k=='D') { duration.triggerClick(); return true; }
    if(k=='b'||k=='B') { toggleSelectionMode(); return true; }
    if(k==' ')
    {
        if(selectionMode) toggleCurrentBand();
        else announce("Use REAPER transport to play or stop");
        return true;
    }
    if(k==juce::KeyPress::deleteKey) { processor.selectAllBands(); if(!selectionMode) row=0; announce("All 32 bands restored"); refreshText(); return true; }
    if(k==juce::KeyPress::returnKey) { tone.triggerClick(); return true; }
    if(k=='+' || k=='-') { processor.setNoiseLevel(juce::jlimit(-120.f,0.f,processor.getNoiseLevel()+(k=='+'?5.f:-5.f))); announce("Noise level "+juce::String(processor.getNoiseLevel(),0)+" dB"); return true; }
    if(k=='o'||k=='O') { processor.saveReport(); processor.reportFile().startAsProcess(); announce("Opening last result"); return true; }
    if(k=='t'||k=='T') { announce("4.5 dB per octave reference. "+juce::String(processor.getSelectedCount())+" bands active"); return true; }
    if(k==juce::KeyPress::tabKey)
    {
        const auto now=juce::Time::getMillisecondCounter();
        tabStreak=(now-lastTab<400)?tabStreak+1:1; if(tabStreak>3) tabStreak=1; lastTab=now;
        if(tabStreak==1) announceCell();
        if(tabStreak==2) announce(lineText());
        if(tabStreak==3) { copy(lineText(),"Line copied"); tabStreak=0; }
        return true;
    }
    if(k>='1'&&k<='3')
    {
        const auto now=juce::Time::getMillisecondCounter();
        const int digit=k-'0';
        const auto text=digit==1?"Band "+juce::String(bandAtRow()+1):digit==2?currentBandLabel():cellText();
        if(digit==digitLast && now-lastDigit<300) copy(text,"Copied to clipboard");
        else if(digit==3 && !selectionMode) announce(juce::String(processor.getResult(bandAtRow()),2)+" dB");
        else if(digit==3) announce(processor.getBandSelected(bandAtRow())?"Selected":"Unselected");
        else announce(text);
        digitLast=digit; lastDigit=now; return true;
    }
    if(k==juce::KeyPress::escapeKey) { if(processor.isScanning()) { processor.cancelScan(); announce("Analysis cancelled"); } else if(selectionMode) toggleSelectionMode(); return true; }
    return false;
}
