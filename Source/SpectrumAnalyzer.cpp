#include "SpectrumAnalyzer.h"
#include "Brand.h"

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent(juce::AudioProcessorValueTreeState& stateToRead,
                                                       AnalyzerDataProvider& postAnalyzerToRead,
                                                       AnalyzerDataProvider& preAnalyzerToRead,
                                                       double& sampleRateToRead,
                                                       const Theme& themeToUse,
                                                       std::array<std::atomic<float>, Params::numBands>* dynOffsetsToRead,
                                                       const OutputMeterState* outputMetersToRead)
    : apvts(stateToRead), postAnalyzer(postAnalyzerToRead), preAnalyzer(preAnalyzerToRead),
      sampleRate(sampleRateToRead), theme(themeToUse), dynOffsets(dynOffsetsToRead),
      outputMeters(outputMetersToRead)
{
    startTimerHz(30);
}

SpectrumAnalyzerComponent::~SpectrumAnalyzerComponent()
{
    stopTimer();
}

void SpectrumAnalyzerComponent::setDisplayRangeHalfDb(float halfRangeDb)
{
    const float half = juce::jlimit(6.0f, 30.0f, halfRangeDb);
    displayRangeHalfDb = half;
    curveMinDb = -half;
    curveMaxDb = half;
    curveCacheValid = false;
    repaint();
}

void SpectrumAnalyzerComponent::resized()
{
    curveCacheValid = false;
}

void SpectrumAnalyzerComponent::setShowPreSpectrum(bool show)
{
    showPreSpectrum = show;
    repaint();
}

void SpectrumAnalyzerComponent::setShowPostSpectrum(bool show)
{
    showPostSpectrum = show;
    repaint();
}

void SpectrumAnalyzerComponent::setSpectrumFrozen(bool frozen)
{
    spectrumFrozen = frozen;
    repaint();
}

void SpectrumAnalyzerComponent::setSpectrumSpanDb(float spanDb)
{
    spectrumSpanDb = (spanDb <= 75.0f) ? 60.0f : 90.0f;
    repaint();
}

void SpectrumAnalyzerComponent::processSpectrumBlock(
    bool gotFft,
    const std::array<float, AnalyzerDataProvider::fftSize / 2>& latest,
    std::array<float, AnalyzerDataProvider::fftSize / 2>& smoothed,
    std::array<float, AnalyzerDataProvider::fftSize / 2>& display,
    bool& initialized)
{
    if (!gotFft || spectrumFrozen)
        return;

    const int n = (int) smoothed.size();

    if (!initialized)
    {
        smoothed = latest;
        display = latest;
        initialized = true;
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float target = latest[(size_t) i];
            const float cur = smoothed[(size_t) i];
            const float coeff = target > cur ? spectrumAttack : spectrumRelease;
            smoothed[(size_t) i] = cur + (target - cur) * coeff;
        }
    }

    constexpr int passes = 3;
    display = smoothed;
    for (int pass = 0; pass < passes; ++pass)
    {
        auto src = display;
        for (int i = 1; i < n - 1; ++i)
        {
            display[(size_t) i] = 0.20f * src[(size_t) (i - 1)]
                                + 0.60f * src[(size_t) i]
                                + 0.20f * src[(size_t) (i + 1)];
        }
    }
}

void SpectrumAnalyzerComponent::timerCallback()
{
    const bool gotPost = postAnalyzer.getMagnitudesDb(latestPostMagnitudesDb);
    const bool gotPre = preAnalyzer.getMagnitudesDb(latestPreMagnitudesDb);

    if (showPostSpectrum)
        processSpectrumBlock(gotPost, latestPostMagnitudesDb, smoothedPostMagnitudesDb,
                             displayPostMagnitudesDb, postSpectrumInitialized);
    else if (gotPost)
        juce::ignoreUnused(latestPostMagnitudesDb);

    if (showPreSpectrum)
        processSpectrumBlock(gotPre, latestPreMagnitudesDb, smoothedPreMagnitudesDb,
                             displayPreMagnitudesDb, preSpectrumInitialized);
    else if (gotPre)
        juce::ignoreUnused(latestPreMagnitudesDb);

    bool dynMovedEnough = false;
    if (dynOffsets != nullptr)
    {
        for (int i = 0; i < Params::numBands; ++i)
        {
            const float target = (*dynOffsets)[(size_t) i].load(std::memory_order_relaxed);
            auto& sm = smoothedDynOffsetDb[(size_t) i];
            sm += (target - sm) * 0.35f;
            if (std::abs(sm - curveParamSnapshot[(size_t) i].dynOffset) >= dynOffsetRebuildThresholdDb)
                dynMovedEnough = true;
        }
    }

    if (dynMovedEnough || curveParamsChanged())
        curveCacheValid = false;

    const bool gestureActive = gesture != Gesture::none || createPreviewActive;
    if (gotPost || gotPre || !curveCacheValid || gestureActive || outputMeters != nullptr)
        repaint();
}

float SpectrumAnalyzerComponent::freqToX(float freq, float width)
{
    constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    const auto norm = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
    return juce::jlimit(0.0f, 1.0f, norm) * width;
}

float SpectrumAnalyzerComponent::xToFreq(float x, float width)
{
    constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    const auto norm = juce::jlimit(0.0f, 1.0f, x / juce::jmax(1.0f, width));
    return minFreq * std::pow(maxFreq / minFreq, norm);
}

float SpectrumAnalyzerComponent::dbToY(float db, float height, float minDb, float maxDb)
{
    const auto norm = (db - minDb) / (maxDb - minDb);
    return height * (1.0f - juce::jlimit(0.0f, 1.0f, norm));
}

float SpectrumAnalyzerComponent::yToDb(float y, float height, float minDb, float maxDb)
{
    const auto norm = 1.0f - juce::jlimit(0.0f, 1.0f, y / juce::jmax(1.0f, height));
    return minDb + norm * (maxDb - minDb);
}

SpectrumAnalyzerComponent::RailLayout SpectrumAnalyzerComponent::layoutRail() const
{
    auto bounds = getLocalBounds().toFloat();
    auto rail = bounds.removeFromRight(railWidth);
    auto meters = rail.removeFromRight(meterWidth);
    auto spec = rail.removeFromRight(specScaleWidth);
    return { bounds, rail, spec, meters };
}

