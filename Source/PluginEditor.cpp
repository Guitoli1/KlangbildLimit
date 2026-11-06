#include "PluginEditor.h"

using namespace vintage;

//==============================================================================
LimitAudioProcessorEditor::LimitAudioProcessorEditor (LimitAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addKnob ("drive",   "GAIN (DB)",       true, "0|+6|+12|+18|+24");
    addKnob ("ceiling", "OBERGRENZE (DBTP)", true, juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "12|\xe2\x88\x92" "9|\xe2\x88\x92" "6|\xe2\x88\x92" "3|0")));
    addKnob ("release", "RELEASE (MS)", false, "10|100|1000");
    addKnob ("clip",    "CLIP (%)",  false, "0|25|50|75|100");
    addKnob ("link",    "STEREO-LINK (%)", false, "0|25|50|75|100");

    setupSwitch (onSwitch, "on", "AN", "AUS", lampRed);
    setupSwitch (autoSwitch, "autorel", "AN", "AUS", lampAmber);
    setupSwitch (matchSwitch, "match", "AN", "AUS", lampAmber);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (vuLeft);
    addAndMakeVisible (vuRight);
    vuLeft.setLabel ("GR");
    vuRight.setLabel ("OUT");
    addAndMakeVisible (readout);

    for (auto* t : { &onSwitch, &autoSwitch, &matchSwitch }) rack::styleToggle (*t);
    setSize (rackW, rackH);
    startTimerHz (60);
}

LimitAudioProcessorEditor::~LimitAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

LimitAudioProcessorEditor::Knob& LimitAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->big = big;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setMouseDragSensitivity (big ? 320 : (steps.isNotEmpty() ? 140 : 220));
    if (steps.isNotEmpty())
        k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void LimitAudioProcessorEditor::setupSwitch (juce::ToggleButton& b, const juce::String& paramID,
                                            const juce::String& onText, const juce::String& offText, juce::Colour lamp)
{
    b.setButtonText ("x");
    b.getProperties().set ("onText", onText);
    b.getProperties().set ("offText", offText);
    b.getProperties().set ("lamp", (int) lamp.getARGB());
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, b));
}

void LimitAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    // links: Pegelreduktion – Zeiger steht ohne Limiting auf 0 VU und wandert nach links
    const float gr = proc.grDb.load();
    vuLeft.update (juce::Decibels::decibelsToGain (-gr - 18.0f) / 1.1107f, 0.0f, dt);
    vuRight.update (proc.vuOut.load(), proc.peakOut.exchange (0.0f), dt);

    // True Peak: höchsten Wert 3 s halten
    const float tp = juce::Decibels::gainToDecibels (proc.truePeakOut.exchange (0.0f), -100.0f);
    if (readout.resetRequested) { readout.resetRequested = false; tpHold = -100.0f; }
    if (tp >= tpHold) { tpHold = tp; tpHoldFrames = 180; }
    else if (--tpHoldFrames <= 0) tpHold = juce::jmax (tp, tpHold - 0.5f);
    if (++frame % 6 == 0)
    {
        readout.tpDb = tpHold;
        readout.grDb = gr;
        readout.ceilingDb = proc.apvts.getRawParameterValue ("ceiling")->load();
        readout.repaint();
    }

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void LimitAudioProcessorEditor::renderPanel()
{
    panelImage = rack::renderPanel (rackW, rackH, 23);
    juce::Graphics g (panelImage);
    const juce::String titles[] = { "EINGANG", "ANZEIGE", "OBERGRENZE", "REGELUNG" };
    for (size_t i = 0; i < sections.size(); ++i)
        rack::drawSection (g, sections[i], titles[i], lnf);
    rack::drawBezel (g, vuBezelL);
    rack::drawBezel (g, vuBezelR);
    for (auto& k : knobs)
        rack::drawKnobLabel (g, k->slider, k->name, lnf, k->big ? 14.0f : 12.0f);
    const juce::String names[] = { "LIMITER", "AUTO-RELEASE", "PEGEL-ABGLEICH" };
    for (size_t i = 0; i < 3; ++i)
        rack::drawLabel (g, names[i], switchLabels[i].toFloat(), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawLabel (g, "REDUKTION", vuBezelL.toFloat().withY ((float) vuBezelL.getBottom() + 4.0f).withHeight (14.0f), lnf, 11.0f);
    rack::drawLabel (g, "AUSGANG", vuBezelR.toFloat().withY ((float) vuBezelR.getBottom() + 4.0f).withHeight (14.0f), lnf, 11.0f);
    rack::drawNameplate (g, nameplate, "LIMIT", "TRUE-PEAK-LIMITER", lnf);
    rack::drawLabel (g, "PROGRAMM", presetBox.getBounds().toFloat().withY ((float) presetBox.getY() - 16.0f).withHeight (14.0f), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawTypeTag (g, rack::u8 ("TYP KB-2000  \xc2\xb7  NR. 0611"), tagArea, lnf);
}

void LimitAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);
}

