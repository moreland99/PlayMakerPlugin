#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Params.h"
#include "FilterBand.h"
#include "Theme.h"

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

// Spectrum + band curves with direct interaction (Phase 5): create, drag, Q, delete, multi-select.
class SpectrumAnalyzerComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzerComponent(juce::AudioProcessorValueTreeState& stateToRead,
                               AnalyzerDataProvider& analyzerToRead,
                               double& sampleRateToRead,
                               const Theme& themeToUse);
    ~SpectrumAnalyzerComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    enum class Gesture
    {
        none,
        dragBand,
        createDrag,
        marquee
    };

    void timerCallback() override;

    static float freqToX(float freq, float width);
    static float xToFreq(float x, float width);
    static float dbToY(float db, float height, float minDb, float maxDb);
    static float yToDb(float y, float height, float minDb, float maxDb);

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBandCurves(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCreatePreview(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawMarquee(juce::Graphics& g);
    void drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                          const FilterBand::StageSet& stages,
                          juce::Colour colour, float strokeWidth);

    juce::Point<float> handlePosition(int bandIndex, juce::Rectangle<float> bounds) const;
    int hitTestBand(juce::Point<float> pos, juce::Rectangle<float> bounds) const;
    int findFirstDisabledBand() const;
    static Params::FilterType defaultTypeForFrequency(float freqHz);

    void setBandEnabled(int bandIndex, bool enabled);
    void setBandType(int bandIndex, Params::FilterType type);
    void setBandFreq(int bandIndex, float freqHz);
    void setBandGain(int bandIndex, float gainDb);
    void setBandQ(int bandIndex, float q);
    void beginBandGesture(int bandIndex);
    void beginBandGesture(int bandIndex, std::initializer_list<const char*> suffixes);
    void endBandGesture(int bandIndex);
    void endBandGesture(int bandIndex, std::initializer_list<const char*> suffixes);

    void selectOnly(int bandIndex);
    void toggleSelection(int bandIndex);
    void clearSelection();
    bool isSelected(int bandIndex) const;
    void selectBandsInMarquee(juce::Rectangle<float> bounds);
    void commitCreateAt(float freqHz, float gainDb);
    void deleteBand(int bandIndex);

    juce::AudioProcessorValueTreeState& apvts;
    AnalyzerDataProvider& analyzer;
    double& sampleRate;
    const Theme& theme;
    std::array<float, AnalyzerDataProvider::fftSize / 2> latestMagnitudesDb {};

    juce::SparseSet<int> selectedBands;
    Gesture gesture = Gesture::none;
    int primaryBand = -1;
    juce::Point<float> gestureStartPos;
    juce::Point<float> gestureCurrentPos;
    float dragStartFreq = 1000.0f;
    float dragStartGain = 0.0f;
    std::array<float, Params::numBands> dragStartFreqs {};
    std::array<float, Params::numBands> dragStartGains {};
    bool createPreviewActive = false;
    float createPreviewFreq = 1000.0f;
    float createPreviewGain = 0.0f;
    Params::FilterType createPreviewType = Params::FilterType::bell;

    // Spectrum uses a wide absolute-level range; band curves/handles use ±gain range
    // so click-to-set-gain lands in a usable part of the display.
    static constexpr float spectrumMinDb = -90.0f;
    static constexpr float spectrumMaxDb = 6.0f;
    static constexpr float curveMinDb = -24.0f;
    static constexpr float curveMaxDb = 24.0f;
    static constexpr float handleHitRadiusPx = 14.0f;
    static constexpr float createDragThresholdPx = 4.0f;
    static constexpr float lowZoneMaxHz = 250.0f;
    static constexpr float highZoneMinHz = 5000.0f;
    static constexpr float defaultQ = 0.707f;
};
