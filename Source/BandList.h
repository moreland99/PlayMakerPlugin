#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <vector>
#include "Params.h"
#include "Theme.h"

// Compact scrollable list of enabled bands — Freq / Gain / Q / Type, multi-select.
class BandListComponent : public juce::Component,
                          private juce::ListBoxModel
{
public:
    BandListComponent(juce::AudioProcessorValueTreeState& stateToRead, Theme themeToUse);

    std::function<void(int bandIndex, bool toggle)> onBandChosen;

    void setTheme(const Theme& t);
    void refresh();
    void setSelectedBands(int primaryBandIndex, const juce::Array<int>& selected);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Row
    {
        int bandIndex = -1;
        int typeIndex = 0;
        float freq = 1000.0f;
        float gain = 0.0f;
        float q = 0.707f;
        bool solo = false;
        bool dynOn = false;

        bool operator==(const Row& other) const
        {
            return bandIndex == other.bandIndex
                && typeIndex == other.typeIndex
                && solo == other.solo
                && dynOn == other.dynOn
                && std::abs(freq - other.freq) < 0.001f
                && std::abs(gain - other.gain) < 0.01f
                && std::abs(q - other.q) < 0.001f;
        }
    };

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                          int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    juce::String getTooltipForRow(int row) override;

    void revealPrimary();
    static juce::String formatFrequency(float freqHz);
    static juce::String typeShortName(int typeIndex);

    juce::AudioProcessorValueTreeState& apvts;
    Theme theme;
    juce::ListBox list;
    std::vector<Row> rows;
    juce::Array<int> selectedBands;
    int primaryBand = -1;
    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> columnBounds;
};