void LimitAudioProcessorEditor::resized()
{
    const int top = 30, bottom = rackH - 34;
    sections[0] = { 72, top, 240, 330 };
    sections[1] = { 324, top, 460, 330 };
    sections[2] = { 796, top, 234, 330 };
    sections[3] = { 72, top + 350, 958, bottom - top - 350 };

    knobs[0]->slider.setBounds (juce::Rectangle<int> (220, 220).withCentre ({ sections[0].getCentreX(), top + 160 }));
    knobs[1]->slider.setBounds (juce::Rectangle<int> (220, 220).withCentre ({ sections[2].getCentreX(), top + 160 }));
    vuBezelL = { sections[1].getX() + 14, top + 22, 210, 150 };
    vuBezelR = { sections[1].getRight() - 14 - 210, top + 22, 210, 150 };
    vuLeft.setBounds (vuBezelL.reduced (8));
    vuRight.setBounds (vuBezelR.reduced (8));
    readout.setBounds (sections[1].getX() + 14, top + 200, sections[1].getWidth() - 28, 116);

    const auto rs = sections[3];
    const int kw = 150, gap = (rs.getWidth() - 3 * kw) / 4;
    for (int i = 2; i < 5; ++i)
        knobs[(size_t) i]->slider.setBounds (rs.getX() + gap + (i - 2) * (kw + gap), rs.getY() + 18, kw, kw);

    const int colX = 1046, colW = rackW - 56 - 16 - colX;
    nameplate = { colX, top - 8, colW, 88 };
    presetBox.setBounds (colX, top + 112, colW, 28);
    juce::ToggleButton* sws[] = { &onSwitch, &autoSwitch, &matchSwitch };
    for (int i = 0; i < 3; ++i)
    {
        const int y = top + 168 + i * 80;
        switchLabels[(size_t) i] = { colX + 4, y, colW, 14 };
        sws[i]->setBounds (colX + 4, y + 18, 150, 40);
    }
    tagArea = { colX - 60, rackH - 28, colW + 60, 16 };
    renderPanel();
}

//==============================================================================
void LimitAudioProcessorEditor::Readout::paint (juce::Graphics& g)
{
    // dunkles Anzeigefeld mit Messing-Rahmen und bernsteinfarbenen Ziffern
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (2.0f, 3.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (gold.brighter (0.25f), 0, b.getY(), gold.darker (0.6f), 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);
    const auto in = b.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1410), 0, in.getY(), juce::Colour (0xff0a0806), 0, in.getBottom(), false));
    g.fillRoundedRectangle (in, 6.0f);

    auto fmt = [] (float v, int dec) { return (v < -0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String())
                                              + juce::String (std::abs (v), dec).replaceCharacter ('.', ','); };
    const float half = in.getWidth() * 0.5f;
    struct Field { juce::String title, value, unit; juce::Colour col; };
    const bool over = tpDb > ceilingDb + 0.05f;
    const Field fields[] = {
        { "TRUE PEAK", tpDb < -99.0f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93\xe2\x80\x93,\xe2\x80\x93")) : fmt (tpDb, 1), "dBTP",
          over ? lampRed : juce::Colour (0xffffb347) },
        { "REDUKTION", grDb < 0.05f ? juce::String ("0,0") : fmt (-grDb, 1), "dB", juce::Colour (0xffffb347) },
    };
    for (int i = 0; i < 2; ++i)
    {
        const auto f = in.withX (in.getX() + half * (float) i).withWidth (half).reduced (12.0f, 8.0f);
        g.setColour (logo.withAlpha (0.55f));
        g.setFont (lnf.labelFont (11.0f, 0.25f));
        g.drawText (fields[i].title, f.withHeight (16.0f), juce::Justification::centredLeft);
        const auto vr = f.withTrimmedTop (18.0f).withTrimmedBottom (4.0f);
        // Glimmen
        g.setColour (fields[i].col.withAlpha (0.18f));
        g.setFont (lnf.labelFont (50.0f, 0.02f));
        g.drawText (fields[i].value, vr.translated (0.0f, 1.0f).expanded (1.0f), juce::Justification::centredLeft);
        g.setColour (fields[i].col);
        g.drawText (fields[i].value, vr, juce::Justification::centredLeft);
        g.setColour (fields[i].col.withAlpha (0.7f));
        g.setFont (lnf.labelFont (13.0f, 0.15f));
        g.drawText (fields[i].unit, vr.withTrimmedRight (2.0f), juce::Justification::bottomRight);
    }
    g.setColour (gold.withAlpha (0.25f));
    g.drawVerticalLine ((int) (in.getX() + half), in.getY() + 10.0f, in.getBottom() - 10.0f);
}
