#include "SpectrumAnalyzer.h"
#include "Brand.h"

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent(juce::AudioProcessorValueTreeState& stateToRead,
                                                       AnalyzerDataProvider& analyzerToRead,
                                                       double& sampleRateToRead,
                                                       const Theme& themeToUse)
    : apvts(stateToRead), analyzer(analyzerToRead), sampleRate(sampleRateToRead), theme(themeToUse)
{
    startTimerHz(30);
}

SpectrumAnalyzerComponent::~SpectrumAnalyzerComponent()
{
    stopTimer();
}

void SpectrumAnalyzerComponent::timerCallback()
{
    if (!analyzer.getMagnitudesDb(latestMagnitudesDb))
        return;

    const int n = (int) smoothedMagnitudesDb.size();

    if (!spectrumInitialized)
    {
        smoothedMagnitudesDb = latestMagnitudesDb;
        displayMagnitudesDb = latestMagnitudesDb;
        spectrumInitialized = true;
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float target = latestMagnitudesDb[(size_t) i];
            const float cur = smoothedMagnitudesDb[(size_t) i];
            const float coeff = target > cur ? spectrumAttack : spectrumRelease;
            smoothedMagnitudesDb[(size_t) i] = cur + (target - cur) * coeff;
        }
    }

    // Spatial blur in dB — turns a spiky FFT into a soft energy silhouette.
    constexpr int passes = 3;
    displayMagnitudesDb = smoothedMagnitudesDb;
    for (int pass = 0; pass < passes; ++pass)
    {
        auto src = displayMagnitudesDb;
        for (int i = 1; i < n - 1; ++i)
        {
            displayMagnitudesDb[(size_t) i] = 0.20f * src[(size_t) (i - 1)]
                                            + 0.60f * src[(size_t) i]
                                            + 0.20f * src[(size_t) (i + 1)];
        }
    }

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

void SpectrumAnalyzerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.background);
    g.fillRect(bounds);

    drawGrid(g, bounds);
    drawSpectrum(g, bounds);
    drawBandCurves(g, bounds);
    drawCreatePreview(g, bounds);
    drawBandHandles(g, bounds);
    drawSelectionReadout(g, bounds);
    drawMarquee(g);

    if (countEnabledBands() == 0 && !createPreviewActive)
        drawEmptyState(g, bounds);
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

void SpectrumAnalyzerComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Subtle vertical decade lines + labelled anchors (easier surgical targeting).
    struct FreqMark { float hz; const char* label; };
    const FreqMark marks[] = {
        { 20.0f, "20" }, { 50.0f, "" }, { 100.0f, "100" }, { 200.0f, "" },
        { 500.0f, "" }, { 1000.0f, "1k" }, { 2000.0f, "" }, { 5000.0f, "" },
        { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(Brand::uiFont(9.5f));
    for (const auto& m : marks)
    {
        const float x = bounds.getX() + freqToX(m.hz, bounds.getWidth());
        const bool major = m.label[0] != '\0';
        g.setColour(theme.grid.withAlpha(major ? 0.55f : 0.28f));
        g.drawVerticalLine((int) x, bounds.getY(), bounds.getBottom());
        if (major)
        {
            g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.45f : 0.32f));
            g.drawText(m.label,
                       juce::Rectangle<float>(x - 16.0f, bounds.getBottom() - 14.0f, 32.0f, 12.0f),
                       juce::Justification::centred, false);
        }
    }

    // 0 dB emphasis + ±12 dB guides on the curve scale.
    for (float db : { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f })
    {
        const float y = bounds.getY() + dbToY(db, bounds.getHeight(), curveMinDb, curveMaxDb);
        const bool zero = std::abs(db) < 0.01f;
        g.setColour(theme.grid.withAlpha(zero ? 0.9f : 0.55f));
        g.drawHorizontalLine((int) y, bounds.getX(), bounds.getRight());
        if (zero || std::abs(db) == 12.0f)
        {
            g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.40f : 0.28f));
            g.drawText(juce::String((int) db),
                       juce::Rectangle<float>(bounds.getRight() - 30.0f, y - 7.0f, 28.0f, 12.0f),
                       juce::Justification::centredRight, false);
        }
    }
}

void SpectrumAnalyzerComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (sampleRate <= 0.0 || !spectrumInitialized)
        return;

    juce::Path path;
    const auto width = bounds.getWidth();
    const auto spectrumH = bounds.getHeight() * spectrumHeightRatio;
    const auto spectrumTop = bounds.getBottom() - spectrumH;
    const int lastBin = AnalyzerDataProvider::fftSize / 2 - 1;
    bool started = false;

    // Sample every 2px with averaged (not peaked) energy — soft mountain, not FFT spikes.
    constexpr int step = 2;
    for (int x = 0; x < (int) width; x += step)
    {
        const float freqL = xToFreq((float) x, width);
        const float freqR = xToFreq((float) juce::jmin((int) width, x + step), width);
        const int binL = juce::jlimit(1, lastBin,
            (int) (freqL * (float) AnalyzerDataProvider::fftSize / (float) sampleRate));
        const int binR = juce::jlimit(1, lastBin,
            (int) (freqR * (float) AnalyzerDataProvider::fftSize / (float) sampleRate));

        float sum = 0.0f;
        int count = 0;
        for (int b = binL; b <= juce::jmax(binL, binR); ++b)
        {
            // Average in approximate linear power so loud spikes don't dominate the silhouette.
            sum += juce::Decibels::decibelsToGain(displayMagnitudesDb[(size_t) b], -100.0f);
            ++count;
        }
        const float db = juce::Decibels::gainToDecibels(sum / (float) juce::jmax(1, count), -100.0f);
        const float y = spectrumTop + dbToY(db, spectrumH, spectrumMinDb, spectrumMaxDb);

        if (!started)
        {
            path.startNewSubPath(bounds.getX() + (float) x, y);
            started = true;
        }
        else
        {
            path.lineTo(bounds.getX() + (float) x, y);
        }
    }

    if (!started)
        return;

    {
        juce::Path fill = path;
        fill.lineTo(bounds.getRight(), bounds.getBottom());
        fill.lineTo(bounds.getX(), bounds.getBottom());
        fill.closeSubPath();
        g.setColour(theme.spectrum.withAlpha(0.14f));
        g.fillPath(fill);
    }
    // Hairline only — the fill carries the shape (Pro-style quiet analyzer).
    g.setColour(theme.spectrum.withAlpha(0.28f));
    g.strokePath(path, juce::PathStrokeType(1.1f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void SpectrumAnalyzerComponent::drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                  const FilterBand::StageSet& stages,
                                                  juce::Colour colour, float strokeWidth,
                                                  float fillAlpha)
{
    if (sampleRate <= 0.0)
        return;

    juce::Path path;
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();
    const float zeroY = bounds.getY() + dbToY(0.0f, height, curveMinDb, curveMaxDb);
    bool started = false;

    for (int x = 0; x < (int) width; ++x)
    {
        const auto norm = (float) x / width;
        const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
        const auto magnitude = FilterBand::getMagnitudeForFrequency(stages, probeFreq, sampleRate);
        const auto db = juce::Decibels::gainToDecibels((float) magnitude, -100.0f);
        const auto y = bounds.getY() + dbToY(db, height, curveMinDb, curveMaxDb);

        if (!started)
        {
            path.startNewSubPath(bounds.getX() + (float) x, y);
            started = true;
        }
        else
        {
            path.lineTo(bounds.getX() + (float) x, y);
        }
    }

    if (started && fillAlpha > 0.001f)
    {
        juce::Path fill = path;
        fill.lineTo(bounds.getRight(), zeroY);
        fill.lineTo(bounds.getX(), zeroY);
        fill.closeSubPath();
        g.setColour(colour.withAlpha(fillAlpha));
        g.fillPath(fill);
    }

    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void SpectrumAnalyzerComponent::drawDynamicRangeFill(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                      const FilterBand::StageSet& staticStages,
                                                      const FilterBand::StageSet& extremeStages,
                                                      juce::Colour colour)
{
    if (sampleRate <= 0.0)
        return;

    juce::Path fill;
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();
    bool started = false;

    for (int x = 0; x < (int) width; ++x)
    {
        const auto norm = (float) x / width;
        const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
        const auto dbA = juce::Decibels::gainToDecibels(
            (float) FilterBand::getMagnitudeForFrequency(staticStages, probeFreq, sampleRate), -100.0f);
        const float yA = bounds.getY() + dbToY(dbA, height, curveMinDb, curveMaxDb);
        const float px = bounds.getX() + (float) x;

        if (!started)
        {
            fill.startNewSubPath(px, yA);
            started = true;
        }
        else
        {
            fill.lineTo(px, yA);
        }
    }

    if (!started)
        return;

    for (int x = (int) width - 1; x >= 0; --x)
    {
        const auto norm = (float) x / width;
        const auto probeFreq = 20.0 * std::pow(1000.0, (double) norm);
        const auto dbB = juce::Decibels::gainToDecibels(
            (float) FilterBand::getMagnitudeForFrequency(extremeStages, probeFreq, sampleRate), -100.0f);
        fill.lineTo(bounds.getX() + (float) x, bounds.getY() + dbToY(dbB, height, curveMinDb, curveMaxDb));
    }
    fill.closeSubPath();
    g.setColour(colour.withAlpha(0.16f));
    g.fillPath(fill);
}

void SpectrumAnalyzerComponent::drawBandCurves(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (sampleRate <= 0.0)
        return;

    // Draw unselected first, selected last so the active band always sits on top.
    auto drawOne = [&](int i, bool selectedPass)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            return;

        const bool selected = isSelected(i);
        if (selected != selectedPass)
            return;

        auto type = static_cast<Params::FilterType>(
            (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load());
        auto freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
        auto gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        auto q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
        auto slope = apvts.getRawParameterValue(Params::bandParamID(i, "slope"))->load();
        auto brickwall = apvts.getRawParameterValue(Params::bandParamID(i, "brickwall"))->load() >= 0.5f;
        const bool dynOn = apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f
                           && Params::typeSupportsDynamics(type);
        const float dynRange = apvts.getRawParameterValue(Params::bandParamID(i, "dynRange"))->load();

        const auto base = Theme::bandColour(i, theme.isLight());
        const auto dynTint = Theme::dynamicsColour(base, theme.isLight());
        const bool dimOthers = !selectedBands.isEmpty() && !selected;
        // Dynamic bands read a shade richer/darker than static ones.
        const auto active = dynOn ? dynTint : base;
        const auto colour = dimOthers ? active.withAlpha(theme.isLight() ? 0.38f : 0.28f)
                                      : (selected ? active : active.withAlpha(theme.isLight() ? 0.88f : 0.78f));
        const float stroke = selected ? (dynOn ? 2.7f : 2.5f) : 1.35f;
        const float fillA = selected ? (theme.isLight() ? (dynOn ? 0.24f : 0.18f) : (dynOn ? 0.20f : 0.14f))
                                     : (dimOthers ? 0.03f : (theme.isLight() ? 0.09f : 0.07f));

        const auto stages = FilterBand::computeStages(type, sampleRate, freq, gain, q, slope, brickwall);

        if (dynOn && std::abs(dynRange) > 0.05f)
        {
            const auto extreme = FilterBand::computeStages(type, sampleRate, freq,
                                                           gain + dynRange, q, slope, brickwall);
            drawDynamicRangeFill(g, bounds, stages, extreme,
                                 dynTint.withAlpha(selected ? 1.0f : 0.6f));
        }

        drawResponsePath(g, bounds, stages, colour, stroke, fillA);
    };

    for (int i = 0; i < Params::numBands; ++i)
        drawOne(i, false);
    for (int i = 0; i < Params::numBands; ++i)
        drawOne(i, true);
}

void SpectrumAnalyzerComponent::drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        const auto pos = handlePosition(i, bounds);
        const bool selected = isSelected(i);
        const bool dimOthers = !selectedBands.isEmpty() && !selected;
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
    const auto bounds = getLocalBounds().toFloat();
    const auto pos = e.position;
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

        gesture = Gesture::dragBand;
        primaryBand = hit;

        for (int i = 0; i < Params::numBands; ++i)
        {
            dragStartFreqs[(size_t) i] = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
            dragStartGains[(size_t) i] = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        }
        dragStartFreq = dragStartFreqs[(size_t) hit];
        dragStartGain = dragStartGains[(size_t) hit];

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
    const auto bounds = getLocalBounds().toFloat();
    gestureCurrentPos = e.position;

    if (gesture == Gesture::dragBand && primaryBand >= 0)
    {
        const auto newFreq = xToFreq(e.position.x - bounds.getX(), bounds.getWidth());
        const auto newGain = yToDb(e.position.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
        const auto freqRatio = newFreq / juce::jmax(1.0e-3f, dragStartFreq);
        const auto gainDelta = newGain - dragStartGain;

        // Fine-tune: Ctrl/Cmd slows the move.
        const float fine = e.mods.isCommandDown() || e.mods.isCtrlDown() ? 0.25f : 1.0f;
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
    const auto bounds = getLocalBounds().toFloat();

    if (gesture == Gesture::dragBand)
    {
        for (int i = 0; i < Params::numBands; ++i)
            if (isSelected(i))
                endBandGesture(i, { "freq", "gain" });
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

    const auto bounds = getLocalBounds().toFloat();
    const auto freq = xToFreq(e.position.x - bounds.getX(), bounds.getWidth());
    const auto gain = yToDb(e.position.y - bounds.getY(), bounds.getHeight(), curveMinDb, curveMaxDb);
    commitCreateAt(freq, gain);
    gesture = Gesture::none;
    createPreviewActive = false;
}

void SpectrumAnalyzerComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto bounds = getLocalBounds().toFloat();
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
