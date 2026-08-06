#include "PresetBrowser.h"
#include "Brand.h"

PresetBrowser::PresetBrowser(PresetManager& presets, ThemeManager& themes)
    : presetManager(presets), themeManager(themes)
{
    presetButton.getProperties().set("pmChrome", true);
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetButton);
    refreshFromManager();
}

void PresetBrowser::refreshFromManager()
{
    presetButton.setButtonText(presetManager.getCurrentPresetDisplayName());
}

void PresetBrowser::applyTheme(const Theme& theme)
{
    presetButton.setColour(juce::TextButton::textColourOffId, theme.softWhite);
    presetButton.setColour(juce::TextButton::textColourOnId, theme.softWhite);
    presetButton.repaint();
}

void PresetBrowser::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void PresetBrowser::resized()
{
    presetButton.setBounds(getLocalBounds());
}

int PresetBrowser::catalogIndexForFactoryId(const juce::String& id) const
{
    return presetManager.getIndexForId(id);
}

void PresetBrowser::addFactoryBasics(juce::PopupMenu& menu)
{
    const juce::String ids[] {
        "factory.init", "factory.mix_polish", "factory.hp_cleanup",
        "factory.vocal_presence", "factory.low_end_tight"
    };

    for (const auto& id : ids)
    {
        const int idx = catalogIndexForFactoryId(id);
        if (idx >= 0)
            menu.addItem(kPresetBase + idx, presetManager.getCatalog()[(size_t) idx].displayName);
    }
}

void PresetBrowser::addFactoryDynamic(juce::PopupMenu& menu)
{
    juce::PopupMenu dynamic;

    juce::PopupMenu drums;
    for (const auto& id : { "factory.dyn.drums.kick_punch", "factory.dyn.drums.snare_crack",
                            "factory.dyn.drums.tom_body", "factory.dyn.drums.overhead_air" })
        if (const int idx = catalogIndexForFactoryId(id); idx >= 0)
            drums.addItem(kPresetBase + idx, presetManager.getCatalog()[(size_t) idx].displayName);

    juce::PopupMenu edm;
    for (const auto& id : { "factory.dyn.edm.kick_punch", "factory.dyn.edm.sub_control",
                            "factory.dyn.edm.lead_brightness", "factory.dyn.edm.mix_clarity" })
        if (const int idx = catalogIndexForFactoryId(id); idx >= 0)
            edm.addItem(kPresetBase + idx, presetManager.getCatalog()[(size_t) idx].displayName);

    juce::PopupMenu inst;
    for (const auto& id : { "factory.dyn.inst.glue_bus", "factory.dyn.inst.piano_tone",
                            "factory.dyn.inst.bass_definition" })
        if (const int idx = catalogIndexForFactoryId(id); idx >= 0)
            inst.addItem(kPresetBase + idx, presetManager.getCatalog()[(size_t) idx].displayName);

    juce::PopupMenu vox;
    for (const auto& id : { "factory.dyn.vox.lead_presence", "factory.dyn.vox.de_ess",
                            "factory.dyn.vox.warmth", "factory.dyn.vox.air" })
        if (const int idx = catalogIndexForFactoryId(id); idx >= 0)
            vox.addItem(kPresetBase + idx, presetManager.getCatalog()[(size_t) idx].displayName);

    dynamic.addSubMenu("Drums", drums);
    dynamic.addSubMenu("EDM", edm);
    dynamic.addSubMenu("Instrumental Mix", inst);
    dynamic.addSubMenu("Vocals", vox);

    menu.addSubMenu("Dynamic", dynamic);
}

void PresetBrowser::addUserPresets(juce::PopupMenu& menu)
{
    juce::PopupMenu userMenu;
    for (int i = 0; i < (int) presetManager.getCatalog().size(); ++i)
    {
        const auto& e = presetManager.getCatalog()[(size_t) i];
        if (e.kind != PresetManager::Kind::user)
            continue;
        userMenu.addItem(kPresetBase + i, e.displayName);
    }

    if (userMenu.getNumItems() > 0)
        menu.addSubMenu("User", userMenu);
}

