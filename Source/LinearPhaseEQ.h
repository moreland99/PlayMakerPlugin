#pragma once

#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <vector>

// FIR path for the phase-corrected / linear-phase modes: builds a symmetric (linear-phase)
// FIR whose magnitude matches the combined static EQ curve, and runs it through JUCE's
// partitioned convolution. Latency is the FIR's group delay, numTaps / 2.
class LinearPhaseEQ
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        channels = (int) spec.numChannels;
        convolution.prepare(spec);
        prepared = true;
    }

    void reset() { convolution.reset(); }

    // Odd tap counts keep the impulse symmetric around a single center sample.
    void setNumTaps(int taps) { numTaps = juce::jmax(3, taps | 1); }
    int getNumTaps() const { return numTaps; }
    int getLatencySamples() const { return numTaps / 2; }

    // Builds the FIR from a magnitude callback and hands it to the convolver.
    // Safe to call from the message thread while audio runs — juce::dsp::Convolution
    // crossfades to the new impulse on a background thread.
    void updateResponse(double sampleRate, const std::function<double(double)>& magnitudeAtFreq)
    {
        if (!prepared || sampleRate <= 0.0)
            return;

        int fftOrder = 1;
        while ((1 << fftOrder) < numTaps * 2)
            ++fftOrder;
        const int fftSize = 1 << fftOrder;

        juce::dsp::FFT fft(fftOrder);
        std::vector<float> freqData((size_t) fftSize * 2, 0.0f);

        // Zero-phase spectrum: real = desired magnitude, imaginary = 0.
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            const auto freq = (double) k * sampleRate / (double) fftSize;
            freqData[(size_t) k * 2] = (float) magnitudeAtFreq(freq);
        }

        fft.performRealOnlyInverseTransform(freqData.data());

        // The zero-phase impulse is circularly centred on sample 0; rotate it so the
        // peak sits at the FIR's center tap, then window to control truncation ripple.
        juce::AudioBuffer<float> impulse(1, numTaps);
        auto* h = impulse.getWritePointer(0);
        const int half = numTaps / 2;
        for (int i = 0; i < numTaps; ++i)
        {
            const int src = (i - half + fftSize) % fftSize;
            const float window = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                          * (float) i / (float) (numTaps - 1)));
            h[i] = freqData[(size_t) src] * window;
        }

        convolution.loadImpulseResponse(std::move(impulse), sampleRate,
                                         channels > 1 ? juce::dsp::Convolution::Stereo::yes
                                                      : juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);
    }

    void process(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        if (prepared)
            convolution.process(context);
    }

private:
    juce::dsp::Convolution convolution;
    int numTaps = 2047;
    int channels = 2;
    bool prepared = false;
};
