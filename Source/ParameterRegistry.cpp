#include "ParameterRegistry.h"

#include <array>
#include <memory>
#include <vector>

namespace ParameterRegistry
{
namespace
{
using namespace std::literals;

constexpr std::array oscillator12Waveforms {
    "Triangle"sv, "Sharktooth"sv, "Sawtooth"sv,
    "Square"sv, "WideRectangle"sv, "NarrowRectangle"sv
};
constexpr std::array oscillator3Waveforms {
    "Triangle"sv, "ReverseSaw"sv, "Sawtooth"sv,
    "Square"sv, "WideRectangle"sv, "NarrowRectangle"sv
};
constexpr std::array oscillatorRanges {
    "LO"sv, "ThirtyTwo"sv, "Sixteen"sv, "Eight"sv, "Four"sv, "Two"sv
};
constexpr std::array volumeValues {
    "VolZero"sv, "VolOne"sv, "VolTwo"sv, "VolThree"sv, "VolFour"sv,
    "VolFive"sv, "VolSix"sv, "VolSeven"sv, "VolEight"sv, "VolNine"sv,
    "VolTen"sv
};
constexpr std::array tuneValues {
    "NegTwoHalf"sv, "NegTwo"sv, "NegOneHalf"sv, "NegOne"sv, "NegHalf"sv,
    "Zero"sv, "PosHalf"sv, "PosOne"sv, "PosOneHalf"sv, "PosTwo"sv,
    "PosTwoHalf"sv
};
constexpr std::array oscillatorFrequencyValues {
    "FreqNegEight"sv, "FreqNegSeven"sv, "FreqNegSix"sv, "FreqNegFive"sv,
    "FreqNegFour"sv, "FreqNegThree"sv, "FreqNegTwo"sv, "FreqNegOne"sv,
    "FreqZero"sv, "FreqPosOne"sv, "FreqPosTwo"sv, "FreqPosThree"sv,
    "FreqPosFour"sv, "FreqPosFive"sv, "FreqPosSix"sv, "FreqPosSeven"sv,
    "FreqPosEight"sv
};
constexpr std::array emphasisValues {
    "EmphZero"sv, "EmphOne"sv, "EmphTwo"sv, "EmphThree"sv, "EmphFour"sv,
    "EmphFive"sv, "EmphSix"sv, "EmphSeven"sv, "EmphEight"sv, "EmphNine"sv,
    "EmphTen"sv
};
constexpr std::array contourValues {
    "ContourZero"sv, "ContourOne"sv, "ContourTwo"sv, "ContourThree"sv,
    "ContourFour"sv, "ContourFive"sv, "ContourSix"sv, "ContourSeven"sv,
    "ContourEight"sv, "ContourNine"sv, "ContourTen"sv
};
constexpr std::array priorityValues { "Low"sv, "High"sv, "Last"sv };
constexpr std::array triggerValues { "Single"sv, "Multi"sv };
constexpr std::span<const std::string_view> noChoices;

constexpr auto choiceDescriptor (std::string_view id,
                                 std::string_view semanticKey,
                                 std::string_view displayName,
                                 float rangeEnd,
                                 float physicalDefault,
                                 std::span<const std::string_view> choices,
                                 UnitKey unitKey,
                                 SmoothingClass smoothing,
                                 int versionHint)
{
    return Descriptor { id, semanticKey, versionHint, displayName, "", unitKey,
                        Kind::choice, 0.0f, rangeEnd, 1.0f, 1.0f, false,
                        physicalDefault, choices, MappingKey::indexedChoice, true,
                        smoothing, PersistenceScope::apvtsState };
}

constexpr auto floatDescriptor (std::string_view id,
                                std::string_view semanticKey,
                                std::string_view displayName,
                                float physicalDefault,
                                SmoothingClass smoothing,
                                int versionHint)
{
    return Descriptor { id, semanticKey, versionHint, displayName, "", UnitKey::normalized,
                        Kind::floating, 0.0f, 1.0f, 0.01f, 1.0f, false,
                        physicalDefault, noChoices, MappingKey::linear, true,
                        smoothing, PersistenceScope::apvtsState };
}

constexpr auto boolDescriptor (std::string_view id,
                               std::string_view semanticKey,
                               std::string_view displayName,
                               float physicalDefault,
                               UnitKey unitKey,
                               int versionHint)
{
    return Descriptor { id, semanticKey, versionHint, displayName, "", unitKey,
                        Kind::boolean, 0.0f, 1.0f, 1.0f, 1.0f, false,
                        physicalDefault, noChoices, MappingKey::boolean, true,
                        SmoothingClass::none, PersistenceScope::apvtsState };
}

constexpr std::array registryDescriptors {
    choiceDescriptor ("osc1Waveform", "oscillator_1_waveform", "Oscillator 1 Waveform", 5.0f, 0.0f, oscillator12Waveforms, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc2Waveform", "oscillator_2_waveform", "Oscillator 2 Waveform", 5.0f, 0.0f, oscillator12Waveforms, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc3Waveform", "oscillator_3_waveform", "Oscillator 3 Waveform", 5.0f, 0.0f, oscillator3Waveforms, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc1Range", "oscillator_1_range", "Oscillator 1 Range", 5.0f, 2.0f, oscillatorRanges, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc2Range", "oscillator_2_range", "Oscillator 2 Range", 5.0f, 2.0f, oscillatorRanges, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc3Range", "oscillator_3_range", "Oscillator 3 Range", 5.0f, 2.0f, oscillatorRanges, UnitKey::choice, SmoothingClass::none, 0),
    choiceDescriptor ("osc1Vol", "oscillator_1_level", "Oscillator 1 Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("osc2Vol", "oscillator_2_level", "Oscillator 2 Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("osc3Vol", "oscillator_3_level", "Oscillator 3 Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("tune", "master_tune", "Master Tune", 10.0f, 5.0f, tuneValues, UnitKey::semitones, SmoothingClass::none, 0),
    choiceDescriptor ("osc2Freq", "oscillator_2_frequency", "Oscillator 2 Frequency Offset", 16.0f, 8.0f, oscillatorFrequencyValues, UnitKey::semitones, SmoothingClass::none, 0),
    choiceDescriptor ("osc3Freq", "oscillator_3_frequency", "Oscillator 3 Frequency Offset", 16.0f, 8.0f, oscillatorFrequencyValues, UnitKey::semitones, SmoothingClass::none, 0),
    floatDescriptor ("filterCutoff", "filter_cutoff", "Filter Cutoff", 0.5f, SmoothingClass::dedicatedCutoff, 0),
    choiceDescriptor ("filterEmphasis", "filter_emphasis", "Filter Emphasis", 10.0f, 0.0f, emphasisValues, UnitKey::panelIndex, SmoothingClass::control, 0),
    choiceDescriptor ("filterContour", "filter_contour_amount", "Filter Amount of Contour", 10.0f, 0.0f, contourValues, UnitKey::panelIndex, SmoothingClass::control, 0),
    choiceDescriptor ("outputVolKnob", "main_output_level", "Main Output Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("extInputVolKnob", "external_input_level", "External Input Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("ctrlGlideKnob", "glide_time", "Glide Time", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::dedicatedGlide, 0),
    choiceDescriptor ("ctrlModMixKnob", "modulation_mix", "Modulation Mix", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::control, 0),
    floatDescriptor ("filterAttackTimeKnob", "filter_attack_time", "Filter Contour Attack Time", 0.0f, SmoothingClass::contourStage, 0),
    floatDescriptor ("filterDecayTimeKnob", "filter_decay_time", "Filter Contour Decay Time", 0.0f, SmoothingClass::contourStage, 0),
    floatDescriptor ("loudnessAttackTimeKnob", "loudness_attack_time", "Loudness Contour Attack Time", 0.5f, SmoothingClass::contourStage, 0),
    floatDescriptor ("loudnessDecayTimeKnob", "loudness_decay_time", "Loudness Contour Decay Time", 0.5f, SmoothingClass::contourStage, 0),
    choiceDescriptor ("filterSustainKnob", "filter_sustain_level", "Filter Contour Sustain Level", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::contourStage, 0),
    choiceDescriptor ("noiseVolKnob", "noise_level", "Noise Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    choiceDescriptor ("loudnessSustainLevelKnob", "loudness_sustain_level", "Loudness Contour Sustain Level", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::contourStage, 0),
    choiceDescriptor ("outputPhonesVolKnob", "phones_output_level", "Phones Volume", 10.0f, 0.0f, volumeValues, UnitKey::panelIndex, SmoothingClass::gainControl, 0),
    floatDescriptor ("feedbackKnob", "feedback_amount", "Feedback Amount", 0.0f, SmoothingClass::control, 0),
    floatDescriptor ("modWheelValue", "modulation_wheel", "Modulation Wheel", 0.0f, SmoothingClass::control, 0),
    floatDescriptor ("pitchWheelValue", "pitch_wheel", "Pitch Wheel", 0.5f, SmoothingClass::dedicatedPitch, 0),
    boolDescriptor ("osc1OnOff", "oscillator_1_enabled", "Oscillator 1 Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("osc2OnOff", "oscillator_2_enabled", "Oscillator 2 Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("osc3OnOff", "oscillator_3_enabled", "Oscillator 3 Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("a440HzOnOff", "a440_enabled", "A-440 Tuner Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("osc3CtrlMode", "oscillator_3_control_mode", "Oscillator 3 Keyboard Control", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("oscModSwitch", "oscillator_modulation_enabled", "Oscillator Modulation Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("noiseOnOffSwitch", "noise_enabled", "Noise Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("extInputVolSwitch", "external_input_enabled", "External Input Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("whitePinkSwitch", "noise_color", "Noise Color", 0.0f, UnitKey::choice, 0),
    boolDescriptor ("filterModSwitch", "filter_modulation_enabled", "Filter Modulation Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("keyboardCtrlSwitch1", "keyboard_control_1_enabled", "Keyboard Control Switch 1", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("keyboardCtrlSwitch2", "keyboard_control_2_enabled", "Keyboard Control Switch 2", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("decaySwitch", "decay_enabled", "Decay Enabled", 0.0f, UnitKey::boolean, 0),
    boolDescriptor ("glideSwitch", "glide_enabled", "Glide Enabled", 0.0f, UnitKey::boolean, 0),
    choiceDescriptor ("keyboard.priorityMode", "keyboard_priority_mode", "Note Priority", 2.0f, 0.0f, priorityValues, UnitKey::choice, SmoothingClass::none, 1),
    choiceDescriptor ("keyboard.triggerMode", "keyboard_trigger_mode", "Trigger Mode", 1.0f, 0.0f, triggerValues, UnitKey::choice, SmoothingClass::none, 1),
    boolDescriptor ("output.mainEnabled", "main_output_enabled", "Main Output Enabled", 1.0f, UnitKey::boolean, 1),
    boolDescriptor ("output.phonesEnabled", "phones_output_enabled", "Phones Output Enabled", 1.0f, UnitKey::boolean, 1)
};
static_assert (registryDescriptors.size() == parameterCount);

juce::StringArray makeChoices (std::span<const std::string_view> values)
{
    juce::StringArray result;
    result.ensureStorageAllocated (static_cast<int> (values.size()));
    for (const auto value : values)
        result.add (juce::String (value.data(), value.size()));
    return result;
}

juce::ParameterID makeParameterID (const Descriptor& descriptor)
{
    return { juce::String (descriptor.id.data(), descriptor.id.size()),
             descriptor.versionHint };
}

juce::String makeString (std::string_view value)
{
    return juce::String (value.data(), value.size());
}
}

std::span<const Descriptor> descriptors() noexcept
{
    return registryDescriptors;
}

const Descriptor& descriptor (Key key) noexcept
{
    jassert (index (key) < registryDescriptors.size());
    return registryDescriptors[index (key)];
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.reserve (registryDescriptors.size());

    for (const auto& descriptor : registryDescriptors)
    {
        const auto id = makeParameterID (descriptor);
        const auto name = makeString (descriptor.displayName);
        const auto label = makeString (descriptor.shortLabel);

        switch (descriptor.kind)
        {
            case Kind::choice:
                parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
                    id, name, makeChoices (descriptor.choiceValues),
                    juce::roundToInt (descriptor.physicalDefault),
                    juce::AudioParameterChoiceAttributes {}
                        .withLabel (label)
                        .withAutomatable (descriptor.automatable)));
                break;

            case Kind::floating:
                parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
                    id, name,
                    juce::NormalisableRange<float> { descriptor.rangeStart,
                                                     descriptor.rangeEnd,
                                                     descriptor.rangeInterval,
                                                     descriptor.rangeSkew,
                                                     descriptor.symmetricSkew },
                    descriptor.physicalDefault,
                    juce::AudioParameterFloatAttributes {}
                        .withLabel (label)
                        .withAutomatable (descriptor.automatable)));
                break;

            case Kind::boolean:
                parameters.push_back (std::make_unique<juce::AudioParameterBool> (
                    id, name, descriptor.physicalDefault >= 0.5f,
                    juce::AudioParameterBoolAttributes {}
                        .withLabel (label)
                        .withAutomatable (descriptor.automatable)));
                break;
        }
    }

    return { parameters.begin(), parameters.end() };
}
}
