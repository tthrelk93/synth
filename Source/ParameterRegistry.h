#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace ParameterRegistry
{
enum class Key : std::uint8_t
{
    osc1Waveform, osc2Waveform, osc3Waveform,
    osc1Range, osc2Range, osc3Range,
    osc1Vol, osc2Vol, osc3Vol, tune, osc2Freq, osc3Freq,
    filterCutoff, filterEmphasis, filterContour, outputVolKnob,
    extInputVolKnob, ctrlGlideKnob, ctrlModMixKnob,
    filterAttackTimeKnob, filterDecayTimeKnob,
    loudnessAttackTimeKnob, loudnessDecayTimeKnob,
    filterSustainKnob, noiseVolKnob, loudnessSustainLevelKnob,
    outputPhonesVolKnob, feedbackKnob, modWheelValue, pitchWheelValue,
    osc1OnOff, osc2OnOff, osc3OnOff, a440HzOnOff, osc3CtrlMode,
    oscModSwitch, noiseOnOffSwitch, extInputVolSwitch, whitePinkSwitch,
    filterModSwitch, keyboardCtrlSwitch1, keyboardCtrlSwitch2,
    decaySwitch, glideSwitch, keyboardPriorityMode, keyboardTriggerMode,
    outputMainEnabled, outputPhonesEnabled, count
};

constexpr std::size_t index (Key key) noexcept
{
    return static_cast<std::size_t> (key);
}

inline constexpr std::size_t parameterCount = index (Key::count);
static_assert (parameterCount == 48);

enum class Kind
{
    choice,
    floating,
    boolean
};

enum class UnitKey
{
    unspecified,
    none,
    choice,
    semitones,
    panelIndex,
    normalized,
    boolean
};

enum class MappingKey
{
    unspecified,
    indexedChoice,
    linear,
    boolean
};

enum class SmoothingClass
{
    unspecified,
    none,
    gainControl,
    control,
    dedicatedPitch,
    dedicatedCutoff,
    dedicatedGlide,
    contourStage
};

enum class PersistenceScope
{
    unspecified,
    apvtsState
};

struct Descriptor
{
    std::string_view id;
    std::string_view semanticKey;
    int versionHint;
    std::string_view displayName;
    std::string_view shortLabel;
    UnitKey unitKey;
    Kind kind;
    float rangeStart;
    float rangeEnd;
    float rangeInterval;
    float rangeSkew;
    bool symmetricSkew;
    float physicalDefault;
    std::span<const std::string_view> choiceValues;
    MappingKey mapping;
    bool automatable;
    SmoothingClass smoothing;
    PersistenceScope persistence;
};

std::span<const Descriptor> descriptors() noexcept;
const Descriptor& descriptor (Key key) noexcept;
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
