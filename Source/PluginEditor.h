#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VintageUI.h"

class LimitAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LimitAudioProcessorEditor (LimitAudioProcessor&);
    ~LimitAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; juce::String name; std::unique_ptr<SliderAttachment> attachment; bool big = false; };

    Knob& addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps = {});
    void setupSwitch (juce::ToggleButton& b, const juce::String& paramID, const juce::String& onText, const juce::String& offText, juce::Colour lamp);
    void timerCallback() override;
    void renderTextures();
    void drawEngraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, juce::Justification);

    LimitAudioProcessor& proc;
    VintageLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ToggleButton onSwitch, autoSwitch, matchSwitch;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    juce::ComboBox presetBox;
    VuMeter vuLeft { lnf, "L" }, vuRight { lnf, "R" };

    // Messanzeige im Kopfbereich: True Peak (mit Halten) und Pegelreduktion
    struct Readout : public juce::Component
    {
        explicit Readout (VintageLookAndFeel& l) : lnf (l) {}
        VintageLookAndFeel& lnf;
        float tpDb = -100.0f, grDb = 0.0f, ceilingDb = -1.0f;
        void paint (juce::Graphics&) override;
        bool resetRequested = false;
        void mouseDown (const juce::MouseEvent&) override { resetRequested = true; }   // Halten zurücksetzen
    } readout { lnf };
    float tpHold = -100.0f; int tpHoldFrames = 0, frame = 0;

    juce::Rectangle<int> faceplate, presetArea, vuBezelL, vuBezelR, dividerArea, tagArea;
    std::array<juce::Rectangle<int>, 3> switchLabels;
    juce::Image tolexImage, faceplateImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimitAudioProcessorEditor)
};
