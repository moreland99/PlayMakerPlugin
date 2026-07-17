#include "Params.h"

namespace Params
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::NormalisableRange<float> freqRange(20.0f, 20000.0f, 0.01f, 1.0f);
    freqRange.setSkewForCentre(1000.0f);

    juce::NormalisableRange<float> gainRange(-24.0f, 24.0f, 0.01f);
    juce::NormalisableRange<float> qRange(0.1f, 18.0f, 0.001f);
    qRange.setSkewForCentre(0.707f);

    for (int i = 0; i < numBands; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandParamID(i, "enabled"), 1),
            "Band " + juce::String(i + 1) + " Enabled",
            false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(bandParamID(i, "type"), 1),
            "Band " + juce::String(i + 1) + " Type",
            filterTypeNames(),
            static_cast<int>(FilterType::bell)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "freq"), 1),
            "Band " + juce::String(i + 1) + " Frequency",
            freqRange,
            1000.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "gain"), 1),
            "Band " + juce::String(i + 1) + " Gain",
            gainRange,
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "q"), 1),
            "Band " + juce::String(i + 1) + " Q",
            qRange,
            0.707f,
            juce::AudioParameterFloatAttributes()));
    }

    return { params.begin(), params.end() };
}

} // namespace Params
