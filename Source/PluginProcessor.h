#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LimiterDsp.h"

class LimitAudioProcessor : public juce::AudioProcessor
{
public:
    LimitAudioProcessor();
    ~LimitAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Preset>& presets();
    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> grDb { 0 }, vuOut { 0 }, peakOut { 0 };
    std::atomic<float> truePeakOut { 0 };    // höchster True-Peak seit der letzten Abfrage (linear)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    limiter::Limiter lim;
    limiter::SafetyLimiter safety;   // zweite, feine Stufe: fängt Rest-Überschreitungen zwischen den Abtastungen ab
    std::array<limiter::TruePeak8, 2> outTp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> work, direct;
    std::array<limiter::Delay, 2> osDelay;   // Weg ohne Clipper: gleich lang verzögert, aber unverändert
    int osLatency = 0;
    std::array<limiter::Delay, 2> dryDelay;
    int maxBlock = 512, latency = 0;

    std::atomic<float>* pDrive = nullptr; std::atomic<float>* pCeil = nullptr; std::atomic<float>* pRel = nullptr;
    std::atomic<float>* pClip = nullptr;  std::atomic<float>* pLink = nullptr; std::atomic<float>* pAuto = nullptr;
    std::atomic<float>* pMatch = nullptr; std::atomic<float>* pOn = nullptr;

    juce::SmoothedValue<float> smDrive, smClip, smMatch, smOn, smUseOs;
    float vuState = 0, vuCoef = 0.001f, grState = 0;
    int currentProgram = 0;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimitAudioProcessor)
};
