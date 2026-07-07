#include "PluginEditor.h"
using namespace bzzz;

static const char* kNames[12] = { "KICK","RUMBLE","SNARE","CLAP","CHH","OHH","RIDE","PERC","TOM","ACID","STAB","RISE" };
static const float kHues[12]  = { 355, 325, 30, 50, 90, 130, 170, 200, 230, 75, 265, 300 };
static const char* kStyleUp[6] = { "BERLIN","DETROIT","LONDON","PARIS","ROTTERDAM","TBILISI" };
static const int   kScale[7] = { 0, 3, 5, 7, 10, 12, 15 };   // rangees du roll (bas -> haut inverse)

static juce::Colour hueC (float h, float s, float l, float a = 1.0f)
{ return juce::Colour::fromHSL (h / 360.0f, s, l, a); }

// ------------------- MiniKnob -------------------
void MiniKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight() - 22.0f);
    juce::Rectangle<float> k ((b.getWidth() - d) / 2, 0, d, d);
    g.setColour (juce::Colour (0xff222228));
    g.fillEllipse (k);
    const float p = toP (juce::jlimit (mn, mx, getV()));
    juce::Path arc;
    arc.addCentredArc (k.getCentreX(), k.getCentreY(), d/2 - 2, d/2 - 2,
                       0, -2.4f, -2.4f + 4.8f * p, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (2.5f));
    const float ang = -2.4f + 4.8f * p;
    g.drawLine (k.getCentreX(), k.getCentreY(),
                k.getCentreX() + std::sin (ang) * (d/2 - 5),
                k.getCentreY() - std::cos (ang) * (d/2 - 5), 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText (label, getLocalBounds().removeFromBottom (20), juce::Justification::centredTop);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (juce::String (getV(), 2), getLocalBounds().removeFromBottom (10), juce::Justification::centred);
}

