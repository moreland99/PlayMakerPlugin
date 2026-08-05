#include "PluginEditor.h"
#include "Brand.h"

PlaymakersEQAudioProcessorEditor::PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor& p)
    : AudioProcessorEditor(&p), eqProcessor(p),
      analyzer(p.apvts, p.getPostAnalyzer(), p.getSampleRateRef(), themeManager.current())
{
    setLookAndFeel(&lookAndFeel);

    const auto themeJSON = eqProcessor.apvts.state.getProperty("themeJSON").toString();
    if (themeJSON.isNotEmpty())
        themeManager.setTheme(Theme::fromJSON(themeJSON, Theme::dark()));

    themeManager.onThemeChanged = [this]
    {
        eqProcessor.apvts.state.setProperty("themeJSON", themeManager.current().toJSON(), nullptr);
        lookAndFeel.setTheme(themeManager.current());
        applyThemeToButtons();
        applyThemeToInspector();
        analyzer.repaint();
        repaint();
    };

    undoButton.onClick = [this] { eqProcessor.undoManager.undo(); };
    redoButton.onClick = [this] { eqProcessor.undoManager.redo(); };
    abButton.onClick = [this]
    {
        eqProcessor.toggleAB();
        abButton.setButtonText(eqProcessor.isOnSlotA() ? "A" : "B");
    };
    copyButton.onClick = [this] { eqProcessor.copyCurrentToOtherSlot(); };
    expandButton.onClick = [this]
    {
        expandedView = !expandedView;
        expandButton.setButtonText(expandedView ? "Compact" : "Expand");
        setSize(expandedView ? expandedWidth : normalWidth,
                expandedView ? expandedHeight : normalHeight);
    };
    themeButton.onClick = [this]
    {
        const bool goingLight = themeManager.current().name != "Light";
        themeManager.setTheme(goingLight ? Theme::light() : Theme::dark());
        themeButton.setButtonText(goingLight ? "Dark" : "Light");
    };

    typeBox.addItemList(Params::filterTypeNames(), 1);
    typeBox.onChange = [this]
    {
        if (updatingInspector)
            return;
        applyTypeToSelection(typeBox.getSelectedItemIndex());
    };

    removeButton.onClick = [this]
    {
        analyzer.deleteSelectedBands();
        refreshInspector();
    };

    dynEnableButton.onClick = [this]
    {
        if (updatingInspector)
            return;
        applyDynEnabledToSelection(dynEnableButton.getToggleState());
        refreshInspector();
    };

    abButton.getProperties().set("pmAccent", true);
    removeButton.getProperties().set("pmAccent", true);
    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &expandButton, &themeButton })
        b->getProperties().set("pmChrome", true);

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
        s->setEnabled(false);
    }

    auto wireMetricEdit = [this](juce::Label& label, const char* suffix)
    {
        label.setEditable(true, false, false);
        label.setTooltip("Click to type an exact value");
        label.onTextChange = nullptr;
        label.onEditorHide = [this, &label, suffix]
        {
            commitMetricFromLabel(label, suffix);
        };
    };
    wireMetricEdit(freqValueLabel, "freq");
    wireMetricEdit(gainValueLabel, "gain");
    wireMetricEdit(qValueLabel, "q");

    analyzer.onSelectionChanged = [this] { refreshInspector(); };

    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &expandButton, &themeButton, &removeButton })
        addAndMakeVisible(*b);

    addAndMakeVisible(analyzer);
    addAndMakeVisible(inspectorTitle);
    addAndMakeVisible(freqCaption);
    addAndMakeVisible(gainCaption);
    addAndMakeVisible(qCaption);
    addAndMakeVisible(freqValueLabel);
    addAndMakeVisible(gainValueLabel);
    addAndMakeVisible(qValueLabel);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeBox);
    addAndMakeVisible(dynSectionLabel);
    addAndMakeVisible(dynEnableButton);
    addAndMakeVisible(dynThresholdSlider);
    addAndMakeVisible(dynRangeSlider);
    addAndMakeVisible(dynRatioSlider);
    addAndMakeVisible(dynAttackSlider);
    addAndMakeVisible(dynReleaseSlider);
    addAndMakeVisible(dynThresholdLabel);
    addAndMakeVisible(dynRangeLabel);
    addAndMakeVisible(dynRatioLabel);
    addAndMakeVisible(dynAttackLabel);
    addAndMakeVisible(dynReleaseLabel);
    addAndMakeVisible(emptyHint);

    applyThemeToButtons();
    applyThemeToInspector();
    refreshInspector();

    setWantsKeyboardFocus(true);
    startTimerHz(15);
    setResizable(true, true);
    setResizeLimits(880, 560, 1800, 1200);
    setSize(normalWidth, normalHeight);
}

