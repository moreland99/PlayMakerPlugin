#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Data-driven theme: a plain value object serialized to/from JSON, read by the UI at paint
// time. Shared convention for the whole PLAYMAKERS suite — no per-theme component classes.
struct Theme
{
    juce::String name = "Dark";
    juce::Colour background   { 0xff1a1a1e };
    juce::Colour header       { 0xff232328 };
    juce::Colour grid         { 0x26ffffff };
    juce::Colour spectrum     { 0x73ffffff };
    juce::Colour curve        { 0xffe0a030 };
    juce::Colour curveSelected{ 0xffffc060 };
    juce::Colour handle       { 0xffe0a030 };
    juce::Colour handleSelected{ 0xffffc060 };
    juce::Colour preview      { 0xff80c0ff };
    juce::Colour text         { 0xffd8d8dc };
    juce::Colour accent       { 0xffe0a030 };

    static Theme dark() { return {}; }

    static Theme light()
    {
        Theme t;
        t.name = "Light";
        t.background = juce::Colour(0xfff2f2f4);
        t.header = juce::Colour(0xffe4e4e8);
        t.grid = juce::Colour(0x33000000);
        t.spectrum = juce::Colour(0x99404048);
        t.curve = juce::Colour(0xffc07818);
        t.curveSelected = juce::Colour(0xff995e0a);
        t.handle = juce::Colour(0xffc07818);
        t.handleSelected = juce::Colour(0xff995e0a);
        t.preview = juce::Colour(0xff2878c0);
        t.text = juce::Colour(0xff2a2a30);
        t.accent = juce::Colour(0xffc07818);
        return t;
    }

    juce::String toJSON() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        auto set = [obj](const char* key, juce::Colour c) { obj->setProperty(key, c.toString()); };
        set("background", background);
        set("header", header);
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

    static Theme fromJSON(const juce::String& json, const Theme& fallback)
    {
        auto parsed = juce::JSON::parse(json);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return fallback;

        Theme t = fallback;
        t.name = obj->getProperty("name").toString();
        auto get = [obj](const char* key, juce::Colour def)
        {
            auto v = obj->getProperty(key);
            return v.isString() ? juce::Colour::fromString(v.toString()) : def;
        };
        t.background = get("background", t.background);
        t.header = get("header", t.header);
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
