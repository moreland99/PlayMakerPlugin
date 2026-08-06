#include "PresetManager.h"

namespace
{

using BC = PresetState::BandConfig;

void bandBell(juce::AudioProcessorValueTreeState& apvts, int i, float freq, float gain, float q = 1.0f)
{
    BC b;
    b.type = Params::FilterType::bell;
    b.freqHz = freq;
    b.gainDb = gain;
    b.q = q;
    PresetState::applyBand(apvts, i, b);
}

void bandDynBell(juce::AudioProcessorValueTreeState& apvts, int i, float freq, float gain, float q,
                 float thresh, float rangeDb, float ratio = 4.0f, float att = 12.0f, float rel = 120.0f)
{
    BC b;
    b.type = Params::FilterType::bell;
    b.freqHz = freq;
    b.gainDb = gain;
    b.q = q;
    b.dynEnabled = true;
    b.dynThresholdDb = thresh;
    b.dynRangeDb = rangeDb;
    b.dynRatio = ratio;
    b.dynAttackMs = att;
    b.dynReleaseMs = rel;
    PresetState::applyBand(apvts, i, b);
}

void bandShelf(juce::AudioProcessorValueTreeState& apvts, int i, Params::FilterType shelf, float freq,
               float gain, float q = 0.707f)
{
    BC b;
    b.type = shelf;
    b.freqHz = freq;
    b.gainDb = gain;
    b.q = q;
    PresetState::applyBand(apvts, i, b);
}

void bandCut(juce::AudioProcessorValueTreeState& apvts, int i, Params::FilterType cut, float freq,
             float slope = 24.0f)
{
    BC b;
    b.type = cut;
    b.freqHz = freq;
    b.gainDb = 0.0f;
    b.q = 0.707f;
    b.slopeDbPerOct = slope;
    PresetState::applyBand(apvts, i, b);
}

void applyInit(juce::AudioProcessorValueTreeState& apvts)
{
    PresetState::disableAllBands(apvts);
    PresetState::setChoice(apvts, "phaseMode", static_cast<int>(Params::PhaseMode::zeroLatency));
}

void applyMixPolish(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandShelf(apvts, 0, Params::FilterType::highShelf, 10000.0f, 1.5f);
    bandShelf(apvts, 1, Params::FilterType::lowShelf, 120.0f, -1.0f);
    bandBell(apvts, 2, 3200.0f, 0.8f, 0.9f);
}

void applyHighPassCleanup(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandCut(apvts, 0, Params::FilterType::lowCut, 35.0f, 24.0f);
    bandBell(apvts, 1, 250.0f, -2.0f, 1.2f);
}

void applyVocalPresence(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 3500.0f, 2.5f, 1.1f);
    bandBell(apvts, 1, 220.0f, -1.5f, 0.8f);
    bandShelf(apvts, 2, Params::FilterType::highShelf, 9000.0f, 2.0f);
}

void applyLowEndTight(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandShelf(apvts, 0, Params::FilterType::lowShelf, 90.0f, -3.0f);
    bandBell(apvts, 1, 55.0f, 2.0f, 0.9f);
}

void applyDrumKickPunch(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 60.0f, 4.0f, 1.0f, -18.0f, 6.0f, 3.0f, 8.0f, 80.0f);
    bandBell(apvts, 1, 3200.0f, 2.0f, 1.4f);
    bandCut(apvts, 2, Params::FilterType::highCut, 16000.0f, 12.0f);
}

void applyDrumSnareCrack(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 4200.0f, 3.5f, 1.6f);
    bandBell(apvts, 1, 180.0f, -2.0f, 1.0f);
    bandDynBell(apvts, 2, 7000.0f, 2.0f, 1.2f, -22.0f, 4.0f);
}

void applyDrumTomBody(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 120.0f, 3.0f, 1.0f);
    bandBell(apvts, 1, 400.0f, -2.5f, 0.9f);
}

void applyDrumOverheadAir(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandShelf(apvts, 0, Params::FilterType::highShelf, 12000.0f, 3.0f);
    bandCut(apvts, 1, Params::FilterType::lowCut, 400.0f, 18.0f);
}

void applyEdmKickPunch(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 45.0f, 5.0f, 0.9f, -16.0f, 8.0f, 4.0f, 5.0f, 60.0f);
    bandDynBell(apvts, 1, 200.0f, -3.0f, 1.2f, -20.0f, -4.0f);
    bandShelf(apvts, 2, Params::FilterType::highShelf, 11000.0f, 2.5f);
}

