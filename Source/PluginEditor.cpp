#include "PluginEditor.h"

PlaymakersEQAudioProcessorEditor::PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(800, 500);
}

PlaymakersEQAudioProcessorEditor::~PlaymakersEQAudioProcessorEditor() = default;

void PlaymakersEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f));
    g.drawFittedText("PLAYMAKERS EQ", getLocalBounds(), juce::Justification::centred, 1);
}

void PlaymakersEQAudioProcessorEditor::resized()
{
}
