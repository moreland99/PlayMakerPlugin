#pragma once

#include <juce_dsp/juce_dsp.h>
#include <complex>
#include "Params.h"

// One stereo-linked band: identical coefficients on both channels, separate filter state.
// A band is a cascade of up to 8 second-order sections. Cut filters use the cascade for
// slope (12..96 dB/oct); tilt types use two fixed stages; everything else uses one stage.
class FilterBand
{
public:
    static constexpr int maxStages = 8;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using ArrayCoeffs = juce::dsp::IIR::ArrayCoefficients<float>;

    struct StageSet
    {
        std::array<std::array<float, 6>, maxStages> coeffs {};
        int numStages = 1;
    };

    // Allocation-free: writes biquad arrays into `out`. Shared by the audio thread
    // and the UI thread (curve drawing) without locking.
    static void assignStages(StageSet& out, Params::FilterType type, double sampleRate,
                             float freqHz, float gainDb, float q,
                             float slopeDbPerOct = 12.0f, bool brickwall = false)
    {
        out.numStages = 1;
        for (auto& c : out.coeffs)
            c = identityArray();

        if (sampleRate <= 0.0)
            return;

        const auto clampedFreq = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), freqHz);
        const auto clampedQ = juce::jmax(0.1f, q);
        const auto linearGain = juce::Decibels::decibelsToGain(gainDb);
        const auto halfGain = juce::Decibels::decibelsToGain(gainDb * 0.5f);
        const auto invHalfGain = juce::Decibels::decibelsToGain(-gainDb * 0.5f);

        switch (type)
        {
            case Params::FilterType::bell:
                out.coeffs[0] = ArrayCoeffs::makePeakFilter(sampleRate, clampedFreq, clampedQ, linearGain);
                return;
            case Params::FilterType::lowShelf:
                out.coeffs[0] = ArrayCoeffs::makeLowShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                return;
            case Params::FilterType::highShelf:
                out.coeffs[0] = ArrayCoeffs::makeHighShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                return;
            case Params::FilterType::notch:
                out.coeffs[0] = ArrayCoeffs::makeNotch(sampleRate, clampedFreq, clampedQ);
                return;
            case Params::FilterType::bandPass:
                out.coeffs[0] = ArrayCoeffs::makeBandPass(sampleRate, clampedFreq, clampedQ);
                return;
            case Params::FilterType::allPass:
                out.coeffs[0] = ArrayCoeffs::makeAllPass(sampleRate, clampedFreq, clampedQ);
                return;

            case Params::FilterType::tiltShelf:
                // Pivot around freq: cut below, boost above (positive gain tilts up).
                out.coeffs[0] = ArrayCoeffs::makeLowShelf(sampleRate, clampedFreq, clampedQ, invHalfGain);
                out.coeffs[1] = ArrayCoeffs::makeHighShelf(sampleRate, clampedFreq, clampedQ, halfGain);
                out.numStages = 2;
                return;

            case Params::FilterType::flatTilt:
            {
                // Tilt that flattens at the spectrum extremes: wide-spaced gentle shelves around freq.
                const auto lowCorner = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), clampedFreq * 0.25f);
                const auto highCorner = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), clampedFreq * 4.0f);
                out.coeffs[0] = ArrayCoeffs::makeLowShelf(sampleRate, lowCorner, 0.5f, invHalfGain);
                out.coeffs[1] = ArrayCoeffs::makeHighShelf(sampleRate, highCorner, 0.5f, halfGain);
                out.numStages = 2;
                return;
            }

            case Params::FilterType::lowCut:
            case Params::FilterType::highCut:
            {
                // Brickwall is its own design path: a full 16th-order Butterworth (8 sections with
                // the proper pole-Q distribution), not just the top of the slope range.
                const int numStages = brickwall
                    ? maxStages
                    : juce::jlimit(1, maxStages, juce::roundToInt(slopeDbPerOct / 12.0f));
                out.numStages = numStages;

                const int order = 2 * numStages;
                for (int k = 0; k < numStages; ++k)
                {
                    // Butterworth section Qs: 1 / (2 cos((2k+1)π / (2n))) for maximal flatness.
                    float stageQ = 1.0f / (2.0f * std::cos(((2.0f * (float) k + 1.0f) * juce::MathConstants<float>::pi)
                                                            / (2.0f * (float) order)));
                    // User Q scales the resonant (last) section only, unless brickwall pins the design.
                    if (!brickwall && k == numStages - 1)
                        stageQ *= clampedQ / 0.707f;

                    out.coeffs[(size_t) k] = type == Params::FilterType::lowCut
                        ? ArrayCoeffs::makeHighPass(sampleRate, clampedFreq, stageQ)
                        : ArrayCoeffs::makeLowPass(sampleRate, clampedFreq, stageQ);
                }
                return;
            }

            case Params::FilterType::numFilterTypes:
            default:
                return;
        }
    }

    static StageSet computeStages(Params::FilterType type, double sampleRate,
                                   float freqHz, float gainDb, float q,
                                   float slopeDbPerOct = 12.0f, bool brickwall = false)
    {
        StageSet set;
        assignStages(set, type, sampleRate, freqHz, gainDb, q, slopeDbPerOct, brickwall);
        return set;
    }

    // Composite magnitude across all stages — the UI / FIR-design curve helper.
    static double getMagnitudeForFrequency(const StageSet& set, double probeFreq, double sampleRate)
    {
        double magnitude = 1.0;
        for (int k = 0; k < set.numStages; ++k)
            magnitude *= biquadMagnitude(set.coeffs[(size_t) k], probeFreq, sampleRate);
        return magnitude;
    }

    static double getMagnitudeForFrequency(Params::FilterType type, double sampleRate,
                                            float freqHz, float gainDb, float q,
                                            float slopeDbPerOct, bool brickwall, double probeFreq)
    {
        StageSet set;
        assignStages(set, type, sampleRate, freqHz, gainDb, q, slopeDbPerOct, brickwall);
        return getMagnitudeForFrequency(set, probeFreq, sampleRate);
    }

    void prepare(const juce::dsp::ProcessSpec& monoSpec)
    {
        sampleRate = monoSpec.sampleRate;
        lastApplied.valid = false;

        for (auto& f : left)
        {
            f.prepare(monoSpec);
            *f.coefficients = identityArray();
        }
        for (auto& f : right)
        {
            f.prepare(monoSpec);
            *f.coefficients = identityArray();
        }
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

        if (lastApplied.matches(type, freqHz, gainDb, q, slopeDbPerOct, brickwall))
            return;

        StageSet set;
        assignStages(set, type, sampleRate, freqHz, gainDb, q, slopeDbPerOct, brickwall);

        // Reset state on stage-count changes so newly activated stages don't ring with stale state.
        if (set.numStages != activeStages)
        {
            activeStages = set.numStages;
            reset();
        }

        for (int k = 0; k < activeStages; ++k)
        {
            *left[(size_t) k].coefficients = set.coeffs[(size_t) k];
            *right[(size_t) k].coefficients = set.coeffs[(size_t) k];
        }

        lastApplied = { type, freqHz, gainDb, q, slopeDbPerOct, brickwall, true };
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
    static constexpr float paramEpsilon = 1.0e-5f;

    struct AppliedParams
    {
        Params::FilterType type = Params::FilterType::bell;
        float freqHz = -1.0f;
        float gainDb = 0.0f;
        float q = -1.0f;
        float slopeDbPerOct = -1.0f;
        bool brickwall = false;
        bool valid = false;

        bool matches(Params::FilterType t, float freq, float gain, float qValue,
                     float slope, bool brick) const
        {
            return valid
                && type == t
                && brickwall == brick
                && std::abs(freqHz - freq) < paramEpsilon
                && std::abs(gainDb - gain) < paramEpsilon
                && std::abs(q - qValue) < paramEpsilon
                && std::abs(slopeDbPerOct - slope) < paramEpsilon;
        }
    };

    static std::array<float, 6> identityArray()
    {
        return { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    }

    static double biquadMagnitude(const std::array<float, 6>& c, double probeFreq, double sampleRate)
    {
        if (sampleRate <= 0.0)
            return 1.0;

        const double freq = juce::jlimit(0.0, sampleRate * 0.5, probeFreq);
        const std::complex<double> jw = std::exp(std::complex<double>(
            0.0, -juce::MathConstants<double>::twoPi * freq / sampleRate));

        std::complex<double> z { 1.0, 0.0 };
        std::complex<double> num { 0.0, 0.0 };
        std::complex<double> den { 0.0, 0.0 };

        for (int n = 0; n < 3; ++n)
        {
            num += (double) c[(size_t) n] * z;
            den += (double) c[(size_t) n + 3] * z;
            z *= jw;
        }

        const double denMag = std::abs(den);
        return denMag > 1.0e-30 ? std::abs(num) / denMag : 0.0;
    }

    std::array<juce::dsp::IIR::Filter<float>, maxStages> left, right;
    int activeStages = 1;
    double sampleRate = 0.0;
    AppliedParams lastApplied;
};
