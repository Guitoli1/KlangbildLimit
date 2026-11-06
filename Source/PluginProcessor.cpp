#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float n) { return a * std::pow (b / a, n); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}

Attr unit (const char* u, int decimals, bool sign = false)
{
    return Attr().withStringFromValueFunction ([=] (float v, int)
                 {
                     auto s = juce::String (std::abs (v) < 0.05f && decimals > 0 ? 0.0f : v, decimals).replaceCharacter ('.', ',');
                     if (sign && v > 0.049f) s = "+" + s;
                     return s + " " + u;
                 })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.replaceCharacter (',', '.').getFloatValue(); });
}
}

juce::AudioProcessorValueTreeState::ParameterLayout LimitAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<APF> (juce::ParameterID { "drive", 1 }, "Gain", juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, unit ("dB", 1, true)));
    l.add (std::make_unique<APF> (juce::ParameterID { "ceiling", 1 }, "Obergrenze", juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f, unit ("dBTP", 1)));
    l.add (std::make_unique<APF> (juce::ParameterID { "release", 1 }, "Release", logRange (10.0f, 1000.0f), 120.0f, unit ("ms", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "clip", 1 }, "Clip", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, unit ("%", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "link", 1 }, "Stereo-Link", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, unit ("%", 0)));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "autorel", 1 }, "Auto-Release", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "match", 1 }, "Pegel-Abgleich", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Limiter an", true));
    return l;
}

const std::vector<LimitAudioProcessor::Preset>& LimitAudioProcessor::presets()
{
    static const std::vector<Preset> p = {
        { "Standard",                   { { "drive", 0 },  { "ceiling", -1.0f }, { "release", 120 }, { "clip", 0 },  { "link", 100 }, { "autorel", 1 } } },
        { "Streaming (-1 dBTP)",        { { "drive", 4 },  { "ceiling", -1.0f }, { "release", 150 }, { "clip", 0 },  { "link", 100 }, { "autorel", 1 } } },
        { "Transparent",                { { "drive", 3 },  { "ceiling", -1.0f }, { "release", 300 }, { "clip", 0 },  { "link", 100 }, { "autorel", 1 } } },
        { "Laut & druckvoll",           { { "drive", 8 },  { "ceiling", -1.0f }, { "release", 60 },  { "clip", 40 }, { "link", 80 },  { "autorel", 1 } } },
        { "Clipper-Stil",               { { "drive", 6 },  { "ceiling", -1.0f }, { "release", 40 },  { "clip", 85 }, { "link", 100 }, { "autorel", 0 } } },
        { "Sicherheitsnetz",            { { "drive", 0 },  { "ceiling", -0.5f }, { "release", 80 },  { "clip", 0 },  { "link", 100 }, { "autorel", 1 } } },
        { "Demo/Probe laut",            { { "drive", 10 }, { "ceiling", -1.0f }, { "release", 80 },  { "clip", 50 }, { "link", 100 }, { "autorel", 1 } } },
    };
    return p;
}

void LimitAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

const juce::String LimitAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size())
               ? juce::String (juce::CharPointer_UTF8 (presets()[(size_t) index].name)) : juce::String();
}

LimitAudioProcessor::LimitAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KlangbildLimit", createLayout())
{
    pDrive = apvts.getRawParameterValue ("drive");   pCeil = apvts.getRawParameterValue ("ceiling");
    pRel = apvts.getRawParameterValue ("release");   pClip = apvts.getRawParameterValue ("clip");
    pLink = apvts.getRawParameterValue ("link");     pAuto = apvts.getRawParameterValue ("autorel");
    pMatch = apvts.getRawParameterValue ("match");   pOn = apvts.getRawParameterValue ("on");
}

bool LimitAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out;
}

void LimitAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);
    const int nch = juce::jlimit (1, 2, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    work.setSize (2, maxBlock);
    direct.setSize (2, maxBlock);
    osLatency = (int) std::lround (oversampler->getLatencyInSamples());
    for (auto& d : osDelay) d.prepare (osLatency);
    lim.prepare (sr, nch);
    safety.prepare (sr, nch);
    for (auto& t : outTp) t.reset();
    latency = lim.latency() + safety.latency() + osLatency;
    setLatencySamples (latency);
    for (auto& d : dryDelay) d.prepare (latency);
    smDrive.reset (sr, 0.03); smDrive.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    smClip.reset (sr, 0.03);  smClip.setCurrentAndTargetValue (pClip->load() * 0.01f);
    smMatch.reset (sr, 0.03); smMatch.setCurrentAndTargetValue (1.0f);
    smOn.reset (sr, 0.02);    smOn.setCurrentAndTargetValue (pOn->load() > 0.5f ? 1.0f : 0.0f);
    smUseOs.reset (sr, 0.02); smUseOs.setCurrentAndTargetValue (pClip->load() > 0.0f ? 1.0f : 0.0f);
    vuCoef = (float) (1.0 - std::exp (-1.0 / (0.01 * sr)));
    vuState = 0; grState = 0;
}

void LimitAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels());
    if (nch == 0 || oversampler == nullptr)
        return;

    const float ceilDb = pCeil->load();
    const float ceiling = juce::Decibels::decibelsToGain (ceilDb);
    lim.setParams (ceilDb, pRel->load(), pAuto->load() > 0.5f, pLink->load() * 0.01f);
    safety.setParams (ceilDb, 15.0f, false, 1.0f);
    smDrive.setTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    smClip.setTargetValue (pClip->load() * 0.01f);
    smMatch.setTargetValue (pMatch->load() > 0.5f ? juce::Decibels::decibelsToGain (-pDrive->load()) : 1.0f);
    smOn.setTargetValue (pOn->load() > 0.5f ? 1.0f : 0.0f);
    smUseOs.setTargetValue (pClip->load() > 0.0f ? 1.0f : 0.0f);

    float* io[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };
    float grMax = 0, pk = 0, tpMax = 0;

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);

        // 1) Gain, dann (vierfach überabgetastet) weicher Clipper
        for (int c = 0; c < 2; ++c)
        {
            const float* src = io[juce::jmin (c, nch - 1)] + start;
            juce::FloatVectorOperations::copy (work.getWritePointer (c), src, n);
        }
        float* w0 = work.getWritePointer (0); float* w1 = work.getWritePointer (1);
        for (int i = 0; i < n; ++i) { const float g = smDrive.getNextValue(); w0[i] *= g; w1[i] *= g; }
        // ohne Clip läuft das Signal am Oversampling vorbei (nur gleich lang verzögert) -> völlig unverfälscht
        float* d0 = direct.getWritePointer (0); float* d1 = direct.getWritePointer (1);
        for (int i = 0; i < n; ++i) { d0[i] = osDelay[0].process (w0[i]); d1[i] = osDelay[1].process (w1[i]); }

        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        const float clip = smClip.getTargetValue();   // pro Block genügt
        const float clipStart = smClip.getCurrentValue();
        smClip.skip (n);
        const size_t un = up.getNumSamples();
        for (size_t c = 0; c < 2; ++c)
        {
            float* d = up.getChannelPointer (c);
            for (size_t i = 0; i < un; ++i)
            {
                const float a = clipStart + (clip - clipStart) * (float) i / (float) un;
                if (a > 0.0f)
                    d[i] += a * (limiter::softClip (d[i], ceiling) - d[i]);
            }
        }
        oversampler->processSamplesDown (block);
        for (int i = 0; i < n; ++i)
        {
            const float u = smUseOs.getNextValue();
            if (u < 1.0f) { w0[i] = d0[i] + (w0[i] - d0[i]) * u; w1[i] = d1[i] + (w1[i] - d1[i]) * u; }
        }

        // 2) True-Peak-Limiter, Pegel-Abgleich, Bypass
        for (int i = 0; i < n; ++i)
        {
            float fr[2] = { w0[i], w1[i] };
            const float g = lim.process (fr) * safety.process (fr);
            const float m = smMatch.getNextValue(), on = smOn.getNextValue();
            grMax = juce::jmax (grMax, -juce::Decibels::gainToDecibels (g, -60.0f));

            for (int c = 0; c < nch; ++c)
            {
                const float dry = dryDelay[(size_t) c].process (io[c][start + i]);
                float y = fr[c] * m;
                y = dry + (y - dry) * on;
                io[c][start + i] = y;
                pk = juce::jmax (pk, std::abs (y));
                tpMax = juce::jmax (tpMax, outTp[(size_t) c].process (y));
            }
            const float a = nch > 1 ? 0.5f * (std::abs (io[0][start + i]) + std::abs (io[1][start + i])) : std::abs (io[0][start + i]);
            vuState += (a - vuState) * vuCoef;
        }
    }

    // Anzeige: Reduktion schnell rein, langsam raus
    grState = grMax > grState ? grMax : grState + (grMax - grState) * 0.15f;
    grDb.store (pOn->load() > 0.5f ? grState : 0.0f);
    vuOut.store (vuState);
    peakOut.store (juce::jmax (pk, peakOut.load()));
    truePeakOut.store (juce::jmax (tpMax, truePeakOut.load()));
}

void LimitAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LimitAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
    apvts.replaceState (tree);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->convertTo0to1 (rp->convertFrom0to1 (rp->getValue())));
}

juce::AudioProcessorEditor* LimitAudioProcessor::createEditor() { return new LimitAudioProcessorEditor (*this); }

#ifndef KB_EMBEDDED
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LimitAudioProcessor(); }
#endif