void SpectrumAnalyzerComponent::paint(juce::Graphics& g)
{
    const auto layout = layoutRail();
    auto full = getLocalBounds().toFloat();
    g.setColour(theme.background);
    g.fillRect(full);

    g.setColour(theme.panel.withAlpha(theme.isLight() ? 0.35f : 0.55f));
    g.fillRect(juce::Rectangle<float>(layout.eqScale.getX(), full.getY(),
                                      full.getRight() - layout.eqScale.getX(), full.getHeight()));
    g.setColour(theme.grid.withAlpha(0.35f));
    g.drawVerticalLine((int) layout.eqScale.getX(), full.getY(), full.getBottom());

    const bool boundsChanged = cachedCurveWidth != (int) layout.graph.getWidth()
                            || cachedCurveHeight != (int) layout.graph.getHeight();
    const bool rangeChanged = std::abs(cachedCurveSampleRate - sampleRate) > 0.5
                           || std::abs(cachedCurveMinDb - curveMinDb) > 0.01f
                           || std::abs(cachedCurveMaxDb - curveMaxDb) > 0.01f;
    if (!curveCacheValid || boundsChanged || rangeChanged || curveParamsChanged())
        rebuildCurveCache(layout.graph);

    drawDbGrid(g, layout.graph, layout.eqScale);
    drawSpectrum(g, layout.graph);
    drawFrequencyGrid(g, layout.graph);
    drawBandCurves(g);
    drawCombinedCurve(g);
    drawCreatePreview(g, layout.graph);
    drawBandHandles(g, layout.graph);
    drawSelectionReadout(g, layout.graph);
    drawMarquee(g);
    drawSpectrumScale(g, layout.specScale, layout.graph);
    drawOutputMeters(g, layout.meters, layout.graph);

    if (countEnabledBands() == 0 && !createPreviewActive)
        drawEmptyState(g, layout.graph);
}

void SpectrumAnalyzerComponent::drawEmptyState(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    Brand::drawEmptyStateLockup(g, bounds, theme);
}

int SpectrumAnalyzerComponent::countEnabledBands() const
{
    int count = 0;
    for (int i = 0; i < Params::numBands; ++i)
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f)
            ++count;
    return count;
}

bool SpectrumAnalyzerComponent::bandAudibleInChain(int bandIndex) const
{
    if (apvts.getRawParameterValue(Params::bandParamID(bandIndex, "enabled"))->load() < 0.5f)
        return false;

    bool anySolo = false;
    for (int j = 0; j < Params::numBands; ++j)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(j, "enabled"))->load() >= 0.5f
            && apvts.getRawParameterValue(Params::bandParamID(j, "solo"))->load() >= 0.5f)
        {
            anySolo = true;
            break;
        }
    }

    if (anySolo && apvts.getRawParameterValue(Params::bandParamID(bandIndex, "solo"))->load() < 0.5f)
        return false;

    return true;
}

void SpectrumAnalyzerComponent::drawDbGrid(juce::Graphics& g, juce::Rectangle<float> graph,
                                           juce::Rectangle<float> eqScale)
{
    const float half = displayRangeHalfDb;
    const float quarter = half * 0.5f;
    g.setFont(Brand::uiFont(9.5f));
    for (float db : { -half, -quarter, 0.0f, quarter, half })
    {
        const float y = graph.getY() + dbToY(db, graph.getHeight(), curveMinDb, curveMaxDb);
        const bool zero = std::abs(db) < 0.01f;
        g.setColour(theme.grid.withAlpha(zero ? 0.9f : 0.55f));
        g.drawHorizontalLine((int) y, graph.getX(), graph.getRight());

        auto dbLabel = juce::Rectangle<float>(eqScale.getX(), y - 7.0f, eqScale.getWidth() - 2.0f, 12.0f)
                           .constrainedWithin(eqScale.reduced(1.0f, 2.0f));
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.55f : 0.48f));
        g.drawText(juce::String((int) std::lround(db)),
                   dbLabel, juce::Justification::centredRight, false);
    }
}

void SpectrumAnalyzerComponent::drawFrequencyGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    struct FreqMark { float hz; const char* label; };
    constexpr FreqMark marks[] = {
        { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(Brand::uiFont(9.5f));
    for (const auto& m : marks)
    {
        const float x = bounds.getX() + freqToX(m.hz, bounds.getWidth());
        g.setColour(theme.grid.withAlpha(0.72f));
        auto line = juce::Rectangle<float>(x - 1.0f, bounds.getY(), 2.0f, bounds.getHeight())
                        .getIntersection(bounds);
        g.fillRect(line);

        juce::Rectangle<float> labelArea (x - 16.0f, bounds.getBottom() - 14.0f, 32.0f, 12.0f);
        auto justification = juce::Justification::centred;
        if (x < bounds.getX() + 24.0f)
        {
            labelArea = { bounds.getX() + 2.0f, bounds.getBottom() - 14.0f, 28.0f, 12.0f };
            justification = juce::Justification::centredLeft;
        }
        else if (x > bounds.getRight() - 24.0f)
        {
            labelArea = { bounds.getRight() - 30.0f, bounds.getBottom() - 14.0f, 28.0f, 12.0f };
            justification = juce::Justification::centredRight;
        }

        labelArea = labelArea.constrainedWithin(bounds.reduced(2.0f));
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.50f : 0.42f));
        g.drawText(m.label, labelArea, justification, false);
    }
}

void SpectrumAnalyzerComponent::drawSpectrumScale(juce::Graphics& g, juce::Rectangle<float> specScale,
                                                  juce::Rectangle<float> graph)
{
    if (!showPostSpectrum && !showPreSpectrum)
        return;

    g.setFont(Brand::uiFont(8.5f));
    const float minDb = -spectrumSpanDb;
    float lastY = -1000.0f;
    for (float db = 0.0f; db >= minDb - 0.01f; db -= 10.0f)
    {
        const float y = graph.getY() + dbToY(db, graph.getHeight(), minDb, 0.0f);
        if (std::abs(y - lastY) < 11.0f)
            continue;
        lastY = y;

        auto label = juce::Rectangle<float>(specScale.getX(), y - 6.0f, specScale.getWidth() - 1.0f, 12.0f)
                         .constrainedWithin(specScale.reduced(0.0f, 1.0f));
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.38f : 0.30f));
        g.drawText(juce::String((int) std::lround(db)),
                   label, juce::Justification::centredRight, false);
    }
}

