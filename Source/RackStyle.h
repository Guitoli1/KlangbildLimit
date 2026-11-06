#pragma once

// Gemeinsamer Rack-Look der Klangbild-Studiogeräte:
// blaugrauer Hammerschlaglack, weißer Siebdruck, schwarze Zeigerknöpfe, Rack-Ohren, Typenschild

#include "VintageUI.h"

namespace rack
{
const juce::Colour lacquer { 0xff5f7487 };
const juce::Colour silk    { 0xfff2eee4 };
const juce::Colour silkDim { 0xc8f2eee4 };
inline juce::String u8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }
}

class RackLookAndFeel : public VintageLookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

inline void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float knobR = juce::jmax (16.0f, R - 27.0f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skala im Siebdruck
    const auto steps = juce::StringArray::fromTokens (slider.getProperties()["stepLabels"].toString(), "|", "");
    g.setColour (rack::silk);
    if (steps.size() > 1)
    {
        for (int i = 0; i < steps.size(); ++i)
        {
            const float a = startAngle + (float) i / (float) (steps.size() - 1) * (endAngle - startAngle);
            const auto p1 = c.getPointOnCircumference (knobR + 4.0f, a), p2 = c.getPointOnCircumference (knobR + 11.0f, a);
            g.drawLine ({ p1, p2 }, 1.6f);
            if (steps[i].isNotEmpty())
            {
                g.setFont (labelFont (steps[i].length() > 3 ? 10.5f : 12.0f, 0.03f));
                const auto tp = c.getPointOnCircumference (knobR + (steps[i].length() > 3 ? 22.0f : 19.0f), a);
                g.drawText (steps[i], juce::Rectangle<float> (56.0f, 16.0f).withCentre (tp), juce::Justification::centred);
            }
        }
    }
    else
    {
        for (int i = 0; i <= 20; ++i)
        {
            const float a = startAngle + (float) i / 20.0f * (endAngle - startAngle);
            const bool major = i % 2 == 0;
            g.drawLine ({ c.getPointOnCircumference (knobR + 4.0f, a), c.getPointOnCircumference (knobR + (major ? 11.0f : 7.0f), a) }, major ? 1.6f : 1.0f);
            if (major)
            {
                g.setFont (labelFont (12.0f, 0.03f));
                g.drawText (juce::String (i / 2), juce::Rectangle<float> (24.0f, 14.0f).withCentre (c.getPointOnCircumference (knobR + 19.0f, a)), juce::Justification::centred);
            }
        }
    }

    // Schatten
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (2.5f, 4.0f)));
    // Rock (geriffelt)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3836), c.x, c.y - knobR, juce::Colour (0xff0c0b0a), c.x, c.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    for (int k = 0; k < 48; ++k)
    {
        const float a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
        g.drawLine ({ c.getPointOnCircumference (knobR * 0.86f, a), c.getPointOnCircumference (knobR - 0.5f, a) }, 1.2f);
    }
    // Kappe
    const float capR = knobR * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4744), c.x - capR * 0.5f, c.y - capR, juce::Colour (0xff161514), c.x + capR * 0.4f, c.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (juce::Rectangle<float> (capR * 1.1f, capR * 0.55f).withCentre (c.translated (-capR * 0.2f, -capR * 0.5f)));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);
    // weiße Zeigerlinie über Kappe und Rock
    g.setColour (juce::Colour (0xfff5f1e6));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, angle), c.getPointOnCircumference (knobR - 1.5f, angle) }, 3.0f);
}


namespace rack
{
using namespace vintage;

// Frontplatte mit Lack, Korn, Kanten und Rack-Ohren
inline juce::Image renderPanel (int w, int h, int seed)
{
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    g.setGradientFill (juce::ColourGradient (lacquer.brighter (0.12f), 0, 0, lacquer.darker (0.18f), 0, (float) h, false));
    g.fillRect (0, 0, w, h);
    juce::Random rnd (seed);
    for (int i = 0; i < w * h / 70; ++i)
    {
        const float r = 1.5f + rnd.nextFloat() * 4.0f;
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + rnd.nextFloat() * 0.02f));
        g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
    }
    {
        juce::Image::BitmapData data (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                auto c = data.getPixelColour (x, y);
                const int n = rnd.nextInt (9) - 4;
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, c.getRed() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getGreen() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getBlue() + n)));
            }
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawHorizontalLine (0, 0.0f, (float) w);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawHorizontalLine (h - 1, 0.0f, (float) w);
    for (float ex : { 0.0f, (float) w - 56.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 56.0f : ex), 0.0f, (float) h);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 57.0f : ex + 1.0f), 0.0f, (float) h);
        std::vector<float> ys { 44.0f, (float) h - 44.0f };
        if (h > 500) ys.push_back ((float) h * 0.5f);
        for (float sy : ys)
        {
            const auto slot = juce::Rectangle<float> (ex + 14.0f, sy - 8.0f, 28.0f, 16.0f);
            g.setColour (juce::Colour (0xff141210));
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (slot.translated (0.0f, 1.0f), 8.0f, 1.0f);
            VintageLookAndFeel::drawScrew (g, slot.getCentre().translated (ex < 1.0f ? 3.0f : -3.0f, 0.0f), 6.5f, sy * 0.01f + ex * 0.003f);
        }
    }
    return img;
}

