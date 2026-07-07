#pragma once
#include "PluginProcessor.h"

// ---- mini knob generique lie a un float interne (pas un parametre hote) ----
class MiniKnob : public juce::Component
{
public:
    std::function<float()> getV;  std::function<void(float)> setV;
    float mn = 0, mx = 1; bool logScale = false;
    juce::String label; juce::Colour accent { 0xffff7a3c };

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override { dragY = e.y; startP = toP (getV()); }
    void mouseDrag (const juce::MouseEvent& e) override
    { setV (toV (juce::jlimit (0.0f, 1.0f, startP + (dragY - e.y) / 160.0f))); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (def >= mn) { setV (def); repaint(); } }
    float def = -1e9f;
private:
    int dragY = 0; float startP = 0;
    float toP (float v) const
    { return logScale ? std::log (juce::jmax (v, mn) / mn) / std::log (mx / mn) : (v - mn) / (mx - mn); }
    float toV (float p) const
    { return logScale ? mn * std::pow (mx / mn, p) : mn + p * (mx - mn); }
};

class BzzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BzzzEditor (BzzzProcessor&);
    ~BzzzEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildSongUI();
    void refreshInspector();
    void selectTrack (int t);
    juce::Rectangle<int> cell (int t, int s) const;
    juce::Rectangle<int> rollCell (int row, int s) const;

    BzzzProcessor& proc;

    // header
    juce::TextButton styleBtns[6], synPwr, songOnBtn, songAddBtn, songClrBtn;
    juce::ComboBox kitBox, patchBox;
    juce::TextButton kitPrev, kitNext, kitRnd, patPrev, patNext, patRnd;
    juce::Slider swing;

    // pads + mutes
    juce::TextButton pads[12], mutes[12];
    int selTrack = 0;

    // inspecteur : knobs internes
    MiniKnob ckCut, ckRes, ckDrv, ckCr, ckDe, ckPi, ckPa, ckSD, ckSR, ckVo;
    juce::TextButton fTypes[6];   // OFF LP HP BP NOTCH PEAK

    // patterns / song
    juce::TextButton slotBtns[BzzzProcessor::kSlots], copyBtn, stylePatBtn, rndPatBtn, clrPatBtn;
    bool copyArm = false;
    juce::OwnedArray<juce::TextButton> songEntryBtns, songDelBtns;

    // master + synth macros (parametres -> attachments)
    juce::Slider mCut, mRes, mDrv, mVol, mDly, mRev, mSC;
    juce::Slider sCut, sRes, sEnv, sLfoR, sLfoA, sVol;
    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Att>> atts;

    // synth internes
    MiniKnob skMix, skDet, skUni, skO2P, skFM, skSub, skNz, skGl,
             skFDec, skDrv2, skCr2, skLfoP, skA, skD, skS, skR, skW;
    juce::TextButton osc1Btns[4], osc2Btns[4], fmodeBtns[6], lfoShapeBtns[3], octDown, octUp;

    juce::Rectangle<int> gridArea, rollArea, accRow, slideRow;
    int uiStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BzzzEditor)
};
