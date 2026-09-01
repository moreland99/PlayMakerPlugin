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
                         button.getLocalBounds().reduced((bool) button.getProperties().getWithDefault("pmCompact", false) ? 3 : 8, 0),
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

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        juce::Colour accent = theme.signalOrange;
        if (auto* c = slider.getProperties().getVarPointer("pmAccentColour"))
            if (c->isString())
                accent = juce::Colour::fromString(c->toString());

        const bool large = (bool) slider.getProperties().getWithDefault("pmLargeKnob", false);
        const bool showDynArc = (bool) slider.getProperties().getWithDefault("pmShowDynArc", false);
        // Same 0–1 position JUCE used for this paint. Path arcs and the needle must share it.
        const float prop = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
        const float startAng = rotaryStartAngle;
        const float endAng = rotaryEndAngle;
        const float needleAngle = juce::jmap(prop, 0.0f, 1.0f, startAng, endAng);

        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float enabledA = slider.isEnabled() ? 1.0f : 0.42f;
        // JUCE rotary/path convention: 0 rad = 12 o'clock, increasing clockwise
        // (sin, -cos). Math cos/sin is 90° off (3 o'clock) and must not be used here.
        auto pointOnRing = [cx, cy] (float ang, float radius) -> juce::Point<float>
        {
            return { cx + radius * std::sin(ang), cy - radius * std::cos(ang) };
        };

        // Fill most of the control — solid dial, not a thin “gadget” ring.
        const float outerR = size * (large ? 0.49f : 0.46f);
        const float arcR = outerR * 0.97f;
        const float bodyR = outerR * (large ? 0.82f : 0.78f);
        const float rimR = bodyR * 0.92f;
        const float faceR = bodyR * 0.74f;
        const float arcW = large ? 4.2f : 2.8f;

        // Soft drop under the dial.
        g.setColour(juce::Colours::black.withAlpha((theme.isLight() ? 0.10f : 0.35f) * enabledA));
        g.fillEllipse(cx - bodyR + 1.0f, cy - bodyR + 2.5f, bodyR * 2.0f, bodyR * 2.0f);

        // Quiet track. No yellow value/progress fill on Freq, Q, or Gain.
        {
            juce::Path track;
            track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAng, endAng, true);
            g.setColour(theme.ink.withAlpha((theme.isLight() ? 0.14f : 0.22f) * enabledA));
            g.strokePath(track, juce::PathStrokeType(arcW, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // Metal-ish body: outer rim + recessed face.
        const auto bodyCol = theme.isLight() ? juce::Colour(0xffeceef0) : juce::Colour(0xff2c2c31);
        const auto rimCol = theme.isLight() ? juce::Colour(0xffd4d7db) : juce::Colour(0xff1a1a1e);
        const auto faceCol = theme.isLight() ? juce::Colour(0xfff7f8f9) : juce::Colour(0xff35353b);

        g.setColour(rimCol.withMultipliedAlpha(enabledA));
        g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        {
            juce::ColourGradient rimGrad(bodyCol.brighter(0.18f), cx, cy - bodyR,
                                         bodyCol.darker(0.22f), cx, cy + bodyR, false);
            g.setGradientFill(rimGrad);
            g.setOpacity(enabledA);
            g.fillEllipse(cx - rimR, cy - rimR, rimR * 2.0f, rimR * 2.0f);
            g.setOpacity(1.0f);
        }

        {
            juce::ColourGradient faceGrad(faceCol.brighter(0.12f), cx - faceR * 0.35f, cy - faceR * 0.45f,
                                          faceCol.darker(0.08f), cx + faceR * 0.2f, cy + faceR * 0.55f, false);
            g.setGradientFill(faceGrad);
            g.setOpacity(enabledA);
            g.fillEllipse(cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);
            g.setOpacity(1.0f);
        }

        // Specular highlight (top edge).
        g.setColour(theme.softWhite.withAlpha((theme.isLight() ? 0.35f : 0.14f) * enabledA));
        g.drawEllipse(cx - faceR + 1.0f, cy - faceR + 1.0f, faceR * 2.0f - 2.0f, faceR * 2.0f - 2.0f, 1.0f);

        // Dyn range/live: same needle-relative dB math as before. Filled face sectors,
        // not exterior strokes. Under the needle. Skip |delta| < 0.05 (Range = 0 stays metal).
        if (showDynArc)
        {
            juce::Colour dynAccent = accent;
            if (auto* c = slider.getProperties().getVarPointer("pmDynArcColour"))
                if (c->isString())
                    dynAccent = juce::Colour::fromString(c->toString());

            const double dynRangeDb = (double) slider.getProperties().getWithDefault("pmDynRangeDb", 0.0);
            const double dynOffsetDb = (double) slider.getProperties().getWithDefault("pmDynOffsetDb", 0.0);
            const double spanDb = slider.getMaximum() - slider.getMinimum();
            const auto faceBounds = juce::Rectangle<float>(cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);

            auto fillDeltaFromNeedle = [&] (double deltaDb, juce::Colour colour)
            {
                if (spanDb <= 1.0e-6 || std::abs(deltaDb) < 0.05)
                    return;
                const float endProp = juce::jlimit(0.0f, 1.0f, prop + (float) (deltaDb / spanDb));
                const float endAngle = juce::jmap(endProp, 0.0f, 1.0f, startAng, endAng);
                if (std::abs(endAngle - needleAngle) < 0.008f)
                    return;

                juce::Path sector;
                sector.addPieSegment(faceBounds, needleAngle, endAngle, 0.28f);
                g.setColour(colour);
                g.fillPath(sector);
            };

            fillDeltaFromNeedle(dynRangeDb, dynAccent.withAlpha(0.30f * enabledA));
            fillDeltaFromNeedle(dynOffsetDb, dynAccent.withAlpha(0.90f * enabledA));
        }

        // Needle — same angle and (sin, -cos) convention as Path::addCentredArc.
        const float needleInner = faceR * 0.12f;
        const float needleOuter = faceR * 0.88f;
        const auto n0 = pointOnRing(needleAngle, needleInner);
        const auto n1 = pointOnRing(needleAngle, needleOuter);
        g.setColour((theme.isLight() ? theme.ink : theme.softWhite).withAlpha(0.95f * enabledA));
        g.drawLine(n0.x, n0.y, n1.x, n1.y, large ? 2.4f : 2.0f);

        const float tip = large ? 3.2f : 2.6f;
        g.setColour(accent.withAlpha(enabledA));
        g.fillEllipse(n1.x - tip, n1.y - tip, tip * 2.0f, tip * 2.0f);

        const float cap = faceR * 0.16f;
        g.setColour((theme.isLight() ? theme.ink.withAlpha(0.18f) : juce::Colour(0xff1e1e22)).withMultipliedAlpha(enabledA));
        g.fillEllipse(cx - cap, cy - cap, cap * 2.0f, cap * 2.0f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float, const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
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
        g.setColour(theme.softWhite);
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