PlaymakersEQAudioProcessorEditor::~PlaymakersEQAudioProcessorEditor()
{
    clearInspectorBindings();
    setLookAndFeel(nullptr);
}

void PlaymakersEQAudioProcessorEditor::clearInspectorBindings()
{
    typeAttachment.reset();
    dynEnableAttachment.reset();
    dynThresholdAttachment.reset();
    dynRangeAttachment.reset();
    dynRatioAttachment.reset();
    dynAttackAttachment.reset();
    dynReleaseAttachment.reset();
    boundBand = -1;
}

void PlaymakersEQAudioProcessorEditor::bindInspectorToBand(int bandIndex)
{
    if (bandIndex == boundBand)
        return;

    clearInspectorBindings();
    if (bandIndex < 0)
        return;

    boundBand = bandIndex;
    auto& apvts = eqProcessor.apvts;

    // Type is applied manually so multi-select can share the change.
    typeAttachment.reset();
    dynEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynEnabled"), dynEnableButton);
    dynThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynThreshold"), dynThresholdSlider);
    dynRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRange"), dynRangeSlider);
    dynRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRatio"), dynRatioSlider);
    dynAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynAttack"), dynAttackSlider);
    dynReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRelease"), dynReleaseSlider);
}

void PlaymakersEQAudioProcessorEditor::applyTypeToSelection(int typeIndex)
{
    auto bands = analyzer.getSelectedBandIndices();
    if (bands.isEmpty() || typeIndex < 0)
        return;

    for (auto band : bands)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
                eqProcessor.apvts.getParameter(Params::bandParamID(band, "type"))))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float) typeIndex));
            p->endChangeGesture();
        }
    }
    refreshInspector();
}

