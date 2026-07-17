#include "PluginEditor.h"

PlaymakersEQAudioProcessorEditor::PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      analyzer(p.apvts, p.getPostAnalyzer(), p.getSampleRateRef())
{
    addAndMakeVisible(analyzer);
    setSize(800, 500);
}

PlaymakersEQAudioProcessorEditor::~PlaymakersEQAudioProcessorEditor() = default;

void PlaymakersEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PlaymakersEQAudioProcessorEditor::resized()
{
    analyzer.setBounds(getLocalBounds().reduced(8));
}
