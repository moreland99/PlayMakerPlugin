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

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(bandParamID(i, "stereoMode"), 1),
            "Band " + juce::String(i + 1) + " Stereo Mode",
            stereoModeNames(),
            static_cast<int>(StereoMode::leftRight)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "balance"), 1),
            "Band " + juce::String(i + 1) + " Balance",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
            0.0f,
            juce::AudioParameterFloatAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "slope"), 1),
            "Band " + juce::String(i + 1) + " Slope",
            juce::NormalisableRange<float>(12.0f, 96.0f, 0.1f),
            12.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB/oct")));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandParamID(i, "brickwall"), 1),
            "Band " + juce::String(i + 1) + " Brickwall",
            false));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandParamID(i, "dynEnabled"), 1),
            "Band " + juce::String(i + 1) + " Dynamic",
            false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynThreshold"), 1),
            "Band " + juce::String(i + 1) + " Dyn Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
            -24.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynRange"), 1),
            "Band " + juce::String(i + 1) + " Dyn Range",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
            -12.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynRatio"), 1),
            "Band " + juce::String(i + 1) + " Dyn Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.5f),
            4.0f,
            juce::AudioParameterFloatAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynAttack"), 1),
            "Band " + juce::String(i + 1) + " Dyn Attack",
            juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.4f),
            10.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynRelease"), 1),
            "Band " + juce::String(i + 1) + " Dyn Release",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.4f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynRelativeBlend"), 1),
            "Band " + juce::String(i + 1) + " Dyn Relative Blend",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.0f,
            juce::AudioParameterFloatAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandParamID(i, "dynSidechainBlend"), 1),
            "Band " + juce::String(i + 1) + " Dyn Sidechain Blend",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.0f,
            juce::AudioParameterFloatAttributes()));
    }

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("phaseMode", 1),
        "Phase Mode",
        phaseModeNames(),
        static_cast<int>(PhaseMode::zeroLatency)));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("linearQuality", 1),
        "Linear Phase Quality",
        linearQualityNames(),
        1));

    return { params.begin(), params.end() };
}

} // namespace Params
