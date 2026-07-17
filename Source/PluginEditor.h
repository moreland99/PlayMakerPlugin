#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "Theme.h"

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

    PlaymakersEQAudioProcessor& eqProcessor;
    ThemeManager themeManager;
    SpectrumAnalyzerComponent analyzer;

    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton abButton { "A" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton themeButton { "Light" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaymakersEQAudioProcessorEditor)
};