void PlaymakersEQAudioProcessorEditor::applyDynEnabledToSelection(bool enabled)
{
    auto bands = analyzer.getSelectedBandIndices();
    for (auto band : bands)
    {
        if (auto* p = eqProcessor.apvts.getParameter(Params::bandParamID(band, "dynEnabled")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    }
}

void PlaymakersEQAudioProcessorEditor::refreshInspector()
{
    updatingInspector = true;

    const auto bands = analyzer.getSelectedBandIndices();
    const int primary = analyzer.getPrimarySelectedBand();
    const bool hasSelection = primary >= 0;

    emptyHint.setVisible(!hasSelection);
    inspectorTitle.setVisible(hasSelection);
    freqCaption.setVisible(hasSelection);
    gainCaption.setVisible(hasSelection);
    qCaption.setVisible(hasSelection);
    freqValueLabel.setVisible(hasSelection);
    gainValueLabel.setVisible(hasSelection);
    qValueLabel.setVisible(hasSelection);
    typeLabel.setVisible(hasSelection);
    typeBox.setVisible(hasSelection);
    removeButton.setVisible(hasSelection);
    dynSectionLabel.setVisible(hasSelection);
    dynEnableButton.setVisible(hasSelection);

    if (!hasSelection)
    {
        clearInspectorBindings();
        for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
            s->setVisible(false);
        for (auto* l : { &dynThresholdLabel, &dynRangeLabel, &dynRatioLabel, &dynAttackLabel, &dynReleaseLabel })
            l->setVisible(false);
        dynSectionBounds = {};
        for (auto& c : metricCardBounds) c = {};
        updatingInspector = false;
        return;
    }

    bindInspectorToBand(primary);

    const auto freq = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "freq"))->load();
    const auto gain = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "gain"))->load();
    const auto q = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "q"))->load();
    const auto typeIndex = (int) eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "type"))->load();
    const auto type = static_cast<Params::FilterType>(typeIndex);
    const bool dynOn = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "dynEnabled"))->load() >= 0.5f;
    const bool canDyn = Params::typeSupportsDynamics(type);

    const auto title = bands.size() > 1
        ? ("Band " + juce::String(primary + 1) + "  (+" + juce::String(bands.size() - 1) + " selected)")
        : ("Band " + juce::String(primary + 1));
    inspectorTitle.setText(title, juce::dontSendNotification);

    auto freqText = freq >= 1000.0f
        ? (juce::String(freq / 1000.0f, freq >= 10000.0f ? 1 : 2) + " kHz")
        : (juce::String(freq, freq >= 100.0f ? 0 : 1) + " Hz");
    freqValueLabel.setText(freqText, juce::dontSendNotification);
    gainValueLabel.setText(juce::String(gain, 1) + " dB", juce::dontSendNotification);
    qValueLabel.setText(juce::String(q, 2), juce::dontSendNotification);

    typeBox.setSelectedItemIndex(typeIndex, juce::dontSendNotification);

    dynEnableButton.setEnabled(canDyn);
    dynEnableButton.setToggleState(canDyn && dynOn, juce::dontSendNotification);
    dynEnableButton.setButtonText(canDyn ? "On" : "N/A");
    dynSectionLabel.setText(canDyn ? "DYNAMICS" : "DYNAMICS (N/A)", juce::dontSendNotification);

    const bool showDynSliders = canDyn && dynOn;
    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
    {
        s->setVisible(showDynSliders);
        s->setEnabled(showDynSliders);
    }
    for (auto* l : { &dynThresholdLabel, &dynRangeLabel, &dynRatioLabel, &dynAttackLabel, &dynReleaseLabel })
        l->setVisible(showDynSliders);

    updatingInspector = false;
    applyBandAccentToInspector(primary);
    resized();
    repaint();
}

void PlaymakersEQAudioProcessorEditor::timerCallback()
{
    eqProcessor.undoManager.beginNewTransaction();
    abButton.setButtonText(eqProcessor.isOnSlotA() ? "A" : "B");

    const int primary = analyzer.getPrimarySelectedBand();
    if (primary < 0)
        return;

    const auto dynOn = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "dynEnabled"))->load() >= 0.5f;
    const auto type = static_cast<Params::FilterType>(
        (int) eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "type"))->load());
    const bool canDyn = Params::typeSupportsDynamics(type);
    const bool wantDynSliders = canDyn && dynOn;

    if (!isEditingMetrics())
    {
        const auto freq = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "freq"))->load();
        const auto gain = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "gain"))->load();
        const auto q = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "q"))->load();

        auto freqText = freq >= 1000.0f
            ? (juce::String(freq / 1000.0f, freq >= 10000.0f ? 1 : 2) + " kHz")
            : (juce::String(freq, freq >= 100.0f ? 0 : 1) + " Hz");
        freqValueLabel.setText(freqText, juce::dontSendNotification);
        gainValueLabel.setText(juce::String(gain, 1) + " dB", juce::dontSendNotification);
        qValueLabel.setText(juce::String(q, 2), juce::dontSendNotification);
    }

    if (wantDynSliders != dynThresholdSlider.isVisible())
        refreshInspector();
}

bool PlaymakersEQAudioProcessorEditor::isEditingMetrics() const
{
    return freqValueLabel.isBeingEdited()
        || gainValueLabel.isBeingEdited()
        || qValueLabel.isBeingEdited();
}

float PlaymakersEQAudioProcessorEditor::parseFrequencyText(const juce::String& text)
{
    auto t = text.trim().toLowerCase();
    float mult = 1.0f;
    if (t.containsChar('k'))
        mult = 1000.0f;
    t = t.retainCharacters("0123456789.+-");
    return t.getFloatValue() * mult;
}

float PlaymakersEQAudioProcessorEditor::parseFloatText(const juce::String& text)
{
    return text.trim().retainCharacters("0123456789.+-").getFloatValue();
}

