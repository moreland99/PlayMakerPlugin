#include "PluginEditor.h"
#include "Brand.h"

PlaymakersEQAudioProcessorEditor::PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor& p)
    : AudioProcessorEditor(&p), eqProcessor(p),
      analyzer(p.apvts, p.getPostAnalyzer(), p.getPreAnalyzer(), p.getSampleRateRef(), themeManager.current(),
               &p.dynDisplayOffsetDb),
      presetBrowser(p.presetManager, themeManager)
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

    auto afterPresetLoad = [this]
    {
        const auto themeJSON = eqProcessor.apvts.state.getProperty("themeJSON").toString();
        if (themeJSON.isNotEmpty())
            themeManager.setTheme(Theme::fromJSON(themeJSON, Theme::dark()));

        lookAndFeel.setTheme(themeManager.current());
        applyThemeToButtons();
        applyThemeToInspector();
        presetBrowser.applyTheme(themeManager.current());
        refreshInspector();
        analyzer.repaint();
        presetBrowser.refreshFromManager();
        repaint();
    };

    eqProcessor.presetManager.onPresetLoaded = afterPresetLoad;

    presetBrowser.onPresetApplied = afterPresetLoad;

    presetBrowser.onThemeChanged = [this]
    {
        eqProcessor.apvts.state.setProperty("themeJSON", themeManager.current().toJSON(), nullptr);
        lookAndFeel.setTheme(themeManager.current());
        applyThemeToButtons();
        applyThemeToInspector();
        presetBrowser.applyTheme(themeManager.current());
        analyzer.repaint();
        repaint();
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

    bandEnabledButton.onClick = [this]
    {
        if (updatingInspector)
            return;
        if (!bandEnabledButton.getToggleState())
        {
            auto bands = analyzer.getSelectedBandIndices();
            for (auto band : bands)
            {
                if (auto* p = eqProcessor.apvts.getParameter(Params::bandParamID(band, "solo")))
                    p->setValueNotifyingHost(0.0f);
            }
        }
        refreshInspector();
    };

    bandSoloButton.onClick = [this]
    {
        if (updatingInspector)
            return;
        applySoloToSelection(bandSoloButton.getToggleState());
        refreshInspector();
        analyzer.repaint();
    };
    bandSoloButton.setTooltip("Solo this band (only solo'd bands affect audio). Shortcut: S");

    dynEnableButton.onClick = [this]
    {
        if (updatingInspector)
            return;
        applyDynEnabledToSelection(dynEnableButton.getToggleState());
        refreshInspector();
    };

    dynThresholdAutoButton.onClick = [this] { applyAutoThresholdCaptureToSelection(); };
    dynThresholdAutoButton.setTooltip("Set threshold from current band level (play audio first)");
    dynAutoThresholdButton.setTooltip("Follow input level as threshold (uses previous block's band level)");
    dynAutoThresholdButton.onClick = [this]
    {
        if (!updatingInspector)
            refreshInspector();
    };
    dynRangeSlider.onValueChange = [this]
    {
        if (updatingInspector)
            return;
        updateDynRangeLabelForBand(analyzer.getPrimarySelectedBand());
    };

    abButton.getProperties().set("pmAccent", true);
    removeButton.getProperties().set("pmAccent", true);
    dynThresholdAutoButton.getProperties().set("pmChrome", true);
    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &expandButton })
        b->getProperties().set("pmChrome", true);
    for (auto* b : { &specPreButton, &specPostButton, &specFreezeButton })
        b->getProperties().set("pmChrome", true);
    pluginBypassButton.getProperties().set("pmChrome", true);
    dynAutoThresholdButton.getProperties().set("pmChrome", true);

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider, &dynSidechainSlider })
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        s->setEnabled(false);
    }

    qKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    qKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    qKnob.setEnabled(false);
    qKnob.setTooltip("Q — also scroll wheel or ⌘-drag handle on the graph");

    slopeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    slopeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    slopeSlider.setEnabled(false);

    balanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    balanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    balanceSlider.setEnabled(false);

    stereoModeBox.addItemList(Params::stereoModeNames(), 1);

    displayRangeBox.addItem("±6 dB", 1);
    displayRangeBox.addItem("±12 dB", 2);
    displayRangeBox.addItem("±24 dB", 3);
    displayRangeBox.addItem("±30 dB", 4);
    displayRangeBox.setSelectedId(3, juce::dontSendNotification);
    displayRangeBox.onChange = [this]
    {
        static const float halves[] = { 6.0f, 12.0f, 24.0f, 30.0f };
        const int id = displayRangeBox.getSelectedId();
        if (id >= 1 && id <= 4)
        {
            analyzer.setDisplayRangeHalfDb(halves[id - 1]);
            eqProcessor.apvts.state.setProperty("displayRangeHalfDb", halves[id - 1], nullptr);
        }
    };

    const float savedRange = (float) eqProcessor.apvts.state.getProperty("displayRangeHalfDb", 24.0);
    if (savedRange == 6.0f) displayRangeBox.setSelectedId(1, juce::dontSendNotification);
    else if (savedRange == 12.0f) displayRangeBox.setSelectedId(2, juce::dontSendNotification);
    else if (savedRange == 30.0f) displayRangeBox.setSelectedId(4, juce::dontSendNotification);
    else displayRangeBox.setSelectedId(3, juce::dontSendNotification);
    analyzer.setDisplayRangeHalfDb(savedRange);

    specPostButton.setToggleState(true, juce::dontSendNotification);
    specPreButton.setToggleState(false, juce::dontSendNotification);
    specSpanBox.addItem("60 dB", 1);
    specSpanBox.addItem("90 dB", 2);
    specSpanBox.setSelectedId(2, juce::dontSendNotification);

    auto specToggled = [this]
    {
        if (!specPreButton.getToggleState() && !specPostButton.getToggleState())
            specPostButton.setToggleState(true, juce::dontSendNotification);
        applyAnalyzerOptions();
    };
    specPreButton.onClick = specToggled;
    specPostButton.onClick = specToggled;
    specFreezeButton.onClick = [this] { applyAnalyzerOptions(); };
    specSpanBox.onChange = [this] { applyAnalyzerOptions(); };

    loadAnalyzerOptionsFromState();

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        eqProcessor.apvts, "outputGain", outputGainSlider);
    pluginBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        eqProcessor.apvts, "pluginBypass", pluginBypassButton);
    pluginBypassButton.setTooltip("Pass audio through without EQ (output gain still applies)");
    pluginBypassButton.onClick = [this] { repaint(); };

    buildTag.setText("UI Aug 10g", juce::dontSendNotification);
    buildTag.setInterceptsMouseClicks(false, false);
    buildTag.setTooltip("Build marker: if missing, remove this plugin and insert again (don't just rescan)");

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

    displayRangeLabel.setVisible(true);
    displayRangeBox.setVisible(true);
    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &expandButton, &removeButton })
        addAndMakeVisible(*b);
    for (auto* b : { &specPreButton, &specPostButton, &specFreezeButton, &pluginBypassButton })
        addAndMakeVisible(*b);
    addAndMakeVisible(specSpanLabel);
    addAndMakeVisible(specSpanBox);
    addAndMakeVisible(outputGainLabel);
    addAndMakeVisible(outputGainSlider);

    addAndMakeVisible(presetBrowser);

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
    addAndMakeVisible(bandOptionsLabel);
    addAndMakeVisible(bandEnabledButton);
    addAndMakeVisible(bandSoloButton);
    addAndMakeVisible(slopeLabel);
    addAndMakeVisible(slopeSlider);
    addAndMakeVisible(brickwallButton);
    addAndMakeVisible(stereoLabel);
    addAndMakeVisible(stereoModeBox);
    addAndMakeVisible(balanceLabel);
    addAndMakeVisible(balanceSlider);
    addAndMakeVisible(displayRangeLabel);
    addAndMakeVisible(displayRangeBox);
    addAndMakeVisible(dynSectionLabel);
    addAndMakeVisible(dynEnableButton);
    addAndMakeVisible(dynThresholdAutoButton);
    addAndMakeVisible(dynAutoThresholdButton);
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
    addAndMakeVisible(dynSidechainLabel);
    addAndMakeVisible(dynSidechainSlider);
    addAndMakeVisible(qKnobLabel);
    addAndMakeVisible(qKnob);
    addAndMakeVisible(buildTag);
    addAndMakeVisible(emptyHint);

    qCaption.setVisible(false);
    qValueLabel.setVisible(false);

    applyThemeToButtons();
    applyThemeToInspector();
    presetBrowser.applyTheme(themeManager.current());
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

