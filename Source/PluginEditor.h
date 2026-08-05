#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "Theme.h"
#include "PlaymakersLookAndFeel.h"

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
    void refreshInspector();
    void bindInspectorToBand(int bandIndex);
    void clearInspectorBindings();
    void applyTypeToSelection(int typeIndex);
    void applyDynEnabledToSelection(bool enabled);
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
    juce::Rectangle<int> dynSectionBounds;

    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton abButton { "A" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton expandButton { "Expand" };
    juce::TextButton themeButton { "Light" };

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

    juce::Label dynSectionLabel { {}, "DYNAMICS" };
    juce::ToggleButton dynEnableButton { "On" };
    juce::Slider dynThresholdSlider;
    juce::Slider dynRangeSlider;
    juce::Slider dynRatioSlider;
    juce::Slider dynAttackSlider;
    juce::Slider dynReleaseSlider;
    juce::Label dynThresholdLabel { {}, "Thresh" };
    juce::Label dynRangeLabel { {}, "Range" };
    juce::Label dynRatioLabel { {}, "Ratio" };
    juce::Label dynAttackLabel { {}, "Attack" };
    juce::Label dynReleaseLabel { {}, "Release" };
    juce::Label emptyHint { {}, "Select a band on the spectrum to edit type, dynamics, or remove it." };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynRatioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynReleaseAttachment;

    int boundBand = -1;
    bool updatingInspector = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaymakersEQAudioProcessorEditor)
};