void SpectrumAnalyzerComponent::drawOutputMeters(juce::Graphics& g, juce::Rectangle<float> meterArea,
                                                 juce::Rectangle<float> graph)
{
    if (outputMeters == nullptr)
        return;

    const float peakL = outputMeters->peakL.load(std::memory_order_relaxed);
    const float peakR = outputMeters->peakR.load(std::memory_order_relaxed);
    const float holdL = outputMeters->holdL.load(std::memory_order_relaxed);
    const float holdR = outputMeters->holdR.load(std::memory_order_relaxed);
    const float holdDb = juce::jmax(holdL, holdR);

    auto readout = meterArea.removeFromTop(14.0f);
    g.setFont(Brand::uiFont(8.0f, true));
    g.setColour(holdDb > -0.1f ? juce::Colour(0xffeb5757)
                               : theme.ink.withAlpha(theme.isLight() ? 0.55f : 0.48f));
    const auto readoutText = holdDb <= -99.0f ? juce::String("-inf")
                                              : juce::String(holdDb, 1);
    g.drawText(readoutText, readout.reduced(1.0f, 0.0f), juce::Justification::centred, false);

    auto bars = meterArea.reduced(3.0f, 4.0f);
    const float gap = 2.0f;
    const float barW = juce::jmax(4.0f, (bars.getWidth() - gap) * 0.5f);
    const auto leftBar = bars.removeFromLeft(barW);
    bars.removeFromLeft(gap);
    const auto rightBar = bars.removeFromLeft(barW);

    const float specMin = -spectrumSpanDb;
    auto drawBar = [&] (juce::Rectangle<float> slot, float peakDb, float holdPeak)
    {
        g.setColour(theme.charcoalBlack.withAlpha(theme.isLight() ? 0.12f : 0.45f));
        g.fillRect(slot);

        const float y = graph.getY() + dbToY(peakDb, graph.getHeight(), specMin, 0.0f);
        auto fill = slot.withTop(juce::jlimit(slot.getY(), slot.getBottom(), y));
        if (fill.getHeight() > 0.5f)
        {
            juce::ColourGradient grad (juce::Colour(0xffeb5757), slot.getX(), slot.getY(),
                                       juce::Colour(0xff1f6b3a), slot.getX(), slot.getBottom(), false);
            grad.addColour(0.12, theme.signalOrange);
            grad.addColour(0.42, juce::Colour(0xff6fcf97));
            g.setGradientFill(grad);
            g.fillRect(fill);
        }

        const float holdY = graph.getY() + dbToY(holdPeak, graph.getHeight(), specMin, 0.0f);
        const float tickY = juce::jlimit(slot.getY(), slot.getBottom() - 1.0f, holdY);
        g.setColour(theme.softWhite.withAlpha(0.85f));
        g.fillRect(slot.getX(), tickY, slot.getWidth(), 1.5f);
    };

    drawBar(leftBar, peakL, holdL);
    drawBar(rightBar, peakR, holdR);
}