// ------------------- ctor -------------------
BzzzEditor::BzzzEditor (BzzzProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    auto flat = [this] (juce::TextButton& b, const juce::String& txt)
    {
        b.setButtonText (txt);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff17171d));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a2416));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.7f));
        b.setColour (juce::TextButton::textColourOnId, hueC (18, 0.95f, 0.62f));
        addAndMakeVisible (b);
    };

    // ---- header : styles ----
    for (int s = 0; s < 6; ++s)
    { flat (styleBtns[s], kStyleUp[s]);
      styleBtns[s].onClick = [this, s] { proc.applyStyleSeqs (s); repaint(); }; }

    // ---- kits / patches ----
    for (int i = 0; i < kNumKits; ++i) kitBox.addItem (juce::String (kitName (i)), i + 1);
    for (int i = 0; i < kNumPatches; ++i) patchBox.addItem (juce::String (patchName (i)), i + 1);
    kitBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff17171d));
    patchBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff17171d));
    kitBox.onChange = [this] { const int i = kitBox.getSelectedId() - 1; if (i >= 0 && i != proc.curKit.load()) { proc.loadKit (i); refreshInspector(); repaint(); } };
    patchBox.onChange = [this] { const int i = patchBox.getSelectedId() - 1; if (i >= 0 && i != proc.curPatch.load()) { proc.loadPatch (i); repaint(); } };
    addAndMakeVisible (kitBox); addAndMakeVisible (patchBox);
    kitBox.setSelectedId (1, juce::dontSendNotification);
    patchBox.setSelectedId (1, juce::dontSendNotification);

    auto nav = [this, &flat] (juce::TextButton& b, const juce::String& t, std::function<void()> fn)
    { flat (b, t); b.onClick = std::move (fn); };
    nav (kitPrev, "<",  [this]{ proc.loadKit (proc.curKit.load() - 1); kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification); refreshInspector(); repaint(); });
    nav (kitNext, ">",  [this]{ proc.loadKit (proc.curKit.load() + 1); kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification); refreshInspector(); repaint(); });
    nav (kitRnd,  "?",  [this]{ proc.loadKit (juce::Random::getSystemRandom().nextInt (kNumKits)); kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification); refreshInspector(); repaint(); });
    nav (patPrev, "<",  [this]{ proc.loadPatch (proc.curPatch.load() - 1); patchBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification); repaint(); });
    nav (patNext, ">",  [this]{ proc.loadPatch (proc.curPatch.load() + 1); patchBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification); repaint(); });
    nav (patRnd,  "?",  [this]{ proc.loadPatch (juce::Random::getSystemRandom().nextInt (kNumPatches)); patchBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification); repaint(); });

    // ---- pads + mutes ----
    for (int i = 0; i < 12; ++i)
    {
        flat (pads[i], kNames[i]);
        pads[i].setColour (juce::TextButton::textColourOffId, hueC (kHues[i], 0.9f, 0.62f));
        pads[i].onClick = [this, i] { proc.previewPad (i); selectTrack (i); };
        flat (mutes[i], "M");
        mutes[i].setClickingTogglesState (true);
        mutes[i].onClick = [this, i] { proc.trackMute[i].store (mutes[i].getToggleState() ? 1 : 0); };
    }

    // ---- inspecteur ----
    auto knob = [this] (MiniKnob& k, const juce::String& lb, float mn, float mx, bool lg,
                        std::function<float()> gv, std::function<void(float)> sv)
    { k.label = lb; k.mn = mn; k.mx = mx; k.logScale = lg; k.getV = std::move (gv); k.setV = std::move (sv);
      addAndMakeVisible (k); };

    auto CH = [this]() -> bzzz::ChannelCfg& { return proc.chCfg[selTrack]; };
    knob (ckCut, "CUTOFF", 60, 16000, true, [CH]{ return CH().cutoff; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().cutoff=v; });
    knob (ckRes, "RESO", .3f, 14, false, [CH]{ return CH().res; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().res=v; });
    knob (ckDrv, "DRIVE", 0, 1, false, [CH]{ return CH().drive; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().drive=v; });
    knob (ckCr,  "CRUSH", 0, 1, false, [CH]{ return CH().crush; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().crush=v; });
    knob (ckDe,  "DECAY", .3f, 2.5f, false, [CH]{ return CH().decay; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().decay=v; });
    knob (ckPi,  "PITCH", -12, 12, false, [CH]{ return CH().pitch; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().pitch=v; });
    knob (ckPa,  "PAN", -1, 1, false, [CH]{ return CH().pan; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().pan=v; });
    knob (ckSD,  "SEND DLY", 0, 1, false, [CH]{ return CH().sendD; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().sendD=v; });
    knob (ckSR,  "SEND REV", 0, 1, false, [CH]{ return CH().sendR; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().sendR=v; });
    knob (ckVo,  "VOLUME", 0, 1, false, [CH]{ return CH().vol; }, [this,CH](float v){ const juce::ScopedLock l(proc.cfgLock); CH().vol=v; });

    static const char* ftn[6] = { "OFF","LP","HP","BP","NOTCH","PEAK" };
    for (int i = 0; i < 6; ++i)
    { flat (fTypes[i], ftn[i]);
      fTypes[i].onClick = [this, i, CH]
      { const juce::ScopedLock l (proc.cfgLock);
        if (i == 0) CH().fon = false;
        else { CH().fon = true; CH().ftype = i - 1; }
        refreshInspector(); }; }

    // ---- patterns / song ----
    for (int i = 0; i < BzzzProcessor::kSlots; ++i)
    { flat (slotBtns[i], juce::String::charToString ((juce::juce_wchar)('A' + i)));
      slotBtns[i].onClick = [this, i]
      { if (copyArm) { proc.copySlotTo (i); copyArm = false; copyBtn.setToggleState (false, juce::dontSendNotification); }
        else proc.gotoSlot (i);
        repaint(); }; }
    flat (copyBtn, "COPIER >"); copyBtn.setClickingTogglesState (true);
    copyBtn.onClick = [this]{ copyArm = copyBtn.getToggleState(); };
    flat (stylePatBtn, "PATTERN DU STYLE"); stylePatBtn.onClick = [this]{ proc.applyStyleSeqs (proc.styleIdx.load()); repaint(); };
    flat (rndPatBtn, "ALEATOIRE"); rndPatBtn.onClick = [this]{ proc.randomPattern(); repaint(); };
    flat (clrPatBtn, "EFFACER"); clrPatBtn.onClick = [this]{ proc.clearPattern(); repaint(); };

    flat (songOnBtn, "MODE SONG : OFF"); songOnBtn.setClickingTogglesState (true);
    songOnBtn.onClick = [this]
    { proc.songOn.store (songOnBtn.getToggleState() ? 1 : 0);
      songOnBtn.setButtonText (songOnBtn.getToggleState() ? "MODE SONG : ON" : "MODE SONG : OFF"); };
    flat (songAddBtn, "+ AJOUTER PATTERN");
    songAddBtn.onClick = [this]
    { const int n = proc.songLen.load();
      if (n < BzzzProcessor::kMaxSong)
      { proc.songSlot[n].store (proc.curSlot.load()); proc.songReps[n].store (1);
        proc.songLen.store (n + 1); rebuildSongUI(); } };
    flat (songClrBtn, "VIDER");
    songClrBtn.onClick = [this]{ proc.songLen.store (0); proc.songPos.store (0); rebuildSongUI(); };

    // ---- master + synth macros (parametres) ----
    auto rot = [this] (juce::Slider& s, const char* pid, float hueV)
    { s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
      s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 14);
      s.setColour (juce::Slider::rotarySliderFillColourId, hueC (hueV, 0.95f, 0.55f));
      s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2a30));
      s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.8f));
      s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
      addAndMakeVisible (s);
      atts.push_back (std::make_unique<Att> (proc.apvts, pid, s)); };
    rot (mCut, "cutoff", 18); rot (mRes, "res", 18); rot (mDrv, "drive", 18); rot (mVol, "volume", 18);
    rot (mDly, "dlymix", 195); rot (mRev, "revmix", 195); rot (mSC, "sc", 340);
    swing.setSliderStyle (juce::Slider::LinearHorizontal);
    swing.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    swing.setColour (juce::Slider::trackColourId, hueC (18, 0.8f, 0.45f));
    addAndMakeVisible (swing);
    atts.push_back (std::make_unique<Att> (proc.apvts, "swing", swing));

    rot (sCut, "scut", 315); rot (sRes, "sres", 315); rot (sEnv, "senv", 315);
    rot (sLfoR, "slfor", 315); rot (sLfoA, "slfoa", 315); rot (sVol, "svol", 315);

    // ---- synth internes ----
    auto SY = [this]() -> bzzz::SynCfg& { return proc.synCfg; };
    auto sk = [this, &knob] (MiniKnob& k, const juce::String& lb, float mn, float mx, bool lg,
                             std::function<float()> gv, std::function<void(float)> sv)
    { knob (k, lb, mn, mx, lg, std::move (gv), std::move (sv)); k.accent = hueC (315, 0.9f, 0.6f); };
    sk (skMix, "MIX", 0, 1, false, [SY]{ return SY().mix; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().mix=v; });
    sk (skDet, "DETUNE", 0, 40, false, [SY]{ return SY().detune; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().detune=v; });
    sk (skUni, "UNISON", 1, 7, false, [SY]{ return (float) SY().unison; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().unison=(int)std::round(v); });
    sk (skO2P, "OSC2 ST", -24, 24, false, [SY]{ return (float) SY().osc2Pitch; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().osc2Pitch=(int)std::round(v); });
    sk (skFM, "FM", 0, 1, false, [SY]{ return SY().fm; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().fm=v; });
    sk (skSub, "SUB", 0, 1, false, [SY]{ return SY().sub; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().sub=v; });
    sk (skNz, "NOISE", 0, .5f, false, [SY]{ return SY().noise; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().noise=v; });
    sk (skGl, "GLIDE", 0, .2f, false, [SY]{ return SY().glide; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().glide=v; });
    sk (skFDec, "F.DECAY", .02f, 1, false, [SY]{ return SY().fDec; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().fDec=v; });
    sk (skDrv2, "DRIVE", 0, 1, false, [SY]{ return SY().drive; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().drive=v; });
    sk (skCr2, "CRUSH", 0, 1, false, [SY]{ return SY().crush; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().crush=v; });
    sk (skLfoP, "LFO>PITCH", 0, 1, false, [SY]{ return SY().lfoPitch; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().lfoPitch=v; });
    sk (skA, "ATT", .001f, .6f, false, [SY]{ return SY().aA; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().aA=v; });
    sk (skD, "DEC", .02f, 1.2f, false, [SY]{ return SY().aD; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().aD=v; });
    sk (skS, "SUS", 0, 1, false, [SY]{ return SY().aS; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().aS=v; });
    sk (skR, "REL", .02f, 2, false, [SY]{ return SY().aR; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().aR=v; });
    sk (skW, "WIDTH", 0, 1, false, [SY]{ return SY().width; }, [this,SY](float v){ const juce::ScopedLock l(proc.cfgLock); SY().width=v; });

    static const char* wf[4] = { "SAW","SQR","SIN","TRI" };
    for (int i = 0; i < 4; ++i)
    { flat (osc1Btns[i], wf[i]); osc1Btns[i].onClick = [this, i, SY]{ const juce::ScopedLock l(proc.cfgLock); SY().osc1=i; repaint(); };
      flat (osc2Btns[i], wf[i]); osc2Btns[i].onClick = [this, i, SY]{ const juce::ScopedLock l(proc.cfgLock); SY().osc2=i; repaint(); }; }
    static const char* fm2[6] = { "LP24","LP12","HP","BP","NOTCH","FORMANT" };
    for (int i = 0; i < 6; ++i)
    { flat (fmodeBtns[i], fm2[i]); fmodeBtns[i].onClick = [this, i, SY]{ const juce::ScopedLock l(proc.cfgLock); SY().fmode=i; repaint(); }; }
    static const char* ls[3] = { "SIN","SQR","SAW" };
    for (int i = 0; i < 3; ++i)
    { flat (lfoShapeBtns[i], ls[i]); lfoShapeBtns[i].onClick = [this, i, SY]{ const juce::ScopedLock l(proc.cfgLock); SY().lfoShape=i; repaint(); }; }

    flat (synPwr, "SYNTH ON"); synPwr.setClickingTogglesState (true);
    synPwr.setToggleState (true, juce::dontSendNotification);
    synPwr.onClick = [this]
    { proc.synthPower.store (synPwr.getToggleState() ? 1 : 0);
      synPwr.setButtonText (synPwr.getToggleState() ? "SYNTH ON" : "SYNTH OFF"); };
    flat (octDown, "OCT-"); octDown.onClick = [this]{ proc.synOct.store (juce::jmax (0, proc.synOct.load()-1)); repaint(); };
    flat (octUp, "OCT+");   octUp.onClick = [this]{ proc.synOct.store (juce::jmin (4, proc.synOct.load()+1)); repaint(); };

    selectTrack (0);
    rebuildSongUI();
    setSize (1240, 860);
    startTimerHz (30);
}

