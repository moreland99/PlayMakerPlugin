#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{

constexpr int numBands = 24;

// Order is fixed for the plugin's lifetime: AudioParameterChoice index == FilterType,
// and that index is what hosts store in automation/session state.
enum class FilterType
{
    bell = 0,
    lowShelf,
    highShelf,
    lowCut,
    highCut,
    notch,
    bandPass,
    allPass,
    tiltShelf,
    flatTilt,
    numFilterTypes
};

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names {
        "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut",
        "Notch", "Band Pass", "All Pass", "Tilt Shelf", "Flat Tilt"
    };
    return names;
}

inline juce::String bandParamID(int bandIndex, const juce::String& suffix)
{
    return "band" + juce::String(bandIndex) + "_" + suffix;
}

struct BandParamPointers
{
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* type = nullptr;
    std::atomic<float>* freq = nullptr;
    std::atomic<float>* gain = nullptr;
    std::atomic<float>* q = nullptr;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace Params