void SpectrumAnalyzerComponent::drawSpectrumTrace(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                 const std::array<float, AnalyzerDataProvider::fftSize / 2>& magnitudesDb,
                                                 juce::Colour stroke, juce::Colour fill, float fillAlpha)
{
    if (sampleRate <= 0.0)
        return;

    const float specMinDb = -spectrumSpanDb;
    const float specMaxDb = 0.0f;
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();
    const int columns = juce::jmax(2, (int) std::ceil(width));
    const int lastBin = AnalyzerDataProvider::fftSize / 2 - 1;
    const float binScale = (float) AnalyzerDataProvider::fftSize / (float) sampleRate;

    auto magAtFreq = [&] (float freq)
    {
        const float binF = juce::jlimit(1.0f, (float) lastBin, freq * binScale);
        const int i0 = juce::jlimit(1, lastBin - 1, (int) binF);
        const float t = juce::jlimit(0.0f, 1.0f, binF - (float) i0);
        return magnitudesDb[(size_t) i0] + t * (magnitudesDb[(size_t) i0 + 1] - magnitudesDb[(size_t) i0]);
    };

    spectrumDrawY.resize((size_t) columns);
    for (int x = 0; x < columns; ++x)
    {
        const float freqL = xToFreq((float) x, width);
        const float freqR = xToFreq((float) juce::jmin(columns, x + 1), width);
        const int binL = juce::jlimit(1, lastBin, (int) (freqL * binScale));
        const int binR = juce::jlimit(1, lastBin, (int) (freqR * binScale));

        float peak = magAtFreq(freqL);
        if (binR > binL)
        {
            for (int b = binL; b <= binR; ++b)
                peak = juce::jmax(peak, magnitudesDb[(size_t) b]);
        }
        else
        {
            peak = juce::jmax(peak, magAtFreq(freqR));
        }

        spectrumDrawY[(size_t) x] = dbToY(peak, height, specMinDb, specMaxDb);
    }

    spectrumDrawScratch = spectrumDrawY;
    for (int pass = 0; pass < 3; ++pass)
    {
        auto& src = (pass & 1) ? spectrumDrawY : spectrumDrawScratch;
        auto& dst = (pass & 1) ? spectrumDrawScratch : spectrumDrawY;
        dst[0] = src[0];
        dst[(size_t) columns - 1] = src[(size_t) columns - 1];
        for (int x = 1; x < columns - 1; ++x)
            dst[(size_t) x] = 0.20f * src[(size_t) (x - 1)]
                            + 0.60f * src[(size_t) x]
                            + 0.20f * src[(size_t) (x + 1)];
    }

    juce::Path path;
    path.startNewSubPath(bounds.getX(), bounds.getY() + spectrumDrawY[0]);
    for (int x = 1; x < columns; ++x)
        path.lineTo(bounds.getX() + (float) x, bounds.getY() + spectrumDrawY[(size_t) x]);

    if (fillAlpha > 0.001f)
    {
        juce::Path fillPath = path;
        fillPath.lineTo(bounds.getRight(), bounds.getBottom());
        fillPath.lineTo(bounds.getX(), bounds.getBottom());
        fillPath.closeSubPath();
        g.setColour(fill.withAlpha(fillAlpha));
        g.fillPath(fillPath);
    }

    g.setColour(stroke);
    g.strokePath(path, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void SpectrumAnalyzerComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (showPreSpectrum && preSpectrumInitialized)
    {
        drawSpectrumTrace(g, bounds, displayPreMagnitudesDb,
                          theme.signalOrange.withAlpha(theme.isLight() ? 0.55f : 0.48f),
                          theme.signalOrange, theme.isLight() ? 0.10f : 0.12f);
    }

    if (showPostSpectrum && postSpectrumInitialized)
    {
        drawSpectrumTrace(g, bounds, displayPostMagnitudesDb,
                          theme.spectrum.withAlpha(0.28f),
                          theme.spectrum, 0.14f);
    }
}

void SpectrumAnalyzerComponent::buildResponsePaths(juce::Path& stroke, juce::Path* fill,
                                                    juce::Rectangle<float> bounds,
                                                    const FilterBand::StageSet& stages)
{
    stroke.clear();
    if (fill != nullptr)
        fill->clear();

    if (sampleRate <= 0.0)
        return;

    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();
    const int points = juce::jmax(2, juce::jmin(curveResolution, (int) width));
    bool started = false;

    for (int i = 0; i < points; ++i)
    {
        const auto norm = points > 1 ? (float) i / (float) (points - 1) : 0.0f;
        const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
        const auto magnitude = FilterBand::getMagnitudeForFrequency(stages, probeFreq, sampleRate);
        const auto db = juce::Decibels::gainToDecibels((float) magnitude, -100.0f);
        const auto x = bounds.getX() + norm * width;
        const auto y = bounds.getY() + dbToY(db, height, curveMinDb, curveMaxDb);

        if (!started)
        {
            stroke.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            stroke.lineTo(x, y);
        }
    }

    if (started && fill != nullptr)
    {
        const float zeroY = bounds.getY() + dbToY(0.0f, height, curveMinDb, curveMaxDb);
        *fill = stroke;
        fill->lineTo(bounds.getRight(), zeroY);
        fill->lineTo(bounds.getX(), zeroY);
        fill->closeSubPath();
    }
}

void SpectrumAnalyzerComponent::drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                  const FilterBand::StageSet& stages,
                                                  juce::Colour colour, float strokeWidth,
                                                  float fillAlpha)
{
    juce::Path stroke, fill;
    buildResponsePaths(stroke, fillAlpha > 0.001f ? &fill : nullptr, bounds, stages);

    if (fillAlpha > 0.001f && !fill.isEmpty())
    {
        g.setColour(colour.withAlpha(fillAlpha));
        g.fillPath(fill);
    }

    if (!stroke.isEmpty())
    {
        g.setColour(colour);
        g.strokePath(stroke, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
}

bool SpectrumAnalyzerComponent::curveParamsChanged() const
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        const auto& snap = curveParamSnapshot[(size_t) i];
        const auto type = (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load();
        const bool dynOn = apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f
                           && Params::typeSupportsDynamics(static_cast<Params::FilterType>(type));

        if (snap.type != type
            || snap.enabled != (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f)
            || snap.solo != (apvts.getRawParameterValue(Params::bandParamID(i, "solo"))->load() >= 0.5f)
            || snap.brickwall != (apvts.getRawParameterValue(Params::bandParamID(i, "brickwall"))->load() >= 0.5f)
            || snap.dynOn != dynOn
            || snap.selected != isSelected(i)
            || std::abs(snap.freq - apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load()) > 1.0e-4f
            || std::abs(snap.gain - apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load()) > 1.0e-4f
            || std::abs(snap.q - apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load()) > 1.0e-4f
            || std::abs(snap.slope - apvts.getRawParameterValue(Params::bandParamID(i, "slope"))->load()) > 1.0e-4f
            || std::abs(snap.dynRange - apvts.getRawParameterValue(Params::bandParamID(i, "dynRange"))->load()) > 1.0e-4f)
            return true;
    }

    return false;
}

void SpectrumAnalyzerComponent::snapshotCurveParams()
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        auto& snap = curveParamSnapshot[(size_t) i];
        snap.type = (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load();
        snap.freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
        snap.gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        snap.q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
        snap.slope = apvts.getRawParameterValue(Params::bandParamID(i, "slope"))->load();
        snap.dynRange = apvts.getRawParameterValue(Params::bandParamID(i, "dynRange"))->load();
        snap.dynOffset = smoothedDynOffsetDb[(size_t) i];
        snap.enabled = apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f;
        snap.solo = apvts.getRawParameterValue(Params::bandParamID(i, "solo"))->load() >= 0.5f;
        snap.brickwall = apvts.getRawParameterValue(Params::bandParamID(i, "brickwall"))->load() >= 0.5f;
        snap.dynOn = snap.enabled
            && apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f
            && Params::typeSupportsDynamics(static_cast<Params::FilterType>(snap.type));
        snap.selected = isSelected(i);
    }
}