void BzzzEditor::selectTrack (int t)
{
    selTrack = juce::jlimit (0, 11, t);
    for (int i = 0; i < 12; ++i)
        pads[i].setColour (juce::TextButton::buttonColourId,
                           i == selTrack ? hueC (kHues[i], 0.6f, 0.22f) : juce::Colour (0xff17171d));
    refreshInspector();
}
void BzzzEditor::refreshInspector()
{
    const auto& c = proc.chCfg[selTrack];
    for (int i = 0; i < 6; ++i)
        fTypes[i].setColour (juce::TextButton::buttonColourId,
            ((i == 0 && !c.fon) || (i > 0 && c.fon && c.ftype == i - 1)) ? juce::Colour (0xff3a2416) : juce::Colour (0xff17171d));
    for (auto* k : { &ckCut,&ckRes,&ckDrv,&ckCr,&ckDe,&ckPi,&ckPa,&ckSD,&ckSR,&ckVo }) k->repaint();
    for (int i = 0; i < 12; ++i)
        mutes[i].setToggleState (proc.trackMute[i].load() != 0, juce::dontSendNotification);
}
void BzzzEditor::rebuildSongUI()
{
    songEntryBtns.clear(); songDelBtns.clear();
    for (int i = 0; i < proc.songLen.load(); ++i)
    {
        auto* b = songEntryBtns.add (new juce::TextButton());
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff17171d));
        b->setButtonText (juce::String::charToString ((juce::juce_wchar)('A' + proc.songSlot[i].load()))
                          + " x" + juce::String (proc.songReps[i].load()));
        b->onClick = [this, i]
        { static const int reps[4] = { 1, 2, 4, 8 };
          int cur = proc.songReps[i].load(), idx = 0;
          for (int j = 0; j < 4; ++j) if (reps[j] == cur) idx = j;
          proc.songReps[i].store (reps[(idx + 1) % 4]); rebuildSongUI(); };
        addAndMakeVisible (b);
        auto* d = songDelBtns.add (new juce::TextButton ("x"));
        d->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a1416));
        d->onClick = [this, i]
        { const int n = proc.songLen.load();
          for (int j = i; j < n - 1; ++j)
          { proc.songSlot[j].store (proc.songSlot[j+1].load()); proc.songReps[j].store (proc.songReps[j+1].load()); }
          proc.songLen.store (n - 1);
          if (proc.songPos.load() >= n - 1) proc.songPos.store (0);
          rebuildSongUI(); };
        addAndMakeVisible (d);
    }
    resized(); repaint();
}