void applyEdmSubControl(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandCut(apvts, 0, Params::FilterType::lowCut, 28.0f, 24.0f);
    bandBell(apvts, 1, 80.0f, 2.0f, 0.85f);
    bandDynBell(apvts, 2, 120.0f, -2.0f, 1.0f, -18.0f, -6.0f);
}

void applyEdmLeadBrightness(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandShelf(apvts, 0, Params::FilterType::highShelf, 8000.0f, 4.0f);
    bandBell(apvts, 1, 2500.0f, 2.0f, 1.0f);
    bandBell(apvts, 2, 400.0f, -2.0f, 0.8f);
}

void applyEdmMixClarity(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 300.0f, -2.0f, 1.0f);
    bandBell(apvts, 1, 2800.0f, 1.5f, 0.9f);
    bandDynBell(apvts, 2, 6000.0f, 2.0f, 1.1f, -24.0f, 5.0f);
}

void applyInstGlueBus(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 250.0f, 1.0f, 0.7f, -20.0f, -3.0f, 2.0f, 30.0f, 200.0f);
    bandShelf(apvts, 1, Params::FilterType::highShelf, 12000.0f, 1.0f);
}

void applyInstPianoTone(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 3200.0f, 2.0f, 0.9f);
    bandBell(apvts, 1, 800.0f, -1.5f, 0.8f);
    bandShelf(apvts, 2, Params::FilterType::lowShelf, 100.0f, 1.5f);
}

void applyInstBassDefinition(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 90.0f, 3.0f, 1.0f, -22.0f, 5.0f);
    bandBell(apvts, 1, 700.0f, -2.0f, 1.1f);
}

void applyVoxLeadPresence(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandBell(apvts, 0, 4200.0f, 2.5f, 1.0f);
    bandBell(apvts, 1, 250.0f, -2.0f, 0.9f);
    bandShelf(apvts, 2, Params::FilterType::highShelf, 10000.0f, 2.0f);
}

void applyVoxDeEss(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 6500.0f, -4.0f, 1.4f, -28.0f, -8.0f, 6.0f, 3.0f, 90.0f);
    bandBell(apvts, 1, 3500.0f, 1.5f, 0.9f);
}

void applyVoxWarmth(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandShelf(apvts, 0, Params::FilterType::lowShelf, 220.0f, 2.5f);
    bandBell(apvts, 1, 5000.0f, -1.0f, 0.8f);
}

void applyVoxAir(juce::AudioProcessorValueTreeState& apvts)
{
    applyInit(apvts);
    bandDynBell(apvts, 0, 12000.0f, 3.0f, 0.7f, -26.0f, 6.0f, 2.5f, 15.0f, 150.0f);
    bandBell(apvts, 1, 3000.0f, 1.0f, 1.0f);
}

} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvtsIn, juce::UndoManager& undoIn)
    : apvts(apvtsIn), undoManager(undoIn)
{
    registerFactoryPresets();
    rebuildCatalog();
    loadUserSettings();
    refreshUserPresets();
}

void PresetManager::registerFactoryPresets()
{
    factoryAppliers["factory.init"] = applyInit;
    factoryAppliers["factory.mix_polish"] = applyMixPolish;
    factoryAppliers["factory.hp_cleanup"] = applyHighPassCleanup;
    factoryAppliers["factory.vocal_presence"] = applyVocalPresence;
    factoryAppliers["factory.low_end_tight"] = applyLowEndTight;

    factoryAppliers["factory.dyn.drums.kick_punch"] = applyDrumKickPunch;
    factoryAppliers["factory.dyn.drums.snare_crack"] = applyDrumSnareCrack;
    factoryAppliers["factory.dyn.drums.tom_body"] = applyDrumTomBody;
    factoryAppliers["factory.dyn.drums.overhead_air"] = applyDrumOverheadAir;

    factoryAppliers["factory.dyn.edm.kick_punch"] = applyEdmKickPunch;
    factoryAppliers["factory.dyn.edm.sub_control"] = applyEdmSubControl;
    factoryAppliers["factory.dyn.edm.lead_brightness"] = applyEdmLeadBrightness;
    factoryAppliers["factory.dyn.edm.mix_clarity"] = applyEdmMixClarity;

    factoryAppliers["factory.dyn.inst.glue_bus"] = applyInstGlueBus;
    factoryAppliers["factory.dyn.inst.piano_tone"] = applyInstPianoTone;
    factoryAppliers["factory.dyn.inst.bass_definition"] = applyInstBassDefinition;

    factoryAppliers["factory.dyn.vox.lead_presence"] = applyVoxLeadPresence;
    factoryAppliers["factory.dyn.vox.de_ess"] = applyVoxDeEss;
    factoryAppliers["factory.dyn.vox.warmth"] = applyVoxWarmth;
    factoryAppliers["factory.dyn.vox.air"] = applyVoxAir;
}

