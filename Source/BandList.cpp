#include "BandList.h"
#include "Brand.h"

BandListComponent::BandListComponent(juce::AudioProcessorValueTreeState& stateToRead, Theme themeToUse)
    : apvts(stateToRead), theme(std::move(themeToUse))
{
    list.setModel(this);
    list.setRowHeight(22);
    list.setOutlineThickness(0);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.getViewport()->setScrollBarsShown(true, false);
    addAndMakeVisible(list);
    refresh();
}

void BandListComponent::setTheme(const Theme& t)
{
    theme = t;
    repaint();
    list.repaint();
}

void BandListComponent::refresh()
{
    std::vector<Row> next;
    next.reserve((size_t) Params::numBands);

    for (int i = 0; i < Params::numBands; ++i)
    {
        if (apvts.getRawParameterValue(Params::bandParamID(i, "enabled"))->load() < 0.5f)
            continue;

        Row row;
        row.bandIndex = i;
        row.typeIndex = (int) apvts.getRawParameterValue(Params::bandParamID(i, "type"))->load();
        row.freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"))->load();
        row.gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"))->load();
        row.q = apvts.getRawParameterValue(Params::bandParamID(i, "q"))->load();
        row.solo = apvts.getRawParameterValue(Params::bandParamID(i, "solo"))->load() >= 0.5f;
        const auto type = static_cast<Params::FilterType>(row.typeIndex);
        row.dynOn = Params::typeSupportsDynamics(type)
            && apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"))->load() >= 0.5f;
        next.push_back(row);
    }

    if (next == rows)
        return;

    rows = std::move(next);
    list.updateContent();
    revealPrimary();
    repaint();
}

void BandListComponent::setSelectedBands(int primaryBandIndex, const juce::Array<int>& selected)
{
    if (primaryBand == primaryBandIndex && selectedBands == selected)
        return;

    primaryBand = primaryBandIndex;
    selectedBands = selected;
    list.repaint();
    revealPrimary();
}

void BandListComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.panel);
    g.fillRect(bounds);
    g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.10f : 0.08f));
    g.drawRect(bounds, 1.0f);

    if (!headerBounds.isEmpty())
    {
        auto head = headerBounds;
        g.setColour(theme.inkMuted);
        g.setFont(Brand::titleFont(9.0f));
        g.drawText("BANDS", head.removeFromLeft(52).toFloat(),
                   juce::Justification::centredLeft, false);

        g.setFont(Brand::uiFont(10.0f, true));
        g.setColour(theme.ink.withAlpha(0.88f));
        g.drawText(juce::String((int) rows.size()) + " / " + juce::String(Params::numBands),
                   head.toFloat(), juce::Justification::centredRight, false);
    }

    if (!columnBounds.isEmpty())
    {
        auto cols = columnBounds.reduced(8, 0);
        g.setFont(Brand::titleFont(8.0f));
        g.setColour(theme.inkMuted.withAlpha(0.8f));
        g.drawText("#", cols.removeFromLeft(18).toFloat(), juce::Justification::centredLeft, false);
        g.drawText("TYPE", cols.removeFromLeft(40).toFloat(), juce::Justification::centredLeft, false);
        g.drawText("FREQ", cols.removeFromLeft(52).toFloat(), juce::Justification::centredRight, false);
        g.drawText("GAIN", cols.removeFromLeft(40).toFloat(), juce::Justification::centredRight, false);
        g.drawText("Q", cols.toFloat(), juce::Justification::centredRight, false);
    }

    if (rows.empty())
    {
        auto hint = getLocalBounds().reduced(10, 0);
        hint.removeFromTop(48);
        g.setColour(theme.inkMuted);
        g.setFont(Brand::uiFont(10.5f));
        g.drawFittedText("No bands yet.\nDouble-click the graph to add one.",
                         hint, juce::Justification::topLeft, 3);
    }
}

void BandListComponent::resized()
{
    auto r = getLocalBounds().reduced(1);
    headerBounds = r.removeFromTop(24).reduced(8, 4);
    columnBounds = r.removeFromTop(14).reduced(0, 0);
    r.removeFromTop(2);
    list.setBounds(r);
}

int BandListComponent::getNumRows()
{
    return (int) rows.size();
}

