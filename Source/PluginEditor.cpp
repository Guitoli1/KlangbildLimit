#include "PluginEditor.h"

using namespace vintage;

//==============================================================================
LimitAudioProcessorEditor::LimitAudioProcessorEditor (LimitAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addKnob ("drive",   "GAIN (DB)",       true, "0|+6|+12|+18|+24");
    addKnob ("ceiling", "OBERGRENZE (DBTP)", true, juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "12|\xe2\x88\x92" "9|\xe2\x88\x92" "6|\xe2\x88\x92" "3|0")));
    addKnob ("release", "RELEASE",   false);
    addKnob ("clip",    "CLIP",      false);
    addKnob ("link",    "STEREO-LINK", false);

    setupSwitch (onSwitch, "on", "AN", "AUS", lampRed);
    setupSwitch (autoSwitch, "autorel", "AN", "AUS", lampAmber);
    setupSwitch (matchSwitch, "match", "AN", "AUS", lampAmber);

    presetBox.getProperties().set ("plate", true);
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

    setSize (1100, 620);
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

void LimitAudioProcessorEditor::renderTextures()
{
    tolexImage = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    {
        juce::Image::BitmapData data (tolexImage, juce::Image::BitmapData::writeOnly);
        juce::Random rnd (7);
        for (int y = 0; y < tolexImage.getHeight(); ++y)
            for (int x = 0; x < tolexImage.getWidth(); ++x)
            {
                const int weave = ((x + y) % 4 == 0 || (x - y + 4096) % 4 == 0) ? -5 : 0;
                const int n = rnd.nextInt (13) - 6 + weave;
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, 34 + n),
                                                         (juce::uint8) juce::jlimit (0, 255, 28 + n),
                                                         (juce::uint8) juce::jlimit (0, 255, 24 + n)));
            }
    }
    if (faceplate.isEmpty())
        return;
    faceplateImage = juce::Image (juce::Image::ARGB, faceplate.getWidth(), faceplate.getHeight(), true);
    juce::Graphics g (faceplateImage);
    const auto r = faceplateImage.getBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (cream.brighter (0.06f), 0, 0, creamDark, 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 6.0f);
    juce::Random rnd (3);
    for (int y = 0; y < faceplate.getHeight(); ++y)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colour (0xff6b5534)).withAlpha (0.015f + rnd.nextFloat() * 0.035f));
        g.drawHorizontalLine (y, 0.0f, r.getWidth());
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                             juce::Colour (0x30694a1c), 0, 0, true));
    g.fillRoundedRectangle (r, 6.0f);
}

