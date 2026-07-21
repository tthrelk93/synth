#pragma once

#include <JuceHeader.h>

#include <span>
#include <string_view>

namespace ParameterRegistry
{
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
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
