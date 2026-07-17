#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Params.h"

// One stereo-linked band: identical coefficients on both channels, separate filter state.
// A band is a cascade of up to 8 second-order sections. Cut filters use the cascade for
// slope (12..96 dB/oct); tilt types use two fixed stages; everything else uses one stage.
class FilterBand
{
public:
    static constexpr int maxStages = 8;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    struct StageSet
    {
        std::array<Coeffs::Ptr, maxStages> stages;
        int numStages = 1;
    };

    // Stateless — shared by the audio thread and the UI thread (curve drawing) without locking.
    static StageSet computeStages(Params::FilterType type, double sampleRate,
                                   float freqHz, float gainDb, float q,
                                   float slopeDbPerOct = 12.0f, bool brickwall = false)
    {
        StageSet set;

        if (sampleRate <= 0.0)
        {
            set.stages[0] = identityCoefficients();
            return set;
        }

        const auto clampedFreq = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), freqHz);
        const auto clampedQ = juce::jmax(0.1f, q);
        const auto linearGain = juce::Decibels::decibelsToGain(gainDb);
        const auto halfGain = juce::Decibels::decibelsToGain(gainDb * 0.5f);
        const auto invHalfGain = juce::Decibels::decibelsToGain(-gainDb * 0.5f);

        switch (type)
        {
            case Params::FilterType::bell:
                set.stages[0] = Coeffs::makePeakFilter(sampleRate, clampedFreq, clampedQ, linearGain);
                return set;
            case Params::FilterType::lowShelf:
                set.stages[0] = Coeffs::makeLowShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                return set;
            case Params::FilterType::highShelf:
                set.stages[0] = Coeffs::makeHighShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                return set;
            case Params::FilterType::notch:
                set.stages[0] = Coeffs::makeNotch(sampleRate, clampedFreq, clampedQ);
                return set;
            case Params::FilterType::bandPass:
                set.stages[0] = Coeffs::makeBandPass(sampleRate, clampedFreq, clampedQ);
                return set;
            case Params::FilterType::allPass:
                set.stages[0] = Coeffs::makeAllPass(sampleRate, clampedFreq, clampedQ);
                return set;

            case Params::FilterType::tiltShelf:
                // Pivot around freq: cut below, boost above (positive gain tilts up).
                set.stages[0] = Coeffs::makeLowShelf(sampleRate, clampedFreq, clampedQ, invHalfGain);
                set.stages[1] = Coeffs::makeHighShelf(sampleRate, clampedFreq, clampedQ, halfGain);
                set.numStages = 2;
                return set;

            case Params::FilterType::flatTilt:
            {
                // Tilt that flattens at the spectrum extremes: wide-spaced gentle shelves around freq.
                const auto lowCorner = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), clampedFreq * 0.25f);
                const auto highCorner = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), clampedFreq * 4.0f);
                set.stages[0] = Coeffs::makeLowShelf(sampleRate, lowCorner, 0.5f, invHalfGain);
                set.stages[1] = Coeffs::makeHighShelf(sampleRate, highCorner, 0.5f, halfGain);
                set.numStages = 2;
                return set;
            }

            case Params::FilterType::lowCut:
            case Params::FilterType::highCut:
            {
                // Brickwall is its own design path: a full 16th-order Butterworth (8 sections with
                // the proper pole-Q distribution), not just the top of the slope range.
                const int numStages = brickwall
                    ? maxStages
                    : juce::jlimit(1, maxStages, juce::roundToInt(slopeDbPerOct / 12.0f));
                set.numStages = numStages;

                const int order = 2 * numStages;
                for (int k = 0; k < numStages; ++k)
                {
                    // Butterworth section Qs: 1 / (2 cos((2k+1)π / (2n))) for maximal flatness.
                    float stageQ = 1.0f / (2.0f * std::cos(((2.0f * (float) k + 1.0f) * juce::MathConstants<float>::pi)
                                                            / (2.0f * (float) order)));
                    // User Q scales the resonant (last) section only, unless brickwall pins the design.
                    if (!brickwall && k == numStages - 1)
                        stageQ *= clampedQ / 0.707f;

                    set.stages[(size_t) k] = type == Params::FilterType::lowCut
                        ? Coeffs::makeHighPass(sampleRate, clampedFreq, stageQ)
                        : Coeffs::makeLowPass(sampleRate, clampedFreq, stageQ);
                }
                return set;
            }

            case Params::FilterType::numFilterTypes:
            default:
                set.stages[0] = identityCoefficients();
                return set;
        }
    }

    // Composite magnitude across all stages — the UI curve helper.
    static double getMagnitudeForFrequency(const StageSet& set, double probeFreq, double sampleRate)
    {
        double magnitude = 1.0;
        for (int k = 0; k < set.numStages; ++k)
            if (set.stages[(size_t) k] != nullptr)
                magnitude *= set.stages[(size_t) k]->getMagnitudeForFrequency(probeFreq, sampleRate);
        return magnitude;
    }

    static double getMagnitudeForFrequency(Params::FilterType type, double sampleRate,
                                            float freqHz, float gainDb, float q,
                                            float slopeDbPerOct, bool brickwall, double probeFreq)
    {
        return getMagnitudeForFrequency(computeStages(type, sampleRate, freqHz, gainDb, q, slopeDbPerOct, brickwall),
                                         probeFreq, sampleRate);
    }

    void prepare(const juce::dsp::ProcessSpec& monoSpec)
    {
        for (auto& f : left) f.prepare(monoSpec);
        for (auto& f : right) f.prepare(monoSpec);
        sampleRate = monoSpec.sampleRate;
        reset();
    }

    void reset()
    {
        for (auto& f : left) f.reset();
        for (auto& f : right) f.reset();
    }

    void update(Params::FilterType type, float freqHz, float gainDb, float q,
                float slopeDbPerOct = 12.0f, bool brickwall = false)
    {
        if (sampleRate <= 0.0)
            return;

        auto set = computeStages(type, sampleRate, freqHz, gainDb, q, slopeDbPerOct, brickwall);

        // Reset state on stage-count changes so newly activated stages don't ring with stale state.
        if (set.numStages != activeStages)
        {
            activeStages = set.numStages;
            reset();
        }

        for (int k = 0; k < activeStages; ++k)
        {
            *left[(size_t) k].coefficients = *set.stages[(size_t) k];
            *right[(size_t) k].coefficients = *set.stages[(size_t) k];
        }
    }

    void processLeft(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        for (int k = 0; k < activeStages; ++k)
            left[(size_t) k].process(context);
    }

    void processRight(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        for (int k = 0; k < activeStages; ++k)
            right[(size_t) k].process(context);
    }

private:
    static Coeffs::Ptr identityCoefficients()
    {
        return new Coeffs(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    }

    std::array<juce::dsp::IIR::Filter<float>, maxStages> left, right;
    int activeStages = 1;
    double sampleRate = 0.0;
};
