#pragma once

#include "PresetManager.h"
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(PresetManager& presets, ThemeManager& themes);

    std::function<void()> onPresetApplied;
    std::function<void()> onThemeChanged;

    void refreshFromManager();
    void applyTheme(const Theme& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int kPresetBase = 100;
    static constexpr int kSaveAs = 9000;
    static constexpr int kOptMidi = 9100;
    static constexpr int kOptDefault = 9101;
    static constexpr int kOptOpenFile = 9102;
    static constexpr int kOptChangeFolder = 9103;
    static constexpr int kOptRestoreFactory = 9104;
    static constexpr int kOptThemeDark = 9105;
    static constexpr int kOptThemeLight = 9106;

    void showPresetMenu();
    void handleMenuResult(int result);
    void addFactoryBasics(juce::PopupMenu& menu);
    void addFactoryDynamic(juce::PopupMenu& menu);
    void addUserPresets(juce::PopupMenu& menu);
    int catalogIndexForFactoryId(const juce::String& id) const;

    PresetManager& presetManager;
    ThemeManager& themeManager;
    juce::TextButton presetButton { "Presets" };
};
