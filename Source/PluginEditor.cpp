#include "PluginEditor.h"

PlaymakersEQAudioProcessorEditor::PlaymakersEQAudioProcessorEditor(PlaymakersEQAudioProcessor& p)
    : AudioProcessorEditor(&p), eqProcessor(p),
      analyzer(p.apvts, p.getPostAnalyzer(), p.getSampleRateRef(), themeManager.current())
{
    // Restore the session's theme (stored as JSON on the state tree — data-driven per spec).
    const auto themeJSON = eqProcessor.apvts.state.getProperty("themeJSON").toString();
    if (themeJSON.isNotEmpty())
        themeManager.setTheme(Theme::fromJSON(themeJSON, Theme::dark()));

    themeManager.onThemeChanged = [this]
    {
        eqProcessor.apvts.state.setProperty("themeJSON", themeManager.current().toJSON(), nullptr);
        applyThemeToButtons();
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
    themeButton.onClick = [this]
    {
        const bool goingLight = themeManager.current().name != "Light";
        themeManager.setTheme(goingLight ? Theme::light() : Theme::dark());
        themeButton.setButtonText(goingLight ? "Dark" : "Light");
    };

    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &themeButton })
        addAndMakeVisible(*b);
    addAndMakeVisible(analyzer);

    applyThemeToButtons();
    setWantsKeyboardFocus(true);
    startTimerHz(2);
    setSize(800, 500);
}

PlaymakersEQAudioProcessorEditor::~PlaymakersEQAudioProcessorEditor() = default;

void PlaymakersEQAudioProcessorEditor::timerCallback()
{
    // Groups parameter edits into coarse undo transactions (standard APVTS pattern).
    eqProcessor.undoManager.beginNewTransaction();
    abButton.setButtonText(eqProcessor.isOnSlotA() ? "A" : "B");
}

void PlaymakersEQAudioProcessorEditor::applyThemeToButtons()
{
    const auto& t = themeManager.current();
    for (auto* b : { &undoButton, &redoButton, &abButton, &copyButton, &themeButton })
    {
        b->setColour(juce::TextButton::buttonColourId, t.header.contrasting(0.05f));
        b->setColour(juce::TextButton::textColourOffId, t.text);
        b->setColour(juce::TextButton::textColourOnId, t.accent);
    }
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
    return false;
}

void PlaymakersEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& t = themeManager.current();
    g.fillAll(t.background);
    g.setColour(t.header);
    g.fillRect(getLocalBounds().removeFromTop(36));
}

void PlaymakersEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(36).reduced(6, 5);

    undoButton.setBounds(header.removeFromLeft(56));
    header.removeFromLeft(4);
    redoButton.setBounds(header.removeFromLeft(56));

    themeButton.setBounds(header.removeFromRight(60));
    header.removeFromRight(4);
    copyButton.setBounds(header.removeFromRight(56));
    header.removeFromRight(4);
    abButton.setBounds(header.removeFromRight(40));

    analyzer.setBounds(bounds.reduced(8));
}
