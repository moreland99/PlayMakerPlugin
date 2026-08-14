#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Params.h"

// Per-band dynamic-EQ detector. Listens to a mono detector signal (main input blended with
// the external sidechain), bandpass-filtered around the band's frequency, and produces a
// gain offset in dB that the processor adds to the band's static gain each block.
//
// The offset is computed once per block (block-rate modulation, not per-sample). With typical
// host block sizes (~1-10 ms) that is well inside the attack/release ranges offered, and it
// lets the existing coefficient-update path apply the modulation with zero extra machinery.
class DynamicBandDetector
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        bandFilter.prepare({ newSampleRate, 8192, 1 });
        reset();
    }

    void reset()
    {
        bandFilter.reset();
        directEnv = 0.0f;
        referenceEnv = 0.0f;
    }

    struct Settings
    {
        float freqHz = 1000.0f;
        float q = 0.707f;
        float thresholdDb = -24.0f;
        float rangeDb = -12.0f;      // sign = direction of gain change; magnitude = max change
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float relativeBlend = 0.0f;  // 0 = direct level, 1 = level relative to full-mix reference
    };

    // detectorMono: main input (already blended with sidechain by the caller).
    // Returns the gain offset in dB to add to the band's static gain for this block.
    float processBlock(const float* detectorMono, int numSamples, const Settings& s)
    {
        if (sampleRate <= 0.0 || numSamples <= 0)
            return 0.0f;

        *bandFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(
            sampleRate, juce::jlimit(20.0f, (float) (sampleRate * 0.5 - 20.0), s.freqHz), juce::jmax(0.1f, s.q));

        const float attackAlpha = envelopeAlpha(s.attackMs);
        const float releaseAlpha = envelopeAlpha(s.releaseMs);

        for (int n = 0; n < numSamples; ++n)
        {
            const float raw = detectorMono[n];
            const float banded = bandFilter.processSample(raw);

            follow(directEnv, std::abs(banded), attackAlpha, releaseAlpha);
            follow(referenceEnv, std::abs(raw), attackAlpha, releaseAlpha);
        }

        const float directDb = juce::Decibels::gainToDecibels(directEnv, -100.0f);
        const float referenceDb = juce::Decibels::gainToDecibels(referenceEnv, -100.0f);

        // Relative mode judges the band's level against the full-mix reference envelope;
        // blend interpolates between absolute (direct) and relative detection.
        const float detectionDb = directDb - s.relativeBlend * referenceDb;
        lastDetectionDb = detectionDb;

        const float over = detectionDb - s.thresholdDb;
        if (over <= 0.0f)
            return 0.0f;

        const float ratio = juce::jmax(1.0f, s.ratio);
        const float changeDb = juce::jmin(over * (1.0f - 1.0f / ratio), std::abs(s.rangeDb));
        return s.rangeDb < 0.0f ? -changeDb : changeDb;
    }

    float getLastDetectionDb() const { return lastDetectionDb; }

private:
    float envelopeAlpha(float timeMs) const
    {
        const auto samples = juce::jmax(1.0, sampleRate * (double) timeMs * 0.001);
        return (float) std::exp(-1.0 / samples);
    }

    static void follow(float& env, float level, float attackAlpha, float releaseAlpha)
    {
        const float alpha = level > env ? attackAlpha : releaseAlpha;
        env = alpha * env + (1.0f - alpha) * level;
    }

    juce::dsp::IIR::Filter<float> bandFilter;
    float directEnv = 0.0f;
    float referenceEnv = 0.0f;
    float lastDetectionDb = -100.0f;
    double sampleRate = 0.0;
};
