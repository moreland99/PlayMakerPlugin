#include "PresetState.h"

namespace PresetState
{

static void setNormalized(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, float norm)
{
    if (auto* p = apvts.getParameter(paramID))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
        p->endChangeGesture();
    }
}

void setBool(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, bool value)
{
    setNormalized(apvts, paramID, value ? 1.0f : 0.0f);
}

void setFloat(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, float realValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(paramID)))
        setNormalized(apvts, paramID, p->convertTo0to1(realValue));
}

void setChoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, int choiceIndex)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramID)))
        setNormalized(apvts, paramID, p->convertTo0to1((float) choiceIndex));
}

void disableAllBands(juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < Params::numBands; ++i)
        setBool(apvts, Params::bandParamID(i, "enabled"), false);
}

void applyBand(juce::AudioProcessorValueTreeState& apvts, int bandIndex, const BandConfig& cfg)
{
    const auto prefix = Params::bandParamID(bandIndex, "");
    juce::ignoreUnused(prefix);

    setBool(apvts, Params::bandParamID(bandIndex, "enabled"), cfg.enabled);
    if (!cfg.enabled)
        return;

    setChoice(apvts, Params::bandParamID(bandIndex, "type"), static_cast<int>(cfg.type));
    setFloat(apvts, Params::bandParamID(bandIndex, "freq"), cfg.freqHz);
    setFloat(apvts, Params::bandParamID(bandIndex, "gain"), cfg.gainDb);
    setFloat(apvts, Params::bandParamID(bandIndex, "q"), cfg.q);
    setFloat(apvts, Params::bandParamID(bandIndex, "slope"), cfg.slopeDbPerOct);
    setBool(apvts, Params::bandParamID(bandIndex, "brickwall"), cfg.brickwall);
    setBool(apvts, Params::bandParamID(bandIndex, "dynEnabled"), cfg.dynEnabled);
    if (cfg.dynEnabled && Params::typeSupportsDynamics(cfg.type))
    {
        setFloat(apvts, Params::bandParamID(bandIndex, "dynThreshold"), cfg.dynThresholdDb);
        setFloat(apvts, Params::bandParamID(bandIndex, "dynRange"), cfg.dynRangeDb);
        setFloat(apvts, Params::bandParamID(bandIndex, "dynRatio"), cfg.dynRatio);
        setFloat(apvts, Params::bandParamID(bandIndex, "dynAttack"), cfg.dynAttackMs);
        setFloat(apvts, Params::bandParamID(bandIndex, "dynRelease"), cfg.dynReleaseMs);
    }
}

juce::ValueTree captureFullState(juce::AudioProcessorValueTreeState& apvts)
{
    return apvts.copyState().createCopy();
}

void applyFullState(juce::AudioProcessorValueTreeState& apvts, const juce::ValueTree& state)
{
    if (state.isValid() && state.hasType(apvts.state.getType()))
        apvts.replaceState(state.createCopy());
}

} // namespace PresetState
