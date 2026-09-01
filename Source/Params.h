#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

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

// True-log frequency map (20 Hz–20 kHz). Shared by the Freq knob, DSP parameter,
// analyzer node X, and the frequency grid so they never disagree.
constexpr float minFreqHz = 20.0f;
constexpr float maxFreqHz = 20000.0f;

inline float freqToNorm(float freqHz)
{
    const double hz = juce::jlimit((double) minFreqHz, (double) maxFreqHz, (double) freqHz);
    const double n = std::log(hz / (double) minFreqHz) / std::log((double) maxFreqHz / (double) minFreqHz);
    return (float) juce::jlimit(0.0, 1.0, n);
}

inline float normToFreq(float norm)
{
    const double n = juce::jlimit(0.0, 1.0, (double) norm);
    return (float) ((double) minFreqHz * std::pow((double) maxFreqHz / (double) minFreqHz, n));
}

inline juce::NormalisableRange<float> frequencyRange()
{
    return {
        minFreqHz,
        maxFreqHz,
        [] (float start, float end, float proportion) -> float
        {
            const double n = juce::jlimit(0.0, 1.0, (double) proportion);
            return (float) ((double) start * std::pow((double) end / (double) start, n));
        },
        [] (float start, float end, float value) -> float
        {
            const double hz = juce::jlimit((double) start, (double) end, (double) value);
            const double n = std::log(hz / (double) start) / std::log((double) end / (double) start);
            return (float) juce::jlimit(0.0, 1.0, n);
        }
    };
}

inline juce::String formatFrequency(float freqHz)
{
    if (freqHz >= 1000.0f)
        return juce::String(freqHz / 1000.0f, freqHz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(freqHz, 1) + " Hz";
}

inline juce::String bandParamID(int bandIndex, const juce::String& suffix)
{
    return "band" + juce::String(bandIndex) + "_" + suffix;
}

struct BandParamPointers
{
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* solo = nullptr;
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
    std::atomic<float>* dynAutoThreshold = nullptr;
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
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* pluginBypass = nullptr;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace Params
