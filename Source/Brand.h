#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Theme.h"

// Shared PLAYMAKERS brand drawing helpers (fonts + kit logos).
namespace Brand
{
inline juce::Typeface::Ptr monumentWideBlack()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PPMonumentWideBlack_otf, BinaryData::PPMonumentWideBlack_otfSize);
    return typeface;
}

inline juce::Typeface::Ptr monumentWideRegular()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PPMonumentWideRegular_otf, BinaryData::PPMonumentWideRegular_otfSize);
    return typeface;
}

inline juce::Typeface::Ptr helveticaRegular()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::Helvetica_ttf, BinaryData::Helvetica_ttfSize);
    return typeface;
}

inline juce::Typeface::Ptr helveticaBold()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::HelveticaBold_ttf, BinaryData::HelveticaBold_ttfSize);
    return typeface;
}

inline juce::Font brandWordmark(float height)
{
    return juce::Font(juce::FontOptions(monumentWideBlack()).withHeight(height));
}

inline juce::Font brandLabel(float height)
{
    return juce::Font(juce::FontOptions(monumentWideRegular()).withHeight(height));
}

inline juce::Font uiFont(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions(bold ? helveticaBold() : helveticaRegular()).withHeight(height));
}

inline juce::Font titleFont(float height)
{
    return brandLabel(height);
}

inline std::unique_ptr<juce::Drawable> loadSvgDrawable(const char* data, int size, juce::Colour fill)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(data, (size_t) size));
    if (xml == nullptr)
        return {};

    auto drawable = juce::Drawable::createFromSVG(*xml);
    if (drawable == nullptr)
        return {};

    // Kit SVGs ship as black fills with no explicit colour — retint for the theme.
    drawable->replaceColour(juce::Colours::black, fill);
    return drawable;
}

// Full PMlogo1 lockup: concentric mark + custom vector PLAYMAKERS wordmark.
inline void drawLogo1Full(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour fill, float opacity = 1.0f)
{
    if (auto drawable = loadSvgDrawable(BinaryData::PMlogo1_svg, BinaryData::PMlogo1_svgSize, fill))
    {
        g.setOpacity(opacity);
        drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        g.setOpacity(1.0f);
    }
}

// Mark-only (no wordmark) — compact header icon.
inline void drawLogo1Mark(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour fill, float opacity = 1.0f)
{
    if (auto drawable = loadSvgDrawable(BinaryData::PMlogo1mark_svg, BinaryData::PMlogo1mark_svgSize, fill))
    {
        g.setOpacity(opacity);
        drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
        g.setOpacity(1.0f);
    }
}

// Header: mark + Monument wordmark (Pro-style top-left brand lockup).
inline void drawHeaderLockup(juce::Graphics& g, juce::Rectangle<float> bounds, const Theme& theme)
{
    auto markArea = bounds.removeFromLeft(bounds.getHeight()).reduced(1.0f);
    drawLogo1Mark(g, markArea, theme.softWhite, 0.95f);

    bounds.removeFromLeft(8.0f);
    g.setColour(theme.softWhite);
    g.setFont(brandWordmark(juce::jmin(13.0f, bounds.getHeight() * 0.62f)));
    g.drawText("PLAY-MAKERS", bounds, juce::Justification::centredLeft, false);
}

// Empty analyzer: full PMlogo1.svg lockup + Helvetica hint (no typed "PLAY-MAKERS").
inline void drawEmptyStateLockup(juce::Graphics& g, juce::Rectangle<float> bounds, const Theme& theme)
{
    const bool light = theme.name == "Light";
    const auto fill = light ? theme.midnightIndigo : theme.softWhite;
    const auto hintColour = fill.withAlpha(0.4f);

    const float logoH = juce::jmin(240.0f, bounds.getHeight() * 0.48f);
    const float logoW = logoH * (518.5f / 656.17f);
    auto logoBounds = juce::Rectangle<float>(logoW, logoH)
                          .withCentre({ bounds.getCentreX(), bounds.getCentreY() - 12.0f });

    drawLogo1Full(g, logoBounds, fill, light ? 0.95f : 0.92f);

    g.setColour(hintColour);
    g.setFont(uiFont(12.0f));
    g.drawText("Double-click empty space to add a band",
               bounds.withTrimmedTop(logoBounds.getBottom() - bounds.getY() + 10.0f).removeFromTop(18.0f),
               juce::Justification::centred, false);
}
} // namespace Brand