void BandListComponent::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                         int width, int height, bool)
{
    if (rowNumber < 0 || rowNumber >= (int) rows.size())
        return;

    const auto& row = rows[(size_t) rowNumber];
    const bool primary = row.bandIndex == primaryBand;
    const bool selected = selectedBands.contains(row.bandIndex);
    const auto accent = Theme::bandColour(row.bandIndex, theme.isLight());
    auto area = juce::Rectangle<int>(0, 0, width, height);

    if (primary)
        g.setColour(accent.withAlpha(theme.isLight() ? 0.16f : 0.14f));
    else if (selected)
        g.setColour(accent.withAlpha(theme.isLight() ? 0.08f : 0.08f));
    else if (rowNumber % 2 == 1)
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.03f : 0.025f));
    else
        g.setColour(juce::Colours::transparentBlack);
    g.fillRect(area);

    g.setColour(accent);
    g.fillRect(0, 3, 3, height - 6);

    auto cols = area.reduced(8, 0);
    const auto text = primary ? theme.ink : theme.ink.withAlpha(0.82f);

    g.setColour(text);
    g.setFont(Brand::uiFont(10.5f, primary));
    g.drawText(juce::String(row.bandIndex + 1), cols.removeFromLeft(18).toFloat(),
               juce::Justification::centredLeft, false);

    auto typeArea = cols.removeFromLeft(40);
    g.drawText(typeShortName(row.typeIndex), typeArea.toFloat(),
               juce::Justification::centredLeft, false);

    g.setFont(Brand::uiFont(10.5f, primary));
    g.setColour(text);
    g.drawText(formatFrequency(row.freq), cols.removeFromLeft(52).toFloat(),
               juce::Justification::centredRight, false);

    const auto gainText = (row.gain > 0.05f ? "+" : "") + juce::String(row.gain, 1);
    g.drawText(gainText, cols.removeFromLeft(40).toFloat(),
               juce::Justification::centredRight, false);
    g.drawText(juce::String(row.q, 2), cols.removeFromLeft(28).toFloat(),
               juce::Justification::centredRight, false);

    if (row.solo || row.dynOn)
    {
        auto mark = cols.reduced(3, 4);
        g.setFont(Brand::uiFont(8.0f, true));
        if (row.solo)
        {
            g.setColour(accent);
            g.drawText("S", mark.removeFromLeft(10).toFloat(), juce::Justification::centred, false);
        }
        if (row.dynOn)
        {
            g.setColour(Theme::dynamicsColour(accent, theme.isLight()));
            g.drawText("D", mark.toFloat(), juce::Justification::centred, false);
        }
    }
}

void BandListComponent::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) rows.size() || onBandChosen == nullptr)
        return;

    const bool toggle = e.mods.isShiftDown() || e.mods.isCommandDown();
    onBandChosen(rows[(size_t) row].bandIndex, toggle);
}

void BandListComponent::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) rows.size() || onBandChosen == nullptr)
        return;
    onBandChosen(rows[(size_t) row].bandIndex, false);
}

juce::String BandListComponent::getTooltipForRow(int row)
{
    if (row < 0 || row >= (int) rows.size())
        return {};

    const auto& r = rows[(size_t) row];
    const auto typeName = (r.typeIndex >= 0 && r.typeIndex < Params::filterTypeNames().size())
        ? Params::filterTypeNames()[r.typeIndex]
        : juce::String("Band");
    return "Band " + juce::String(r.bandIndex + 1) + "  " + typeName
        + "  " + formatFrequency(r.freq);
}

void BandListComponent::revealPrimary()
{
    if (primaryBand < 0)
        return;

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        if (rows[(size_t) i].bandIndex == primaryBand)
        {
            list.scrollToEnsureRowIsOnscreen(i);
            break;
        }
    }
}

juce::String BandListComponent::formatFrequency(float freqHz)
{
    return Params::formatFrequency(freqHz);
}

juce::String BandListComponent::typeShortName(int typeIndex)
{
    static const char* names[] = {
        "Bell", "LSh", "HSh", "HP", "LP", "Notch", "BP", "AP", "Tilt", "Flat"
    };
    constexpr int n = 10;
    if (typeIndex < 0 || typeIndex >= n)
        return "—";
    return names[typeIndex];
}
