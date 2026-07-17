#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"

class PlaymakersEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor&);
    ~PlaymakersEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PlaymakersEQAudioProcessor& processor;
    SpectrumAnalyzerComponent analyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaymakersEQAudioProcessorEditor)
};