void PlaymakersEQAudioProcessorEditor::loadAnalyzerOptionsFromState()
{
    auto& state = eqProcessor.apvts.state;
    const bool showPre = (int) state.getProperty("analyzerShowPre", 0) != 0;
    const bool showPost = (int) state.getProperty("analyzerShowPost", 1) != 0;
    const bool freeze = (int) state.getProperty("analyzerFrozen", 0) != 0;
    const float span = (float) state.getProperty("analyzerSpanDb", 90.0);

    specPreButton.setToggleState(showPre, juce::dontSendNotification);
    specPostButton.setToggleState(showPost || !showPre, juce::dontSendNotification);
    specFreezeButton.setToggleState(freeze, juce::dontSendNotification);
    specSpanBox.setSelectedId(span <= 75.0f ? 1 : 2, juce::dontSendNotification);
    applyAnalyzerOptions();
}

void PlaymakersEQAudioProcessorEditor::applyAnalyzerOptions()
{
    if (!specPreButton.getToggleState() && !specPostButton.getToggleState())
        specPostButton.setToggleState(true, juce::dontSendNotification);

    const bool showPre = specPreButton.getToggleState();
    const bool showPost = specPostButton.getToggleState();
    const bool freeze = specFreezeButton.getToggleState();
    const float span = specSpanBox.getSelectedId() == 1 ? 60.0f : 90.0f;

    analyzer.setShowPreSpectrum(showPre);
    analyzer.setShowPostSpectrum(showPost);
    analyzer.setSpectrumFrozen(freeze);
    analyzer.setSpectrumSpanDb(span);

    auto& state = eqProcessor.apvts.state;
    state.setProperty("analyzerShowPre", showPre ? 1 : 0, nullptr);
    state.setProperty("analyzerShowPost", showPost ? 1 : 0, nullptr);
    state.setProperty("analyzerFrozen", freeze ? 1 : 0, nullptr);
    state.setProperty("analyzerSpanDb", span, nullptr);

    specPreButton.setAlpha(showPre ? 1.0f : 0.55f);
    specPostButton.setAlpha(showPost ? 1.0f : 0.55f);
    specFreezeButton.setAlpha(freeze ? 1.0f : 0.75f);
}

