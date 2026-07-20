#pragma once

#include <JuceHeader.h>

class PresetManager {
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);

    // Validation sets an absolute directory before constructing the plug-in
    // editor so tests never touch the user's real preset directory. Passing an
    // empty File clears the override after a scoped test.
    static bool setStandaloneLifecycleTestDirectory(const juce::File& directory);
    static juce::File getStandaloneLifecycleTestDirectory();
    static juce::File getLastConstructedPresetDirectory();

    juce::StringArray getPresetNames() const;
    bool savePreset(const juce::String& name);
    bool loadPreset(const juce::String& name);
    bool presetExists(const juce::String& name) const;

    juce::String getLastPresetName() const;
    void setLastPresetName(const juce::String& name);
    juce::File getPresetDirectory() const;

private:
    juce::File getPresetFile(const juce::String& name) const;
    juce::File findPresetFile(const juce::String& name) const;
    juce::File getLastPresetFile() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::File presetDirectory;
};
