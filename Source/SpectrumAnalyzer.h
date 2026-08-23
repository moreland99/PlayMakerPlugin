#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
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
                               AnalyzerDataProvider& postAnalyzerToRead,
                               AnalyzerDataProvider& preAnalyzerToRead,
                               double& sampleRateToRead,
                               const Theme& themeToUse,
                               std::array<std::atomic<float>, Params::numBands>* dynOffsetsToRead);
    ~SpectrumAnalyzerComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Selection API for the band inspector.
    int getPrimarySelectedBand() const;
    juce::Array<int> getSelectedBandIndices() const;
    void deleteSelectedBands();
    juce::Point<float> getPrimaryHandlePosition() const;
    std::function<void()> onSelectionChanged;
    std::function<void()> onBandMoved;

    void setDisplayRangeHalfDb(float halfRangeDb);
    float getDisplayRangeHalfDb() const { return displayRangeHalfDb; }

    void setShowPreSpectrum(bool show);
    void setShowPostSpectrum(bool show);
    void setSpectrumFrozen(bool frozen);
    void setSpectrumSpanDb(float spanDb);

    bool getShowPreSpectrum() const { return showPreSpectrum; }
    bool getShowPostSpectrum() const { return showPostSpectrum; }
    bool isSpectrumFrozen() const { return spectrumFrozen; }
    float getSpectrumSpanDb() const { return spectrumSpanDb; }

private:
    enum class Gesture
    {
        none,
        dragBand,
        dragBandQ,
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
    void drawSpectrumTrace(juce::Graphics& g, juce::Rectangle<float> bounds,
                           const std::array<float, AnalyzerDataProvider::fftSize / 2>& magnitudesDb,
                           juce::Colour stroke, juce::Colour fill, float fillAlpha);
    void processSpectrumBlock(bool gotFft, const std::array<float, AnalyzerDataProvider::fftSize / 2>& latest,
                              std::array<float, AnalyzerDataProvider::fftSize / 2>& smoothed,
                              std::array<float, AnalyzerDataProvider::fftSize / 2>& display,
                              bool& initialized);
    void drawCombinedCurve(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBandCurves(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCreatePreview(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawMarquee(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSelectionReadout(juce::Graphics& g, juce::Rectangle<float> bounds);
    void notifySelectionChanged();
    static juce::String formatFrequency(float freqHz);
    void drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                          const FilterBand::StageSet& stages,
                          juce::Colour colour, float strokeWidth, float fillAlpha = 0.0f);
    void drawDynamicRangeFill(juce::Graphics& g, juce::Rectangle<float> bounds,
                              const FilterBand::StageSet& staticStages,
                              const FilterBand::StageSet& extremeStages,
                              juce::Colour colour);

    juce::Point<float> handlePosition(int bandIndex, juce::Rectangle<float> bounds) const;
    int hitTestBand(juce::Point<float> pos, juce::Rectangle<float> bounds) const;
    int findFirstDisabledBand() const;
    int countEnabledBands() const;
    bool bandAudibleInChain(int bandIndex) const;
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
    AnalyzerDataProvider& postAnalyzer;
    AnalyzerDataProvider& preAnalyzer;
    double& sampleRate;
    const Theme& theme;
    std::array<std::atomic<float>, Params::numBands>* dynOffsets = nullptr;
    std::array<float, Params::numBands> smoothedDynOffsetDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> latestPostMagnitudesDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> smoothedPostMagnitudesDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> displayPostMagnitudesDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> latestPreMagnitudesDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> smoothedPreMagnitudesDb {};
    std::array<float, AnalyzerDataProvider::fftSize / 2> displayPreMagnitudesDb {};
    bool postSpectrumInitialized = false;
    bool preSpectrumInitialized = false;
    bool showPreSpectrum = false;
    bool showPostSpectrum = true;
    bool spectrumFrozen = false;
    float spectrumSpanDb = 90.0f;

    juce::SparseSet<int> selectedBands;
    Gesture gesture = Gesture::none;
    int primaryBand = -1;
    juce::Point<float> gestureStartPos;
    juce::Point<float> gestureCurrentPos;
    float dragStartFreq = 1000.0f;
    float dragStartGain = 0.0f;
    std::array<float, Params::numBands> dragStartFreqs {};
    std::array<float, Params::numBands> dragStartGains {};
    std::array<float, Params::numBands> dragStartQs {};
    bool createPreviewActive = false;
    float createPreviewFreq = 1000.0f;
    float createPreviewGain = 0.0f;
    Params::FilterType createPreviewType = Params::FilterType::bell;

    float displayRangeHalfDb = 24.0f;
    float curveMinDb = -24.0f;
    float curveMaxDb = 24.0f;
    static constexpr float spectrumAttack = 0.22f;
    static constexpr float spectrumRelease = 0.08f;
    static constexpr float handleHitRadiusPx = 14.0f;
    static constexpr float createDragThresholdPx = 4.0f;
    static constexpr float lowZoneMaxHz = 250.0f;
    static constexpr float highZoneMinHz = 5000.0f;
    static constexpr float defaultQ = 0.707f;
};