void PresetBrowser::showPresetMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader("Basics");
    addFactoryBasics(menu);
    menu.addSeparator();
    addFactoryDynamic(menu);
    menu.addSeparator();
    addUserPresets(menu);
    menu.addSeparator();
    menu.addItem(kSaveAs, "Save As…");

    juce::PopupMenu options;
    options.addItem(kOptMidi, "Enable MIDI program changes", true,
                    presetManager.isMidiProgramChangeEnabled());
    options.addItem(kOptDefault, "Save as default");
    options.addItem(kOptOpenFile, "Open other preset…");
    options.addItem(kOptChangeFolder, "Change preset folder…");
    options.addItem(kOptRestoreFactory, "Restore factory presets");
    options.addSeparator();
    options.addItem(kOptThemeDark, "Theme: Dark", true, themeManager.current().name == "Dark");
    options.addItem(kOptThemeLight, "Theme: Light", true, themeManager.current().name == "Light");
    menu.addSubMenu("Options", options);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
                       [this](int result) { handleMenuResult(result); });
}

void PresetBrowser::handleMenuResult(int result)
{
    if (result == 0)
        return;

    if (result == kSaveAs)
    {
        juce::AlertWindow w("Save Preset", "Enter a name for this preset:", juce::AlertWindow::QuestionIcon);
        w.addTextEditor("name", presetManager.getCurrentPresetDisplayName(), "Preset name");
        w.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        w.enterModalState(true, juce::ModalCallbackFunction::create([this, wPtr = &w](int r) mutable
        {
            if (r != 1 || wPtr == nullptr)
                return;
            const auto name = wPtr->getTextEditorContents("name");
            if (presetManager.saveUserPresetAs(name))
            {
                refreshFromManager();
                if (onPresetApplied)
                    onPresetApplied();
            }
        }));
        return;
    }

    if (result == kOptMidi)
    {
        presetManager.setMidiProgramChangeEnabled(!presetManager.isMidiProgramChangeEnabled());
        return;
    }

    if (result == kOptDefault)
    {
        presetManager.saveCurrentAsDefault();
        return;
    }

    if (result == kOptOpenFile)
    {
        auto chooser = std::make_shared<juce::FileChooser>("Open preset", presetManager.getPresetFolder(), "*.preset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 const auto f = fc.getResult();
                                 if (!f.existsAsFile())
                                     return;
                                 if (presetManager.loadPresetFromFile(f))
                                 {
                                     refreshFromManager();
                                     if (onPresetApplied)
                                         onPresetApplied();
                                 }
                             });
        return;
    }

    if (result == kOptChangeFolder)
    {
        auto chooser = std::make_shared<juce::FileChooser>("Preset folder", presetManager.getPresetFolder());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this, chooser](const juce::FileChooser& fc)
                             {
                                 const auto f = fc.getResult();
                                 if (f.isDirectory())
                                 {
                                     presetManager.setPresetFolder(f);
                                     refreshFromManager();
                                 }
                             });
        return;
    }

    if (result == kOptRestoreFactory)
    {
        presetManager.restoreFactoryInit();
        refreshFromManager();
        if (onPresetApplied)
            onPresetApplied();
        return;
    }

    if (result == kOptThemeDark)
    {
        themeManager.setTheme(Theme::dark());
        if (onThemeChanged)
            onThemeChanged();
        return;
    }

    if (result == kOptThemeLight)
    {
        themeManager.setTheme(Theme::light());
        if (onThemeChanged)
            onThemeChanged();
        return;
    }

    if (result >= kPresetBase && result < kSaveAs)
    {
        const int idx = result - kPresetBase;
        if (presetManager.loadPresetByIndex(idx))
        {
            refreshFromManager();
            if (onPresetApplied)
                onPresetApplied();
        }
        return;
    }
}