void PlaymakersEQAudioProcessorEditor::clearInspectorBindings()
{
    typeAttachment.reset();
    bandEnabledAttachment.reset();
    bandSoloAttachment.reset();
    slopeAttachment.reset();
    brickwallAttachment.reset();
    stereoModeAttachment.reset();
    balanceAttachment.reset();
    dynEnableAttachment.reset();
    dynThresholdAttachment.reset();
    dynAutoThresholdAttachment.reset();
    dynRangeAttachment.reset();
    dynRatioAttachment.reset();
    dynAttackAttachment.reset();
    dynReleaseAttachment.reset();
    qKnobAttachment.reset();
    dynSidechainAttachment.reset();
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
    bandEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "enabled"), bandEnabledButton);
    bandSoloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "solo"), bandSoloButton);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "slope"), slopeSlider);
    brickwallAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "brickwall"), brickwallButton);
    stereoModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, Params::bandParamID(bandIndex, "stereoMode"), stereoModeBox);
    balanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "balance"), balanceSlider);
    dynEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynEnabled"), dynEnableButton);
    dynThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynThreshold"), dynThresholdSlider);
    dynAutoThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynAutoThreshold"), dynAutoThresholdButton);
    dynRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRange"), dynRangeSlider);
    dynRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRatio"), dynRatioSlider);
    dynAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynAttack"), dynAttackSlider);
    dynReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynRelease"), dynReleaseSlider);
    qKnobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "q"), qKnob);
    dynSidechainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, Params::bandParamID(bandIndex, "dynSidechainBlend"), dynSidechainSlider);
}