void LimitAudioProcessorEditor::drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                                             juce::Font f, juce::Justification j)
{
    g.setFont (f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawFittedText (text, r.translated (0.0f, 1.0f).toNearestInt(), j, 2, 1.0f);
    g.setColour (ink);
    g.drawFittedText (text, r.toNearestInt(), j, 2, 1.0f);
}

void LimitAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (tolexImage, 0, 0);

    // Logo
    const auto scriptF = lnf.scriptFont (64.0f);
    const auto logoArea = juce::Rectangle<float> (40.0f, 8.0f, 400.0f, 84.0f);
    g.setFont (scriptF);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText ("Klangbild", logoArea.translated (2.0f, 3.0f), juce::Justification::centredLeft);
    g.setColour (logo);
    g.drawText ("Klangbild", logoArea, juce::Justification::centredLeft);
    const float sw = (float) juce::GlyphArrangement::getStringWidthInt (scriptF, "Klangbild");
    g.setColour (gold);
    g.setFont (lnf.labelFont (26.0f, 0.35f));
    g.drawText ("LIMIT", juce::Rectangle<float> (logoArea.getX() + sw + 14.0f, logoArea.getY() + 30.0f, 140.0f, 40.0f), juce::Justification::centredLeft);
    g.setColour (logo.withAlpha (0.55f));
    g.setFont (lnf.labelFont (12.0f, 0.3f));
    g.drawText (juce::CharPointer_UTF8 ("TRUE-PEAK-LIMITER \xc2\xb7 MASTERING \xc2\xb7 STREAMING"), juce::Rectangle<float> (44.0f, 90.0f, 420.0f, 16.0f), juce::Justification::centredLeft);

    const auto pa = presetArea.toFloat();
    g.setColour (logo.withAlpha (0.6f));
    g.setFont (lnf.labelFont (10.0f, 0.3f));
    g.drawText ("PROGRAMM", pa.withY (pa.getY() - 15.0f).withHeight (13.0f), juce::Justification::centredLeft);

    // Frontplatte
    const auto fp = faceplate.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (fp.expanded (4.0f).translated (0, 2.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (gold.brighter (0.3f), 0, fp.getY(), gold.darker (0.6f), 0, fp.getBottom(), false));
    g.fillRoundedRectangle (fp.expanded (3.0f), 8.0f);
    g.drawImageAt (faceplateImage, faceplate.getX(), faceplate.getY());
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (fp, 6.0f, 1.0f);
    for (auto [x, y, a] : { std::tuple { fp.getX() + 12.0f, fp.getY() + 12.0f, 0.4f }, std::tuple { fp.getRight() - 12.0f, fp.getY() + 12.0f, 1.3f },
                            std::tuple { fp.getX() + 12.0f, fp.getBottom() - 12.0f, 2.2f }, std::tuple { fp.getRight() - 12.0f, fp.getBottom() - 12.0f, 0.9f } })
        VintageLookAndFeel::drawScrew (g, { x, y }, 5.0f, a);

    // Rahmen um die beiden VU-Meter
    for (auto vbi : { vuBezelL, vuBezelR })
    {
        const auto vb = vbi.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (vb.translated (2.0f, 3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2520), 0, vb.getY(), juce::Colour (0xff120f0c), 0, vb.getBottom(), false));
        g.fillRoundedRectangle (vb, 12.0f);
    }

    // Trennlinie zwischen Haupt- und Nebenreglern
    for (int dy : { 0, 1 })
    {
        g.setColour (dy == 0 ? ink.withAlpha (0.4f) : juce::Colours::white.withAlpha (0.5f));
        g.drawHorizontalLine (dividerArea.getY() + dy, (float) dividerArea.getX(), (float) dividerArea.getRight());
    }

    for (auto& k : knobs)
    {
        const auto sb = k->slider.getBounds().toFloat();
        drawEngraved (g, k->name, { sb.getX() - 20.0f, sb.getBottom() - 4.0f, sb.getWidth() + 40.0f, k->big ? 24.0f : 18.0f },
                      lnf.labelFont (k->big ? 18.0f : 13.0f, k->big ? 0.2f : 0.14f), juce::Justification::centred);
    }

    const juce::String names[] = { "LIMITER", "AUTO-RELEASE", "PEGEL-ABGLEICH" };
    for (size_t i = 0; i < 3; ++i)
        drawEngraved (g, names[i], switchLabels[i].toFloat(), lnf.labelFont (12.0f, 0.14f), juce::Justification::centred);

    const auto tag = tagArea.toFloat();
    g.setColour (ink.withAlpha (0.85f));
    g.fillRoundedRectangle (tag, 3.0f);
    g.setColour (gold);
    g.setFont (lnf.labelFont (11.0f, 0.2f));
    g.drawText ("MODELL KB-2000", tag, juce::Justification::centred);
}

void LimitAudioProcessorEditor::resized()
{
    const int W = getWidth();
    presetArea = { 44, 128, 260, 32 };
    presetBox.setBounds (presetArea);

    faceplate = { 24, 178, W - 48, 420 };
    const auto in = faceplate.reduced (30, 20);

    // obere Reihe: Eingang | VU L | VU R | Ausgang
    const int topH = 250;
    knobs[0]->slider.setBounds (in.getX() - 10, in.getY() + 10, 230, 210);
    knobs[1]->slider.setBounds (in.getRight() - 220, in.getY() + 10, 230, 210);
    vuBezelL = juce::Rectangle<int> (282, 214).withCentre ({ in.getCentreX() - 148, in.getY() + 118 });
    vuBezelR = juce::Rectangle<int> (282, 214).withCentre ({ in.getCentreX() + 148, in.getY() + 118 });
    vuLeft.setBounds (vuBezelL.reduced (9));
    vuRight.setBounds (vuBezelR.reduced (9));
    dividerArea = { in.getX() + 10, in.getY() + topH + 6, in.getWidth() - 20, 2 };

    // untere Reihe: Release, Clip, Stereo-Link | Schalter
    const int rowY = in.getY() + topH + 14;
    for (int i = 2; i < 5; ++i)
        knobs[(size_t) i]->slider.setBounds (in.getX() + 20 + (i - 2) * 140, rowY, 130, 108);

    juce::ToggleButton* sws[] = { &onSwitch, &autoSwitch, &matchSwitch };
    const int restX = in.getX() + 20 + 3 * 140, cx = (restX + in.getRight()) / 2;
    for (int i = 0; i < 3; ++i)
    {
        const int x = cx - 225 + i * 150;
        switchLabels[(size_t) i] = { x, rowY + 8, 150, 16 };
        sws[i]->setBounds (x + 15, rowY + 30, 120, 40);
    }
    tagArea = { cx - 100, rowY + 84, 200, 24 };

    readout.setBounds (W - 470, 26, 430, 122);

    renderTextures();
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