void BzzzEditor::timerCallback()
{
    const int s = proc.playStep.load();
    if (s != uiStep) { uiStep = s; repaint (gridArea.expanded (4)); repaint (rollArea.expanded (4)); }
}

juce::Rectangle<int> BzzzEditor::cell (int t, int s) const
{
    const int cw = gridArea.getWidth() / 16, chh = gridArea.getHeight() / 12;
    return { gridArea.getX() + s * cw + 1, gridArea.getY() + t * chh + 1, cw - 2, chh - 2 };
}
juce::Rectangle<int> BzzzEditor::rollCell (int row, int s) const
{
    const int cw = rollArea.getWidth() / 16, rh = rollArea.getHeight() / 7;
    return { rollArea.getX() + s * cw + 1, rollArea.getY() + row * rh + 1, cw - 2, rh - 2 };
}

void BzzzEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (gridArea.contains (pt))
    {
        const int cw = gridArea.getWidth() / 16, chh = gridArea.getHeight() / 12;
        const int s = (pt.x - gridArea.getX()) / cw, t = (pt.y - gridArea.getY()) / chh;
        if (s >= 0 && s < 16 && t >= 0 && t < 12)
        { auto& c = proc.grid[t][s]; c.store ((c.load() + 1) % 3); repaint (cell (t, s).expanded (2)); }
        return;
    }
    if (rollArea.contains (pt))
    {
        const int cw = rollArea.getWidth() / 16, rh = rollArea.getHeight() / 7;
        const int s = (pt.x - rollArea.getX()) / cw, row = (pt.y - rollArea.getY()) / rh;
        if (s >= 0 && s < 16 && row >= 0 && row < 7)
        { const int note = kScale[6 - row];
          proc.rollNote[s].store (proc.rollNote[s].load() == note ? -100 : note);
          repaint (rollArea); }
        return;
    }
    if (accRow.contains (pt))
    { const int s = (pt.x - accRow.getX()) / (accRow.getWidth() / 16);
      if (s >= 0 && s < 16) { proc.rollAcc[s].store (proc.rollAcc[s].load() ? 0 : 1); repaint (accRow); } return; }
    if (slideRow.contains (pt))
    { const int s = (pt.x - slideRow.getX()) / (slideRow.getWidth() / 16);
      if (s >= 0 && s < 16) { proc.rollSlide[s].store (proc.rollSlide[s].load() ? 0 : 1); repaint (slideRow); } return; }
}

void BzzzEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0f));
    g.setGradientFill (juce::ColourGradient (hueC (18, 0.55f, 0.09f), 0, 0,
                                             juce::Colour (0xff0b0b0f), (float) getWidth(), (float) getHeight(), true));
    g.fillAll();

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText ("B'ZZZ", 20, 12, 96, 26, juce::Justification::left);
    g.setColour (hueC (18, 0.95f, 0.58f));
    g.drawText ("FREQUENCY", 96, 12, 190, 26, juce::Justification::left);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("V2 FULL WORKSTATION - PAYE TA RUCHE - drums + FABLE ENGINE + 2000 presets - sync hote",
                20, 38, 600, 12, juce::Justification::left);

    // surbrillance style courant
    for (int s = 0; s < 6; ++s)
        styleBtns[s].setColour (juce::TextButton::buttonColourId,
            proc.styleIdx.load() == s ? juce::Colour (0xff3a2416) : juce::Colour (0xff17171d));
    for (int i = 0; i < BzzzProcessor::kSlots; ++i)
        slotBtns[i].setColour (juce::TextButton::buttonColourId,
            proc.curSlot.load() == i ? juce::Colour (0xff3a2416) : (copyArm ? juce::Colour (0xff2d2612) : juce::Colour (0xff17171d)));

    // grille drums
    for (int t = 0; t < 12; ++t)
        for (int s = 0; s < 16; ++s)
        {
            auto r = cell (t, s).toFloat();
            const int v = proc.grid[t][s].load();
            juce::Colour c = (s % 4 == 0) ? juce::Colour (0xff1c1c23) : juce::Colour (0xff15151b);
            if (v == 1) c = hueC (kHues[t], 0.85f, 0.40f);
            if (v == 2) c = hueC (kHues[t], 0.95f, 0.62f);
            if (s == uiStep) c = c.brighter (v > 0 ? 0.5f : 0.16f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 2.5f);
        }

    // roll synthe
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("FABLE ENGINE - SEQUENCE (clic: note | ACC | SLIDE)  OCT " + juce::String (proc.synOct.load()),
                rollArea.getX(), rollArea.getY() - 14, 400, 12, juce::Justification::left);
    for (int row = 0; row < 7; ++row)
        for (int s = 0; s < 16; ++s)
        {
            auto r = rollCell (row, s).toFloat();
            const int note = kScale[6 - row];
            const bool on = proc.rollNote[s].load() == note;
            juce::Colour c = (s % 4 == 0) ? juce::Colour (0xff1c1c23) : juce::Colour (0xff15151b);
            if (on) c = hueC (315, 0.9f, proc.rollAcc[s].load() ? 0.68f : 0.5f);
            if (s == uiStep) c = c.brighter (on ? 0.4f : 0.14f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 2.0f);
        }
    // rangees ACC / SLIDE
    for (int s = 0; s < 16; ++s)
    {
        auto ra = accRow.toFloat(); const float cw = ra.getWidth() / 16;
        juce::Rectangle<float> r (ra.getX() + s * cw + 1, ra.getY() + 1, cw - 2, ra.getHeight() - 2);
        g.setColour (proc.rollAcc[s].load() ? juce::Colour (0xffFFD54F) : juce::Colour (0xff15151b));
        g.fillRoundedRectangle (r, 2.0f);
        auto rs = slideRow.toFloat();
        juce::Rectangle<float> r2 (rs.getX() + s * cw + 1, rs.getY() + 1, cw - 2, rs.getHeight() - 2);
        g.setColour (proc.rollSlide[s].load() ? juce::Colour (0xff4FC3F7) : juce::Colour (0xff15151b));
        g.fillRoundedRectangle (r2, 2.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawText ("ACC",   accRow.getX() - 34, accRow.getY(), 30, accRow.getHeight(), juce::Justification::centredRight);
    g.drawText ("SLIDE", slideRow.getX() - 40, slideRow.getY(), 36, slideRow.getHeight(), juce::Justification::centredRight);

    // libelles sections
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (hueC (18, 0.9f, 0.6f));
    g.drawText ("MASTER", mCut.getX(), mCut.getY() - 14, 200, 12, juce::Justification::left);
    g.setColour (hueC (315, 0.9f, 0.62f));
    g.drawText ("SYNTH MACROS (automatisables)", sCut.getX(), sCut.getY() - 14, 260, 12, juce::Justification::left);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawText ("INSPECTEUR : " + juce::String (kNames[selTrack]),
                ckCut.getX(), ckCut.getY() - 14, 300, 12, juce::Justification::left);
    g.drawText ("SONG (clic entree = reps x1 x2 x4 x8)", songOnBtn.getX(), songOnBtn.getY() - 14, 300, 12, juce::Justification::left);
}

void BzzzEditor::resized()
{
    auto a = getLocalBounds().reduced (18);
    a.removeFromTop (40);

    // ligne styles + synth pwr
    auto row1 = a.removeFromTop (26);
    for (auto& b : styleBtns) b.setBounds (row1.removeFromLeft (86).reduced (2));
    row1.removeFromLeft (8);
    synPwr.setBounds (row1.removeFromLeft (86).reduced (2));
    octDown.setBounds (row1.removeFromLeft (46).reduced (2));
    octUp.setBounds (row1.removeFromLeft (46).reduced (2));
    swing.setBounds (row1.removeFromLeft (200).reduced (2));

    a.removeFromTop (4);
    // ligne kits / patches
    auto row2 = a.removeFromTop (26);
    kitPrev.setBounds (row2.removeFromLeft (26).reduced (1));
    kitBox.setBounds (row2.removeFromLeft (230).reduced (1));
    kitNext.setBounds (row2.removeFromLeft (26).reduced (1));
    kitRnd.setBounds (row2.removeFromLeft (26).reduced (1));
    row2.removeFromLeft (14);
    patPrev.setBounds (row2.removeFromLeft (26).reduced (1));
    patchBox.setBounds (row2.removeFromLeft (230).reduced (1));
    patNext.setBounds (row2.removeFromLeft (26).reduced (1));
    patRnd.setBounds (row2.removeFromLeft (26).reduced (1));

    a.removeFromTop (6);
    // pads + mutes
    auto padsRow = a.removeFromTop (30);
    for (int i = 0; i < 12; ++i)
    {
        auto pr = padsRow.removeFromLeft (padsRow.getWidth() / (12 - i));
        pads[i].setBounds (pr.removeFromLeft (pr.getWidth() - 24).reduced (1));
        mutes[i].setBounds (pr.reduced (1));
    }

    a.removeFromTop (6);
    // inspecteur : filtres + 10 knobs
    auto insp = a.removeFromTop (86);
    auto ftr = insp.removeFromLeft (74);
    for (int i = 0; i < 6; ++i) fTypes[i].setBounds (ftr.removeFromTop (insp.getHeight() / 6).reduced (1));
    MiniKnob* iks[10] = { &ckCut,&ckRes,&ckDrv,&ckCr,&ckDe,&ckPi,&ckPa,&ckSD,&ckSR,&ckVo };
    const int kw = insp.getWidth() / 10;
    for (auto* k : iks) k->setBounds (insp.removeFromLeft (kw).reduced (4));

    a.removeFromTop (14);
    // patterns A-H + actions
    auto row3 = a.removeFromTop (24);
    for (auto& b : slotBtns) b.setBounds (row3.removeFromLeft (30).reduced (1));
    row3.removeFromLeft (8);
    copyBtn.setBounds (row3.removeFromLeft (80).reduced (1));
    stylePatBtn.setBounds (row3.removeFromLeft (130).reduced (1));
    rndPatBtn.setBounds (row3.removeFromLeft (90).reduced (1));
    clrPatBtn.setBounds (row3.removeFromLeft (80).reduced (1));

    a.removeFromTop (4);
    gridArea = a.removeFromTop (232);

    a.removeFromTop (8);
    // song
    auto row4 = a.removeFromTop (24);
    songOnBtn.setBounds (row4.removeFromLeft (128).reduced (1));
    songAddBtn.setBounds (row4.removeFromLeft (140).reduced (1));
    songClrBtn.setBounds (row4.removeFromLeft (66).reduced (1));
    row4.removeFromLeft (8);
    for (int i = 0; i < songEntryBtns.size(); ++i)
    {
        songEntryBtns[i]->setBounds (row4.removeFromLeft (52).reduced (1));
        songDelBtns[i]->setBounds (row4.removeFromLeft (20).reduced (1));
    }

    a.removeFromTop (16);
    // roll synthe + acc/slide
    rollArea = a.removeFromTop (112).withTrimmedLeft (46);
    accRow   = a.removeFromTop (16).withTrimmedLeft (46);
    slideRow = a.removeFromTop (16).withTrimmedLeft (46);

    a.removeFromTop (8);
    // synth : formes + fmodes + lfo shapes
    auto row5 = a.removeFromTop (22);
    for (auto& b : osc1Btns) b.setBounds (row5.removeFromLeft (44).reduced (1));
    row5.removeFromLeft (6);
    for (auto& b : osc2Btns) b.setBounds (row5.removeFromLeft (44).reduced (1));
    row5.removeFromLeft (10);
    for (auto& b : fmodeBtns) b.setBounds (row5.removeFromLeft (58).reduced (1));
    row5.removeFromLeft (10);
    for (auto& b : lfoShapeBtns) b.setBounds (row5.removeFromLeft (40).reduced (1));

    a.removeFromTop (4);
    // synth internes : 17 mini-knobs
    auto skr = a.removeFromTop (78);
    MiniKnob* sks[17] = { &skMix,&skDet,&skUni,&skO2P,&skFM,&skSub,&skNz,&skGl,&skFDec,&skDrv2,&skCr2,&skLfoP,&skA,&skD,&skS,&skR,&skW };
    const int sw = skr.getWidth() / 17;
    for (auto* k : sks) k->setBounds (skr.removeFromLeft (sw).reduced (2));

    a.removeFromTop (18);
    // master + synth macros
    auto bott = a.removeFromTop (104);
    juce::Slider* ms[7] = { &mCut,&mRes,&mDrv,&mVol,&mDly,&mRev,&mSC };
    for (auto* s : ms) s->setBounds (bott.removeFromLeft (86).reduced (4));
    bott.removeFromLeft (24);
    juce::Slider* ss[6] = { &sCut,&sRes,&sEnv,&sLfoR,&sLfoA,&sVol };
    for (auto* s : ss) s->setBounds (bott.removeFromLeft (86).reduced (4));
}
