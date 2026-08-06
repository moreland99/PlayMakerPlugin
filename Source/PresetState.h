#pragma once

#include "Params.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace PresetState
{

void setBool(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, bool value);
void setFloat(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, float realValue);
void setChoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, int choiceIndex);

void disableAllBands(juce::AudioProcessorValueTreeState& apvts);

struct BandConfig
{
    bool enabled = true;
    Params::FilterType type = Params::FilterType::bell;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
    float slopeDbPerOct = 12.0f;
    bool brickwall = false;
    bool dynEnabled = false;
    float dynThresholdDb = -24.0f;
    float dynRangeDb = -12.0f;
    float dynRatio = 4.0f;
    float dynAttackMs = 10.0f;
    float dynReleaseMs = 100.0f;
};

void applyBand(juce::AudioProcessorValueTreeState& apvts, int bandIndex, const BandConfig& cfg);

juce::ValueTree captureFullState(juce::AudioProcessorValueTreeState& apvts);
void applyFullState(juce::AudioProcessorValueTreeState& apvts, const juce::ValueTree& state);

} // namespace PresetState