void SpectrumAnalyzerComponent::rebuildCurveCache(juce::Rectangle<float> bounds)
{
    combinedCurvePath.clear();
    cachedCurveWidth = (int) bounds.getWidth();
    cachedCurveHeight = (int) bounds.getHeight();
    cachedCurveSampleRate = sampleRate;
    cachedCurveMinDb = curveMinDb;
    cachedCurveMaxDb = curveMaxDb;

    if (sampleRate <= 0.0 || bounds.getWidth() < 2.0f)
    {
        for (auto& cache : bandCurveCache)
            cache.enabled = false;
        curveCacheValid = true;
        snapshotCurveParams();
        return;
    }

    std::array<FilterBand::StageSet, Params::numBands> stages {};
    std::array<bool, Params::numBands> audible {};
    int enabledCount = 0;

    for (int i = 0; i < Params::numBands; ++i)
    {
        auto& cache = bandCurveCache[(size_t) i];
        cache.enabled = apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f;
        cache.hasDynFill = false;
        cache.stroke.clear();
        cache.fill.clear();
        cache.dynFill.clear();

        if (!cache.enabled)
            continue;

        ++enabledCount;
        audible[(size_t) i] = bandAudibleInChain(i);
        cache.selected = isSelected(i);

        auto type = static_cast<Params::FilterType>(
            (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load());
        const bool dynOn = apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f
                           && Params::typeSupportsDynamics(type);
        auto freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
        auto gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        if (dynOn && dynOffsets != nullptr)
            gain += smoothedDynOffsetDb[(size_t) i];
        auto q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
        auto slope = apvts.getRawParameterValue(Params::bandParamID(i, "slope"))->load();
        auto brickwall = apvts.getRawParameterValue(Params::bandParamID(i, "brickwall"))->load() >= 0.5f;
        const float dynRange = apvts.getRawParameterValue(Params::bandParamID(i, "dynRange"))->load();

        const auto base = Theme::bandColour(i, theme.isLight());
        const auto dynTint = Theme::dynamicsColour(base, theme.isLight());
        const bool dimOthers = (!selectedBands.isEmpty() && !cache.selected) || !audible[(size_t) i];
        const auto active = dynOn ? dynTint : base;
        cache.colour = !audible[(size_t) i] ? active.withAlpha(theme.isLight() ? 0.20f : 0.14f)
                       : dimOthers ? active.withAlpha(theme.isLight() ? 0.38f : 0.28f)
                                 : (cache.selected ? active : active.withAlpha(theme.isLight() ? 0.88f : 0.78f));
        cache.strokeWidth = cache.selected ? (dynOn ? 2.9f : 2.5f) : (audible[(size_t) i] ? 1.35f : 0.9f);
        cache.fillAlpha = cache.selected ? (theme.isLight() ? (dynOn ? 0.24f : 0.18f) : (dynOn ? 0.20f : 0.14f))
                                         : (dimOthers ? 0.03f : (theme.isLight() ? 0.09f : 0.07f));

        FilterBand::assignStages(stages[(size_t) i], type, sampleRate, freq, gain, q, slope, brickwall);
        buildResponsePaths(cache.stroke, &cache.fill, bounds, stages[(size_t) i]);

        if (dynOn && std::abs(dynRange) > 0.05f)
        {
            FilterBand::StageSet extreme;
            FilterBand::assignStages(extreme, type, sampleRate, freq, gain + dynRange, q, slope, brickwall);

            cache.dynFill.clear();
            const auto width = bounds.getWidth();
            const auto height = bounds.getHeight();
            const int points = juce::jmax(2, juce::jmin(curveResolution, (int) width));
            bool started = false;

            for (int p = 0; p < points; ++p)
            {
                const auto norm = points > 1 ? (float) p / (float) (points - 1) : 0.0f;
                const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
                const auto dbA = juce::Decibels::gainToDecibels(
                    (float) FilterBand::getMagnitudeForFrequency(stages[(size_t) i], probeFreq, sampleRate), -100.0f);
                const float x = bounds.getX() + norm * width;
                const float yA = bounds.getY() + dbToY(dbA, height, curveMinDb, curveMaxDb);

                if (!started)
                {
                    cache.dynFill.startNewSubPath(x, yA);
                    started = true;
                }
                else
                {
                    cache.dynFill.lineTo(x, yA);
                }
            }

            if (started)
            {
                for (int p = points - 1; p >= 0; --p)
                {
                    const auto norm = points > 1 ? (float) p / (float) (points - 1) : 0.0f;
                    const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
                    const auto dbB = juce::Decibels::gainToDecibels(
                        (float) FilterBand::getMagnitudeForFrequency(extreme, probeFreq, sampleRate), -100.0f);
                    cache.dynFill.lineTo(bounds.getX() + norm * width,
                                         bounds.getY() + dbToY(dbB, height, curveMinDb, curveMaxDb));
                }
                cache.dynFill.closeSubPath();
                cache.hasDynFill = true;
                cache.dynFillColour = dynTint.withAlpha(cache.selected ? 1.0f : 0.6f);
            }
        }
    }

    if (enabledCount > 0)
    {
        const auto width = bounds.getWidth();
        const auto height = bounds.getHeight();
        const int points = juce::jmax(2, juce::jmin(curveResolution, (int) width));
        bool started = false;

        for (int p = 0; p < points; ++p)
        {
            const auto norm = points > 1 ? (float) p / (float) (points - 1) : 0.0f;
            const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
            double totalMag = 1.0;

            for (int i = 0; i < Params::numBands; ++i)
                if (bandCurveCache[(size_t) i].enabled && audible[(size_t) i])
                    totalMag *= FilterBand::getMagnitudeForFrequency(stages[(size_t) i], probeFreq, sampleRate);

            const auto db = juce::Decibels::gainToDecibels((float) totalMag, -100.0f);
            const float x = bounds.getX() + norm * width;
            const float y = bounds.getY() + dbToY(db, height, curveMinDb, curveMaxDb);

            if (!started)
            {
                combinedCurvePath.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                combinedCurvePath.lineTo(x, y);
            }
        }
    }

    snapshotCurveParams();
    curveCacheValid = true;
}

void SpectrumAnalyzerComponent::drawCombinedCurve(juce::Graphics& g)
{
    if (combinedCurvePath.isEmpty())
        return;

    g.setColour(theme.softWhite.withAlpha(theme.isLight() ? 0.55f : 0.72f));
    g.strokePath(combinedCurvePath, juce::PathStrokeType(2.35f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
}

void SpectrumAnalyzerComponent::drawBandCurves(juce::Graphics& g)
{
    auto drawOne = [&](bool selectedPass)
    {
        for (int i = 0; i < Params::numBands; ++i)
        {
            const auto& cache = bandCurveCache[(size_t) i];
            if (!cache.enabled || cache.selected != selectedPass)
                continue;

            if (cache.hasDynFill && !cache.dynFill.isEmpty())
            {
                g.setColour(cache.dynFillColour.withAlpha(0.16f));
                g.fillPath(cache.dynFill);
            }

            if (cache.fillAlpha > 0.001f && !cache.fill.isEmpty())
            {
                g.setColour(cache.colour.withAlpha(cache.fillAlpha));
                g.fillPath(cache.fill);
            }

            if (!cache.stroke.isEmpty())
            {
                g.setColour(cache.colour);
                g.strokePath(cache.stroke, juce::PathStrokeType(cache.strokeWidth, juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::rounded));
            }
        }
    };

    drawOne(false);
    drawOne(true);
}

void SpectrumAnalyzerComponent::drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        const auto pos = handlePosition(i, bounds);
        const bool selected = isSelected(i);
        const bool audible = bandAudibleInChain(i);
        const bool dimOthers = (!selectedBands.isEmpty() && !selected) || !audible;
        auto colour = Theme::bandColour(i, theme.isLight());
        const auto type = static_cast<Params::FilterType>(
            (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load());
        const bool dynOn = Params::typeSupportsDynamics(type)
            && apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f;
        if (dynOn)
            colour = Theme::dynamicsColour(colour, theme.isLight());
        if (dimOthers)
            colour = colour.withAlpha(theme.isLight() ? 0.42f : 0.35f);

        const float radius = selected ? 5.8f : 4.2f;

        if (selected)
        {
            g.setColour(colour.withAlpha(0.28f));
            g.fillEllipse(pos.x - radius - 4.0f, pos.y - radius - 4.0f,
                          (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
        }

        g.setColour(colour);
        g.fillEllipse(pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(theme.softWhite.withAlpha(selected ? 0.85f : 0.35f));
        g.drawEllipse(pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f, 1.15f);
    }
}

void SpectrumAnalyzerComponent::drawSelectionReadout(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const int band = getPrimarySelectedBand();
    if (band < 0)
        return;

    const auto freq = apvts.getRawParameterValue(Params::bandParamID(band, "freq"))->load();
    const auto gain = apvts.getRawParameterValue(Params::bandParamID(band, "gain"))->load();
    const auto q = apvts.getRawParameterValue(Params::bandParamID(band, "q"))->load();
    const auto pos = handlePosition(band, bounds);
    const auto colour = Theme::bandColour(band, theme.isLight());

    const auto label = formatFrequency(freq)
                       + "   " + juce::String(gain, 1) + " dB"
                       + "   Q " + juce::String(q, 2);

    g.setFont(Brand::uiFont(11.0f, true));
    const auto textW = 168.0f;
    const auto textH = 18.0f;
    auto bubble = juce::Rectangle<float>(textW, textH)
                      .withCentre({ pos.x, pos.y - 20.0f });
    bubble = bubble.constrainedWithin(bounds.reduced(4.0f));
    auto frame = bubble.expanded(8.0f, 4.0f);

    g.setColour(theme.isLight() ? theme.softWhite.withAlpha(0.96f) : theme.panel.withAlpha(0.94f));
    g.fillRect(frame);
    g.setColour(colour.withAlpha(0.9f));
    g.drawRect(frame, 1.0f);
    g.setColour(theme.isLight() ? theme.ink : theme.softWhite);
    g.drawFittedText(label, bubble.toNearestInt(), juce::Justification::centred, 1);
}

juce::String SpectrumAnalyzerComponent::formatFrequency(float freqHz)
{
    if (freqHz >= 1000.0f)
        return juce::String(freqHz / 1000.0f, freqHz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(freqHz, freqHz >= 100.0f ? 0 : 1) + " Hz";
}

int SpectrumAnalyzerComponent::getPrimarySelectedBand() const
{
    if (primaryBand >= 0 && isSelected(primaryBand)
        && apvts.getRawParameterValue(Params::bandParamID(primaryBand, "enabled"))->load() >= 0.5f)
        return primaryBand;

    for (int i = 0; i < Params::numBands; ++i)
        if (isSelected(i) && apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f)
            return i;
    return -1;
}

juce::Point<float> SpectrumAnalyzerComponent::getPrimaryHandlePosition() const
{
    const int band = getPrimarySelectedBand();
    const auto bounds = layoutRail().graph;
    if (band < 0)
        return bounds.getCentre();
    return handlePosition(band, bounds);
}

juce::Array<int> SpectrumAnalyzerComponent::getSelectedBandIndices() const
{
    juce::Array<int> result;
    for (int i = 0; i < Params::numBands; ++i)
        if (isSelected(i) && apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() >= 0.5f)
            result.add(i);
    return result;
}

void SpectrumAnalyzerComponent::deleteSelectedBands()
{
    auto bands = getSelectedBandIndices();
    for (int i = bands.size(); --i >= 0;)
        deleteBand(bands[i]);
}

void SpectrumAnalyzerComponent::notifySelectionChanged()
{
    if (onSelectionChanged)
        onSelectionChanged();
}

void SpectrumAnalyzerComponent::drawCreatePreview(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!createPreviewActive || sampleRate <= 0.0)
        return;

    const auto stages = FilterBand::computeStages(createPreviewType, sampleRate,
                                                   createPreviewFreq, createPreviewGain, defaultQ);
    drawResponsePath(g, bounds, stages, theme.preview.withAlpha(0.9f), 1.8f);

    const auto x = bounds.getX() + freqToX(createPreviewFreq, bounds.getWidth());
    const auto y = bounds.getY() + dbToY(createPreviewGain, bounds.getHeight(), curveMinDb, curveMaxDb);
    g.setColour(theme.preview);
    g.fillEllipse(x - 4.5f, y - 4.5f, 9.0f, 9.0f);
}

void SpectrumAnalyzerComponent::drawMarquee(juce::Graphics& g)
{
    if (gesture != Gesture::marquee)
        return;

    auto rect = juce::Rectangle<float>(gestureStartPos, gestureCurrentPos);
    g.setColour(theme.text.withAlpha(0.15f));
    g.fillRect(rect);
    g.setColour(theme.text.withAlpha(0.7f));
    g.drawRect(rect, 1.0f);
}

juce::Point<float> SpectrumAnalyzerComponent::handlePosition(int bandIndex, juce::Rectangle<float> bounds) const
{
    const auto freq = apvts.getRawParameterValue(Params::bandParamID(bandIndex, "freq"))->load();
    const auto gain = apvts.getRawParameterValue(Params::bandParamID(bandIndex, "gain"))->load();
    return { bounds.getX() + freqToX(freq, bounds.getWidth()),
             bounds.getY() + dbToY(gain, bounds.getHeight(), curveMinDb, curveMaxDb) };
}

int SpectrumAnalyzerComponent::hitTestBand(juce::Point<float> pos, juce::Rectangle<float> bounds) const
{
    int best = -1;
    float bestDist = handleHitRadiusPx;

    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        const auto handle = handlePosition(i, bounds);
        const auto dist = pos.getDistanceFrom(handle);
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }

    return best;
}

int SpectrumAnalyzerComponent::findFirstDisabledBand() const
{
    for (int i = 0; i < Params::numBands; ++i)
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            return i;
    return -1;
}

Params::FilterType SpectrumAnalyzerComponent::defaultTypeForFrequency(float freqHz)
{
    if (freqHz < lowZoneMaxHz)
        return Params::FilterType::lowShelf;
    if (freqHz > highZoneMinHz)
        return Params::FilterType::highShelf;
    return Params::FilterType::bell;
}

void SpectrumAnalyzerComponent::setBandEnabled(int bandIndex, bool enabled)
{
    if (auto* p = apvts.getParameter(Params::bandParamID(bandIndex, "enabled")))
        p->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
}

void SpectrumAnalyzerComponent::setBandType(int bandIndex, Params::FilterType type)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            apvts.getParameter(Params::bandParamID(bandIndex, "type"))))
        p->setValueNotifyingHost(p->convertTo0to1((float) static_cast<int>(type)));
}

void SpectrumAnalyzerComponent::setBandFreq(int bandIndex, float freqHz)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
            apvts.getParameter(Params::bandParamID(bandIndex, "freq"))))
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(20.0f, 20000.0f, freqHz)));
}

