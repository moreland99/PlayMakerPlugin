#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "Theme.h"
#include "PlaymakersLookAndFeel.h"
#include "PresetBrowser.h"

class PlaymakersEQAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor&);
    ~PlaymakersEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void applyThemeToButtons();
    void applyThemeToInspector();
    void applyBandAccentToInspector(int bandIndex);
    void applyAnalyzerOptions();
    void loadAnalyzerOptionsFromState();
    void refreshInspector();
    void bindInspectorToBand(int bandIndex);
    void clearInspectorBindings();
    void applyTypeToSelection(int typeIndex);
    void applyDynEnabledToSelection(bool enabled);
    void applySoloToSelection(bool soloEnabled);
    void applyAutoThresholdCaptureToSelection();
    void updateDynRangeLabelForBand(int bandIndex);
    void commitMetricFromLabel(juce::Label& label, const char* paramSuffix);
    bool isEditingMetrics() const;
    static float parseFrequencyText(const juce::String& text);
    static float parseFloatText(const juce::String& text);

    PlaymakersEQAudioProcessor& eqProcessor;
    ThemeManager themeManager;
    PlaymakersLookAndFeel lookAndFeel { themeManager.current() };
    SpectrumAnalyzerComponent analyzer;

    juce::Rectangle<int> brandLockupBounds;
    juce::Rectangle<int> inspectorBounds;
    juce::Rectangle<int> metricCardBounds[3];
    juce::Rectangle<int> bandOptionsBounds;
    juce::Rectangle<int> dynSectionBounds;

    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton abButton { "A" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton expandButton { "Expand" };
    PresetBrowser presetBrowser;
    juce::Label buildTag;

    bool expandedView = false;
    static constexpr int normalWidth = 1100;
    static constexpr int normalHeight = 700;
    static constexpr int expandedWidth = 1400;
    static constexpr int expandedHeight = 860;

    // Selected-band inspector
    juce::Label inspectorTitle;
    juce::Label freqCaption { {}, "FREQ" };
    juce::Label gainCaption { {}, "GAIN" };
    juce::Label qCaption { {}, "Q" };
    juce::Label freqValueLabel;
    juce::Label gainValueLabel;
    juce::Label qValueLabel;
    juce::Label typeLabel { {}, "TYPE" };
    juce::ComboBox typeBox;
    juce::TextButton removeButton { "Remove" };

    juce::Label bandOptionsLabel { {}, "BAND" };
    juce::ToggleButton bandEnabledButton { "Active" };
    juce::ToggleButton bandSoloButton { "Solo" };
    juce::Label slopeLabel { {}, "Slope" };
    juce::Slider slopeSlider;
    juce::ToggleButton brickwallButton { "Brickwall" };
    juce::Label stereoLabel { {}, "Stereo" };
    juce::ComboBox stereoModeBox;
    juce::Label balanceLabel { {}, "Bal" };
    juce::Slider balanceSlider;

    juce::Label displayRangeLabel { {}, "Range" };
    juce::ComboBox displayRangeBox;

    juce::ToggleButton specPreButton { "Pre" };
    juce::ToggleButton specPostButton { "Post" };
    juce::ToggleButton specFreezeButton { "Freeze" };
    juce::Label specSpanLabel { {}, "Spec" };
    juce::ComboBox specSpanBox;
    juce::Rectangle<int> analyzerSpecBarBounds;

    juce::Label outputGainLabel { {}, "Out" };
    juce::Slider outputGainSlider;
    juce::ToggleButton pluginBypassButton { "Bypass" };

    juce::Label dynSectionLabel { {}, "DYNAMICS" };
    juce::ToggleButton dynEnableButton { "On" };
    juce::Slider dynThresholdSlider;
    juce::Slider dynRangeSlider;
    juce::Slider dynRatioSlider;
    juce::Slider dynAttackSlider;
    juce::Slider dynReleaseSlider;
    juce::Label dynThresholdLabel { {}, "Thresh" };
    juce::TextButton dynThresholdAutoButton { "Auto" };
    juce::ToggleButton dynAutoThresholdButton { "Track" };
    juce::Label dynRangeLabel { {}, "Range" };
    juce::Label dynRatioLabel { {}, "Ratio" };
    juce::Label dynAttackLabel { {}, "Attack" };
    juce::Label dynReleaseLabel { {}, "Release" };
    juce::Slider qKnob;
    juce::Label qKnobLabel { {}, "Q" };

    juce::Label dynSidechainLabel { {}, "Sidechain" };
    juce::Slider dynSidechainSlider;
    juce::Label emptyHint { {},
        "Click a band on the graph to edit frequency, gain, Q, and dynamics in this panel.\n"
        "Double-click empty space to add a band · Scroll or ⌘-drag a handle for Q · Option-click a band to remove" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandEnabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandSoloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> brickwallAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> balanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynAutoThresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynReleaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qKnobAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynSidechainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pluginBypassAttachment;

    int boundBand = -1;
    bool updatingInspector = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaymakersEQAudioProcessorEditor)
};