void PlaymakersEQAudioProcessorEditor::commitMetricFromLabel(juce::Label& label, const char* paramSuffix)
{
    const int band = analyzer.getPrimarySelectedBand();
    if (band < 0)
        return;

    auto* p = dynamic_cast<juce::AudioParameterFloat*>(
        eqProcessor.apvts.getParameter(Params::bandParamID(band, paramSuffix)));
    if (p == nullptr)
        return;

    float value = juce::String(paramSuffix) == "freq" ? parseFrequencyText(label.getText())
                                                      : parseFloatText(label.getText());
    if (!std::isfinite(value))
    {
        refreshInspector();
        return;
    }

    value = p->getNormalisableRange().snapToLegalValue(value);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(value));
    p->endChangeGesture();
    refreshInspector();
}

void PlaymakersEQAudioProcessorEditor::applyThemeToButtons()
{
    abButton.getProperties().set("pmAccent", true);
    removeButton.getProperties().set("pmAccent", true);
    lookAndFeel.setTheme(themeManager.current());

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
}

void PlaymakersEQAudioProcessorEditor::applyThemeToInspector()
{
    const auto& t = themeManager.current();

    auto prep = [&](juce::Label& l, float size, bool bold = false)
    {
        l.setColour(juce::Label::textColourId, t.ink.withAlpha(0.92f));
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l.setColour(juce::TextEditor::textColourId, t.ink);
        l.setColour(juce::TextEditor::backgroundColourId,
                    t.isLight() ? t.softWhite : t.panel.brighter(0.1f));
        l.setColour(juce::TextEditor::outlineColourId, t.ink.withAlpha(0.28f));
        l.setColour(juce::TextEditor::highlightColourId, t.signalOrange.withAlpha(0.45f));
        l.setColour(juce::TextEditor::highlightedTextColourId, t.softWhite);
        l.setFont(Brand::uiFont(size, bold));
        l.setJustificationType(juce::Justification::centredLeft);
    };

    auto prepCaption = [&](juce::Label& l)
    {
        l.setColour(juce::Label::textColourId, t.inkMuted);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l.setFont(Brand::titleFont(9.0f));
        l.setJustificationType(juce::Justification::centredLeft);
    };

    prep(inspectorTitle, 12.0f, true);
    prepCaption(freqCaption);
    prepCaption(gainCaption);
    prepCaption(qCaption);
    prepCaption(typeLabel);
    prepCaption(dynSectionLabel);
    prep(freqValueLabel, 15.0f, true);
    prep(gainValueLabel, 15.0f, true);
    prep(qValueLabel, 15.0f, true);
    prep(emptyHint, 12.0f, false);
    prep(dynThresholdLabel, 11.0f, false);
    prep(dynRangeLabel, 11.0f, false);
    prep(dynRatioLabel, 11.0f, false);
    prep(dynAttackLabel, 11.0f, false);
    prep(dynReleaseLabel, 11.0f, false);

    emptyHint.setColour(juce::Label::textColourId, t.inkMuted);

    typeBox.setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.95f));
    typeBox.setColour(juce::ComboBox::arrowColourId, t.ink.withAlpha(0.7f));
    typeBox.setColour(juce::ComboBox::outlineColourId, t.ink.withAlpha(0.22f));
    typeBox.setColour(juce::ComboBox::backgroundColourId,
                      t.isLight() ? t.softWhite.withAlpha(0.7f) : t.softWhite.withAlpha(0.04f));

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
    {
        s->setColour(juce::Slider::textBoxTextColourId, t.ink.withAlpha(0.95f));
        s->setColour(juce::Slider::textBoxBackgroundColourId,
                     t.isLight() ? t.softWhite.withAlpha(0.85f) : t.softWhite.withAlpha(0.04f));
        s->setColour(juce::Slider::textBoxOutlineColourId, t.ink.withAlpha(0.18f));
    }

    applyBandAccentToInspector(analyzer.getPrimarySelectedBand());
}

