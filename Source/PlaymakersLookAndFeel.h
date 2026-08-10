#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Brand.h"
#include "Theme.h"

// PLAYMAKERS LookAndFeel — flat, sharp chrome (square corners, integrated panels).
class PlaymakersLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit PlaymakersLookAndFeel(const Theme& themeToUse) { setTheme(themeToUse); }

    void setTheme(const Theme& t)
    {
        theme = t;
        setColour(juce::ResizableWindow::backgroundColourId, t.background);
        setColour(juce::Label::textColourId, t.ink.withAlpha(0.9f));
        setColour(juce::TextButton::textColourOffId, t.softWhite); // header chrome default
        setColour(juce::TextButton::textColourOnId, t.softWhite);
        setColour(juce::ComboBox::textColourId, t.ink.withAlpha(0.92f));
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::backgroundColourId,
                  t.isLight() ? t.softWhite : t.panel.brighter(0.04f));
        setColour(juce::PopupMenu::textColourId, t.isLight() ? t.ink : t.softWhite.withAlpha(0.9f));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  t.isLight() ? t.ink.withAlpha(0.06f) : t.softWhite.withAlpha(0.08f));
        setColour(juce::PopupMenu::highlightedTextColourId, t.isLight() ? t.ink : t.softWhite);
        setColour(juce::Slider::textBoxTextColourId, t.ink.withAlpha(0.85f));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::CaretComponent::caretColourId, t.signalOrange);
    }

    const Theme& getTheme() const { return theme; }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return Brand::uiFont(11.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return Brand::uiFont(11.5f); }
    juce::Font getPopupMenuFont() override { return Brand::uiFont(12.0f); }
    juce::Font getLabelFont(juce::Label&) override { return Brand::uiFont(11.5f); }
    juce::Font getSliderPopupFont(juce::Slider&) override { return Brand::uiFont(11.0f); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool accent = (bool) button.getProperties().getWithDefault("pmAccent", false);
        const bool onChrome = (bool) button.getProperties().getWithDefault("pmChrome", false);
        const float alphaMul = button.isEnabled() ? 1.0f : 0.4f;

        juce::Colour accentFill = theme.signalOrange;
        if (auto* c = button.getProperties().getVarPointer("pmAccentColour"))
            if (c->isString())
                accentFill = juce::Colour::fromString(c->toString());

        if (accent)
        {
            auto fill = accentFill;
            if (down) fill = fill.darker(0.12f);
            else if (highlighted) fill = fill.brighter(0.05f);
            g.setColour(fill.withMultipliedAlpha(alphaMul));
            g.fillRect(bounds);
            return;
        }

        // Header sits on dark chrome in both themes; inspector follows panel ink.
        const auto ghost = onChrome ? theme.softWhite : theme.ink;
        const float fillA = down ? 0.14f : (highlighted ? 0.09f : (theme.isLight() && !onChrome ? 0.06f : 0.04f));
        g.setColour(ghost.withAlpha(fillA * alphaMul));
        g.fillRect(bounds);
        g.setColour(ghost.withAlpha((highlighted ? 0.28f : 0.16f) * alphaMul));
        g.drawRect(bounds, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        const bool accent = (bool) button.getProperties().getWithDefault("pmAccent", false);
        const bool onChrome = (bool) button.getProperties().getWithDefault("pmChrome", false);
        g.setFont(Brand::uiFont(11.0f));

        juce::Colour textCol = theme.softWhite;
        if (accent)
            textCol = theme.softWhite;
        else if (onChrome)
            textCol = theme.softWhite.withAlpha(button.isEnabled() ? 0.88f : 0.35f);
        else
            textCol = theme.ink.withAlpha(button.isEnabled() ? 0.88f : 0.35f);

        g.setColour(textCol);
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(8, 0),
                         juce::Justification::centred, 1, 0.9f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
        const auto ink = theme.ink;

        g.setColour(ink.withAlpha(isButtonDown ? 0.10f : (theme.isLight() ? 0.06f : 0.045f)));
        g.fillRect(bounds);

        juce::Colour outline = ink.withAlpha(theme.isLight() ? 0.22f : 0.16f);
        if (box.hasKeyboardFocus(false))
        {
            outline = theme.signalOrange.withAlpha(0.65f);
            if (auto* c = box.getProperties().getVarPointer("pmAccentColour"))
                if (c->isString())
                    outline = juce::Colour::fromString(c->toString()).withAlpha(0.85f);
        }

        g.setColour(outline);
        g.drawRect(bounds, 1.0f);

        const float cx = (float) width - 12.0f;
        const float cy = (float) height * 0.5f;
        g.setColour(ink.withAlpha(0.55f));
        g.drawLine(cx - 3.2f, cy - 1.2f, cx, cy + 1.6f, 1.1f);
        g.drawLine(cx, cy + 1.6f, cx + 3.2f, cy - 1.2f, 1.1f);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(10, 0, juce::jmax(10, box.getWidth() - 28), box.getHeight());
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.setColour(theme.isLight() ? theme.softWhite : theme.panel.brighter(0.05f));
        g.fillRect(0, 0, width, height);
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.16f : 0.10f));
        g.drawRect(0, 0, width, height, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool, const juce::String& text, const juce::String&,
                            const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour(theme.ink.withAlpha(0.10f));
            g.fillRect(area.reduced(12, 0).withHeight(1).withY(area.getCentreY()));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.06f : 0.07f));
            g.fillRect(area);
            g.setColour(theme.signalOrange);
            g.fillRect(area.getX(), area.getY() + 3, 2, area.getHeight() - 6);
        }

        g.setFont(Brand::uiFont(12.0f));
        g.setColour(theme.ink.withAlpha(isActive ? (isHighlighted ? 1.0f : 0.86f) : 0.35f));
        g.drawText(text, area.reduced(14, 0), juce::Justification::centredLeft, true);

        if (isTicked)
        {
            g.setColour(theme.signalOrange);
            juce::Path check;
            const float x = (float) area.getRight() - 16.0f;
            const float y = (float) area.getCentreY();
            check.startNewSubPath(x - 3.2f, y);
            check.lineTo(x - 0.8f, y + 2.2f);
            check.lineTo(x + 3.6f, y - 2.8f);
            g.strokePath(check, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                           bool highlighted, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto box = bounds.removeFromLeft(14.0f).withSizeKeepingCentre(12.0f, 12.0f);
        const auto ink = theme.ink;

        g.setColour(ink.withAlpha(highlighted ? 0.10f : 0.05f));
        g.fillRect(box);
        g.setColour(ink.withAlpha(0.28f));
        g.drawRect(box, 1.0f);

        if (button.getToggleState())
        {
            juce::Colour on = theme.signalOrange;
            if (auto* c = button.getProperties().getVarPointer("pmAccentColour"))
                if (c->isString())
                    on = juce::Colour::fromString(c->toString());
            g.setColour(on.withAlpha(button.isEnabled() ? 1.0f : 0.35f));
            g.fillRect(box.reduced(2.5f));
        }

        g.setColour(ink.withAlpha(button.isEnabled() ? 0.85f : 0.35f));
        g.setFont(Brand::uiFont(11.5f));
        g.drawText(button.getButtonText(),
                   bounds.withTrimmedLeft(8.0f), juce::Justification::centredLeft, false);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float, const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            if (style == juce::Slider::RotaryHorizontalVerticalDrag
                || style == juce::Slider::RotaryVerticalDrag
                || style == juce::Slider::RotaryHorizontalDrag)
            {
                juce::Colour accent = theme.signalOrange;
                if (auto* c = slider.getProperties().getVarPointer("pmAccentColour"))
                    if (c->isString())
                        accent = juce::Colour::fromString(c->toString());

                const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(2.0f);
                const float angle = juce::jmap((float) slider.getValue(), (float) slider.getMinimum(), (float) slider.getMaximum(),
                                               juce::MathConstants<float>::pi * 1.15f,
                                               juce::MathConstants<float>::pi * 2.85f);
                const float cx = bounds.getCentreX();
                const float cy = bounds.getCentreY();
                const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;

                g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.10f : 0.12f));
                juce::Path track;
                track.addCentredArc(cx, cy, r, r, 0.0f,
                                      juce::MathConstants<float>::pi * 1.15f,
                                      juce::MathConstants<float>::pi * 2.85f, true);
                g.strokePath(track, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

                g.setColour(accent.withAlpha(slider.isEnabled() ? 0.95f : 0.35f));
                juce::Path valueArc;
                valueArc.addCentredArc(cx, cy, r, r, 0.0f,
                                       juce::MathConstants<float>::pi * 1.15f, angle, true);
                g.strokePath(valueArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));

                g.setColour(accent);
                g.fillEllipse(cx + std::cos(angle) * r - 2.5f, cy + std::sin(angle) * r - 2.5f, 5.0f, 5.0f);
                return;
            }

            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        juce::Colour accent = theme.signalOrange;
        if (auto* c = slider.getProperties().getVarPointer("pmAccentColour"))
            if (c->isString())
                accent = juce::Colour::fromString(c->toString());

        const float cy = (float) y + (float) height * 0.5f;
        auto track = juce::Rectangle<float>((float) x, cy - 1.5f, (float) width, 3.0f);
        g.setColour(theme.ink.withAlpha(theme.isLight() ? 0.12f : 0.10f));
        g.fillRect(track);

        const float fillW = juce::jlimit(0.0f, (float) width, sliderPos - (float) x);
        g.setColour(accent.withAlpha(slider.isEnabled() ? 0.95f : 0.3f));
        g.fillRect(track.withWidth(fillW));

        auto thumb = juce::Rectangle<float>(11.0f, 11.0f).withCentre({ sliderPos, cy });
        g.setColour(theme.isLight() ? theme.softWhite : theme.softWhite);
        g.fillEllipse(thumb);
        g.setColour(accent);
        g.drawEllipse(thumb.reduced(0.5f), 1.4f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));
        if (!label.isBeingEdited())
        {
            g.setColour(label.findColour(juce::Label::textColourId)
                             .withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.4f));
            g.setFont(getLabelFont(label));
            g.drawFittedText(label.getText(),
                             label.getBorderSize().subtractedFrom(label.getLocalBounds()),
                             label.getJustificationType(), 1, 0.9f);
        }
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox(slider);
        l->setColour(juce::Label::textColourId, theme.ink.withAlpha(0.92f));
        l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::outlineColourId, theme.ink.withAlpha(theme.isLight() ? 0.18f : 0.12f));
        l->setColour(juce::TextEditor::textColourId, theme.ink);
        l->setColour(juce::TextEditor::backgroundColourId,
                     theme.isLight() ? theme.softWhite : theme.panel.brighter(0.08f));
        l->setColour(juce::TextEditor::outlineColourId, theme.ink.withAlpha(0.25f));
        l->setColour(juce::TextEditor::highlightedTextColourId, theme.softWhite);
        l->setColour(juce::TextEditor::highlightColourId, theme.signalOrange.withAlpha(0.55f));
        l->setFont(Brand::uiFont(12.0f, true));
        return l;
    }

private:
    Theme theme;
};