inline void drawSection (juce::Graphics& g, juce::Rectangle<int> rect, const juce::String& title, VintageLookAndFeel& lnf)
{
    const auto r = rect.toFloat();
    g.setColour (silkDim);
    juce::Path box; box.addRoundedRectangle (r, 5.0f);
    g.strokePath (box, juce::PathStrokeType (1.2f));
    if (title.isEmpty()) return;
    const auto f = lnf.labelFont (13.0f, 0.3f);
    const float tw = (float) juce::GlyphArrangement::getStringWidthInt (f, title) + 20.0f;
    const auto tr = juce::Rectangle<float> (tw, 16.0f).withCentre ({ r.getCentreX(), r.getY() });
    g.setColour (lacquer);
    g.fillRect (tr);
    g.setColour (silk);
    g.setFont (f);
    g.drawText (title, tr, juce::Justification::centred);
}

inline void drawBezel (juce::Graphics& g, juce::Rectangle<int> rect, float corner = 12.0f)
{
    const auto r = rect.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (2.0f, 3.0f), corner);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a28), 0, r.getY(), juce::Colour (0xff121110), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
}

inline void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, VintageLookAndFeel& lnf, float size = 12.0f,
                       juce::Justification j = juce::Justification::centred)
{
    g.setColour (silk);
    g.setFont (lnf.labelFont (size, 0.16f));
    g.drawText (text, r, j);
}

inline void drawKnobLabel (juce::Graphics& g, const juce::Slider& s, const juce::String& text, VintageLookAndFeel& lnf, float size = 12.0f)
{
    const auto sb = s.getBounds().toFloat();
    drawLabel (g, text, { sb.getX() - 30.0f, sb.getBottom() - 8.0f, sb.getWidth() + 60.0f, 16.0f }, lnf, size);
}

inline void drawNameplate (juce::Graphics& g, juce::Rectangle<int> rect, const juce::String& model, const juce::String& subtitle, VintageLookAndFeel& lnf)
{
    const auto np = rect.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (np.translated (1.5f, 2.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2522), 0, np.getY(), juce::Colour (0xff121010), 0, np.getBottom(), false));
    g.fillRoundedRectangle (np, 4.0f);
    g.setColour (gold.withAlpha (0.7f));
    g.drawRoundedRectangle (np.reduced (3.0f), 3.0f, 1.0f);
    g.setFont (lnf.scriptFont (34.0f));
    g.setColour (logo);
    g.drawText ("Klangbild", np.withTrimmedLeft (12.0f).withHeight (44.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft);
    g.setColour (gold);
    g.setFont (lnf.labelFont (17.0f, 0.45f));
    g.drawText (model, np.withTrimmedLeft (16.0f).withTrimmedTop (44.0f).withHeight (20.0f), juce::Justification::centredLeft);
    g.setColour (logo.withAlpha (0.6f));
    g.setFont (lnf.labelFont (9.0f, 0.2f));
    g.drawText (subtitle, np.withTrimmedLeft (16.0f).withTrimmedTop (66.0f).withHeight (14.0f), juce::Justification::centredLeft);
}

inline void drawTypeTag (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, VintageLookAndFeel& lnf)
{
    g.setColour (silkDim);
    g.setFont (lnf.labelFont (10.0f, 0.3f));
    g.drawText (text, r.toFloat(), juce::Justification::centredRight);
}

inline void styleToggle (juce::ToggleButton& b) { b.setColour (juce::ToggleButton::textColourId, silk); }
} // namespace rack
