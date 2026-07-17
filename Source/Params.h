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

// Order fixed for the plugin's lifetime, same reasoning as FilterType.
enum class StereoMode
{
    leftRight = 0,
    leftOnly,
    rightOnly,
    midSide,
    midOnly,
    sideOnly,
    numStereoModes
};

inline const juce::StringArray& stereoModeNames()
{
    static const juce::StringArray names {
        "Left/Right", "Left Only", "Right Only", "Mid/Side", "Mid Only", "Side Only"
    };
    return names;
}

// Order fixed for the plugin's lifetime, same reasoning as FilterType.
enum class PhaseMode
{
    zeroLatency = 0,
    lowLatencyCorrected,
    linearPhase,
    numPhaseModes
};

inline const juce::StringArray& phaseModeNames()
{
    static const juce::StringArray names { "Zero Latency", "Low-Latency Phase-Corrected", "Linear Phase" };
    return names;
}

inline const juce::StringArray& linearQualityNames()
{
    static const juce::StringArray names { "Low", "Medium", "High" };
    return names;
}

// Only these filter types have a gain parameter that dynamics can modulate.
inline bool typeSupportsDynamics(FilterType t)
{
    return t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf
        || t == FilterType::tiltShelf || t == FilterType::flatTilt;
}

// Slope (cascaded stages) only applies to the cut filters.
inline bool typeSupportsSlope(FilterType t)
{
    return t == FilterType::lowCut || t == FilterType::highCut;
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
    std::atomic<float>* stereoMode = nullptr;
    std::atomic<float>* balance = nullptr;
    std::atomic<float>* slope = nullptr;
    std::atomic<float>* brickwall = nullptr;
    std::atomic<float>* dynEnabled = nullptr;
    std::atomic<float>* dynThreshold = nullptr;
    std::atomic<float>* dynRange = nullptr;
    std::atomic<float>* dynRatio = nullptr;
    std::atomic<float>* dynAttack = nullptr;
    std::atomic<float>* dynRelease = nullptr;
    std::atomic<float>* dynRelativeBlend = nullptr;
    std::atomic<float>* dynSidechainBlend = nullptr;
};

struct GlobalParamPointers
{
    std::atomic<float>* phaseMode = nullptr;
    std::atomic<float>* linearQuality = nullptr;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace Params