void PlaymakersEQAudioProcessorEditor::applySoloToSelection(bool soloEnabled)
{
    auto bands = analyzer.getSelectedBandIndices();
    if (bands.isEmpty())
        return;

    if (soloEnabled)
    {
        for (int i = 0; i < Params::numBands; ++i)
        {
            if (bands.contains(i))
                continue;
            if (auto* p = eqProcessor.apvts.getParameter(Params::bandParamID(i, "solo")))
                p->setValueNotifyingHost(0.0f);
        }
    }

    for (auto band : bands)
    {
        if (auto* p = eqProcessor.apvts.getParameter(Params::bandParamID(band, "solo")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(soloEnabled ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    }
}

void PlaymakersEQAudioProcessorEditor::applyAutoThresholdCaptureToSelection()
{
    const auto bands = analyzer.getSelectedBandIndices();
    if (bands.isEmpty())
        return;

    for (auto band : bands)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                eqProcessor.apvts.getParameter(Params::bandParamID(band, "dynThreshold"))))
        {
            const float level = eqProcessor.getDynDetectionMeterDb(band);
            if (!std::isfinite(level) || level <= -95.0f)
                continue;

            const float value = p->getNormalisableRange().snapToLegalValue(level);
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(value));
            p->endChangeGesture();
        }
    }
    refreshInspector();
}