void SpectrumAnalyzerComponent::setBandGain(int bandIndex, float gainDb)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
            apvts.getParameter(Params::bandParamID(bandIndex, "gain"))))
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(curveMinDb, curveMaxDb, gainDb)));
}

void SpectrumAnalyzerComponent::setBandQ(int bandIndex, float q)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
            apvts.getParameter(Params::bandParamID(bandIndex, "q"))))
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(0.1f, 18.0f, q)));
}

void SpectrumAnalyzerComponent::beginBandGesture(int bandIndex)
{
    beginBandGesture(bandIndex, { "freq", "gain", "q", "type", "enabled" });
}

void SpectrumAnalyzerComponent::beginBandGesture(int bandIndex, std::initializer_list<const char*> suffixes)
{
    for (auto* suffix : suffixes)
        if (auto* p = apvts.getParameter(Params::bandParamID(bandIndex, suffix)))
            p->beginChangeGesture();
}

void SpectrumAnalyzerComponent::endBandGesture(int bandIndex)
{
    endBandGesture(bandIndex, { "freq", "gain", "q", "type", "enabled" });
}

void SpectrumAnalyzerComponent::endBandGesture(int bandIndex, std::initializer_list<const char*> suffixes)
{
    for (auto* suffix : suffixes)
        if (auto* p = apvts.getParameter(Params::bandParamID(bandIndex, suffix)))
            p->endChangeGesture();
}

