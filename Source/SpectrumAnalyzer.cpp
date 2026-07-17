#include "SpectrumAnalyzer.h"

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent(juce::AudioProcessorValueTreeState& stateToRead,
                                                       AnalyzerDataProvider& analyzerToRead,
                                                       double& sampleRateToRead)
    : apvts(stateToRead), analyzer(analyzerToRead), sampleRate(sampleRateToRead)
{
    startTimerHz(30);
}

SpectrumAnalyzerComponent::~SpectrumAnalyzerComponent()
{
    stopTimer();
}

void SpectrumAnalyzerComponent::timerCallback()
{
    if (analyzer.getMagnitudesDb(latestMagnitudesDb))
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
    g.fillAll(juce::Colour(0xff1a1a1e));

    drawGrid(g, bounds);
    drawSpectrum(g, bounds);
    drawBandCurves(g, bounds);
    drawCreatePreview(g, bounds);
    drawBandHandles(g, bounds);
    drawMarquee(g);
}

void SpectrumAnalyzerComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::white.withAlpha(0.15f));

    for (float freq : { 100.0f, 1000.0f, 10000.0f })
    {
        auto x = freqToX(freq, bounds.getWidth());
        g.drawVerticalLine((int) x, bounds.getY(), bounds.getBottom());
    }

    // 0 dB on the curve scale (display centre) plus spectrum reference lines.
    for (float db : { 0.0f, -12.0f, 12.0f })
    {
        auto y = dbToY(db, bounds.getHeight(), curveMinDb, curveMaxDb);
        g.drawHorizontalLine((int) y, bounds.getX(), bounds.getRight());
    }
}

void SpectrumAnalyzerComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
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
        const auto freq = 20.0f * std::pow(1000.0f, norm);
        const auto bin = juce::jlimit(1, AnalyzerDataProvider::fftSize / 2 - 1,
                                       (int) (freq * AnalyzerDataProvider::fftSize / sampleRate));
        const auto db = latestMagnitudesDb[(size_t) bin];
        const auto y = dbToY(db, height, spectrumMinDb, spectrumMaxDb);

        if (!started)
        {
            path.startNewSubPath(bounds.getX() + (float) x, bounds.getY() + y);
            started = true;
        }
        else
        {
            path.lineTo(bounds.getX() + (float) x, bounds.getY() + y);
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.strokePath(path, juce::PathStrokeType(1.0f));
}

void SpectrumAnalyzerComponent::drawResponsePath(juce::Graphics& g, juce::Rectangle<float> bounds,
                                                  Params::FilterType type, float freq, float gain, float q,
                                                  juce::Colour colour, float strokeWidth)
{
    if (sampleRate <= 0.0)
        return;

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
        const auto y = dbToY(db, height, curveMinDb, curveMaxDb);

        if (!started)
        {
            path.startNewSubPath(bounds.getX() + (float) x, bounds.getY() + y);
            started = true;
        }
        else
        {
            path.lineTo(bounds.getX() + (float) x, bounds.getY() + y);
        }
    }

    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(strokeWidth));
}

void SpectrumAnalyzerComponent::drawBandCurves(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        auto type = static_cast<Params::FilterType>(
            (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load());
        auto freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
        auto gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        auto q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();

        const bool selected = isSelected(i);
        const auto colour = selected ? juce::Colour(0xffffc060)
                                     : juce::Colour(0xffe0a030).withAlpha(selectedBands.isEmpty() ? 0.85f : 0.35f);
        const float stroke = selected ? 2.2f : 1.4f;

        drawResponsePath(g, bounds, type, freq, gain, q, colour, stroke);
    }
}

void SpectrumAnalyzerComponent::drawBandHandles(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        const auto pos = handlePosition(i, bounds);
        const bool selected = isSelected(i);
        const auto fill = selected ? juce::Colour(0xffffc060) : juce::Colour(0xffe0a030);
        const float radius = selected ? 5.5f : 4.0f;

        g.setColour(fill.withAlpha(0.95f));
        g.fillEllipse(pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawEllipse(pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
    }
}

void SpectrumAnalyzerComponent::drawCreatePreview(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!createPreviewActive)
        return;

    drawResponsePath(g, bounds, createPreviewType, createPreviewFreq, createPreviewGain, defaultQ,
                     juce::Colour(0xff80c0ff).withAlpha(0.9f), 1.8f);

    const auto x = bounds.getX() + freqToX(createPreviewFreq, bounds.getWidth());
    const auto y = bounds.getY() + dbToY(createPreviewGain, bounds.getHeight(), curveMinDb, curveMaxDb);
    g.setColour(juce::Colour(0xff80c0ff));
    g.fillEllipse(x - 4.5f, y - 4.5f, 9.0f, 9.0f);
}

void SpectrumAnalyzerComponent::drawMarquee(juce::Graphics& g)
{
    if (gesture != Gesture::marquee)
        return;

    auto rect = juce::Rectangle<float>(gestureStartPos, gestureCurrentPos);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRect(rect);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
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
    repaint();
}

void SpectrumAnalyzerComponent::toggleSelection(int bandIndex)
{
    if (isSelected(bandIndex))
        selectedBands.removeRange({ bandIndex, bandIndex + 1 });
    else
        selectedBands.addRange({ bandIndex, bandIndex + 1 });
    repaint();
}

void SpectrumAnalyzerComponent::clearSelection()
{
    selectedBands.clear();
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
}

void SpectrumAnalyzerComponent::deleteBand(int bandIndex)
{
    beginBandGesture(bandIndex, { "enabled" });
    setBandEnabled(bandIndex, false);
    endBandGesture(bandIndex, { "enabled" });
    selectedBands.removeRange({ bandIndex, bandIndex + 1 });
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