void PresetManager::rebuildCatalog()
{
    catalog.clear();

    auto addFactory = [this](const juce::String& id, const juce::String& name)
    {
        catalog.push_back({ id, name, Kind::factory, {} });
    };

    addFactory("factory.init", "Init / Flat");
    addFactory("factory.mix_polish", "Mix Bus Polish");
    addFactory("factory.hp_cleanup", "High Pass Cleanup");
    addFactory("factory.vocal_presence", "Vocal Presence");
    addFactory("factory.low_end_tight", "Low End Tighten");

    addFactory("factory.dyn.drums.kick_punch", "Kick Punch");
    addFactory("factory.dyn.drums.snare_crack", "Snare Crack");
    addFactory("factory.dyn.drums.tom_body", "Tom Body");
    addFactory("factory.dyn.drums.overhead_air", "Overhead Air");

    addFactory("factory.dyn.edm.kick_punch", "Kick Punch");
    addFactory("factory.dyn.edm.sub_control", "Sub Control");
    addFactory("factory.dyn.edm.lead_brightness", "Lead Brightness");
    addFactory("factory.dyn.edm.mix_clarity", "Mix Clarity");

    addFactory("factory.dyn.inst.glue_bus", "Glue Bus");
    addFactory("factory.dyn.inst.piano_tone", "Piano Tone");
    addFactory("factory.dyn.inst.bass_definition", "Bass Definition");

    addFactory("factory.dyn.vox.lead_presence", "Lead Vocal Presence");
    addFactory("factory.dyn.vox.de_ess", "De-Ess");
    addFactory("factory.dyn.vox.warmth", "Warmth");
    addFactory("factory.dyn.vox.air", "Air");
}

void PresetManager::refreshUserPresets()
{
    rebuildCatalog();

    const auto folder = getPresetFolder();
    if (!folder.isDirectory())
        folder.createDirectory();

    juce::Array<juce::File> files;
    folder.findChildFiles(files, juce::File::findFiles, false, "*.preset");

    for (const auto& f : files)
    {
        if (f.getFileNameWithoutExtension().startsWithChar('_'))
            continue;

        Entry e;
        e.id = "user." + f.getFileNameWithoutExtension();
        e.displayName = f.getFileNameWithoutExtension();
        e.kind = Kind::user;
        e.userFile = f;
        catalog.push_back(e);
    }
}

juce::File PresetManager::getPresetFolder() const
{
    if (customPresetFolder.isDirectory())
        return customPresetFolder;

    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("PLAYMAKERS EQ")
        .getChildFile("Presets");
}

void PresetManager::setPresetFolder(const juce::File& folder)
{
    customPresetFolder = folder;
    getPresetFolder().createDirectory();
    refreshUserPresets();
    saveUserSettings();
}

juce::File PresetManager::getSettingsFile() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("PLAYMAKERS EQ")
        .getChildFile("preset_settings.xml");
}

juce::File PresetManager::getDefaultPresetFile() const
{
    return getPresetFolder().getChildFile("_default.preset");
}

void PresetManager::loadUserSettings()
{
    const auto file = getSettingsFile();
    if (!file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        customPresetFolder = juce::File(xml->getStringAttribute("presetFolder"));
        midiProgramChangesEnabled = xml->getBoolAttribute("midiProgramChanges", false);
        currentPresetId = xml->getStringAttribute("lastPresetId", currentPresetId);
    }
}

void PresetManager::saveUserSettings() const
{
    juce::XmlElement root("PlaymakersEQPresets");
    if (customPresetFolder.isDirectory())
        root.setAttribute("presetFolder", customPresetFolder.getFullPathName());
    root.setAttribute("midiProgramChanges", midiProgramChangesEnabled);
    root.setAttribute("lastPresetId", currentPresetId);

    getSettingsFile().getParentDirectory().createDirectory();
    root.writeTo(getSettingsFile());
}

int PresetManager::getIndexForId(const juce::String& id) const
{
    for (int i = 0; i < (int) catalog.size(); ++i)
        if (catalog[(size_t) i].id == id)
            return i;
    return -1;
}