void SpectrumAnalyzerComponent::selectOnly(int bandIndex)
{
    selectedBands.clear();
    if (bandIndex >= 0)
        selectedBands.addRange({ bandIndex, bandIndex + 1 });
    primaryBand = bandIndex;
    notifySelectionChanged();
    repaint();
}

void SpectrumAnalyzerComponent::toggleSelection(int bandIndex)
{
    if (isSelected(bandIndex))
        selectedBands.removeRange({ bandIndex, bandIndex + 1 });
    else
        selectedBands.addRange({ bandIndex, bandIndex + 1 });
    primaryBand = isSelected(bandIndex) ? bandIndex : getPrimarySelectedBand();
    notifySelectionChanged();
    repaint();
}

void SpectrumAnalyzerComponent::clearSelection()
{
    selectedBands.clear();
    primaryBand = -1;
    notifySelectionChanged();
    repaint();
}

bool SpectrumAnalyzerComponent::isSelected(int bandIndex) const
{
    return selectedBands.contains(bandIndex);
}

void SpectrumAnalyzerComponent::selectBandsInMarquee(juce::Rectangle<float> bounds)
{
    auto rect = juce::Rectangle<float>(gestureStartPos, gestureCurrentPos);
    if (rect.isEmpty())
        return;

    selectedBands.clear();
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;
        if (rect.contains(handlePosition(i, bounds)))
            selectedBands.addRange({ i, i + 1 });
    }
    primaryBand = getPrimarySelectedBand();
    notifySelectionChanged();
}

void SpectrumAnalyzerComponent::deleteBand(int bandIndex)
{
    beginBandGesture(bandIndex, { "enabled" });
    setBandEnabled(bandIndex, false);
    endBandGesture(bandIndex, { "enabled" });
    selectedBands.removeRange({ bandIndex, bandIndex + 1 });
    if (primaryBand == bandIndex)
        primaryBand = -1;
    notifySelectionChanged();
    repaint();
}

void SpectrumAnalyzerComponent::commitCreateAt(float freqHz, float gainDb)
{
    const int slot = findFirstDisabledBand();
    if (slot < 0)
        return;

    const auto type = defaultTypeForFrequency(freqHz);
    beginBandGesture(slot);
    setBandType(slot, type);
    setBandFreq(slot, freqHz);
    setBandGain(slot, gainDb);
    setBandQ(slot, defaultQ);
    setBandEnabled(slot, true);
    endBandGesture(slot);
    selectOnly(slot);
}

void SpectrumAnalyzerComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto bounds = layoutRail().graph;
    const auto pos = e.position;
    if (!bounds.contains(pos) && hitTestBand(pos, bounds) < 0)
    {
        gesture = Gesture::none;
        return;
    }
    gestureStartPos = pos;
    gestureCurrentPos = pos;
    createPreviewActive = false;

    // Second click of a double-click is handled in mouseDoubleClick.
    if (e.getNumberOfClicks() >= 2)
        return;

    const int hit = hitTestBand(pos, bounds);

    // Modifier-click deletes the hit band (Alt/Option on macOS).
    if (hit >= 0 && e.mods.isAltDown())
    {
        deleteBand(hit);
        gesture = Gesture::none;
        return;
    }

    if (hit >= 0)
    {
        if (e.mods.isShiftDown())
        {
            toggleSelection(hit);
            gesture = Gesture::none;
            return;
        }

        if (!isSelected(hit))
            selectOnly(hit);

        primaryBand = hit;

        for (int i = 0; i < Params::numBands; ++i)
        {
            dragStartFreqs[(size_t) i] = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
            dragStartGains[(size_t) i] = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
            dragStartQs[(size_t) i] = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
        }
        dragStartFreq = dragStartFreqs[(size_t) hit];
        dragStartGain = dragStartGains[(size_t) hit];

        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            gesture = Gesture::dragBandQ;
            for (int i = 0; i < Params::numBands; ++i)
                if (isSelected(i))
                    beginBandGesture(i, { "q" });
            return;
        }

        gesture = Gesture::dragBand;
        for (int i = 0; i < Params::numBands; ++i)
            if (isSelected(i))
                beginBandGesture(i, { "freq", "gain" });

        return;
    }

    if (e.mods.isShiftDown())
    {
        gesture = Gesture::marquee;
        return;
    }

    clearSelection();
    // Potential create-drag; activated once the pointer moves past the threshold.
    gesture = Gesture::createDrag;
    primaryBand = -1;
    createPreviewFreq = xToFreq(pos.x - bounds.getX(), bounds.getWidth());
    createPreviewGain = yToDb(pos.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
    createPreviewType = defaultTypeForFrequency(createPreviewFreq);
}

void SpectrumAnalyzerComponent::mouseDrag(const juce::MouseEvent& e)
{
    const auto bounds = layoutRail().graph;
    gestureCurrentPos = e.position;

    if (gesture == Gesture::dragBand && primaryBand >= 0)
    {
        const auto newFreq = xToFreq(e.position.x - bounds.getX(), bounds.getWidth());
        const auto newGain = yToDb(e.position.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
        const auto freqRatio = newFreq / juce::jmax(1.0e-3f, dragStartFreq);
        const auto gainDelta = newGain - dragStartGain;

        // Fine-tune freq/gain: Shift slows the move (Cmd/Ctrl + drag adjusts Q).
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const auto appliedFreq = dragStartFreq * std::pow(freqRatio, fine);
        const auto appliedGain = dragStartGain + gainDelta * fine;
        const auto appliedRatio = appliedFreq / juce::jmax(1.0e-3f, dragStartFreq);
        const auto appliedDelta = appliedGain - dragStartGain;

        for (int i = 0; i < Params::numBands; ++i)
        {
            if (!isSelected(i))
                continue;
            setBandFreq(i, dragStartFreqs[(size_t) i] * appliedRatio);
            setBandGain(i, dragStartGains[(size_t) i] + appliedDelta);
        }
        if (onBandMoved)
            onBandMoved();
        repaint();
        return;
    }

    if (gesture == Gesture::dragBandQ && primaryBand >= 0)
    {
        const float dy = gestureStartPos.y - e.position.y;
        const float qDelta = dy * 0.018f * (e.mods.isShiftDown() ? 0.2f : 1.0f);
        for (int i = 0; i < Params::numBands; ++i)
        {
            if (!isSelected(i))
                continue;
            setBandQ(i, dragStartQs[(size_t) i] + qDelta);
        }
        if (onBandMoved)
            onBandMoved();
        repaint();
        return;
    }

    if (gesture == Gesture::createDrag)
    {
        if (e.position.getDistanceFrom(gestureStartPos) >= createDragThresholdPx)
            createPreviewActive = true;

        if (createPreviewActive)
        {
            createPreviewFreq = xToFreq(gestureStartPos.x - bounds.getX(), bounds.getWidth());
            createPreviewGain = yToDb(e.position.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
            createPreviewType = defaultTypeForFrequency(createPreviewFreq);
            repaint();
        }
        return;
    }

    if (gesture == Gesture::marquee)
        repaint();
}

void SpectrumAnalyzerComponent::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    const auto bounds = layoutRail().graph;

    if (gesture == Gesture::dragBand)
    {
        for (int i = 0; i < Params::numBands; ++i)
            if (isSelected(i))
                endBandGesture(i, { "freq", "gain" });
    }
    else if (gesture == Gesture::dragBandQ)
    {
        for (int i = 0; i < Params::numBands; ++i)
            if (isSelected(i))
                endBandGesture(i, { "q" });
    }
    else if (gesture == Gesture::createDrag && createPreviewActive)
    {
        commitCreateAt(createPreviewFreq, createPreviewGain);
    }
    else if (gesture == Gesture::marquee)
    {
        selectBandsInMarquee(bounds);
    }

    createPreviewActive = false;
    gesture = Gesture::none;
    primaryBand = -1;
    repaint();
}

void SpectrumAnalyzerComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
        return;

    const auto bounds = layoutRail().graph;
    if (!bounds.contains(e.position))
        return;

    const auto freq = xToFreq(e.position.x - bounds.getX(), bounds.getWidth());
    const auto gain = yToDb(e.position.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
    commitCreateAt(freq, gain);
    gesture = Gesture::none;
    createPreviewActive = false;
}

void SpectrumAnalyzerComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto bounds = layoutRail().graph;
    int target = hitTestBand(e.position, bounds);

    if (target < 0 && selectedBands.size() == 1)
        target = selectedBands[0];

    if (target < 0 && !selectedBands.isEmpty())
    {
        // Adjust Q on every selected band.
        const float delta = wheel.deltaY * (e.mods.isShiftDown() ? 0.05f : 0.35f);
        for (int i = 0; i < Params::numBands; ++i)
        {
            if (!isSelected(i))
                continue;
            const auto q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
            beginBandGesture(i, { "q" });
            setBandQ(i, q + delta);
            endBandGesture(i, { "q" });
        }
        repaint();
        return;
    }

    if (target < 0)
        return;

    if (!isSelected(target))
        selectOnly(target);

    const float delta = wheel.deltaY * (e.mods.isShiftDown() ? 0.05f : 0.35f);
    const auto q = apvts.getRawParameterValue(Params::bandParamID(target, "q"))->load();
    beginBandGesture(target, { "q" });
    setBandQ(target, q + delta);
    endBandGesture(target, { "q" });
    repaint();
}