void PlaymakersEQAudioProcessorEditor::updateDynRangeLabelForBand(int bandIndex)
{
    if (bandIndex < 0)
        return;
    const auto rangeDb = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(bandIndex, "dynRange"))->load();
    dynRangeLabel.setText(rangeDb >= 0.0f ? "Expand" : "Compress", juce::dontSendNotification);
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
    qKnobLabel.setVisible(hasSelection);
    qKnob.setVisible(hasSelection);
    freqValueLabel.setVisible(hasSelection);
    gainValueLabel.setVisible(hasSelection);
    typeLabel.setVisible(hasSelection);
    typeBox.setVisible(hasSelection);
    removeButton.setVisible(hasSelection);
    bandOptionsLabel.setVisible(hasSelection);
    bandEnabledButton.setVisible(hasSelection);
    bandSoloButton.setVisible(hasSelection);
    slopeLabel.setVisible(false);
    slopeSlider.setVisible(false);
    brickwallButton.setVisible(false);
    stereoLabel.setVisible(hasSelection);
    stereoModeBox.setVisible(hasSelection);
    balanceLabel.setVisible(hasSelection);
    balanceSlider.setVisible(hasSelection);
    dynSectionLabel.setVisible(hasSelection);
    dynEnableButton.setVisible(hasSelection);

    if (!hasSelection)
    {
        clearInspectorBindings();
        for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
            s->setVisible(false);
        for (auto* l : { &dynThresholdLabel, &dynRangeLabel, &dynRatioLabel, &dynAttackLabel, &dynReleaseLabel })
            l->setVisible(false);
        dynThresholdAutoButton.setVisible(false);
        dynAutoThresholdButton.setVisible(false);
        dynSidechainLabel.setVisible(false);
        dynSidechainSlider.setVisible(false);
        bandOptionsBounds = {};
        qKnob.setEnabled(false);
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

    const bool canSlope = Params::typeSupportsSlope(type);
    slopeLabel.setVisible(hasSelection && canSlope);
    slopeSlider.setVisible(hasSelection && canSlope);
    slopeSlider.setEnabled(hasSelection && canSlope);
    brickwallButton.setVisible(hasSelection && canSlope);
    brickwallButton.setEnabled(hasSelection && canSlope);

    const bool bandOn = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "enabled"))->load() >= 0.5f;
    bandEnabledButton.setEnabled(hasSelection);
    bandSoloButton.setEnabled(hasSelection && bandOn);
    stereoModeBox.setEnabled(hasSelection);
    balanceSlider.setEnabled(hasSelection);

    bandEnabledButton.setToggleState(bandOn, juce::dontSendNotification);

    const bool soloOn = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "solo"))->load() >= 0.5f;
    bandSoloButton.setToggleState(soloOn, juce::dontSendNotification);

    qKnob.setEnabled(hasSelection && bandOn);
    qValueLabel.setVisible(hasSelection);

    const bool showDynSliders = canDyn && dynOn;
    const bool autoTrack = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "dynAutoThreshold"))->load() >= 0.5f;
    const bool showSidechain = canDyn && hasSelection;
    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider })
    {
        s->setVisible(showDynSliders);
        s->setEnabled(showDynSliders);
    }
    dynThresholdSlider.setEnabled(showDynSliders && !autoTrack);
    dynThresholdAutoButton.setVisible(showDynSliders);
    dynThresholdAutoButton.setEnabled(showDynSliders);
    dynAutoThresholdButton.setVisible(showDynSliders);
    dynAutoThresholdButton.setEnabled(showDynSliders);
    for (auto* l : { &dynThresholdLabel, &dynRangeLabel, &dynRatioLabel, &dynAttackLabel, &dynReleaseLabel })
        l->setVisible(showDynSliders);
    updateDynRangeLabelForBand(primary);
    dynSidechainLabel.setVisible(showSidechain);
    dynSidechainSlider.setVisible(showSidechain);
    dynSidechainSlider.setEnabled(showSidechain);

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

    if (primary >= 0)
    {
        const auto type = static_cast<Params::FilterType>(
            (int) eqProcessor.apvts.getRawParameterValue(Params::bandParamID(primary, "type"))->load());
        const bool wantSlope = Params::typeSupportsSlope(type);
        if (wantSlope != slopeSlider.isVisible())
            refreshInspector();
        else if (dynThresholdSlider.isVisible())
            updateDynRangeLabelForBand(primary);
    }
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

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider, &dynSidechainSlider, &slopeSlider, &outputGainSlider })
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    qKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    balanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
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
    prepCaption(bandOptionsLabel);
    prepCaption(slopeLabel);
    prepCaption(stereoLabel);
    prepCaption(balanceLabel);
    prepCaption(dynSectionLabel);
    prepCaption(displayRangeLabel);
    prep(freqValueLabel, 15.0f, true);
    prep(gainValueLabel, 15.0f, true);
    prep(qValueLabel, 15.0f, true);
    prep(emptyHint, 11.0f, false);
    emptyHint.setJustificationType(juce::Justification::centredLeft);
    prep(dynThresholdLabel, 11.0f, false);
    prep(dynRangeLabel, 11.0f, false);
    prep(dynRatioLabel, 11.0f, false);
    prep(dynAttackLabel, 11.0f, false);
    prep(dynReleaseLabel, 11.0f, false);

    prep(buildTag, 9.0f, false);
    buildTag.setColour(juce::Label::textColourId, t.inkMuted.withAlpha(0.75f));
    buildTag.setJustificationType(juce::Justification::centredRight);
    emptyHint.setColour(juce::Label::textColourId, t.inkMuted);

    typeBox.setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.95f));
    typeBox.setColour(juce::ComboBox::arrowColourId, t.ink.withAlpha(0.7f));
    typeBox.setColour(juce::ComboBox::outlineColourId, t.ink.withAlpha(0.22f));
    typeBox.setColour(juce::ComboBox::backgroundColourId,
                      t.isLight() ? t.softWhite.withAlpha(0.7f) : t.softWhite.withAlpha(0.04f));
    stereoModeBox.setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.95f));
    stereoModeBox.setColour(juce::ComboBox::arrowColourId, t.ink.withAlpha(0.7f));
    stereoModeBox.setColour(juce::ComboBox::outlineColourId, t.ink.withAlpha(0.22f));
    stereoModeBox.setColour(juce::ComboBox::backgroundColourId,
                            t.isLight() ? t.softWhite.withAlpha(0.7f) : t.softWhite.withAlpha(0.04f));
    displayRangeBox.setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.95f));
    displayRangeBox.setColour(juce::ComboBox::arrowColourId, t.ink.withAlpha(0.7f));
    displayRangeBox.setColour(juce::ComboBox::outlineColourId, t.ink.withAlpha(0.22f));
    displayRangeBox.setColour(juce::ComboBox::backgroundColourId,
                              t.isLight() ? t.softWhite.withAlpha(0.7f) : t.softWhite.withAlpha(0.04f));
    specSpanBox.setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.95f));
    specSpanBox.setColour(juce::ComboBox::arrowColourId, t.ink.withAlpha(0.7f));
    specSpanBox.setColour(juce::ComboBox::outlineColourId, t.ink.withAlpha(0.22f));
    specSpanBox.setColour(juce::ComboBox::backgroundColourId,
                          t.isLight() ? t.softWhite.withAlpha(0.7f) : t.softWhite.withAlpha(0.04f));
    prepCaption(specSpanLabel);
    prepCaption(outputGainLabel);

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider, &dynSidechainSlider, &slopeSlider, &outputGainSlider })
    {
        s->setColour(juce::Slider::textBoxTextColourId, t.ink.withAlpha(0.95f));
        s->setColour(juce::Slider::textBoxBackgroundColourId,
                     t.isLight() ? t.softWhite.withAlpha(0.85f) : t.softWhite.withAlpha(0.04f));
        s->setColour(juce::Slider::textBoxOutlineColourId, t.ink.withAlpha(0.18f));
    }
    qKnob.setColour(juce::Slider::textBoxTextColourId, t.ink.withAlpha(0.95f));
    qKnob.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    qKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

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
    stereoModeBox.getProperties().set("pmAccentColour", colourStr);
    bandSoloButton.getProperties().set("pmAccentColour", colourStr);
    bandEnabledButton.getProperties().set("pmAccentColour", colourStr);
    brickwallButton.getProperties().set("pmAccentColour", colourStr);
    dynEnableButton.getProperties().set("pmAccentColour", dynColourStr);

    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider, &dynSidechainSlider, &slopeSlider, &balanceSlider })
        s->getProperties().set("pmAccentColour", dynColourStr);
    qKnob.getProperties().set("pmAccentColour", colourStr);

    removeButton.repaint();
    bandEnabledButton.repaint();
    bandSoloButton.repaint();
    brickwallButton.repaint();
    dynEnableButton.repaint();
    typeBox.repaint();
    stereoModeBox.repaint();
    for (auto* s : { &dynThresholdSlider, &dynRangeSlider, &dynRatioSlider, &dynAttackSlider, &dynReleaseSlider, &dynSidechainSlider, &slopeSlider, &balanceSlider, &qKnob })
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
    if ((key == juce::KeyPress('s', juce::ModifierKeys(), 0) || key == juce::KeyPress('S', juce::ModifierKeys(), 0))
        && analyzer.getPrimarySelectedBand() >= 0)
    {
        const int band = analyzer.getPrimarySelectedBand();
        const bool cur = eqProcessor.apvts.getRawParameterValue(Params::bandParamID(band, "solo"))->load() >= 0.5f;
        applySoloToSelection(!cur);
        refreshInspector();
        analyzer.repaint();
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

    if (!analyzerSpecBarBounds.isEmpty())
    {
        g.setColour(t.isLight() ? t.softWhite.withAlpha(0.35f) : t.softWhite.withAlpha(0.03f));
        g.fillRect(analyzerSpecBarBounds.toFloat());
    }

    if (eqProcessor.apvts.getRawParameterValue("pluginBypass")->load() >= 0.5f)
    {
        auto graph = analyzer.getBounds().toFloat();
        g.setColour(t.background.withAlpha(t.isLight() ? 0.25f : 0.32f));
        g.fillRect(graph);
        g.setColour(t.inkMuted.withAlpha(0.85f));
        g.setFont(Brand::uiFont(13.0f, true));
        g.drawText("BYPASS", graph, juce::Justification::centred, false);
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

    buildTag.setVisible(true);
    buildTag.setBounds(header.getRight() - 62, header.getY() + 4, 58, 14);

    auto rangeArea = header.removeFromRight(128);
    header.removeFromRight(8);
    displayRangeLabel.setBounds(rangeArea.removeFromLeft(40).withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
    displayRangeBox.setBounds(rangeArea.withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
    displayRangeLabel.setJustificationType(juce::Justification::centredRight);

    auto presetArea = header.removeFromLeft(juce::jmin(280, header.getWidth() - 200));
    presetBrowser.setBounds(presetArea.withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
    header.removeFromLeft(8);

    auto placeRight = [&header](juce::TextButton& b, int w)
    {
        auto area = header.removeFromRight(w);
        header.removeFromRight(6);
        b.setBounds(area.withHeight(26).withY(header.getY() + (header.getHeight() - 26) / 2));
    };

    placeRight(expandButton, 72);
    placeRight(copyButton, 58);
    placeRight(abButton, 34);

    const bool hasBand = analyzer.getPrimarySelectedBand() >= 0;
    const bool dynExpanded = hasBand && (dynThresholdSlider.isVisible() || dynSidechainSlider.isVisible());
    const int bandOptionsH = hasBand ? 34 : 0;
    const int inspectorH = !hasBand ? 56 : (dynExpanded ? (dynThresholdSlider.isVisible() ? 196 : 118) : 156) + bandOptionsH;
    inspectorBounds = bounds.removeFromBottom(inspectorH);

    auto graphArea = bounds.reduced(8, 4);
    analyzerSpecBarBounds = graphArea.removeFromTop(26);
    analyzer.setBounds(graphArea);

    auto specBar = analyzerSpecBarBounds.reduced(4, 2);

    auto outArea = specBar.removeFromRight(210);
    pluginBypassButton.setBounds(outArea.removeFromRight(62).withHeight(22));
    outArea.removeFromRight(6);
    outputGainSlider.setBounds(outArea.removeFromRight(118).withHeight(22));
    outArea.removeFromRight(4);
    outputGainLabel.setBounds(outArea.removeFromRight(24).withHeight(22));

    specPreButton.setBounds(specBar.removeFromLeft(44).withHeight(22));
    specBar.removeFromLeft(4);
    specPostButton.setBounds(specBar.removeFromLeft(48).withHeight(22));
    specBar.removeFromLeft(8);
    specFreezeButton.setBounds(specBar.removeFromLeft(58).withHeight(22));
    specBar.removeFromLeft(12);
    specSpanLabel.setBounds(specBar.removeFromLeft(32).withHeight(22));
    specSpanBox.setBounds(specBar.removeFromLeft(72).withHeight(22));

    for (auto& c : metricCardBounds) c = {};
    bandOptionsBounds = {};
    dynSectionBounds = {};

    auto panel = inspectorBounds.reduced(12, 10);
    if (emptyHint.isVisible())
    {
        emptyHint.setBounds(panel);
        return;
    }

    auto top = panel.removeFromTop(56);
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

    const int gaps = 8;
    const int cardW = juce::jmax(100, (top.getWidth() - gaps) / 3);
    auto placeMetric = [&](int index, juce::Label& caption, juce::Label& value)
    {
        auto card = top.removeFromLeft(cardW);
        if (index < 2)
            top.removeFromLeft(8);
        metricCardBounds[index] = card;
        auto inner = card.reduced(10, 4);
        caption.setBounds(inner.removeFromTop(12));
        value.setBounds(inner);
        value.setJustificationType(juce::Justification::centredLeft);
    };
    placeMetric(0, freqCaption, freqValueLabel);
    placeMetric(1, gainCaption, gainValueLabel);
    auto qCard = top;
    metricCardBounds[2] = qCard;
    auto qInner = qCard.reduced(6, 2);
    qKnobLabel.setBounds(qInner.removeFromTop(12));
    auto qNumRow = qInner.removeFromBottom(14);
    qValueLabel.setBounds(qNumRow);
    qValueLabel.setVisible(hasBand);
    qValueLabel.setJustificationType(juce::Justification::centred);
    qKnob.setBounds(qInner);

    panel.removeFromTop(6);
    bandOptionsBounds = panel.removeFromTop(28);
    auto opt = bandOptionsBounds.reduced(0, 2);
    bandOptionsLabel.setBounds(opt.removeFromLeft(36));
    opt.removeFromLeft(4);
    bandEnabledButton.setBounds(opt.removeFromLeft(58));
    opt.removeFromLeft(8);
    bandSoloButton.setBounds(opt.removeFromLeft(56));
    opt.removeFromLeft(10);
    if (slopeSlider.isVisible())
    {
        slopeLabel.setBounds(opt.removeFromLeft(36).withTrimmedTop(6));
        slopeSlider.setBounds(opt.removeFromLeft(118));
        opt.removeFromLeft(6);
        brickwallButton.setBounds(opt.removeFromLeft(78));
        opt.removeFromLeft(10);
    }
    stereoLabel.setBounds(opt.removeFromLeft(42).withTrimmedTop(6));
    stereoModeBox.setBounds(opt.removeFromLeft(108).withHeight(24));
    opt.removeFromLeft(8);
    balanceLabel.setBounds(opt.removeFromLeft(22).withTrimmedTop(6));
    balanceSlider.setBounds(opt.removeFromLeft(juce::jmax(60, opt.getWidth())));

    panel.removeFromTop(4);
    const bool dynOpen = dynThresholdSlider.isVisible();
    dynSectionBounds = panel.removeFromTop(dynOpen ? 92 : (dynSidechainSlider.isVisible() ? 36 : 30));
    auto dynInner = dynSectionBounds.reduced(12, 8);

    auto dynHead = dynInner.removeFromTop(18);
    dynSectionLabel.setBounds(dynHead.removeFromLeft(120));
    dynEnableButton.setBounds(dynHead.removeFromLeft(60));

    if (dynThresholdSlider.isVisible())
    {
        dynInner.removeFromTop(6);
        auto row = dynInner.removeFromTop(26);
        const int slotW = row.getWidth() / 5;
        auto place = [&row, slotW](juce::Label& l, juce::Slider& s)
        {
            auto slot = row.removeFromLeft(slotW).reduced(3, 0);
            l.setBounds(slot.removeFromLeft(52).withTrimmedTop(5));
            s.setBounds(slot);
        };
        {
            auto slot = row.removeFromLeft(slotW).reduced(3, 0);
            dynThresholdLabel.setBounds(slot.removeFromLeft(36).withTrimmedTop(5));
            dynThresholdAutoButton.setBounds(slot.removeFromLeft(34).withHeight(20).withTrimmedTop(3));
            dynAutoThresholdButton.setBounds(slot.removeFromLeft(42).withHeight(20).withTrimmedTop(3));
            dynThresholdSlider.setBounds(slot);
        }
        place(dynRangeLabel, dynRangeSlider);
        place(dynRatioLabel, dynRatioSlider);
        place(dynAttackLabel, dynAttackSlider);
        place(dynReleaseLabel, dynReleaseSlider);

        dynInner.removeFromTop(4);
        auto scRow = dynInner.removeFromTop(22);
        dynSidechainLabel.setBounds(scRow.removeFromLeft(72).withTrimmedTop(4));
        dynSidechainSlider.setBounds(scRow.reduced(0, 1));
    }
    else if (dynSidechainSlider.isVisible())
    {
        auto scRow = dynInner;
        dynSidechainLabel.setBounds(scRow.removeFromLeft(72).withTrimmedTop(4));
        dynSidechainSlider.setBounds(scRow.reduced(0, 1));
    }

    if (hasBand)
    {
        removeButton.toFront(false);
        bandSoloButton.toFront(false);
    }
}
