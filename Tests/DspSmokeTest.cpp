#include <juce_dsp/juce_dsp.h>
#include "../Source/FilterBand.h"

namespace
{
double rmsOfSine(FilterBand& band, double sampleRate, double toneHz, int numSamples)
{
    juce::AudioBuffer<float> buffer(1, numSamples);
    for (int n = 0; n < numSamples; ++n)
        buffer.setSample(0, n, (float) std::sin(juce::MathConstants<double>::twoPi * toneHz * (double) n / sampleRate));

    juce::dsp::AudioBlock<float> block(buffer);
    band.processLeft(juce::dsp::ProcessContextReplacing<float>(block));

    // Discard the filter's settling region, measure steady state.
    const int settle = numSamples / 2;
    double sumSq = 0.0;
    for (int n = settle; n < numSamples; ++n)
    {
        auto s = buffer.getSample(0, n);
        sumSq += (double) s * (double) s;
    }
    return std::sqrt(sumSq / (numSamples - settle));
}

bool near(double value, double target, double tolerance)
{
    return std::abs(value - target) <= tolerance;
}
}

int main()
{
    const double sampleRate = 44100.0;
    const int blockSize = 8192;
    int failures = 0;

    auto runCaseFull = [&](const char* name, Params::FilterType type, float freq, float gainDb, float q,
                            float slope, bool brickwall,
                            double probeHz, double expectedGainLinear, double tolerance)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };

        // Unprocessed reference band left at its default identity coefficients.
        FilterBand referenceBand;
        referenceBand.prepare(spec);
        const double inputRms = rmsOfSine(referenceBand, sampleRate, probeHz, blockSize);

        FilterBand testBand;
        testBand.prepare(spec);
        testBand.update(type, freq, gainDb, q, slope, brickwall);
        const double outputRms = rmsOfSine(testBand, sampleRate, probeHz, blockSize);

        const double measuredGain = outputRms / inputRms;
        const bool pass = near(measuredGain, expectedGainLinear, tolerance);
        std::printf("[%s] probe=%.1fHz expectedGain=%.3f measuredGain=%.3f -> %s\n",
                     name, probeHz, expectedGainLinear, measuredGain, pass ? "PASS" : "FAIL");
        if (!pass)
            ++failures;
    };

    auto runCase = [&](const char* name, Params::FilterType type, float freq, float gainDb, float q,
                        double probeHz, double expectedGainLinear, double tolerance)
    {
        runCaseFull(name, type, freq, gainDb, q, 12.0f, false, probeHz, expectedGainLinear, tolerance);
    };

    // Bell +12dB @ 1kHz, Q=1: at the center frequency, gain should approach the full linear boost.
    const double bellGainLinear = juce::Decibels::decibelsToGain(12.0f);
    runCase("Bell @ center", Params::FilterType::bell, 1000.0f, 12.0f, 1.0f, 1000.0, bellGainLinear, 0.15 * bellGainLinear);
    // Same Bell, probed two octaves away: should be close to unity (untouched).
    runCase("Bell far from center", Params::FilterType::bell, 1000.0f, 12.0f, 1.0f, 100.0, 1.0, 0.15);

    // Low cut (high-pass) at 1kHz: a low probe tone well below cutoff should be heavily attenuated.
    runCase("Low cut below cutoff", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 100.0, 0.1, 0.1);
    // Low cut: a tone well above cutoff should pass close to unity.
    runCase("Low cut above cutoff", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 8000.0, 1.0, 0.2);

    // High cut (low-pass) at 1kHz: a high probe tone well above cutoff should be heavily attenuated.
    runCase("High cut above cutoff", Params::FilterType::highCut, 1000.0f, 0.0f, 0.707f, 8000.0, 0.1, 0.1);
    // High cut: a tone well below cutoff should pass close to unity.
    runCase("High cut below cutoff", Params::FilterType::highCut, 1000.0f, 0.0f, 0.707f, 100.0, 1.0, 0.2);

    // Low shelf +12dB @ 200Hz: a tone well below the corner should be boosted close to the full shelf gain.
    const double shelfGainLinear = juce::Decibels::decibelsToGain(12.0f);
    runCase("Low shelf below corner", Params::FilterType::lowShelf, 200.0f, 12.0f, 0.707f, 40.0, shelfGainLinear, 0.2 * shelfGainLinear);
    // Low shelf: a tone well above the corner should be close to unity (unshelved region).
    runCase("Low shelf above corner", Params::FilterType::lowShelf, 200.0f, 12.0f, 0.707f, 5000.0, 1.0, 0.2);

    // High shelf +12dB @ 5kHz: a tone well above the corner should be boosted close to the full shelf gain.
    runCase("High shelf above corner", Params::FilterType::highShelf, 5000.0f, 12.0f, 0.707f, 15000.0, shelfGainLinear, 0.2 * shelfGainLinear);
    // High shelf: a tone well below the corner should be close to unity.
    runCase("High shelf below corner", Params::FilterType::highShelf, 5000.0f, 12.0f, 0.707f, 200.0, 1.0, 0.2);

    // Notch @ 1kHz: the center tone should be nulled, an octave away should pass.
    runCase("Notch @ center", Params::FilterType::notch, 1000.0f, 0.0f, 2.0f, 1000.0, 0.0, 0.05);
    runCase("Notch off center", Params::FilterType::notch, 1000.0f, 0.0f, 2.0f, 4000.0, 1.0, 0.15);

    // Band pass @ 1kHz: center passes near unity, two octaves away is attenuated.
    runCase("Band pass @ center", Params::FilterType::bandPass, 1000.0f, 0.0f, 1.0f, 1000.0, 1.0, 0.15);
    runCase("Band pass off center", Params::FilterType::bandPass, 1000.0f, 0.0f, 1.0f, 100.0, 0.1, 0.1);

    // All pass @ 1kHz: magnitude untouched everywhere.
    runCase("All pass @ center", Params::FilterType::allPass, 1000.0f, 0.0f, 1.0f, 1000.0, 1.0, 0.1);

    // Tilt shelf +12dB @ 1kHz: about -6dB well below the pivot, about +6dB well above it.
    const double halfDown = juce::Decibels::decibelsToGain(-6.0f);
    const double halfUp = juce::Decibels::decibelsToGain(6.0f);
    runCase("Tilt shelf low side", Params::FilterType::tiltShelf, 1000.0f, 12.0f, 0.707f, 60.0, halfDown, 0.15);
    runCase("Tilt shelf high side", Params::FilterType::tiltShelf, 1000.0f, 12.0f, 0.707f, 15000.0, halfUp, 0.3);

    // Flat tilt must not be identity: the low side of a +12dB tilt should sit clearly below unity.
    runCase("Flat tilt low side", Params::FilterType::flatTilt, 1000.0f, 12.0f, 0.707f, 40.0, halfDown, 0.2);

    // Slope: 96 dB/oct low cut is drastically steeper than 12 dB/oct one octave below cutoff.
    runCaseFull("Low cut 96dB/oct", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 96.0f, false, 500.0, 0.0, 0.01);
    runCaseFull("Low cut 96dB/oct passband", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 96.0f, false, 8000.0, 1.0, 0.2);

    // Brickwall low cut: same steep rejection via the 16th-order Butterworth design path.
    runCaseFull("Brickwall low cut stopband", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 12.0f, true, 500.0, 0.0, 0.01);
    runCaseFull("Brickwall low cut passband", Params::FilterType::lowCut, 1000.0f, 0.0f, 0.707f, 12.0f, true, 8000.0, 1.0, 0.2);

    if (failures == 0)
    {
        std::printf("\nALL DSP SMOKE TESTS PASSED\n");
        return 0;
    }

    std::printf("\n%d DSP SMOKE TEST(S) FAILED\n", failures);
    return 1;
}
