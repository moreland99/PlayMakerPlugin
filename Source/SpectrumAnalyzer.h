#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <vector>
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

        // JUCE's real FFT is unnormalized. 2/N restores a 0 dBFS sine (Hann + single-sided)
        // to ~0 dB so the display isn't pinned to the top of the graph.
        constexpr float norm = 2.0f / (float) fftSize;
        for (int i = 0; i < fftSize / 2; ++i)
            magnitudesDb[(size_t) i] = juce::Decibels::gainToDecibels(fftData[(size_t) i] * norm, -100.0f);

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

// Lock-free stereo output meter: audio thread writes, UI thread reads.
struct OutputMeterState
{
    std::atomic<float> peakL { -100.0f };
    std::atomic<float> peakR { -100.0f };
    std::atomic<float> holdL { -100.0f };
    std::atomic<float> holdR { -100.0f };
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
                               std::array<std::atomic<float>, Params::numBands>* dynOffsetsToRead,
                               const OutputMeterState* outputMetersToRead = nullptr);
    ~SpectrumAnalyzerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Selection API for the band inspector / band list.
    int getPrimarySelectedBand() const;
    juce::Array<int> getSelectedBandIndices() const;
    void deleteSelectedBands();
    juce::Point<float> getPrimaryHandlePosition() const;
    juce::Rectangle<float> getGraphArea() const;
    void selectOnly(int bandIndex);
    void toggleSelection(int bandIndex);
    bool isSelected(int bandIndex) const;
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

    struct RailLayout
    {
        juce::Rectangle<float> graph;
        juce::Rectangle<float> eqScale;
        juce::Rectangle<float> specScale;
        juce::Rectangle<float> meters;
    };

    RailLayout layoutRail() const;
    void drawDbGrid(juce::Graphics& g, juce::Rectangle<float> graph, juce::Rectangle<float> eqScale);
    void drawFrequencyGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrumScale(juce::Graphics& g, juce::Rectangle<float> specScale, juce::Rectangle<float> graph);
    void drawOutputMeters(juce::Graphics& g, juce::Rectangle<float> meterArea, juce::Rectangle<float> graph);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrumTrace(juce::Graphics& g, juce::Rectangle<float> bounds,
                           const std::array<float, AnalyzerDataProvider::fftSize / 2>& magnitudesDb,
                           juce::Colour stroke, juce::Colour fill, float fillAlpha);
    void processSpectrumBlock(bool gotFft, const std::array<float, AnalyzerDataProvider::fftSize / 2>& latest,
                              std::array<float, AnalyzerDataProvider::fftSize / 2>& smoothed,
                              std::array<float, AnalyzerDataProvider::fftSize / 2>& display,
                              bool& initialized);
    void rebuildCurveCache(juce::Rectangle<float> bounds);
    bool curveParamsChanged() const;
    void snapshotCurveParams();
    void buildResponsePaths(juce::Path& stroke, juce::Path* fill,
                            juce::Rectangle<float> bounds, const FilterBand::StageSet& stages);
    void drawCombinedCurve(juce::Graphics& g);
    void drawBandCurves(juce::Graphics& g);
    void drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCreatePreview(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawMarquee(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g, juce::Rectangle<float> bounds);
    void notifySelectionChanged();
    void drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                          const FilterBand::StageSet& stages,
                          juce::Colour colour, float strokeWidth, float fillAlpha = 0.0f);

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

    void clearSelection();
    void selectBandsInMarquee(juce::Rectangle<float> bounds);
    void commitCreateAt(float freqHz, float gainDb);
    void deleteBand(int bandIndex);

    juce::AudioProcessorValueTreeState& apvts;
    AnalyzerDataProvider& postAnalyzer;
    AnalyzerDataProvider& preAnalyzer;
    double& sampleRate;
    const Theme& theme;
    std::array<std::atomic<float>, Params::numBands>* dynOffsets = nullptr;
    const OutputMeterState* outputMeters = nullptr;
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

    struct BandCurveSnapshot
    {
        int type = 0;
        float freq = -1.0f;
        float gain = 0.0f;
        float q = -1.0f;
        float slope = -1.0f;
        float dynRange = 0.0f;
        float dynOffset = 0.0f;
        bool enabled = false;
        bool solo = false;
        bool brickwall = false;
        bool dynOn = false;
        bool selected = false;
    };

    struct BandCurveCache
    {
        juce::Path stroke;
        juce::Path fill;
        juce::Path dynFill;
        juce::Colour colour;
        juce::Colour dynFillColour;
        float strokeWidth = 1.35f;
        float fillAlpha = 0.0f;
        bool enabled = false;
        bool selected = false;
        bool hasDynFill = false;
    };

    juce::Path combinedCurvePath;
    std::array<BandCurveCache, Params::numBands> bandCurveCache {};
    std::array<BandCurveSnapshot, Params::numBands> curveParamSnapshot {};
    bool curveCacheValid = false;
    double cachedCurveSampleRate = 0.0;
    int cachedCurveWidth = 0;
    int cachedCurveHeight = 0;
    float cachedCurveMinDb = 0.0f;
    float cachedCurveMaxDb = 0.0f;

    static constexpr int curveResolution = 384;
    static constexpr float dynOffsetRebuildThresholdDb = 0.1f;
    std::vector<float> spectrumDrawY;
    std::vector<float> spectrumDrawScratch;
    static constexpr float spectrumAttack = 0.22f;
    static constexpr float spectrumRelease = 0.08f;
    static constexpr float handleHitRadiusPx = 14.0f;
    static constexpr float createDragThresholdPx = 4.0f;
    static constexpr float lowZoneMaxHz = 250.0f;
    static constexpr float highZoneMinHz = 5000.0f;
    static constexpr float defaultQ = 0.707f;
    static constexpr float railWidth = 66.0f;
    static constexpr float specScaleWidth = 20.0f;
    static constexpr float meterWidth = 22.0f;
};
