#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Params.h"
#include "FilterBand.h"

// Lock-free-ish single-producer/single-consumer FFT capture: audio thread pushes samples,
// UI thread polls for a completed block. A brief data race on the "ready" flag is an
// accepted tradeoff here since this only ever feeds a visual display.
class AnalyzerDataProvider
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    AnalyzerDataProvider() : fft(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (fifoIndex == fftSize)
            {
                if (!nextBlockReady)
                {
                    std::copy(fifo.begin(), fifo.end(), fftData.begin());
                    nextBlockReady = true;
                }
                fifoIndex = 0;
            }
            fifo[(size_t) fifoIndex++] = data[i];
        }
    }

    // Fills magnitudesDb (size fftSize/2) with dB magnitudes if a new block was ready; returns false otherwise.
    bool getMagnitudesDb(std::array<float, fftSize / 2>& magnitudesDb)
    {
        if (!nextBlockReady)
            return false;
        nextBlockReady = false;

        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        for (int i = 0; i < fftSize / 2; ++i)
            magnitudesDb[(size_t) i] = juce::Decibels::gainToDecibels(fftData[(size_t) i], -100.0f);

        return true;
    }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, (size_t) fftSize * 2> fftData {};
    std::array<float, (size_t) fftSize> fifo {};
    int fifoIndex = 0;
    bool nextBlockReady = false;
};

// Read-only: draws the post-EQ spectrum plus each active band's frequency-response curve.
// No interaction yet (that's Phase 5).
class SpectrumAnalyzerComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzerComponent(juce::AudioProcessorValueTreeState& stateToRead,
                               AnalyzerDataProvider& analyzerToRead,
                               double& sampleRateToRead)
        : apvts(stateToRead), analyzer(analyzerToRead), sampleRate(sampleRateToRead)
    {
        startTimerHz(30);
    }

    ~SpectrumAnalyzerComponent() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff1a1a1e));

        drawGrid(g, bounds);
        drawSpectrum(g, bounds);
        drawBandCurves(g, bounds);
    }

private:
    void timerCallback() override
    {
        if (analyzer.getMagnitudesDb(latestMagnitudesDb))
            repaint();
    }

    static float freqToX(float freq, float width)
    {
        constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
        const auto norm = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
        return norm * width;
    }

    static float dbToY(float db, float height, float minDb, float maxDb)
    {
        const auto norm = (db - minDb) / (maxDb - minDb);
        return height * (1.0f - juce::jlimit(0.0f, 1.0f, norm));
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour(juce::Colours::white.withAlpha(0.15f));

        for (float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            auto x = freqToX(freq, bounds.getWidth());
            g.drawVerticalLine((int) x, bounds.getY(), bounds.getBottom());
        }

        for (float db : { 0.0f, -30.0f, -60.0f, -90.0f })
        {
            auto y = dbToY(db, bounds.getHeight(), minDb, maxDb);
            g.drawHorizontalLine((int) y, bounds.getX(), bounds.getRight());
        }
    }

    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (sampleRate <= 0.0)
            return;

        juce::Path path;
        const auto width = bounds.getWidth();
        const auto height = bounds.getHeight();
        bool started = false;

        for (int x = 0; x < (int) width; ++x)
        {
            const auto norm = (float) x / width;
            const auto freq = 20.0f * std::pow(1000.0f, norm); // 20Hz .. 20kHz over the width
            const auto bin = juce::jlimit(1, AnalyzerDataProvider::fftSize / 2 - 1,
                                           (int) (freq * AnalyzerDataProvider::fftSize / sampleRate));
            const auto db = latestMagnitudesDb[(size_t) bin];
            const auto y = dbToY(db, height, minDb, maxDb);

            if (!started)
            {
                path.startNewSubPath(bounds.getX() + x, bounds.getY() + y);
                started = true;
            }
            else
            {
                path.lineTo(bounds.getX() + x, bounds.getY() + y);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

    void drawBandCurves(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (sampleRate <= 0.0)
            return;

        for (int i = 0; i < Params::numBands; ++i)
        {
            auto* enabledParam = apvts.getRawParameterValue(Params::bandParamID(i, "enabled"));
            if (enabledParam->load() < 0.5f)
                continue;

            auto type = static_cast<Params::FilterType>(
                (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load());
            auto freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
            auto gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
            auto q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();

            auto coeffs = FilterBand::computeCoefficients(type, sampleRate, freq, gain, q);

            juce::Path path;
            const auto width = bounds.getWidth();
            const auto height = bounds.getHeight();
            bool started = false;

            for (int x = 0; x < (int) width; ++x)
            {
                const auto norm = (float) x / width;
                const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
                const auto magnitude = coeffs->getMagnitudeForFrequency(probeFreq, sampleRate);
                const auto db = juce::Decibels::gainToDecibels((float) magnitude, -100.0f);
                const auto y = dbToY(db, height, minDb, maxDb);

                if (!started)
                {
                    path.startNewSubPath(bounds.getX() + x, bounds.getY() + y);
                    started = true;
                }
                else
                {
                    path.lineTo(bounds.getX() + x, bounds.getY() + y);
                }
            }

            g.setColour(juce::Colour(0xffe0a030).withAlpha(0.85f));
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    AnalyzerDataProvider& analyzer;
    double& sampleRate;
    std::array<float, AnalyzerDataProvider::fftSize / 2> latestMagnitudesDb {};

    static constexpr float minDb = -90.0f;
    static constexpr float maxDb = 6.0f;
};
