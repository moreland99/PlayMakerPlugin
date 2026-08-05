#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Params.h"

// Data-driven theme: a plain value object serialized to/from JSON, read by the UI at paint
// time. Shared convention for the whole PLAYMAKERS suite — no per-theme component classes.
//
// Brand kit tokens:
//   Signal Orange   #DE5F41
//   Midnight Indigo #161342  (brand accent / light-mode header — not the EQ canvas)
//   Charcoal Black  #1B1B1F  (primary dark canvas — surgical EQ readability)
//   Soft White      #F9FAFA
struct Theme
{
    juce::String name = "Dark";

    // Core kit tokens (kept explicit so suite plugins can share them).
    juce::Colour signalOrange   { 0xffde5f41 };
    juce::Colour midnightIndigo { 0xff161342 };
    juce::Colour charcoalBlack  { 0xff1b1b1f };
    juce::Colour softWhite      { 0xfff9fafa };

    // Dark EQ canvas = charcoal (spectrum reads cleaner than deep indigo).
    juce::Colour background   { 0xff1b1b1f };
    juce::Colour backgroundEnd{ 0xff1b1b1f }; // kept for JSON compat; same as background
    juce::Colour header       { 0xff141416 }; // slightly deeper charcoal chrome
    juce::Colour panel        { 0xff121214 }; // inspector / floating chrome
    juce::Colour grid         { 0x1af9fafa };
    juce::Colour spectrum     { 0x66c8ccd0 }; // soft grey silhouette
    juce::Colour curve        { 0x8cf9fafa }; // fallback / quiet
    juce::Colour curveSelected{ 0xffde5f41 }; // fallback selection
    juce::Colour handle       { 0xb3f9fafa };
    juce::Colour handleSelected{ 0xffde5f41 };
    juce::Colour preview      { 0xccf9fafa };
    juce::Colour text         { 0xfff9fafa };
    juce::Colour accent       { 0xffde5f41 }; // brand chrome only (header rule, A button)
    juce::Colour ink          { 0xfff9fafa }; // primary text on panel
    juce::Colour inkMuted     { 0x99f9fafa };

    bool isLight() const { return name == "Light"; }

    // Distinct per-band colours (Pro-style readability; Playmakers palette, not a clone).
    static juce::Colour bandColour(int bandIndex, bool forLightCanvas = false)
    {
        static constexpr uint32_t darkPalette[] = {
            0xfff2c94c, // amber
            0xffde5f41, // signal orange
            0xff56ccf2, // sky
            0xffbb6bd9, // violet
            0xff6fcf97, // mint
            0xffeb5757, // coral
            0xff2d9cdb, // blue
            0xfff2994a, // warm orange
            0xff9b51e0, // purple
            0xff27ae60, // green
            0xff45b7d1, // teal
            0xfff5a623, // gold
        };
        // Deeper / more saturated — pops on Soft White canvas.
        static constexpr uint32_t lightPalette[] = {
            0xffd4a017, // deep amber
            0xffc94a30, // deep signal
            0xff0f8fc4, // deep sky
            0xff8e3ec9, // deep violet
            0xff2fa86a, // deep mint
            0xffd64545, // deep coral
            0xff1a7fbf, // deep blue
            0xffd9822b, // deep warm orange
            0xff7a32c9, // deep purple
            0xff1e9a4f, // deep green
            0xff2a9bb0, // deep teal
            0xffc9890a, // deep gold
        };
        const auto* palette = forLightCanvas ? lightPalette : darkPalette;
        constexpr int n = 12;
        const int i = ((bandIndex % n) + n) % n;
        return juce::Colour(palette[i]);
    }

    static Theme dark() { return {}; }

    static Theme light()
    {
        Theme t;
        t.name = "Light";
        // Cool soft white canvas (not cream) + indigo brand chrome.
        t.background = juce::Colour(0xfff2f3f6);
        t.backgroundEnd = juce::Colour(0xfff2f3f6);
        t.header = juce::Colour(0xff161342);
        t.panel = juce::Colour(0xffe6e8ee);          // light control strip
        t.grid = juce::Colour(0x22161342);
        t.spectrum = juce::Colour(0x3d161342);
        t.curve = juce::Colour(0xaa161342);
        t.curveSelected = juce::Colour(0xffde5f41);
        t.handle = juce::Colour(0xb3161342);
        t.handleSelected = juce::Colour(0xffde5f41);
        t.preview = juce::Colour(0xccde5f41);
        t.text = juce::Colour(0xfff9fafa);           // header text on indigo
        t.accent = juce::Colour(0xffde5f41);
        t.ink = juce::Colour(0xff161342);
        t.inkMuted = juce::Colour(0x99161342);
        return t;
    }

    juce::String toJSON() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        auto set = [obj](const char* key, juce::Colour c) { obj->setProperty(key, c.toString()); };
        set("background", background);
        set("backgroundEnd", backgroundEnd);
        set("header", header);
        set("panel", panel);
        set("grid", grid);
        set("spectrum", spectrum);
        set("curve", curve);
        set("curveSelected", curveSelected);
        set("handle", handle);
        set("handleSelected", handleSelected);
        set("preview", preview);
        set("text", text);
        set("accent", accent);
        return juce::JSON::toString(juce::var(obj));
    }

    // Named Dark/Light always resolve to the current brand presets so kit updates ship cleanly.
    static Theme fromJSON(const juce::String& json, const Theme& fallback)
    {
        auto parsed = juce::JSON::parse(json);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return fallback;

        const auto name = obj->getProperty("name").toString();
        if (name == "Light")
            return light();
        if (name == "Dark" || name.isEmpty())
            return dark();

        Theme t = fallback;
        t.name = name;
        auto get = [obj](const char* key, juce::Colour def)
        {
            auto v = obj->getProperty(key);
            return v.isString() ? juce::Colour::fromString(v.toString()) : def;
        };
        t.background = get("background", t.background);
        t.backgroundEnd = get("backgroundEnd", t.backgroundEnd);
        t.header = get("header", t.header);
        t.panel = get("panel", t.panel);
        t.grid = get("grid", t.grid);
        t.spectrum = get("spectrum", t.spectrum);
        t.curve = get("curve", t.curve);
        t.curveSelected = get("curveSelected", t.curveSelected);
        t.handle = get("handle", t.handle);
        t.handleSelected = get("handleSelected", t.handleSelected);
        t.preview = get("preview", t.preview);
        t.text = get("text", t.text);
        t.accent = get("accent", t.accent);
        return t;
    }
};

// Owns the active theme; components keep a const Theme& into this and repaint on change.
class ThemeManager
{
public:
    const Theme& current() const { return theme; }

    void setTheme(const Theme& newTheme)
    {
        theme = newTheme;
        if (onThemeChanged)
            onThemeChanged();
    }

    std::function<void()> onThemeChanged;

private:
    Theme theme = Theme::dark();
};
