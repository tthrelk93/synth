#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    presetDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("MiniMoog")
                          .getChildFile("Presets");
    presetDirectory.createDirectory();
}

juce::StringArray PresetManager::getPresetNames() const {
    juce::StringArray presets;
    if (!presetDirectory.exists()) {
        return presets;
    }

    auto files = presetDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& file : files) {
        juce::String presetName = file.getFileNameWithoutExtension();
        if (auto xml = juce::XmlDocument::parse(file)) {
            const auto namedPreset = xml->getStringAttribute("presetName");
            if (namedPreset.isNotEmpty()) {
                presetName = namedPreset;
            }
        }
        presets.add(presetName);
    }
    presets.sort(true);
    presets.removeDuplicates(false);
    return presets;
}

bool PresetManager::savePreset(const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) {
        return false;
    }
    presetDirectory.createDirectory();
    auto file = getPresetFile(trimmed);
    auto state = apvts.copyState();
    auto xml = state.createXml();
    xml->setAttribute("presetName", trimmed);
    if (!xml->writeTo(file)) {
        return false;
    }
    setLastPresetName(trimmed);
    return true;
}

bool PresetManager::loadPreset(const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) {
        return false;
    }
    auto file = findPresetFile(trimmed);
    if (!file.existsAsFile()) {
        return false;
    }
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) {
        return false;
    }
    if (!xml->hasTagName(apvts.state.getType())) {
        return false;
    }
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    setLastPresetName(trimmed);
    return true;
}

bool PresetManager::presetExists(const juce::String& name) const {
    auto file = findPresetFile(name);
    return file.existsAsFile();
}

juce::String PresetManager::getLastPresetName() const {
    auto file = getLastPresetFile();
    if (!file.existsAsFile()) {
        return {};
    }
    return file.loadFileAsString().trim();
}

void PresetManager::setLastPresetName(const juce::String& name) {
    presetDirectory.createDirectory();
    getLastPresetFile().replaceWithText(name.trim());
}

juce::File PresetManager::getPresetDirectory() const {
    return presetDirectory;
}

juce::File PresetManager::getPresetFile(const juce::String& name) const {
    const auto safeName = juce::File::createLegalFileName(name.trim());
    return presetDirectory.getChildFile(safeName).withFileExtension("xml");
}

juce::File PresetManager::findPresetFile(const juce::String& name) const {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || !presetDirectory.exists()) {
        return {};
    }

    auto directFile = getPresetFile(trimmed);
    if (directFile.existsAsFile()) {
        return directFile;
    }

    auto files = presetDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& file : files) {
        if (auto xml = juce::XmlDocument::parse(file)) {
            if (xml->getStringAttribute("presetName") == trimmed) {
                return file;
            }
        }
    }
    return directFile;
}

juce::File PresetManager::getLastPresetFile() const {
    return presetDirectory.getChildFile("last_preset.txt");
}
