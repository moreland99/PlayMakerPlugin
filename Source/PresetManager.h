#pragma once

#include "PresetState.h"
#include <functional>
#include <map>
#include <vector>

class PresetManager
{
public:
    enum class Kind { factory, user };

    struct Entry
    {
        juce::String id;
        juce::String displayName;
        Kind kind = Kind::factory;
        juce::File userFile;
    };

    PresetManager(juce::AudioProcessorValueTreeState& apvtsIn, juce::UndoManager& undoIn);

    void loadUserSettings();
    void saveUserSettings() const;

    juce::File getPresetFolder() const;
    void setPresetFolder(const juce::File& folder);

    juce::File getSettingsFile() const;
    juce::File getDefaultPresetFile() const;

    const std::vector<Entry>& getCatalog() const { return catalog; }
    void refreshUserPresets();

    int getIndexForId(const juce::String& id) const;
    const Entry* getEntry(int index) const;
    juce::String getCurrentPresetId() const { return currentPresetId; }
    juce::String getCurrentPresetDisplayName() const;

    bool loadPresetById(const juce::String& id);
    bool loadPresetByIndex(int index);
    bool loadPresetFromFile(const juce::File& file);
    bool saveUserPresetAs(const juce::String& name);
    bool saveCurrentAsDefault();
    void loadDefaultPresetIfPresent();
    void restoreFactoryInit();

    bool isMidiProgramChangeEnabled() const { return midiProgramChangesEnabled; }
    void setMidiProgramChangeEnabled(bool enabled);

    int getProgramCount() const;
    int getCurrentProgramIndex() const { return currentProgramIndex; }
    bool loadFactoryProgram(int programNumber);
    juce::String getFactoryProgramName(int programNumber) const;

    std::function<void()> onPresetLoaded;

private:
    void rebuildCatalog();
    void registerFactoryPresets();
    void applyFactoryPreset(const juce::String& id);
    bool loadUserPresetFile(const juce::File& file);
    void setCurrent(const Entry& entry, int programIndex);

    juce::AudioProcessorValueTreeState& apvts;
    juce::UndoManager& undoManager;

    std::vector<Entry> catalog;
    juce::String currentPresetId { "factory.init" };
    int currentProgramIndex = 0;

    juce::File customPresetFolder;
    bool midiProgramChangesEnabled = false;

    using FactoryFn = std::function<void(juce::AudioProcessorValueTreeState&)>;
    std::map<juce::String, FactoryFn> factoryAppliers;
};
