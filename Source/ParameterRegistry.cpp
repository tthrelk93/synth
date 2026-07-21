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
constexpr std::span<const std::string_view> noChoices;

constexpr auto choiceDescriptor (std::string_view id,
                                 std::string_view semanticKey,
                                 std::string_view displayName,
                                 float rangeEnd,
                                 float physicalDefault,
                                 std::span<const std::string_view> choices)
{
    return Descriptor { id, semanticKey, 0, displayName, "", UnitKey::none,
                        Kind::choice, 0.0f, rangeEnd, 1.0f, 1.0f, false,
                        physicalDefault, choices, MappingKey::indexedChoice, true,
                        SmoothingClass::none, PersistenceScope::apvtsState };
}

constexpr auto floatDescriptor (std::string_view id,
                                std::string_view semanticKey,
                                std::string_view displayName,
                                float physicalDefault)
{
    return Descriptor { id, semanticKey, 0, displayName, "", UnitKey::none,
                        Kind::floating, 0.0f, 1.0f, 0.01f, 1.0f, false,
                        physicalDefault, noChoices, MappingKey::linear, true,
                        SmoothingClass::none, PersistenceScope::apvtsState };
}

constexpr auto boolDescriptor (std::string_view id,
                               std::string_view semanticKey,
                               std::string_view displayName)
{
    return Descriptor { id, semanticKey, 0, displayName, "", UnitKey::none,
                        Kind::boolean, 0.0f, 1.0f, 1.0f, 1.0f, false,
                        0.0f, noChoices, MappingKey::boolean, true,
                        SmoothingClass::none, PersistenceScope::apvtsState };
}

constexpr std::array legacyDescriptors {
    choiceDescriptor ("osc1Waveform", "oscillator_1_waveform", "Oscillator 1 Waveform", 5.0f, 0.0f, oscillator12Waveforms),
    choiceDescriptor ("osc2Waveform", "oscillator_2_waveform", "Oscillator 2 Waveform", 5.0f, 0.0f, oscillator12Waveforms),
    choiceDescriptor ("osc3Waveform", "oscillator_3_waveform", "Oscillator 3 Waveform", 5.0f, 0.0f, oscillator3Waveforms),
    choiceDescriptor ("osc1Range", "oscillator_1_range", "Oscillator 1 Range", 5.0f, 2.0f, oscillatorRanges),
    choiceDescriptor ("osc2Range", "oscillator_2_range", "Oscillator 2 Range", 5.0f, 2.0f, oscillatorRanges),
    choiceDescriptor ("osc3Range", "oscillator_3_range", "Oscillator 3 Range", 5.0f, 2.0f, oscillatorRanges),
    choiceDescriptor ("osc1Vol", "oscillator_1_level", "Oscillator 1 Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("osc2Vol", "oscillator_2_level", "Oscillator 2 Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("osc3Vol", "oscillator_3_level", "Oscillator 3 Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("tune", "master_tune", "Tune", 10.0f, 0.0f, tuneValues),
    choiceDescriptor ("osc2Freq", "oscillator_2_frequency", "Oscillator 2 Frequency", 16.0f, 8.0f, oscillatorFrequencyValues),
    choiceDescriptor ("osc3Freq", "oscillator_3_frequency", "Oscillator 3 Frequency", 16.0f, 8.0f, oscillatorFrequencyValues),
    floatDescriptor ("filterCutoff", "filter_cutoff", "Filter Cutoff Frequency", 0.5f),
    choiceDescriptor ("filterEmphasis", "filter_emphasis", "Filter Emphasis", 10.0f, 0.0f, emphasisValues),
    choiceDescriptor ("filterContour", "filter_contour_amount", "Filter Contour", 10.0f, 0.0f, contourValues),
    choiceDescriptor ("outputVolKnob", "main_output_level", "Output Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("extInputVolKnob", "external_input_level", "Output Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("ctrlGlideKnob", "glide_time", "Output Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("ctrlModMixKnob", "modulation_mix", "Output Volume", 10.0f, 0.0f, volumeValues),
    floatDescriptor ("filterAttackTimeKnob", "filter_attack_time", "Filter Attack Time", 0.0f),
    floatDescriptor ("filterDecayTimeKnob", "filter_decay_time", "Filter Decay Time", 0.0f),
    floatDescriptor ("loudnessAttackTimeKnob", "loudness_attack_time", "Loudness Attack Time", 0.5f),
    floatDescriptor ("loudnessDecayTimeKnob", "loudness_decay_time", "Loudness Decay Time", 0.5f),
    choiceDescriptor ("filterSustainKnob", "filter_sustain_level", "Filter Sustain", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("noiseVolKnob", "noise_level", "Noise Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("loudnessSustainLevelKnob", "loudness_sustain_level", "Output Volume", 10.0f, 0.0f, volumeValues),
    choiceDescriptor ("outputPhonesVolKnob", "phones_output_level", "Output Volume", 10.0f, 0.0f, volumeValues),
    floatDescriptor ("feedbackKnob", "feedback_amount", "Feedback Knob", 0.0f),
    floatDescriptor ("modWheelValue", "modulation_wheel", "Mod Wheel Value", 0.0f),
    floatDescriptor ("pitchWheelValue", "pitch_wheel", "Pitch Wheel Value", 0.5f),
    boolDescriptor ("osc1OnOff", "oscillator_1_enabled", "Oscillator 1 On/Off"),
    boolDescriptor ("osc2OnOff", "oscillator_2_enabled", "Oscillator 2 On/Off"),
    boolDescriptor ("osc3OnOff", "oscillator_3_enabled", "Oscillator 3 On/Off"),
    boolDescriptor ("a440HzOnOff", "a440_enabled", "A440Hz On/Off"),
    boolDescriptor ("osc3CtrlMode", "oscillator_3_control_mode", "Oscillator 3 Control Mode"),
    boolDescriptor ("oscModSwitch", "oscillator_modulation_enabled", "Oscillator Mod Switch"),
    boolDescriptor ("noiseOnOffSwitch", "noise_enabled", "Noise On/Off"),
    boolDescriptor ("extInputVolSwitch", "external_input_enabled", "External Input On/Off"),
    boolDescriptor ("whitePinkSwitch", "noise_color", "White / Pink"),
    boolDescriptor ("filterModSwitch", "filter_modulation_enabled", "Filter Mod Switch"),
    boolDescriptor ("keyboardCtrlSwitch1", "keyboard_control_1_enabled", "Keyboard Control Switch 1"),
    boolDescriptor ("keyboardCtrlSwitch2", "keyboard_control_2_enabled", "Keyboard Control Switch 1"),
    boolDescriptor ("decaySwitch", "decay_enabled", "Decay Switch"),
    boolDescriptor ("glideSwitch", "glide_enabled", "Glide Switch")
};

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
    return legacyDescriptors;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.reserve (legacyDescriptors.size());

    for (const auto& descriptor : legacyDescriptors)
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