void PlaymakersEQAudioProcessorEditor::applyBandAccentToInspector(int bandIndex)
{
    const auto& t = themeManager.current();
    const auto colour = bandIndex >= 0 ? Theme::bandColour(bandIndex, t.isLight()) : t.signalOrange;

    bool dynOn = false;
    if (bandIndex >= 0)
    {
        const auto type = static_cast<Params::FilterType>(
            (int) eqProcessor.apvts.getRawParameterValue(Params::bandParamID(bandIndex, "type"))->load());
        dynOn = Params::typeSupportsDynamics(type)
            && eqProcessor.apvts.getRawParameterValue(Params::bandParamID(bandIndex, "dynEnabled"))->load() >= 0.5f;
    }

    const auto dynColour = dynOn ? Theme::dynamicsColour(colour, t.isLight()) : colour;
    const auto colourStr = colour.toString();
    const auto dynColourStr = dynColour.toString();

    inspectorTitle.setColour(juce::Label::textColourId, colour);
    dynSectionLabel.setColour(juce::Label::textColourId, dynColour.withAlpha(dynOn ? 1.0f : 0.7f));
    removeButton.getProperties().set("pmAccentColour", colourStr);
    typeBox.getProperties().set("pmAccentColour", colourStr);
    dynEnableButton.getProperties().set("pmAccentColour", dynColourStr);

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
        s->getProperties().set("pmAccentColour", dynColourStr);

    removeButton.repaint();
    dynEnableButton.repaint();
    typeBox.repaint();
    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
        s->repaint();
}

bool PlaymakersEQAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    const auto noShift = juce::ModifierKeys::commandModifier;
    const auto withShift = juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier;

    if (key == juce::KeyPress('z', juce::ModifierKeys(noShift), 0))
    {
        eqProcessor.undoManager.undo();
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys(withShift), 0)
        || key == juce::KeyPress('y', juce::ModifierKeys(noShift), 0))
    {
        eqProcessor.undoManager.redo();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        analyzer.deleteSelectedBands();
        refreshInspector();
        return true;
    }
    return false;
}

void PlaymakersEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& t = themeManager.current();
    g.fillAll(t.background);

    auto headerBounds = getLocalBounds().removeFromTop(44);
    g.setColour(t.header);
    g.fillRect(headerBounds);
    g.setColour(t.accent);
    g.fillRect(headerBounds.removeFromBottom(2));
    Brand::drawHeaderLockup(g, brandLockupBounds.toFloat(), t);

    g.setColour(t.panel);
    g.fillRect(inspectorBounds);

    const int primary = analyzer.getPrimarySelectedBand();
    const auto bandCol = primary >= 0 ? Theme::bandColour(primary, t.isLight()) : t.accent;
    bool dynOn = false;
    if (primary >= 0)
    {
        const auto type = static_cast<Params::FilterType>(
            (int) eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "type"))->load());
        dynOn = Params::typeSupportsDynamics(type)
            && eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "dynEnabled"))->load() >= 0.5f;
    }
    const auto dynCol = dynOn ? Theme::dynamicsColour(bandCol, t.isLight()) : bandCol;

    g.setColour(bandCol.withAlpha(0.65f));
    g.fillRect(inspectorBounds.withHeight(2));

    if (primary >= 0)
    {
        g.setColour(bandCol);
        g.fillRect(inspectorBounds.getX(), inspectorBounds.getY(), 3, inspectorBounds.getHeight());

        // Metric cards — FREQ / GAIN / Q as discrete readouts.
        for (const auto& card : metricCardBounds)
        {
            if (card.isEmpty())
                continue;
            g.setColour(t.isLight() ? t.softWhite.withAlpha(0.85f) : t.softWhite.withAlpha(0.04f));
            g.fillRect(card.toFloat());
            g.setColour(bandCol.withAlpha(t.isLight() ? 0.45f : 0.35f));
            g.fillRect((float) card.getX(), (float) card.getY(), 2.0f, (float) card.getHeight());
            g.setColour(t.ink.withAlpha(t.isLight() ? 0.10f : 0.08f));
            g.drawRect(card.toFloat(), 1.0f);
        }

        // Dynamics section plate — richer colour when Dynamics is On.
        if (!dynSectionBounds.isEmpty())
        {
            g.setColour(dynOn
                ? dynCol.withAlpha(t.isLight() ? 0.14f : 0.10f)
                : (t.isLight() ? t.softWhite.withAlpha(0.55f) : t.softWhite.withAlpha(0.035f)));
            g.fillRect(dynSectionBounds.toFloat());
            g.setColour(dynCol.withAlpha(dynOn ? (t.isLight() ? 0.55f : 0.45f)
                                               : (t.isLight() ? 0.22f : 0.18f)));
            g.drawRect(dynSectionBounds.toFloat(), dynOn ? 1.5f : 1.0f);
        }
    }
}

void PlaymakersEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(44).reduced(12, 8);

    brandLockupBounds = header.removeFromLeft(158);
    header.removeFromLeft(12);

    auto placeHeaderBtn = [&header](juce::TextButton& b, int w)
    {
        b.setBounds(header.removeFromLeft(w).withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
        header.removeFromLeft(6);
    };

    placeHeaderBtn(undoButton, 60);
    placeHeaderBtn(redoButton, 60);

    auto placeRight = [&header](juce::TextButton& b, int w)
    {
        auto area = header.removeFromRight(w);
        header.removeFromRight(6);
        b.setBounds(area.withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
    };

    placeRight(themeButton, 58);
    placeRight(expandButton, 72);
    placeRight(copyButton, 58);
    placeRight(abButton, 34);

    const bool hasBand = analyzer.getPrimarySelectedBand() >= 0;
    const int inspectorH = hasBand ? 152 : 40;
    inspectorBounds = bounds.removeFromBottom(inspectorH);
    analyzer.setBounds(bounds.reduced(8, 4));

    for (auto& c : metricCardBounds) c = {};
    dynSectionBounds = {};

    auto panel = inspectorBounds.reduced(12, 10);
    if (emptyHint.isVisible())
    {
        emptyHint.setBounds(panel);
        return;
    }

    auto top = panel.removeFromTop(48);
    inspectorTitle.setBounds(top.removeFromLeft(92).withTrimmedTop(14));
    top.removeFromLeft(8);

    // Right chrome first, then stretch metric cards across remaining width.
    auto removeArea = top.removeFromRight(86);
    top.removeFromRight(8);
    auto typeCol = top.removeFromRight(168);
    top.removeFromRight(8);
    removeButton.setBounds(removeArea.withHeight(30).withY(top.getY() + 12));
    typeLabel.setBounds(typeCol.removeFromTop(12));
    typeBox.setBounds(typeCol.withHeight(28));

    const int gaps = 8 * 2;
    const int cardW = juce::jmax(110, (top.getWidth() - gaps) / 3);
    auto placeMetric = [&](int index, juce::Label& caption, juce::Label& value)
    {
        auto card = top.removeFromLeft(cardW);
        if (index < 2)
            top.removeFromLeft(8);
        metricCardBounds[index] = card;
        auto inner = card.reduced(12, 6);
        caption.setBounds(inner.removeFromTop(12));
        value.setBounds(inner);
        value.setJustificationType(juce::Justification::centredLeft);
    };
    placeMetric(0, freqCaption, freqValueLabel);
    placeMetric(1, gainCaption, gainValueLabel);
    placeMetric(2, qCaption, qValueLabel);

    panel.removeFromTop(8);
    dynSectionBounds = panel.removeFromTop(dynThresholdSlider.isVisible() ? 66 : 32);
    auto dynInner = dynSectionBounds.reduced(12, 8);

    auto dynHead = dynInner.removeFromTop(18);
    dynSectionLabel.setBounds(dynHead.removeFromLeft(120));
    dynEnableButton.setBounds(dynHead.removeFromLeft(60));

    if (dynThresholdSlider.isVisible())
    {
        dynInner.removeFromTop(6);
        auto row = dynInner.removeFromTop(28);
        const int slotW = row.getWidth() / 5;
        auto place = [&row, slotW](juce::Label& l, juce::Slider& s)
        {
            auto slot = row.removeFromLeft(slotW).reduced(4, 0);
            l.setBounds(slot.removeFromLeft(58).withTrimmedTop(6));
            s.setBounds(slot);
        };
        place(dynThresholdLabel, dynThresholdSlider);
        place(dynRangeLabel, dynRangeSlider);
        place(dynRatioLabel, dynRatioSlider);
        place(dynAttackLabel, dynAttackSlider);
        place(dynReleaseLabel, dynReleaseSlider);
    }
}