const PresetManager::Entry* PresetManager::getEntry(int index) const
{
    if (index < 0 || index >= (int) catalog.size())
        return nullptr;
    return &catalog[(size_t) index];
}

juce::String PresetManager::getCurrentPresetDisplayName() const
{
    if (currentPresetId == "user._default")
        return "Default";

    if (const auto idx = getIndexForId(currentPresetId); idx >= 0)
        return catalog[(size_t) idx].displayName;

    if (currentPresetId.startsWith("user."))
        return currentPresetId.fromFirstOccurrenceOf("user.", false, false);

    return "Custom";
}

void PresetManager::applyFactoryPreset(const juce::String& id)
{
    const auto it = factoryAppliers.find(id);
    if (it == factoryAppliers.end())
        return;

    undoManager.beginNewTransaction("Load Preset");
    it->second(apvts);
}

bool PresetManager::loadUserPresetFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        undoManager.beginNewTransaction("Load Preset");
        PresetState::applyFullState(apvts, juce::ValueTree::fromXml(*xml));
        return true;
    }
    return false;
}

void PresetManager::setCurrent(const Entry& entry, int programIndex)
{
    currentPresetId = entry.id;
    currentProgramIndex = programIndex;
    saveUserSettings();
    if (onPresetLoaded)
        onPresetLoaded();
}

bool PresetManager::loadPresetById(const juce::String& id)
{
    const int index = getIndexForId(id);
    if (index < 0)
        return false;
    return loadPresetByIndex(index);
}

bool PresetManager::loadPresetByIndex(int index)
{
    const auto* entry = getEntry(index);
    if (entry == nullptr)
        return false;

    bool ok = false;
    if (entry->kind == Kind::factory)
    {
        applyFactoryPreset(entry->id);
        ok = true;
    }
    else
        ok = loadUserPresetFile(entry->userFile);

    if (ok)
        setCurrent(*entry, index);

    return ok;
}

bool PresetManager::saveUserPresetAs(const juce::String& name)
{
    const auto safe = name.trim().replaceCharacters("\\/:*?\"<>|", "__________");
    if (safe.isEmpty())
        return false;

    getPresetFolder().createDirectory();
    const auto file = getPresetFolder().getChildFile(safe + ".preset");

    if (auto xml = PresetState::captureFullState(apvts).createXml())
    {
        if (!xml->writeTo(file))
            return false;

        refreshUserPresets();
        const auto idx = getIndexForId("user." + safe);
        if (idx >= 0)
            setCurrent(catalog[(size_t) idx], idx);
        return true;
    }
    return false;
}

bool PresetManager::saveCurrentAsDefault()
{
    getPresetFolder().createDirectory();
    if (auto xml = PresetState::captureFullState(apvts).createXml())
        return xml->writeTo(getDefaultPresetFile());
    return false;
}

void PresetManager::loadDefaultPresetIfPresent()
{
    if (getDefaultPresetFile().existsAsFile())
    {
        loadUserPresetFile(getDefaultPresetFile());
        currentPresetId = "user._default";
    }
    else if (getIndexForId(currentPresetId) >= 0)
        loadPresetById(currentPresetId);
}

bool PresetManager::loadPresetFromFile(const juce::File& file)
{
    if (!loadUserPresetFile(file))
        return false;

    currentPresetId = "user." + file.getFileNameWithoutExtension();
    saveUserSettings();
    if (onPresetLoaded)
        onPresetLoaded();
    return true;
}

void PresetManager::restoreFactoryInit()
{
    loadPresetById("factory.init");
}

void PresetManager::setMidiProgramChangeEnabled(bool enabled)
{
    midiProgramChangesEnabled = enabled;
    saveUserSettings();
}

int PresetManager::getProgramCount() const
{
    if (!midiProgramChangesEnabled)
        return 1;

    int count = 0;
    for (const auto& e : catalog)
        if (e.kind == Kind::factory)
            ++count;
    return juce::jmax(1, count);
}

bool PresetManager::loadFactoryProgram(int programNumber)
{
    int seen = 0;
    for (int i = 0; i < (int) catalog.size(); ++i)
    {
        if (catalog[(size_t) i].kind != Kind::factory)
            continue;
        if (seen == programNumber)
            return loadPresetByIndex(i);
        ++seen;
    }
    return false;
}

juce::String PresetManager::getFactoryProgramName(int programNumber) const
{
    int seen = 0;
    for (const auto& e : catalog)
    {
        if (e.kind != Kind::factory)
            continue;
        if (seen == programNumber)
            return e.displayName;
        ++seen;
    }
    return {};
}
