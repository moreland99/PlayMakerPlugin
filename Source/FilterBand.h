#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Params.h"

// One stereo-linked band: identical coefficients on both channels, separate filter state.
class FilterBand
{
public:
    void prepare(const juce::dsp::ProcessSpec& monoSpec)
    {
        left.prepare(monoSpec);
        right.prepare(monoSpec);
        sampleRate = monoSpec.sampleRate;
        reset();
    }

    void reset()
    {
        left.reset();
        right.reset();
    }

    void update(Params::FilterType type, float freqHz, float gainDb, float q)
    {
        if (sampleRate <= 0.0)
            return;

        const auto clampedFreq = juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), freqHz);
        const auto clampedQ = juce::jmax(0.1f, q);
        const auto linearGain = juce::Decibels::decibelsToGain(gainDb);

        juce::dsp::IIR::Coefficients<float>::Ptr newCoefficients;

        switch (type)
        {
            case Params::FilterType::bell:
                newCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, clampedFreq, clampedQ, linearGain);
                break;
            case Params::FilterType::lowShelf:
                newCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                break;
            case Params::FilterType::highShelf:
                newCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, clampedFreq, clampedQ, linearGain);
                break;
            case Params::FilterType::lowCut:
                newCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, clampedFreq, clampedQ);
                break;
            case Params::FilterType::highCut:
                newCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, clampedFreq, clampedQ);
                break;
            case Params::FilterType::notch:
            case Params::FilterType::bandPass:
            case Params::FilterType::allPass:
            case Params::FilterType::tiltShelf:
            case Params::FilterType::flatTilt:
            case Params::FilterType::numFilterTypes:
            default:
                // Not implemented until Phase 6 — pass signal through unchanged.
                newCoefficients = new juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
                break;
        }

        *left.coefficients = *newCoefficients;
        *right.coefficients = *newCoefficients;
    }

    void processLeft(const juce::dsp::ProcessContextReplacing<float>& context) { left.process(context); }
    void processRight(const juce::dsp::ProcessContextReplacing<float>& context) { right.process(context); }

private:
    juce::dsp::IIR::Filter<float> left, right;
    double sampleRate = 0.0;
};
