#include "PluginProcessor.h"
#include "PresetManager.h"

#include "ParameterRegistry.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#define SYNTH_STRINGIFY_IMPL(value) #value
#define SYNTH_STRINGIFY(value) SYNTH_STRINGIFY_IMPL(value)

namespace
{
using namespace std::literals;

class TestContext
{
public:
    void expect (bool condition, std::string_view message)
    {
        if (condition)
            return;

        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    int result() const
    {
        if (failures == 0)
            std::cout << "Model D processor contract passed\n";
        else
            std::cerr << failures << " processor contract assertion(s) failed\n";

        return failures == 0 ? 0 : 1;
    }

private:
    int failures = 0;
};

using ChannelSet = juce::AudioChannelSet;
using BusesLayout = juce::AudioProcessor::BusesLayout;

BusesLayout makeLayout (const ChannelSet& mainOutput,
                        const ChannelSet& externalInput,
                        const ChannelSet& phonesOutput)
{
    BusesLayout layout;
    layout.inputBuses.add (externalInput);
    layout.outputBuses.add (mainOutput);
    layout.outputBuses.add (phonesOutput);
    return layout;
}

std::string channelSetName (const ChannelSet& set)
{
    if (set == ChannelSet::disabled())
        return "disabled";
    if (set == ChannelSet::mono())
        return "mono";
    if (set == ChannelSet::stereo())
        return "stereo";

    return set.getDescription().toStdString();
}

void setParameter (MoogMiniAudioProcessor& processor,
                   const juce::String& parameterId,
                   float normalisedValue,
                   TestContext& test)
{
    auto* parameter = processor.apvts.getParameter (parameterId);
    test.expect (parameter != nullptr,
                 "existing parameter must be available: " + parameterId.toStdString());

    if (parameter != nullptr)
        parameter->setValueNotifyingHost (normalisedValue);
}

bool isFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

double absoluteSum (const juce::AudioBuffer<float>& buffer)
{
    double result = 0.0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            result += std::abs (buffer.getSample (channel, sample));

    return result;
}

juce::File legacyFixtureFile (const juce::String& relativePath)
{
    return juce::File { SYNTH_SOURCE_ROOT }.getChildFile (relativePath);
}

bool equalsStringView (const juce::String& actual, std::string_view expected)
{
    return std::string_view { actual.toRawUTF8() } == expected;
}

constexpr std::array legacyParameterIds {
    "osc1Waveform"sv, "osc2Waveform"sv, "osc3Waveform"sv,
    "osc1Range"sv, "osc2Range"sv, "osc3Range"sv,
    "osc1Vol"sv, "osc2Vol"sv, "osc3Vol"sv,
    "tune"sv, "osc2Freq"sv, "osc3Freq"sv,
    "filterCutoff"sv, "filterEmphasis"sv, "filterContour"sv,
    "outputVolKnob"sv, "extInputVolKnob"sv, "ctrlGlideKnob"sv,
    "ctrlModMixKnob"sv, "filterAttackTimeKnob"sv,
    "filterDecayTimeKnob"sv, "loudnessAttackTimeKnob"sv,
    "loudnessDecayTimeKnob"sv, "filterSustainKnob"sv,
    "noiseVolKnob"sv, "loudnessSustainLevelKnob"sv,
    "outputPhonesVolKnob"sv, "feedbackKnob"sv, "modWheelValue"sv,
    "pitchWheelValue"sv, "osc1OnOff"sv, "osc2OnOff"sv,
    "osc3OnOff"sv, "a440HzOnOff"sv, "osc3CtrlMode"sv,
    "oscModSwitch"sv, "noiseOnOffSwitch"sv, "extInputVolSwitch"sv,
    "whitePinkSwitch"sv, "filterModSwitch"sv, "keyboardCtrlSwitch1"sv,
    "keyboardCtrlSwitch2"sv, "decaySwitch"sv, "glideSwitch"sv
};

constexpr std::array newParameterIds {
    "keyboard.priorityMode"sv,
    "keyboard.triggerMode"sv,
    "output.mainEnabled"sv,
    "output.phonesEnabled"sv
};

constexpr auto correctedDisplayNames = std::to_array<std::pair<std::string_view,
                                                                std::string_view>> ({
    { "tune"sv, "Master Tune"sv },
    { "osc2Freq"sv, "Oscillator 2 Frequency Offset"sv },
    { "osc3Freq"sv, "Oscillator 3 Frequency Offset"sv },
    { "filterCutoff"sv, "Filter Cutoff"sv },
    { "filterContour"sv, "Filter Amount of Contour"sv },
    { "outputVolKnob"sv, "Main Output Volume"sv },
    { "extInputVolKnob"sv, "External Input Volume"sv },
    { "ctrlGlideKnob"sv, "Glide Time"sv },
    { "ctrlModMixKnob"sv, "Modulation Mix"sv },
    { "filterAttackTimeKnob"sv, "Filter Contour Attack Time"sv },
    { "filterDecayTimeKnob"sv, "Filter Contour Decay Time"sv },
    { "loudnessAttackTimeKnob"sv, "Loudness Contour Attack Time"sv },
    { "loudnessDecayTimeKnob"sv, "Loudness Contour Decay Time"sv },
    { "filterSustainKnob"sv, "Filter Contour Sustain Level"sv },
    { "loudnessSustainLevelKnob"sv, "Loudness Contour Sustain Level"sv },
    { "outputPhonesVolKnob"sv, "Phones Volume"sv },
    { "feedbackKnob"sv, "Feedback Amount"sv },
    { "modWheelValue"sv, "Modulation Wheel"sv },
    { "pitchWheelValue"sv, "Pitch Wheel"sv },
    { "osc1OnOff"sv, "Oscillator 1 Enabled"sv },
    { "osc2OnOff"sv, "Oscillator 2 Enabled"sv },
    { "osc3OnOff"sv, "Oscillator 3 Enabled"sv },
    { "a440HzOnOff"sv, "A-440 Tuner Enabled"sv },
    { "osc3CtrlMode"sv, "Oscillator 3 Keyboard Control"sv },
    { "oscModSwitch"sv, "Oscillator Modulation Enabled"sv },
    { "noiseOnOffSwitch"sv, "Noise Enabled"sv },
    { "extInputVolSwitch"sv, "External Input Enabled"sv },
    { "whitePinkSwitch"sv, "Noise Color"sv },
    { "filterModSwitch"sv, "Filter Modulation Enabled"sv },
    { "keyboardCtrlSwitch1"sv, "Keyboard Control Switch 1"sv },
    { "keyboardCtrlSwitch2"sv, "Keyboard Control Switch 2"sv },
    { "decaySwitch"sv, "Decay Enabled"sv },
    { "glideSwitch"sv, "Glide Enabled"sv }
});

template <std::size_t Size>
bool contains (const std::array<std::string_view, Size>& values, std::string_view candidate)
{
    return std::find (values.begin(), values.end(), candidate) != values.end();
}

const ParameterRegistry::Descriptor* findDescriptor (
    std::span<const ParameterRegistry::Descriptor> descriptors,
    std::string_view id)
{
    const auto found = std::find_if (descriptors.begin(), descriptors.end(), [id] (const auto& descriptor)
    {
        return descriptor.id == id;
    });
    return found == descriptors.end() ? nullptr : &*found;
}

std::string_view kindName (ParameterRegistry::Kind kind)
{
    switch (kind)
    {
        case ParameterRegistry::Kind::choice: return "choice";
        case ParameterRegistry::Kind::floating: return "float";
        case ParameterRegistry::Kind::boolean: return "bool";
    }

    return "invalid";
}

std::string_view unitKeyName (ParameterRegistry::UnitKey unitKey)
{
    switch (unitKey)
    {
        case ParameterRegistry::UnitKey::unspecified: return "unspecified";
        case ParameterRegistry::UnitKey::none: return "none";
        case ParameterRegistry::UnitKey::choice: return "choice";
        case ParameterRegistry::UnitKey::semitones: return "semitones";
        case ParameterRegistry::UnitKey::panelIndex: return "panelIndex";
        case ParameterRegistry::UnitKey::normalized: return "normalized";
        case ParameterRegistry::UnitKey::boolean: return "boolean";
    }

    return "invalid";
}

std::string_view mappingKeyName (ParameterRegistry::MappingKey mapping)
{
    switch (mapping)
    {
        case ParameterRegistry::MappingKey::unspecified: return "unspecified";
        case ParameterRegistry::MappingKey::indexedChoice: return "indexedChoice";
        case ParameterRegistry::MappingKey::linear: return "linear";
        case ParameterRegistry::MappingKey::boolean: return "boolean";
    }

    return "invalid";
}

std::string_view smoothingClassName (ParameterRegistry::SmoothingClass smoothing)
{
    switch (smoothing)
    {
        case ParameterRegistry::SmoothingClass::unspecified: return "unspecified";
        case ParameterRegistry::SmoothingClass::none: return "none";
        case ParameterRegistry::SmoothingClass::gainControl: return "gainControl";
        case ParameterRegistry::SmoothingClass::control: return "control";
        case ParameterRegistry::SmoothingClass::dedicatedPitch: return "dedicatedPitch";
        case ParameterRegistry::SmoothingClass::dedicatedCutoff: return "dedicatedCutoff";
        case ParameterRegistry::SmoothingClass::dedicatedGlide: return "dedicatedGlide";
        case ParameterRegistry::SmoothingClass::contourStage: return "contourStage";
    }

    return "invalid";
}

std::string_view persistenceScopeName (ParameterRegistry::PersistenceScope persistence)
{
    switch (persistence)
    {
        case ParameterRegistry::PersistenceScope::unspecified: return "unspecified";
        case ParameterRegistry::PersistenceScope::apvtsState: return "apvtsState";
    }

    return "invalid";
}

juce::String makeRegistryV2Export()
{
    auto registry = juce::DynamicObject::Ptr { new juce::DynamicObject };
    registry->setProperty ("schema", "model-d.parameter-registry.v2");

    juce::Array<juce::var> parameters;
    const auto descriptors = ParameterRegistry::descriptors();
    parameters.ensureStorageAllocated (static_cast<int> (descriptors.size()));

    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto& descriptor = descriptors[index];
        auto entry = juce::DynamicObject::Ptr { new juce::DynamicObject };
        entry->setProperty ("index", static_cast<int> (index));
        entry->setProperty ("id", juce::String { descriptor.id.data(), descriptor.id.size() });
        entry->setProperty ("semantic_key", juce::String { descriptor.semanticKey.data(), descriptor.semanticKey.size() });
        entry->setProperty ("version_hint", descriptor.versionHint);
        entry->setProperty ("display_name", juce::String { descriptor.displayName.data(), descriptor.displayName.size() });
        entry->setProperty ("short_label", juce::String { descriptor.shortLabel.data(), descriptor.shortLabel.size() });
        entry->setProperty ("unit_key", juce::String { unitKeyName (descriptor.unitKey).data(), unitKeyName (descriptor.unitKey).size() });
        entry->setProperty ("kind", juce::String { kindName (descriptor.kind).data(), kindName (descriptor.kind).size() });
        entry->setProperty ("range_start", descriptor.rangeStart);
        entry->setProperty ("range_end", descriptor.rangeEnd);
        entry->setProperty ("range_interval", descriptor.rangeInterval);
        entry->setProperty ("range_skew", descriptor.rangeSkew);
        entry->setProperty ("symmetric_skew", descriptor.symmetricSkew);
        entry->setProperty ("physical_default", descriptor.physicalDefault);

        juce::Array<juce::var> choices;
        choices.ensureStorageAllocated (static_cast<int> (descriptor.choiceValues.size()));
        for (const auto choice : descriptor.choiceValues)
            choices.add (juce::String { choice.data(), choice.size() });
        entry->setProperty ("choice_values", juce::var { choices });

        entry->setProperty ("mapping_key", juce::String { mappingKeyName (descriptor.mapping).data(), mappingKeyName (descriptor.mapping).size() });
        entry->setProperty ("automatable", descriptor.automatable);
        entry->setProperty ("smoothing_class", juce::String { smoothingClassName (descriptor.smoothing).data(), smoothingClassName (descriptor.smoothing).size() });
        entry->setProperty ("persistence_scope", juce::String { persistenceScopeName (descriptor.persistence).data(), persistenceScopeName (descriptor.persistence).size() });
        parameters.add (juce::var { entry.get() });
    }

    registry->setProperty ("parameters", juce::var { parameters });
    return juce::JSON::toString (juce::var { registry.get() },
                                 juce::JSON::FormatOptions {}
                                     .withSpacing (juce::JSON::Spacing::none))
        + "\n";
}

bool registryFixtureMatchesExport (const juce::File& fixture,
                                   const juce::String& expectedExport)
{
    juce::MemoryBlock actualBytes;
    if (! fixture.loadFileAsData (actualBytes))
        return false;

    const juce::MemoryBlock expectedBytes { expectedExport.toRawUTF8(),
                                            expectedExport.getNumBytesAsUTF8() };
    return actualBytes == expectedBytes;
}

juce::var makeLegacyParameterInventory (MoogMiniAudioProcessor& processor)
{
    auto inventory = juce::DynamicObject::Ptr { new juce::DynamicObject };
    inventory->setProperty ("schema", "model-d.legacy-parameter-inventory.v1");

    juce::Array<juce::var> parameters;
    const auto liveParameters = processor.getParameters();
    parameters.ensureStorageAllocated (liveParameters.size());

    for (int index = 0; index < liveParameters.size(); ++index)
    {
        const auto* parameter = dynamic_cast<const juce::RangedAudioParameter*> (liveParameters[index]);
        if (parameter == nullptr)
            continue;

        auto entry = juce::DynamicObject::Ptr { new juce::DynamicObject };
        const auto& range = parameter->getNormalisableRange();
        entry->setProperty ("index", index);
        entry->setProperty ("id", parameter->getParameterID());
        entry->setProperty ("version_hint", parameter->getVersionHint());
        entry->setProperty ("name", parameter->getName (256));
        entry->setProperty ("unit", parameter->getLabel());
        entry->setProperty ("range_start", range.start);
        entry->setProperty ("range_end", range.end);
        entry->setProperty ("range_interval", range.interval);
        entry->setProperty ("range_skew", range.skew);
        entry->setProperty ("range_symmetric_skew", range.symmetricSkew);
        entry->setProperty ("normalized_default", parameter->getDefaultValue());
        entry->setProperty ("physical_default",
                            parameter->convertFrom0to1 (parameter->getDefaultValue()));
        entry->setProperty ("automatable", parameter->isAutomatable());
        entry->setProperty ("discrete", parameter->isDiscrete());
        entry->setProperty ("boolean", parameter->isBoolean());
        entry->setProperty ("meta", parameter->isMetaParameter());

        if (const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (parameter))
        {
            entry->setProperty ("type", "choice");
            juce::Array<juce::var> choices;
            choices.ensureStorageAllocated (choice->choices.size());
            for (const auto& value : choice->choices)
                choices.add (value);
            entry->setProperty ("choices", juce::var { choices });
        }
        else if (dynamic_cast<const juce::AudioParameterFloat*> (parameter) != nullptr)
        {
            entry->setProperty ("type", "float");
        }
        else if (dynamic_cast<const juce::AudioParameterBool*> (parameter) != nullptr)
        {
            entry->setProperty ("type", "bool");
        }
        else
        {
            entry->setProperty ("type", "unsupported");
        }

        parameters.add (juce::var { entry.get() });
    }

    inventory->setProperty ("parameters", juce::var { parameters });
    return juce::var { inventory.get() };
}

juce::String serialiseLegacyParameterInventory (MoogMiniAudioProcessor& processor)
{
    return juce::JSON::toString (makeLegacyParameterInventory (processor),
                                 juce::JSON::FormatOptions {}
                                     .withSpacing (juce::JSON::Spacing::none))
        + "\n";
}

std::unique_ptr<juce::XmlElement> serialiseProcessorState (MoogMiniAudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    return std::unique_ptr<juce::XmlElement> (
        MoogMiniAudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())));
}

juce::String canonicaliseXmlFixture (const juce::XmlElement& state)
{
    return state.toString().replace ("\r\n", "\n");
}

juce::MemoryBlock binaryFromXml (const juce::XmlElement& xml)
{
    juce::MemoryBlock result;
    MoogMiniAudioProcessor::copyXmlToBinary (xml, result);
    return result;
}

juce::MemoryBlock serialiseProcessorStateBytes (MoogMiniAudioProcessor& processor)
{
    juce::MemoryBlock result;
    processor.getStateInformation (result);
    return result;
}

juce::XmlElement* findParameterXml (juce::XmlElement& root, std::string_view id)
{
    auto* parameters = root.hasTagName ("parameters")
                         ? &root : root.getChildByName ("parameters");
    if (parameters == nullptr)
        return nullptr;
    for (auto* child = parameters->getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
        if (child->hasTagName ("PARAM")
            && equalsStringView (child->getStringAttribute ("id"), id))
            return child;
    return nullptr;
}

float physicalParameterValue (MoogMiniAudioProcessor& processor, std::string_view id)
{
    const auto parameterId = juce::String { id.data(), id.size() };
    const auto* value = processor.apvts.getRawParameterValue (parameterId);
    return value == nullptr ? std::numeric_limits<float>::quiet_NaN() : value->load();
}

bool fixtureMatchesXml (const juce::File& fixture, const juce::XmlElement& xml)
{
    juce::MemoryBlock expected;
    if (! fixture.loadFileAsData (expected))
        return false;
    const auto actualText = canonicaliseXmlFixture (xml);
    const juce::MemoryBlock actual { actualText.toRawUTF8(), actualText.getNumBytesAsUTF8() };
    return actual == expected;
}

std::unique_ptr<juce::XmlElement> copyStateXml (const juce::XmlElement& xml);

void setRepresentativeParameterState (MoogMiniAudioProcessor& processor)
{
    for (auto* parameter : processor.getParameters())
    {
        const auto candidate = parameter->getDefaultValue() == 0.75f ? 0.25f : 0.75f;
        parameter->setValueNotifyingHost (candidate);
    }
}

void expectFixtureHash (TestContext& test,
                        const juce::File& file,
                        std::string_view expectedHash,
                        std::string_view description)
{
    const auto actualHash = juce::SHA256 { file }.toHexString();
    test.expect (equalsStringView (actualHash, expectedHash),
                 std::string { description } + " SHA-256 must remain unchanged");
}

void expectLegacyStateFixture (TestContext& test,
                               const juce::File& file,
                               std::string_view expectedHash,
                               std::string_view description)
{
    expectFixtureHash (test, file, expectedHash, description);
    const auto state = juce::parseXML (file);
    test.expect (state != nullptr, std::string { description } + " must be valid XML");
    if (state == nullptr)
        return;

    test.expect (state->hasTagName ("Parameters"),
                 std::string { description } + " must retain the unversioned Parameters root");
    test.expect (state->getNumAttributes() == 0,
                 std::string { description } + " root must remain unversioned");
    test.expect (state->getNumChildElements() == static_cast<int> (legacyParameterIds.size()),
                 std::string { description } + " must contain exactly 44 parameter values");

    std::vector<std::string_view> expectedStateOrder (legacyParameterIds.begin(),
                                                      legacyParameterIds.end());
    std::sort (expectedStateOrder.begin(), expectedStateOrder.end());
    std::set<std::string> observedIds;
    int index = 0;
    for (const auto* child = state->getFirstChildElement();
         child != nullptr;
         child = child->getNextElement(), ++index)
    {
        test.expect (child->hasTagName ("PARAM"),
                     std::string { description } + " children must all be PARAM elements");
        test.expect (child->hasAttribute ("id") && child->hasAttribute ("value"),
                     std::string { description } + " PARAM elements must contain id/value attributes");
        const auto id = child->getStringAttribute ("id");
        test.expect (observedIds.emplace (id.toStdString()).second,
                     std::string { description } + " parameter IDs must be unique");
        if (static_cast<std::size_t> (index) < expectedStateOrder.size())
            test.expect (equalsStringView (id, expectedStateOrder[static_cast<std::size_t> (index)]),
                         std::string { description } + " parameter ID order must remain exact");
    }
}

void testLegacyParameterFixtures (TestContext& test)
{
    const auto inventoryFile = legacyFixtureFile ("Tests/fixtures/parameters/legacy-parameter-inventory.json");
    const auto defaultStateFile = legacyFixtureFile ("Tests/fixtures/state/legacy-default-state.xml");
    const auto representativeStateFile = legacyFixtureFile ("Tests/fixtures/state/legacy-representative-state.xml");

    test.expect (inventoryFile.existsAsFile(),
                 "legacy parameter inventory fixture must exist");
    test.expect (defaultStateFile.existsAsFile(),
                 "legacy default state fixture must exist");
    test.expect (representativeStateFile.existsAsFile(),
                 "legacy representative state fixture must exist");

    if (inventoryFile.existsAsFile())
    {
        expectFixtureHash (test, inventoryFile,
                           "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d",
                           "legacy parameter inventory fixture");
        juce::var expectedInventory;
        const auto parsed = juce::JSON::parse (inventoryFile.loadFileAsString(), expectedInventory);
        test.expect (parsed.wasOk() && expectedInventory.isObject(),
                     "legacy parameter inventory fixture must be valid JSON object");
        if (parsed.wasOk() && expectedInventory.isObject())
        {
            const auto* object = expectedInventory.getDynamicObject();
            test.expect (object->getProperty ("schema").toString()
                             == "model-d.legacy-parameter-inventory.v1",
                         "legacy parameter inventory schema must remain v1");
            const auto* parameters = object->getProperty ("parameters").getArray();
            test.expect (parameters != nullptr
                             && parameters->size() == static_cast<int> (legacyParameterIds.size()),
                         "legacy parameter inventory must contain exactly 44 IDs");
            if (parameters != nullptr)
            {
                const auto count = std::min (parameters->size(),
                                             static_cast<int> (legacyParameterIds.size()));
                for (int index = 0; index < count; ++index)
                {
                    const auto* entry = (*parameters)[index].getDynamicObject();
                    test.expect (entry != nullptr,
                                 "legacy parameter inventory entries must be objects");
                    if (entry == nullptr)
                        continue;
                    test.expect (equalsStringView (entry->getProperty ("id").toString(),
                                                   legacyParameterIds[static_cast<std::size_t> (index)]),
                                 "legacy parameter inventory ID/order must remain exact");
                    test.expect (static_cast<int> (entry->getProperty ("version_hint")) == 0,
                                 "legacy parameter inventory version hints must remain zero");
                }
            }
        }
    }

    if (defaultStateFile.existsAsFile())
        expectLegacyStateFixture (
            test, defaultStateFile,
            "07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21",
            "legacy default state fixture");
    if (representativeStateFile.existsAsFile())
        expectLegacyStateFixture (
            test, representativeStateFile,
            "e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa",
            "legacy representative state fixture");
}

void testParameterRegistry (TestContext& test)
{
    const auto descriptors = ParameterRegistry::descriptors();
    test.expect (descriptors.size() == 48,
                 "parameter registry must contain exactly 48 v2 descriptors");

    const auto prefixCount = std::min (descriptors.size(), legacyParameterIds.size());
    for (std::size_t index = 0; index < prefixCount; ++index)
    {
        test.expect (descriptors[index].id == legacyParameterIds[index],
                     "all 44 legacy IDs must remain an exact order-preserving prefix");
        test.expect (descriptors[index].versionHint == 0,
                     "all 44 legacy descriptors must retain version hint zero");
    }

    for (std::size_t offset = 0; offset < newParameterIds.size(); ++offset)
    {
        const auto index = legacyParameterIds.size() + offset;
        if (index >= descriptors.size())
            break;
        test.expect (descriptors[index].id == newParameterIds[offset],
                     "the four v2 IDs must be appended in exact approved order");
        test.expect (descriptors[index].versionHint == 1,
                     "the four v2 descriptors must use version hint one");
        test.expect (descriptors[index].automatable,
                     "the four v2 descriptors must be automatable");
    }

    std::set<std::string> ids;
    std::set<std::string> semanticKeys;

    constexpr std::array semanticChoiceIds {
        "osc1Waveform"sv, "osc2Waveform"sv, "osc3Waveform"sv,
        "osc1Range"sv, "osc2Range"sv, "osc3Range"sv,
        "whitePinkSwitch"sv, "keyboard.priorityMode"sv,
        "keyboard.triggerMode"sv
    };
    constexpr std::array semitoneIds { "tune"sv, "osc2Freq"sv, "osc3Freq"sv };
    constexpr std::array gainControlIds {
        "osc1Vol"sv, "osc2Vol"sv, "osc3Vol"sv, "outputVolKnob"sv,
        "extInputVolKnob"sv, "noiseVolKnob"sv, "outputPhonesVolKnob"sv
    };
    constexpr std::array controlIds {
        "ctrlModMixKnob"sv, "filterEmphasis"sv, "filterContour"sv,
        "feedbackKnob"sv, "modWheelValue"sv
    };
    constexpr std::array contourStageIds {
        "filterAttackTimeKnob"sv, "filterDecayTimeKnob"sv,
        "loudnessAttackTimeKnob"sv, "loudnessDecayTimeKnob"sv,
        "filterSustainKnob"sv, "loudnessSustainLevelKnob"sv
    };

    for (const auto& descriptor : descriptors)
    {
        test.expect (! descriptor.id.empty(), "registry parameter ID must be nonempty");
        test.expect (! descriptor.semanticKey.empty(), "registry semantic key must be nonempty");
        test.expect (ids.emplace (descriptor.id).second, "registry parameter IDs must be unique");
        test.expect (semanticKeys.emplace (descriptor.semanticKey).second,
                     "registry semantic keys must be unique");
        test.expect (! descriptor.displayName.empty(), "registry display name must be nonempty");
        test.expect (descriptor.unitKey != ParameterRegistry::UnitKey::unspecified,
                     "registry unit key must be explicit");
        test.expect (descriptor.mapping != ParameterRegistry::MappingKey::unspecified,
                     "registry normalized mapping key must be explicit");
        test.expect (descriptor.smoothing != ParameterRegistry::SmoothingClass::unspecified,
                     "registry smoothing class must be explicit");
        test.expect (descriptor.persistence != ParameterRegistry::PersistenceScope::unspecified,
                     "registry persistence scope must be explicit");
        test.expect (std::isfinite (descriptor.rangeStart)
                         && std::isfinite (descriptor.rangeEnd)
                         && std::isfinite (descriptor.rangeInterval)
                         && std::isfinite (descriptor.rangeSkew)
                         && std::isfinite (descriptor.physicalDefault),
                     "registry range/default metadata must be finite");
        test.expect (descriptor.rangeStart < descriptor.rangeEnd,
                     "registry range start must be less than range end");
        test.expect (descriptor.rangeInterval > 0.0f,
                     "registry range interval must be positive");
        test.expect (descriptor.rangeSkew > 0.0f,
                     "registry range skew must be positive");
        test.expect (descriptor.physicalDefault >= descriptor.rangeStart
                         && descriptor.physicalDefault <= descriptor.rangeEnd,
                     "registry physical default must lie within its range");
        test.expect ((descriptor.kind == ParameterRegistry::Kind::choice)
                         == ! descriptor.choiceValues.empty(),
                     "choice values must be present for choice parameters only");

        const auto expectedUnit = contains (semanticChoiceIds, descriptor.id)
                                    ? ParameterRegistry::UnitKey::choice
                                : contains (semitoneIds, descriptor.id)
                                    ? ParameterRegistry::UnitKey::semitones
                                : descriptor.kind == ParameterRegistry::Kind::choice
                                    ? ParameterRegistry::UnitKey::panelIndex
                                : descriptor.kind == ParameterRegistry::Kind::floating
                                    ? ParameterRegistry::UnitKey::normalized
                                    : ParameterRegistry::UnitKey::boolean;
        test.expect (descriptor.unitKey == expectedUnit,
                     "every descriptor must use its exact semantic unit policy");

        const auto expectedMapping = descriptor.kind == ParameterRegistry::Kind::choice
                                       ? ParameterRegistry::MappingKey::indexedChoice
                                   : descriptor.kind == ParameterRegistry::Kind::floating
                                       ? ParameterRegistry::MappingKey::linear
                                       : ParameterRegistry::MappingKey::boolean;
        test.expect (descriptor.mapping == expectedMapping,
                     "every descriptor must use its exact non-invented mapping key");

        const auto expectedSmoothing = contains (gainControlIds, descriptor.id)
                                         ? ParameterRegistry::SmoothingClass::gainControl
                                     : contains (controlIds, descriptor.id)
                                         ? ParameterRegistry::SmoothingClass::control
                                     : descriptor.id == "pitchWheelValue"
                                         ? ParameterRegistry::SmoothingClass::dedicatedPitch
                                     : descriptor.id == "filterCutoff"
                                         ? ParameterRegistry::SmoothingClass::dedicatedCutoff
                                     : descriptor.id == "ctrlGlideKnob"
                                         ? ParameterRegistry::SmoothingClass::dedicatedGlide
                                     : contains (contourStageIds, descriptor.id)
                                         ? ParameterRegistry::SmoothingClass::contourStage
                                         : ParameterRegistry::SmoothingClass::none;
        test.expect (descriptor.smoothing == expectedSmoothing,
                     "every descriptor must use its exact smoothing class");
        test.expect (descriptor.persistence == ParameterRegistry::PersistenceScope::apvtsState,
                     "every descriptor must retain APVTS-state persistence");
    }

    for (const auto& [id, expectedName] : correctedDisplayNames)
    {
        const auto* descriptor = findDescriptor (descriptors, id);
        test.expect (descriptor != nullptr && descriptor->displayName == expectedName,
                     "corrected legacy display name must match the approved v2 contract");
    }

    const auto* priority = findDescriptor (descriptors, "keyboard.priorityMode");
    const auto* trigger = findDescriptor (descriptors, "keyboard.triggerMode");
    const auto* mainEnabled = findDescriptor (descriptors, "output.mainEnabled");
    const auto* phonesEnabled = findDescriptor (descriptors, "output.phonesEnabled");

    test.expect (priority != nullptr
                     && priority->kind == ParameterRegistry::Kind::choice
                     && priority->displayName == "Note Priority"
                     && priority->physicalDefault == 0.0f
                     && priority->choiceValues.size() == 3
                     && priority->choiceValues[0] == "Low"
                     && priority->choiceValues[1] == "High"
                     && priority->choiceValues[2] == "Last",
                 "Note Priority descriptor must match the exact approved contract");
    test.expect (trigger != nullptr
                     && trigger->kind == ParameterRegistry::Kind::choice
                     && trigger->displayName == "Trigger Mode"
                     && trigger->physicalDefault == 0.0f
                     && trigger->choiceValues.size() == 2
                     && trigger->choiceValues[0] == "Single"
                     && trigger->choiceValues[1] == "Multi",
                 "Trigger Mode descriptor must match the exact approved contract");
    test.expect (mainEnabled != nullptr
                     && mainEnabled->kind == ParameterRegistry::Kind::boolean
                     && mainEnabled->displayName == "Main Output Enabled"
                     && mainEnabled->physicalDefault == 1.0f,
                 "Main Output Enabled descriptor must default on");
    test.expect (phonesEnabled != nullptr
                     && phonesEnabled->kind == ParameterRegistry::Kind::boolean
                     && phonesEnabled->displayName == "Phones Output Enabled"
                     && phonesEnabled->physicalDefault == 1.0f,
                 "Phones Output Enabled descriptor must default on");

    const auto* tune = findDescriptor (descriptors, "tune");
    test.expect (tune != nullptr && tune->physicalDefault == 5.0f
                     && tune->choiceValues.size() > 5 && tune->choiceValues[5] == "Zero",
                 "new instances must default Master Tune to index 5/Zero");

    MoogMiniAudioProcessor processor;
    const auto liveParameters = processor.getParameters();
    test.expect (static_cast<std::size_t> (liveParameters.size()) == descriptors.size(),
                 "processor live layout size must match the registry");

    const auto comparedCount = std::min (descriptors.size(),
                                         static_cast<std::size_t> (liveParameters.size()));
    for (std::size_t index = 0; index < comparedCount; ++index)
    {
        const auto& descriptor = descriptors[index];
        const auto* parameter = dynamic_cast<const juce::RangedAudioParameter*> (
            liveParameters[static_cast<int> (index)]);
        test.expect (parameter != nullptr, "registry live parameter must be ranged");
        if (parameter == nullptr)
            continue;

        const auto& range = parameter->getNormalisableRange();
        test.expect (equalsStringView (parameter->getParameterID(), descriptor.id),
                     "processor live parameter ID/order must come from the registry");
        test.expect (parameter->getVersionHint() == descriptor.versionHint,
                     "processor live version hint must match the registry");
        test.expect (equalsStringView (parameter->getName (256), descriptor.displayName),
                     "processor live display name must match the registry");
        test.expect (equalsStringView (parameter->getLabel(), descriptor.shortLabel),
                     "processor live short label must match the registry");
        test.expect (range.start == descriptor.rangeStart
                         && range.end == descriptor.rangeEnd
                         && range.interval == descriptor.rangeInterval
                         && range.skew == descriptor.rangeSkew
                         && range.symmetricSkew == descriptor.symmetricSkew,
                     "processor live physical range must match the registry");
        test.expect (parameter->convertFrom0to1 (parameter->getDefaultValue())
                         == descriptor.physicalDefault,
                     "processor live physical default must match the registry");
        test.expect (parameter->isAutomatable() == descriptor.automatable,
                     "processor live automatable flag must match the registry");

        if (descriptor.kind == ParameterRegistry::Kind::choice)
        {
            const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (parameter);
            test.expect (choice != nullptr, "registry choice descriptor must build a choice parameter");
            if (choice != nullptr)
            {
                test.expect (static_cast<std::size_t> (choice->choices.size())
                                 == descriptor.choiceValues.size(),
                             "processor live choice count must match the registry");
                const auto choiceCount = std::min (descriptor.choiceValues.size(),
                                                   static_cast<std::size_t> (choice->choices.size()));
                for (std::size_t choiceIndex = 0; choiceIndex < choiceCount; ++choiceIndex)
                {
                    test.expect (equalsStringView (
                                     choice->choices[static_cast<int> (choiceIndex)],
                                     descriptor.choiceValues[choiceIndex]),
                                 "processor live choice order/text must match the registry");
                    const auto physicalIndex = static_cast<float> (choiceIndex);
                    const auto normalised = parameter->convertTo0to1 (physicalIndex);
                    test.expect (parameter->convertFrom0to1 (normalised) == physicalIndex,
                                 "choice index-normalized round trips must be exact");
                }
                test.expect (parameter->convertTo0to1 (descriptor.rangeStart) == 0.0f
                                 && parameter->convertTo0to1 (descriptor.rangeEnd) == 1.0f,
                             "choice normalized endpoints must be exact");
            }
        }
        else if (descriptor.kind == ParameterRegistry::Kind::floating)
        {
            test.expect (dynamic_cast<const juce::AudioParameterFloat*> (parameter) != nullptr,
                         "registry float descriptor must build a float parameter");
        }
        else if (descriptor.kind == ParameterRegistry::Kind::boolean)
        {
            test.expect (dynamic_cast<const juce::AudioParameterBool*> (parameter) != nullptr,
                         "registry bool descriptor must build a bool parameter");
        }
    }

    const auto inventoryFile = legacyFixtureFile (
        "Tests/fixtures/parameters/legacy-parameter-inventory.json");
    if (inventoryFile.existsAsFile())
    {
        juce::var expectedInventory;
        const auto parsed = juce::JSON::parse (inventoryFile.loadFileAsString(), expectedInventory);
        test.expect (parsed.wasOk() && expectedInventory.isObject(),
                     "legacy parameter inventory fixture must be valid for registry comparison");
        if (parsed.wasOk() && expectedInventory.isObject())
        {
            const auto* parameters = expectedInventory.getDynamicObject()
                                         ->getProperty ("parameters").getArray();
            if (parameters != nullptr)
            {
                const auto count = std::min ({ descriptors.size(), legacyParameterIds.size(),
                                               static_cast<std::size_t> (parameters->size()),
                                               static_cast<std::size_t> (liveParameters.size()) });
                for (std::size_t index = 0; index < count; ++index)
                {
                    const auto* legacy = (*parameters)[static_cast<int> (index)].getDynamicObject();
                    if (legacy == nullptr)
                        continue;
                    const auto& descriptor = descriptors[index];
                    const auto* live = dynamic_cast<const juce::RangedAudioParameter*> (
                        liveParameters[static_cast<int> (index)]);
                    test.expect (live != nullptr,
                                 "legacy inventory entries must map to live ranged parameters");
                    if (live == nullptr)
                        continue;

                    const auto legacyType = legacy->getProperty ("type").toString();
                    const auto expectedKind = legacyType == "choice"
                                                ? ParameterRegistry::Kind::choice
                                            : legacyType == "float"
                                                ? ParameterRegistry::Kind::floating
                                                : ParameterRegistry::Kind::boolean;
                    test.expect (legacyType == "choice" || legacyType == "float"
                                     || legacyType == "bool",
                                 "legacy inventory parameter type must be recognized");
                    test.expect (descriptor.kind == expectedKind,
                                 "legacy descriptor kind must match immutable inventory type");
                    test.expect ((dynamic_cast<const juce::AudioParameterChoice*> (live) != nullptr)
                                     == (legacyType == "choice")
                                     && (dynamic_cast<const juce::AudioParameterFloat*> (live) != nullptr)
                                     == (legacyType == "float")
                                     && (dynamic_cast<const juce::AudioParameterBool*> (live) != nullptr)
                                     == (legacyType == "bool"),
                                 "legacy live subclass must match immutable inventory type");
                    test.expect (descriptor.automatable
                                     == static_cast<bool> (legacy->getProperty ("automatable")),
                                 "legacy descriptor automatable flag must match immutable inventory");
                    test.expect (live->isAutomatable()
                                     == static_cast<bool> (legacy->getProperty ("automatable"))
                                     && live->isDiscrete()
                                     == static_cast<bool> (legacy->getProperty ("discrete"))
                                     && live->isBoolean()
                                     == static_cast<bool> (legacy->getProperty ("boolean"))
                                     && live->isMetaParameter()
                                     == static_cast<bool> (legacy->getProperty ("meta")),
                                 "legacy live host flags must match immutable inventory");
                    const auto expectedName = std::find_if (
                        correctedDisplayNames.begin(), correctedDisplayNames.end(),
                        [&descriptor] (const auto& item) { return item.first == descriptor.id; });
                    const auto displayName = expectedName == correctedDisplayNames.end()
                                               ? legacy->getProperty ("name").toString()
                                               : juce::String { expectedName->second.data(),
                                                                expectedName->second.size() };
                    test.expect (equalsStringView (displayName, descriptor.displayName),
                                 "unlisted legacy names must remain unchanged and listed names exact");
                    test.expect (descriptor.rangeStart == static_cast<float> (legacy->getProperty ("range_start"))
                                     && descriptor.rangeEnd == static_cast<float> (legacy->getProperty ("range_end"))
                                     && descriptor.rangeInterval == static_cast<float> (legacy->getProperty ("range_interval"))
                                     && descriptor.rangeSkew == static_cast<float> (legacy->getProperty ("range_skew"))
                                     && descriptor.symmetricSkew == static_cast<bool> (legacy->getProperty ("range_symmetric_skew")),
                                 "legacy physical host-storage ranges and normalization must remain exact");
                    const auto expectedDefault = descriptor.id == "tune"
                                                   ? 5.0f
                                                   : static_cast<float> (legacy->getProperty ("physical_default"));
                    test.expect (descriptor.physicalDefault == expectedDefault,
                                 "only Tune may change its legacy physical default");
                    if (const auto* choices = legacy->getProperty ("choices").getArray())
                    {
                        test.expect (descriptor.choiceValues.size()
                                         == static_cast<std::size_t> (choices->size()),
                                     "legacy choice counts must remain exact");
                        const auto choiceCount = std::min (descriptor.choiceValues.size(),
                                                           static_cast<std::size_t> (choices->size()));
                        for (std::size_t choiceIndex = 0; choiceIndex < choiceCount; ++choiceIndex)
                            test.expect (equalsStringView ((*choices)[static_cast<int> (choiceIndex)].toString(),
                                                           descriptor.choiceValues[choiceIndex]),
                                         "legacy choice strings/order must remain exact");
                    }
                }
            }
        }
    }

    const auto expectLiveDefault = [&] (std::string_view id,
                                        float expectedPhysical,
                                        std::string_view expectedText)
    {
        const auto parameterId = juce::String { id.data(), id.size() };
        const auto* parameter = processor.apvts.getParameter (parameterId);
        test.expect (parameter != nullptr, "required v2 default parameter must exist");
        if (parameter == nullptr)
            return;
        test.expect (parameter->convertFrom0to1 (parameter->getDefaultValue()) == expectedPhysical,
                     "required v2 physical default must match");
        test.expect (equalsStringView (parameter->getText (parameter->getDefaultValue(), 256),
                                       expectedText),
                     "required v2 default text must match");
    };
    expectLiveDefault ("tune", 5.0f, "Zero");
    expectLiveDefault ("keyboard.priorityMode", 0.0f, "Low");
    expectLiveDefault ("keyboard.triggerMode", 0.0f, "Single");
    expectLiveDefault ("output.mainEnabled", 1.0f, "On");
    expectLiveDefault ("output.phonesEnabled", 1.0f, "On");

    const auto registryFixture = legacyFixtureFile (
        "Tests/fixtures/parameters/parameter-registry-v2.json");
    test.expect (registryFixture.existsAsFile(),
                 "canonical parameter registry v2 fixture must exist");
    if (registryFixture.existsAsFile())
        test.expect (registryFixtureMatchesExport (registryFixture, makeRegistryV2Export()),
                     "canonical registry export must equal the v2 fixture byte-for-byte");

    const auto bomFixture = juce::File::createTempFile ("-parameter-registry-v2-bom.json");
    const auto expectedExport = makeRegistryV2Export();
    juce::MemoryOutputStream bomBytes;
    constexpr std::array<std::uint8_t, 3> utf8Bom { 0xef, 0xbb, 0xbf };
    bomBytes.write (utf8Bom.data(), utf8Bom.size());
    bomBytes.write (expectedExport.toRawUTF8(), expectedExport.getNumBytesAsUTF8());
    test.expect (bomFixture.replaceWithData (bomBytes.getData(), bomBytes.getDataSize()),
                 "BOM-prefixed registry negative fixture must be writable");
    test.expect (! registryFixtureMatchesExport (bomFixture, expectedExport),
                 "byte-exact registry guard must reject a UTF-8 BOM prefix");
    test.expect (bomFixture.deleteFile(),
                 "BOM-prefixed registry negative fixture must be removed");

    testLegacyParameterFixtures (test);
}

int captureLegacyFixture (std::string_view mode)
{
    if (mode == "capture-registry-v2")
    {
        std::cout << makeRegistryV2Export().toStdString();
        return 0;
    }

    MoogMiniAudioProcessor processor;
    if (mode == "capture-state-v2-native")
    {
        const auto state = serialiseProcessorState (processor);
        if (state != nullptr)
            std::cout << canonicaliseXmlFixture (*state).toStdString();
        return state == nullptr ? 1 : 0;
    }
    if (mode == "capture-state-v2-migrated-default"
        || mode == "capture-state-v2-migrated-representative")
    {
        const auto fixture = legacyFixtureFile (
            mode == "capture-state-v2-migrated-default"
                ? "Tests/fixtures/state/legacy-default-state.xml"
                : "Tests/fixtures/state/legacy-representative-state.xml");
        const auto legacy = juce::parseXML (fixture);
        if (legacy == nullptr)
            return 1;
        const auto binary = binaryFromXml (*legacy);
        const auto result = processor.restoreState (binary.getData(),
                                                    static_cast<int> (binary.getSize()));
        const auto state = serialiseProcessorState (processor);
        if (! result.succeeded() || state == nullptr)
            return 1;
        std::cout << canonicaliseXmlFixture (*state).toStdString();
        return 0;
    }
    if (mode == "capture-parameters")
        std::cout << serialiseLegacyParameterInventory (processor).toStdString();
    else if (mode == "capture-default-state")
    {
        const auto state = serialiseProcessorState (processor);
        if (state != nullptr)
            std::cout << canonicaliseXmlFixture (*state).toStdString();
    }
    else if (mode == "capture-representative-state")
    {
        setRepresentativeParameterState (processor);
        const auto state = serialiseProcessorState (processor);
        if (state != nullptr)
            std::cout << canonicaliseXmlFixture (*state).toStdString();
    }
    else
    {
        return 2;
    }

    return 0;
}

void testGeneratedWrapperContract (TestContext& test)
{
    test.expect (JucePlugin_IsSynth == 1, "generated JucePlugin_IsSynth must be 1");
    test.expect (JucePlugin_WantsMidiInput == 1,
                 "generated JucePlugin_WantsMidiInput must be 1");
    test.expect (JucePlugin_ProducesMidiOutput == 0,
                 "generated JucePlugin_ProducesMidiOutput must be 0");
    test.expect (JucePlugin_IsMidiEffect == 0,
                 "generated JucePlugin_IsMidiEffect must be 0");
    test.expect (std::string_view { JucePlugin_Vst3Category } == "Instrument|Synth",
                 "generated VST3 category must be exactly Instrument|Synth");
    test.expect (std::string_view { SYNTH_STRINGIFY (JucePlugin_AUMainType) } == "'aumu'",
                 "generated AU main type must be aumu/Music Device");

    MoogMiniAudioProcessor processor;
    test.expect (processor.acceptsMidi(), "processor must accept MIDI");
    test.expect (! processor.producesMidi(), "processor must not produce MIDI");
    test.expect (! processor.isMidiEffect(), "processor must not be a MIDI effect");
}

bool testDefaultBusContract (MoogMiniAudioProcessor& processor, TestContext& test)
{
    const auto exactBusCounts = processor.getBusCount (true) == 1
                             && processor.getBusCount (false) == 2;
    test.expect (processor.getBusCount (true) == 1,
                 "processor must declare exactly one input bus");
    test.expect (processor.getBusCount (false) == 2,
                 "processor must declare exactly two output buses");

    if (! exactBusCounts)
        return false;

    const auto* external = processor.getBus (true, 0);
    const auto* main = processor.getBus (false, 0);
    const auto* phones = processor.getBus (false, 1);
    test.expect (external != nullptr && external->getName() == "External Input",
                 "input bus 0 must be named External Input");
    test.expect (main != nullptr && main->getName() == "Main Output",
                 "output bus 0 must be named Main Output");
    test.expect (phones != nullptr && phones->getName() == "Phones/Cue",
                 "output bus 1 must be named Phones/Cue");
    test.expect (external != nullptr && ! external->isEnabledByDefault(),
                 "External Input must be disabled by default");
    test.expect (main != nullptr && main->isEnabledByDefault(),
                 "Main Output must be enabled by default");
    test.expect (phones != nullptr && ! phones->isEnabledByDefault(),
                 "Phones/Cue must be disabled by default");
    test.expect (external != nullptr && external->getDefaultLayout() == ChannelSet::stereo(),
                 "External Input default layout must be stereo");
    test.expect (main != nullptr && main->getDefaultLayout() == ChannelSet::stereo(),
                 "Main Output default layout must be stereo");
    test.expect (phones != nullptr && phones->getDefaultLayout() == ChannelSet::stereo(),
                 "Phones/Cue default layout must be stereo");

    return true;
}

void testVST3InputRole (MoogMiniAudioProcessor& processor, TestContext& test)
{
    auto* extensions = processor.getVST3ClientExtensions();
    test.expect (extensions != nullptr, "processor must expose VST3 client extensions");

    if (extensions != nullptr)
        test.expect (! extensions->getPluginHasMainInput(),
                     "VST3 input bus 0 must be marked auxiliary, not main");
}

void testLayoutTable (MoogMiniAudioProcessor& processor, TestContext& test)
{
    const std::array mainLayouts { ChannelSet::mono(), ChannelSet::stereo() };
    const std::array optionalLayouts {
        ChannelSet::disabled(), ChannelSet::mono(), ChannelSet::stereo()
    };

    int acceptedCount = 0;
    for (const auto& main : mainLayouts)
    {
        for (const auto& external : optionalLayouts)
        {
            for (const auto& phones : optionalLayouts)
            {
                const auto accepted = processor.isBusesLayoutSupported (
                    makeLayout (main, external, phones));
                acceptedCount += accepted ? 1 : 0;
                std::cout << "LAYOUT accepted main=" << channelSetName (main)
                          << " external=" << channelSetName (external)
                          << " phones=" << channelSetName (phones)
                          << " result=" << (accepted ? "true" : "false") << '\n';
                test.expect (accepted, "one of the 18 required layouts was rejected");
            }
        }
    }
    test.expect (acceptedCount == 18, "exactly all 18 valid layouts must be accepted");

    struct RejectedLayout
    {
        const char* name;
        BusesLayout layout;
    };

    auto missingInput = makeLayout (ChannelSet::stereo(),
                                    ChannelSet::disabled(),
                                    ChannelSet::disabled());
    missingInput.inputBuses.clear();
    auto missingPhones = makeLayout (ChannelSet::stereo(),
                                     ChannelSet::disabled(),
                                     ChannelSet::disabled());
    missingPhones.outputBuses.remove (1);
    auto extraInput = makeLayout (ChannelSet::stereo(),
                                  ChannelSet::disabled(),
                                  ChannelSet::disabled());
    extraInput.inputBuses.add (ChannelSet::disabled());
    auto extraOutput = makeLayout (ChannelSet::stereo(),
                                   ChannelSet::disabled(),
                                   ChannelSet::disabled());
    extraOutput.outputBuses.add (ChannelSet::disabled());

    const std::vector<RejectedLayout> rejected {
        { "disabled-main", makeLayout (ChannelSet::disabled(), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "surround-main", makeLayout (ChannelSet::create5point1(), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "discrete-main", makeLayout (ChannelSet::discreteChannels (3), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "surround-external", makeLayout (ChannelSet::stereo(), ChannelSet::create5point1(), ChannelSet::disabled()) },
        { "discrete-external", makeLayout (ChannelSet::stereo(), ChannelSet::discreteChannels (3), ChannelSet::disabled()) },
        { "surround-phones", makeLayout (ChannelSet::stereo(), ChannelSet::disabled(), ChannelSet::create5point1()) },
        { "discrete-phones", makeLayout (ChannelSet::stereo(), ChannelSet::disabled(), ChannelSet::discreteChannels (3)) },
        { "missing-input", missingInput },
        { "missing-phones", missingPhones },
        { "extra-input", extraInput },
        { "extra-output", extraOutput }
    };

    for (const auto& entry : rejected)
    {
        const auto accepted = processor.isBusesLayoutSupported (entry.layout);
        std::cout << "LAYOUT rejected case=" << entry.name
                  << " result=" << (accepted ? "true" : "false") << '\n';
        test.expect (! accepted, std::string { "invalid layout was accepted: " } + entry.name);
    }
}

bool configureLayout (MoogMiniAudioProcessor& processor,
                      const BusesLayout& layout,
                      TestContext& test,
                      std::string_view context)
{
    const auto configured = processor.setBusesLayout (layout);
    test.expect (configured, std::string { context } + ": layout must configure");
    return configured;
}

void testDefaultSilence (TestContext& test)
{
    MoogMiniAudioProcessor processor;
    processor.setRateAndBufferSizeDetails (48000.0, 128);
    processor.prepareToPlay (48000.0, 128);

    juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 128);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    test.expect (isFinite (buffer), "default no-MIDI render must be finite");
    test.expect (absoluteSum (buffer) == 0.0,
                 "default no-MIDI render must be deterministically silent");
}

void verifyA440Routing (MoogMiniAudioProcessor& processor,
                       int mainChannels,
                       int phonesChannels,
                       TestContext& test,
                       std::string_view context)
{
    const auto mainLayout = mainChannels == 1 ? ChannelSet::mono() : ChannelSet::stereo();
    const auto phonesLayout = phonesChannels == 1 ? ChannelSet::mono() : ChannelSet::stereo();
    const auto layout = makeLayout (mainLayout, ChannelSet::disabled(), phonesLayout);
    if (! configureLayout (processor, layout, test, context))
        return;

    processor.setRateAndBufferSizeDetails (48000.0, 128);
    processor.prepareToPlay (48000.0, 128);

    const auto processChannels = std::max (processor.getTotalNumInputChannels(),
                                           processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (processChannels, 128);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    auto main = processor.getBusBuffer (buffer, false, 0);
    auto phones = processor.getBusBuffer (buffer, false, 1);
    test.expect (isFinite (main), std::string { context } + ": Main Output must be finite");
    test.expect (isFinite (phones), std::string { context } + ": Phones/Cue must be finite");
    test.expect (absoluteSum (main) > 0.01,
                 std::string { context } + ": internal A440 must produce non-silent Main Output");

    for (int channel = 1; channel < main.getNumChannels(); ++channel)
        for (int sample = 0; sample < main.getNumSamples(); ++sample)
            test.expect (main.getSample (channel, sample) == main.getSample (0, sample),
                         std::string { context } + ": stereo Main Output must be dual mono");

    for (int channel = 0; channel < phones.getNumChannels(); ++channel)
        for (int sample = 0; sample < phones.getNumSamples(); ++sample)
            test.expect (phones.getSample (channel, sample) == main.getSample (0, sample),
                         std::string { context } + ": Phones/Cue must mirror Main Output at unity");

    juce::AudioBuffer<float> zeroSampleBuffer (processChannels, 0);
    processor.processBlock (zeroSampleBuffer, midi);
    std::cout << "ROUTING main=" << mainChannels
              << " phones=" << phonesChannels
              << " result=true\n";
}

struct ExternalInputRender
{
    std::vector<float> main;
    std::array<std::vector<float>, 2> phones;
};

ExternalInputRender renderExternalInput (int externalChannels,
                                         const std::vector<float>& left,
                                         const std::vector<float>& right,
                                         TestContext& test,
                                         std::string_view context)
{
    MoogMiniAudioProcessor processor;
    // Establish an explicit external-input-only patch before MIDI opens the
    // existing filter/loudness envelopes.
    for (const auto* parameterId : { "osc1OnOff", "osc2OnOff", "osc3OnOff",
                                     "a440HzOnOff", "noiseOnOffSwitch",
                                     "oscModSwitch", "filterModSwitch" })
        setParameter (processor, parameterId, 0.0f, test);
    for (const auto* parameterId : { "osc1Vol", "osc2Vol", "osc3Vol",
                                     "noiseVolKnob", "filterEmphasis" })
        setParameter (processor, parameterId, 0.0f, test);
    setParameter (processor, "feedbackKnob", 0.0f, test);
    setParameter (processor, "extInputVolSwitch", 1.0f, test);
    setParameter (processor, "extInputVolKnob", 1.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);
    setParameter (processor, "filterCutoff", 1.0f, test);
    setParameter (processor, "filterAttackTimeKnob", 0.0f, test);
    setParameter (processor, "loudnessAttackTimeKnob", 0.0f, test);

    const auto externalLayout = externalChannels == 1
                              ? ChannelSet::mono() : ChannelSet::stereo();
    if (! configureLayout (processor,
                           makeLayout (ChannelSet::mono(),
                                       externalLayout,
                                       ChannelSet::stereo()),
                           test,
                           context))
        return {};

    const auto numSamples = static_cast<int> (left.size());
    test.expect (externalChannels == 1 || right.size() == left.size(),
                 std::string { context } + ": stereo input vectors must have equal length");
    processor.setRateAndBufferSizeDetails (48000.0, numSamples);
    processor.prepareToPlay (48000.0, numSamples);

    const auto processChannels = std::max (processor.getTotalNumInputChannels(),
                                           processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (processChannels, numSamples);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    // Poison channels that are output-only in this layout. A processor that reads
    // the full process buffer as input will feed these distinguishable values into
    // the engine instead of sourcing only the declared External Input bus.
    for (int channel = externalChannels; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            buffer.setSample (channel, sample,
                              1000.0f * static_cast<float> (channel + 1)
                                  + static_cast<float> (sample));

    auto external = processor.getBusBuffer (buffer, true, 0);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        external.setSample (0, sample, left[static_cast<size_t> (sample)]);
        if (externalChannels == 2)
            external.setSample (1, sample, right[static_cast<size_t> (sample)]);
    }

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    processor.processBlock (buffer, midi);

    auto main = processor.getBusBuffer (buffer, false, 0);
    auto phones = processor.getBusBuffer (buffer, false, 1);
    test.expect (isFinite (main), std::string { context } + ": Main Output must be finite");
    test.expect (isFinite (phones), std::string { context } + ": Phones/Cue must be finite");

    ExternalInputRender result;
    result.main.resize (static_cast<size_t> (numSamples));
    for (auto& channel : result.phones)
        channel.resize (static_cast<size_t> (numSamples));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto index = static_cast<size_t> (sample);
        result.main[index] = main.getSample (0, sample);
        result.phones[0][index] = phones.getSample (0, sample);
        result.phones[1][index] = phones.getSample (1, sample);
    }
    return result;
}

bool expectSamplesEqual (const std::vector<float>& actual,
                         const std::vector<float>& expected,
                         TestContext& test,
                         std::string_view context)
{
    test.expect (actual.size() == expected.size(),
                 std::string { context } + ": sample counts must match");
    if (actual.size() != expected.size())
        return false;

    float maximumDifference = 0.0f;
    for (size_t index = 0; index < actual.size(); ++index)
        maximumDifference = std::max (maximumDifference,
                                      std::abs (actual[index] - expected[index]));

    const auto matches = maximumDifference <= 1.0e-6f;
    test.expect (matches,
                 std::string { context } + ": rendered samples must match");
    return matches;
}

double absoluteSampleSum (const std::vector<float>& samples)
{
    double result = 0.0;
    for (const auto sample : samples)
        result += std::abs (sample);
    return result;
}

double absoluteDifferenceSum (const std::vector<float>& first,
                              const std::vector<float>& second)
{
    if (first.size() != second.size())
        return 0.0;

    double result = 0.0;
    for (size_t index = 0; index < first.size(); ++index)
        result += std::abs (first[index] - second[index]);
    return result;
}

void testExternalInputRouting (TestContext& test)
{
    constexpr int numSamples = 512;
    std::vector<float> mono (numSamples);
    std::vector<float> left (numSamples);
    std::vector<float> right (numSamples);
    std::vector<float> average (numSamples);
    const std::vector<float> zero (numSamples, 0.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto index = static_cast<size_t> (sample);
        mono[index] = 0.15f + 0.002f * static_cast<float> (sample % 17);
        left[index] = 0.31f + 0.003f * static_cast<float> (sample % 13);
        right[index] = -0.07f + 0.001f * static_cast<float> (sample % 11);
        average[index] = 0.5f * (left[index] + right[index]);
    }

    const auto zeroMono = renderExternalInput (1, zero, {}, test,
                                               "zero mono External Input control");
    const auto zeroStereo = renderExternalInput (2, zero, zero, test,
                                                 "zero stereo External Input control");
    const auto zeroMonoSum = absoluteSampleSum (zeroMono.main);
    const auto zeroStereoSum = absoluteSampleSum (zeroStereo.main);
    const auto zeroControlsSilent = zeroMonoSum <= 1.0e-7 && zeroStereoSum <= 1.0e-7;
    test.expect (zeroControlsSilent,
                 "explicit external-input-only patch must be silent for zero input");
    std::cout << "EXTERNAL_INPUT case=zero-control mono_abs_sum=" << zeroMonoSum
              << " stereo_abs_sum=" << zeroStereoSum
              << " result=" << (zeroControlsSilent ? "true" : "false") << '\n';

    const auto monoRender = renderExternalInput (1, mono, {}, test, "mono External Input");
    const auto duplicatedStereo = renderExternalInput (2, mono, mono, test,
                                                        "duplicated stereo External Input");
    const auto monoDifferenceFromZero = absoluteDifferenceSum (monoRender.main,
                                                               zeroMono.main);
    const auto monoSensitive = monoDifferenceFromZero > 0.01;
    test.expect (monoSensitive,
                 "nonzero mono External Input must differ materially from zero-input control");
    const auto monoMainMatches = expectSamplesEqual (monoRender.main, duplicatedStereo.main,
                                                     test, "mono input duplication");
    const auto monoCueLeftMatches = expectSamplesEqual (monoRender.phones[0],
                                                        duplicatedStereo.phones[0], test,
                                                        "mono input duplication cue left");
    const auto monoCueRightMatches = expectSamplesEqual (monoRender.phones[1],
                                                         duplicatedStereo.phones[1], test,
                                                         "mono input duplication cue right");
    const auto monoMatches = monoSensitive
                          && monoMainMatches
                          && monoCueLeftMatches
                          && monoCueRightMatches;
    std::cout << "EXTERNAL_INPUT case=mono-duplication difference_from_zero="
              << monoDifferenceFromZero << " result="
              << (monoMatches ? "true" : "false") << '\n';

    const auto stereoRender = renderExternalInput (2, left, right, test,
                                                    "distinguishable stereo External Input");
    const auto averagedMono = renderExternalInput (1, average, {}, test,
                                                    "averaged mono External Input");
    const auto stereoDifferenceFromZero = absoluteDifferenceSum (stereoRender.main,
                                                                 zeroStereo.main);
    const auto stereoSensitive = stereoDifferenceFromZero > 0.01;
    test.expect (stereoSensitive,
                 "nonzero stereo External Input must differ materially from zero-input control");
    const auto stereoMainMatches = expectSamplesEqual (stereoRender.main, averagedMono.main,
                                                       test, "stereo input downmix");
    const auto stereoCueLeftMatches = expectSamplesEqual (stereoRender.phones[0],
                                                          averagedMono.phones[0], test,
                                                          "stereo input downmix cue left");
    const auto stereoCueRightMatches = expectSamplesEqual (stereoRender.phones[1],
                                                           averagedMono.phones[1], test,
                                                           "stereo input downmix cue right");
    const auto stereoMatches = stereoSensitive
                            && stereoMainMatches
                            && stereoCueLeftMatches
                            && stereoCueRightMatches;
    std::cout << "EXTERNAL_INPUT case=stereo-downmix difference_from_zero="
              << stereoDifferenceFromZero << " result="
              << (stereoMatches ? "true" : "false") << '\n';
}

void testProcessingContract (TestContext& test)
{
    testDefaultSilence (test);
    testExternalInputRouting (test);

    MoogMiniAudioProcessor processor;
    setParameter (processor, "a440HzOnOff", 1.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);

    for (const auto mainChannels : { 1, 2 })
    {
        for (const auto phonesChannels : { 1, 2 })
        {
            const auto context = std::string { "A440 routing main=" }
                               + std::to_string (mainChannels)
                               + " phones=" + std::to_string (phonesChannels);
            processor.releaseResources();
            processor.reset();
            verifyA440Routing (processor, mainChannels, phonesChannels, test, context);
        }
    }
}

void testEditorConstructionPreservesParameters (TestContext& test)
{
    const auto previousPresetDirectory = PresetManager::getStandaloneLifecycleTestDirectory();
    const auto presetDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getNonexistentChildFile ("model-d-unit-editor-presets", {}, false);
    const auto overrideSet = PresetManager::setStandaloneLifecycleTestDirectory (presetDirectory);
    test.expect (overrideSet, "editor regression must isolate its preset directory");
    if (! overrideSet)
        return;

    {
        MoogMiniAudioProcessor processor;
        setParameter (processor, "osc1Range", 0.6f, test);
        setParameter (processor, "osc2Freq", 0.25f, test);
        test.expect (static_cast<float> (processor.apvts.getParameterAsValue ("osc1Range").getValue())
                         != processor.apvts.getRawParameterValue ("osc1Range")->load(),
                     "editor regression fixture must begin with a stale mirrored value");
        const auto parameters = processor.getParameters();
        std::vector<float> valuesBeforeEditor;
        valuesBeforeEditor.reserve (parameters.size());
        for (const auto* parameter : parameters)
            valuesBeforeEditor.push_back (parameter->getValue());

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        test.expect (editor != nullptr, "processor must create its custom editor");
        test.expect (PresetManager::getLastConstructedPresetDirectory() == presetDirectory,
                     "editor regression must construct presets only in its temporary directory");
        juce::Timer::callAfterDelay (100, []
        {
            juce::MessageManager::getInstance()->stopDispatchLoop();
        });
        juce::MessageManager::getInstance()->runDispatchLoop();

        const int parameterCount = parameters.size();
        test.expect (static_cast<size_t> (parameterCount) == valuesBeforeEditor.size(),
                     "editor construction must not change the parameter inventory");
        for (int index = 0;
             index < parameterCount && static_cast<size_t> (index) < valuesBeforeEditor.size();
             ++index)
        {
            const auto vectorIndex = static_cast<size_t> (index);
            test.expect (std::abs (parameters[index]->getValue() - valuesBeforeEditor[vectorIndex]) < 1.0e-6f,
                         "editor construction must not change parameter "
                             + std::to_string (index));
        }
    }

    const auto overrideReset = PresetManager::setStandaloneLifecycleTestDirectory (previousPresetDirectory);
    test.expect (overrideReset, "editor regression must restore its preset-directory override");
    test.expect (presetDirectory.deleteRecursively(),
                 "editor regression must remove its temporary preset directory");
}

void testUnitContract (TestContext& test)
{
    Oscillator oscillator;
    oscillator.setSampleRate (48000.0f);
    oscillator.setRange (Oscillator::Eight);
    oscillator.setWaveform (Oscillator::Sin);
    oscillator.start (440.0f);

    double renderedMagnitude = 0.0;
    for (int sample = 0; sample < 480; ++sample)
    {
        const auto value = oscillator.processNextSample (0.0f, true);
        test.expect (std::isfinite (value), "foundational oscillator samples must be finite");
        renderedMagnitude += std::abs (value);
    }

    test.expect (renderedMagnitude > 0.0,
                 "foundational oscillator render must be non-silent");
    testEditorConstructionPreservesParameters (test);
}

void testStateSmoke (TestContext& test)
{
    MoogMiniAudioProcessor source;
    setParameter (source, "filterCutoff", 0.75f, test);

    juce::MemoryBlock state;
    source.getStateInformation (state);
    test.expect (state.getSize() > 0, "existing processor state must serialize");

    MoogMiniAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    const auto* sourceParameter = source.apvts.getParameter ("filterCutoff");
    const auto* restoredParameter = restored.apvts.getParameter ("filterCutoff");
    test.expect (sourceParameter != nullptr && restoredParameter != nullptr,
                 "existing state-smoke parameter must be available");
    if (sourceParameter != nullptr && restoredParameter != nullptr)
        test.expect (std::abs (sourceParameter->getValue() - restoredParameter->getValue()) < 1.0e-6f,
                     "existing processor state must restore its serialized parameter value");
    test.expect (! restored.shouldAutoLoadLastPreset(),
                 "restored processor state must retain the existing host-restore marker");
}

void testStateV2Contract (TestContext& test)
{
    MoogMiniAudioProcessor processor;
    const auto state = serialiseProcessorState (processor);
    test.expect (state != nullptr, "new processor state must serialize as XML");
    if (state == nullptr)
        return;

    test.expect (state->hasTagName ("modelDState"),
                 "new processor state root must be modelDState");
    test.expect (state->getNumAttributes() == 3,
                 "modelDState must contain exactly three ordered properties");
    if (state->getNumAttributes() == 3)
    {
        test.expect (state->getAttributeName (0) == "stateVersion"
                         && state->getStringAttribute ("stateVersion") == "2",
                     "stateVersion must be the first property and strict integer 2");
        test.expect (state->getAttributeName (1) == "engineVersion"
                         && state->getStringAttribute ("engineVersion")
                                == JucePlugin_VersionString,
                     "engineVersion must be the second property and product metadata");
        test.expect (state->getAttributeName (2) == "calibrationProfileId"
                         && state->getStringAttribute ("calibrationProfileId") == "baseline",
                     "calibrationProfileId must be the third property and baseline");
    }

    constexpr std::array expectedChildren {
        "parameters"sv, "compatibility"sv, "ui"sv,
        "recreation"sv, "extensions"sv, "migrationLog"sv
    };
    test.expect (state->getNumChildElements() == static_cast<int> (expectedChildren.size()),
                 "modelDState must contain exactly six reserved children");
    auto* child = state->getFirstChildElement();
    for (const auto expectedName : expectedChildren)
    {
        test.expect (child != nullptr && equalsStringView (child->getTagName(), expectedName),
                     "modelDState reserved child order must be canonical");
        if (child != nullptr)
            child = child->getNextElement();
    }

    const auto* parameters = state->getChildByName ("parameters");
    const auto descriptors = ParameterRegistry::descriptors();
    test.expect (parameters != nullptr
                     && parameters->getNumChildElements() == static_cast<int> (descriptors.size()),
                 "canonical parameters must contain all 48 registry entries");
    if (parameters != nullptr)
    {
        auto* parameter = parameters->getFirstChildElement();
        for (const auto& descriptor : descriptors)
        {
            test.expect (parameter != nullptr && parameter->hasTagName ("PARAM")
                             && equalsStringView (parameter->getStringAttribute ("id"), descriptor.id),
                         "canonical parameters must use production-registry order");
            test.expect (parameter != nullptr
                             && parameter->getDoubleAttribute ("value")
                                    == static_cast<double> (descriptor.physicalDefault),
                         "new state must serialize exact registry physical defaults");
            if (parameter != nullptr)
                parameter = parameter->getNextElement();
        }
    }

    const auto* compatibility = state->getChildByName ("compatibility");
    test.expect (compatibility != nullptr
                     && compatibility->getStringAttribute ("contourContract")
                            == "canonicalContours"
                     && compatibility->getStringAttribute ("sourceVersion") == "2"
                     && compatibility->getStringAttribute ("migratedAtVersion") == "0"
                     && compatibility->getStringAttribute ("sourceHash").isEmpty(),
                 "new state must use native canonical compatibility metadata");
    test.expect (processor.shouldAutoLoadLastPreset(),
                 "serializing a new state must not set the host-restore lifecycle flag");

    const auto nativeFixture = legacyFixtureFile (
        "Tests/fixtures/state/native-default-state-v2.xml");
    test.expect (nativeFixture.existsAsFile(), "native v2 capture fixture must exist");
    if (nativeFixture.existsAsFile())
        test.expect (fixtureMatchesXml (nativeFixture, *state),
                     "native v2 production capture must equal its fixture byte-for-byte");
}

void expectMigrationFixture (TestContext& test,
                             const juce::File& legacyFixture,
                             const juce::File& migratedFixture,
                             std::string_view description)
{
    const auto legacy = juce::parseXML (legacyFixture);
    test.expect (legacy != nullptr, std::string { description } + " legacy XML must parse");
    if (legacy == nullptr)
        return;

    const auto legacyBinary = binaryFromXml (*legacy);
    MoogMiniAudioProcessor migrated;
    const auto result = migrated.restoreState (legacyBinary.getData(),
                                               static_cast<int> (legacyBinary.getSize()));
    test.expect (result.code == StateContract::RestoreCode::successMigratedV0,
                 std::string { description } + " must return success_migrated_v0");
    test.expect (migrated.getContourContract()
                     == StateContract::ContourContract::legacyCrossedContours,
                 std::string { description } + " must retain legacy crossed contours");
    test.expect (! migrated.shouldAutoLoadLastPreset(),
                 std::string { description } + " must set the successful host-restore marker");

    for (const auto& descriptor : ParameterRegistry::descriptors())
    {
        const auto* source = findParameterXml (*legacy, descriptor.id);
        const auto expected = source != nullptr
                                ? static_cast<float> (source->getDoubleAttribute ("value"))
                                : descriptor.physicalDefault;
        test.expect (physicalParameterValue (migrated, descriptor.id) == expected,
                     std::string { description }
                         + " must preserve every present physical value without pair swaps");
    }
    test.expect (physicalParameterValue (migrated, "keyboard.priorityMode") == 0.0f
                     && physicalParameterValue (migrated, "keyboard.triggerMode") == 0.0f
                     && physicalParameterValue (migrated, "output.mainEnabled") == 1.0f
                     && physicalParameterValue (migrated, "output.phonesEnabled") == 1.0f,
                 std::string { description } + " must add Low/Single/Main/Phones defaults");

    const auto migratedXml = serialiseProcessorState (migrated);
    test.expect (migratedXml != nullptr,
                 std::string { description } + " migrated v2 XML must serialize");
    test.expect (migratedFixture.existsAsFile(),
                 std::string { description } + " migrated v2 fixture must exist");
    if (migratedXml != nullptr && migratedFixture.existsAsFile())
        test.expect (fixtureMatchesXml (migratedFixture, *migratedXml),
                     std::string { description }
                         + " migration must equal the checked v2 fixture byte-for-byte");

    const auto firstSave = serialiseProcessorStateBytes (migrated);
    MoogMiniAudioProcessor reloaded;
    const auto reloadResult = reloaded.restoreState (firstSave.getData(),
                                                     static_cast<int> (firstSave.getSize()));
    const auto secondSave = serialiseProcessorStateBytes (reloaded);
    test.expect (reloadResult.code == StateContract::RestoreCode::success
                     && reloaded.getContourContract()
                            == StateContract::ContourContract::legacyCrossedContours
                     && firstSave == secondSave,
                 std::string { description }
                     + " save-again must remain deterministic v2 legacy mode");

    MoogMiniAudioProcessor repeated;
    const auto repeatedResult = repeated.restoreState (
        legacyBinary.getData(), static_cast<int> (legacyBinary.getSize()));
    test.expect (repeatedResult.code == StateContract::RestoreCode::successMigratedV0
                     && firstSave == serialiseProcessorStateBytes (repeated),
                 std::string { description } + " repeated v0 migration must be deterministic");
}

void testStateV2Migration (TestContext& test)
{
    expectMigrationFixture (
        test,
        legacyFixtureFile ("Tests/fixtures/state/legacy-default-state.xml"),
        legacyFixtureFile ("Tests/fixtures/state/migrated-default-state-v2.xml"),
        "legacy default state");
    expectMigrationFixture (
        test,
        legacyFixtureFile ("Tests/fixtures/state/legacy-representative-state.xml"),
        legacyFixtureFile ("Tests/fixtures/state/migrated-representative-state-v2.xml"),
        "legacy representative state");

    const auto representative = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-representative-state.xml"));
    test.expect (representative != nullptr, "partial v0 source fixture must parse");
    if (representative != nullptr)
    {
        auto partial = juce::parseXML (representative->toString());
        auto* missing = partial == nullptr ? nullptr
                                           : findParameterXml (*partial, "filterAttackTimeKnob");
        test.expect (missing != nullptr, "partial v0 test must find its one removed contour");
        if (partial != nullptr && missing != nullptr)
        {
            partial->removeChildElement (missing, true);
            const auto binary = binaryFromXml (*partial);
            MoogMiniAudioProcessor migrated;
            const auto result = migrated.restoreState (binary.getData(),
                                                       static_cast<int> (binary.getSize()));
            test.expect (result.code == StateContract::RestoreCode::successMigratedV0,
                         "partial v0 must migrate successfully");
            test.expect (physicalParameterValue (migrated, "filterAttackTimeKnob") == 0.0f,
                         "partial v0 must use the missing contour's historical default only");
            test.expect (physicalParameterValue (migrated, "filterDecayTimeKnob") == 0.75f
                             && physicalParameterValue (migrated, "loudnessAttackTimeKnob") == 0.75f
                             && physicalParameterValue (migrated, "loudnessDecayTimeKnob") == 0.75f
                             && physicalParameterValue (migrated, "filterSustainKnob") == 8.0f
                             && physicalParameterValue (migrated, "loudnessSustainLevelKnob") == 8.0f,
                         "partial v0 must preserve every other contour value without swapping");
            const auto output = serialiseProcessorState (migrated);
            const auto* log = output == nullptr ? nullptr : output->getChildByName ("migrationLog");
            int preciseWarnings = 0;
            if (log != nullptr)
                for (auto* entry = log->getFirstChildElement(); entry != nullptr;
                     entry = entry->getNextElement())
                    preciseWarnings += entry->getStringAttribute ("warningCode")
                                           == "defaulted.parameter.filterAttackTimeKnob" ? 1 : 0;
            test.expect (preciseWarnings == 1,
                         "partial v0 must log its exact missing contour warning once");
        }

        auto distinctContours = copyStateXml (*representative);
        if (distinctContours != nullptr)
        {
            constexpr std::array distinctValues {
                std::pair { "filterAttackTimeKnob", "0.11" },
                std::pair { "filterDecayTimeKnob", "0.22" },
                std::pair { "filterSustainKnob", "3" },
                std::pair { "loudnessAttackTimeKnob", "0.66" },
                std::pair { "loudnessDecayTimeKnob", "0.77" },
                std::pair { "loudnessSustainLevelKnob", "9" }
            };
            for (const auto& [id, value] : distinctValues)
                findParameterXml (*distinctContours, id)->setAttribute ("value", value);
            const auto binary = binaryFromXml (*distinctContours);
            MoogMiniAudioProcessor migrated;
            const auto result = migrated.restoreState (binary.getData(),
                                                       static_cast<int> (binary.getSize()));
            test.expect (result.succeeded()
                             && migrated.getContourContract()
                                    == StateContract::ContourContract::legacyCrossedContours
                             && std::abs (physicalParameterValue (migrated, "filterAttackTimeKnob")
                                             - 0.11f) < 1.0e-6f
                             && std::abs (physicalParameterValue (migrated, "filterDecayTimeKnob")
                                             - 0.22f) < 1.0e-6f
                             && physicalParameterValue (migrated, "filterSustainKnob") == 3.0f
                             && std::abs (physicalParameterValue (migrated, "loudnessAttackTimeKnob")
                                             - 0.66f) < 1.0e-6f
                             && std::abs (physicalParameterValue (migrated, "loudnessDecayTimeKnob")
                                             - 0.77f) < 1.0e-6f
                             && physicalParameterValue (migrated, "loudnessSustainLevelKnob") == 9.0f,
                         "v0 migration must never swap any of six distinct contour ID values");
        }
    }

    const auto legacyDefault = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (legacyDefault != nullptr)
    {
        const auto binary = binaryFromXml (*legacyDefault);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(),
                                                   static_cast<int> (binary.getSize()));
        test.expect (result.succeeded() && physicalParameterValue (migrated, "tune") == 0.0f,
                     "present v0 Tune index 0 must restore as 0, not new-instance index 5");

        auto missingTune = copyStateXml (*legacyDefault);
        auto* tune = missingTune == nullptr ? nullptr : findParameterXml (*missingTune, "tune");
        if (missingTune != nullptr && tune != nullptr)
        {
            missingTune->removeChildElement (tune, true);
            const auto missingTuneBinary = binaryFromXml (*missingTune);
            MoogMiniAudioProcessor missingTuneMigrated;
            const auto missingTuneResult = missingTuneMigrated.restoreState (
                missingTuneBinary.getData(), static_cast<int> (missingTuneBinary.getSize()));
            const auto missingTuneOutput = serialiseProcessorState (missingTuneMigrated);
            const auto* log = missingTuneOutput == nullptr ? nullptr
                                                           : missingTuneOutput->getChildByName ("migrationLog");
            int warningCount = 0;
            if (log != nullptr)
                for (auto* entry = log->getFirstChildElement(); entry != nullptr;
                     entry = entry->getNextElement())
                    warningCount += entry->getStringAttribute ("warningCode")
                                        == "defaulted.parameter.tune" ? 1 : 0;
            test.expect (missingTuneResult.succeeded()
                             && physicalParameterValue (missingTuneMigrated, "tune") == 0.0f
                             && warningCount == 1,
                         "missing v0 Tune must use historical index 0 and log its exact warning");
        }
    }

    auto allKnown = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    test.expect (allKnown != nullptr, "all-known v0 source must parse");
    if (allKnown != nullptr)
    {
        constexpr std::array appendedValues {
            std::pair { "keyboard.priorityMode", 2.0 },
            std::pair { "keyboard.triggerMode", 1.0 },
            std::pair { "output.mainEnabled", 0.0 },
            std::pair { "output.phonesEnabled", 0.0 }
        };
        for (const auto& [id, value] : appendedValues)
        {
            auto parameter = std::make_unique<juce::XmlElement> ("PARAM");
            parameter->setAttribute ("id", id);
            parameter->setAttribute ("value", value);
            allKnown->addChildElement (parameter.release());
        }
        const auto binary = binaryFromXml (*allKnown);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(),
                                                   static_cast<int> (binary.getSize()));
        test.expect (result.succeeded()
                         && physicalParameterValue (migrated, "keyboard.priorityMode") == 2.0f
                         && physicalParameterValue (migrated, "keyboard.triggerMode") == 1.0f
                         && physicalParameterValue (migrated, "output.mainEnabled") == 0.0f
                         && physicalParameterValue (migrated, "output.phonesEnabled") == 0.0f,
                     "v0 must recognize and preserve supplied values for all 48 current IDs");
    }

    auto legacyPreset = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (legacyPreset != nullptr)
    {
        legacyPreset->setAttribute ("presetName", "Legacy Bass");
        const auto binary = binaryFromXml (*legacyPreset);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(),
                                                   static_cast<int> (binary.getSize()));
        const auto output = serialiseProcessorState (migrated);
        const auto* extensions = output == nullptr ? nullptr : output->getChildByName ("extensions");
        const auto* preset = extensions == nullptr ? nullptr
                                                    : extensions->getChildByName ("legacyPreset");
        test.expect (result.succeeded() && preset != nullptr
                         && preset->getStringAttribute ("presetName") == "Legacy Bass",
                     "bounded legacy presetName must survive migration as opaque extension metadata");

        legacyPreset->setAttribute ("unsafeProperty", "reject");
        const auto unsafeBinary = binaryFromXml (*legacyPreset);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (unsafeBinary.getData(),
                                           static_cast<int> (unsafeBinary.getSize())).code
                         == StateContract::RestoreCode::unexpectedProperty,
                     "unknown legacy root properties must remain rejected");
    }

    auto unknownLegacy = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (unknownLegacy != nullptr)
    {
        auto unknown = std::make_unique<juce::XmlElement> ("PARAM");
        unknown->setAttribute ("id", "vendor.safeValue");
        unknown->setAttribute ("value", "12.5");
        unknownLegacy->addChildElement (unknown.release());
        const auto binary = binaryFromXml (*unknownLegacy);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto output = serialiseProcessorState (migrated);
        const auto* extensions = output == nullptr ? nullptr : output->getChildByName ("extensions");
        const auto* legacyParameters = extensions == nullptr ? nullptr
                                                              : extensions->getChildByName ("legacyParameters");
        const auto* preserved = legacyParameters == nullptr ? nullptr
                                                             : legacyParameters->getChildByName ("legacyParameter");
        test.expect (result.succeeded() && preserved != nullptr
                         && preserved->getStringAttribute ("id") == "vendor.safeValue"
                         && preserved->getDoubleAttribute ("value") == 12.5,
                     "well-formed unknown v0 PARAM must survive as opaque extension data");

        findParameterXml (*unknownLegacy, "vendor.safeValue")->setAttribute ("value", "NaN");
        const auto unsafeBinary = binaryFromXml (*unknownLegacy);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (unsafeBinary.getData(),
                                           static_cast<int> (unsafeBinary.getSize())).code
                         == StateContract::RestoreCode::nonFiniteValue,
                     "non-finite unknown v0 PARAM must be rejected");
    }

    auto semanticA = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    auto semanticB = semanticA == nullptr ? nullptr : copyStateXml (*semanticA);
    if (semanticA != nullptr && semanticB != nullptr)
    {
        auto* first = semanticB->getFirstChildElement();
        semanticB->removeChildElement (first, false);
        semanticB->addChildElement (first);
        findParameterXml (*semanticB, "filterCutoff")->setAttribute ("value", "0.500000");
        const auto binaryA = binaryFromXml (*semanticA);
        const auto binaryB = binaryFromXml (*semanticB);
        MoogMiniAudioProcessor migratedA;
        MoogMiniAudioProcessor migratedB;
        const auto resultA = migratedA.restoreState (binaryA.getData(), static_cast<int> (binaryA.getSize()));
        const auto resultB = migratedB.restoreState (binaryB.getData(), static_cast<int> (binaryB.getSize()));
        test.expect (resultA.succeeded() && resultB.succeeded()
                         && serialiseProcessorStateBytes (migratedA)
                                == serialiseProcessorStateBytes (migratedB),
                     "semantically identical v0 trees must produce identical hashes and v2 bytes");
    }
}

void testStateV2ExtensionRoundTrip (TestContext& test)
{
    MoogMiniAudioProcessor source;
    auto xml = serialiseProcessorState (source);
    test.expect (xml != nullptr, "safe-extension source state must serialize");
    if (xml == nullptr)
        return;
    auto* extensions = xml->getChildByName ("extensions");
    test.expect (extensions != nullptr, "canonical state must expose extensions child");
    if (extensions == nullptr)
        return;

    auto vendor = std::make_unique<juce::XmlElement> ("vendorData");
    vendor->setAttribute ("vendor", "example.test");
    vendor->setAttribute ("payload", "safe-value-01");
    auto leaf = std::make_unique<juce::XmlElement> ("nestedValue");
    leaf->setAttribute ("name", "mode");
    leaf->setAttribute ("value", "alpha");
    vendor->addChildElement (leaf.release());
    extensions->addChildElement (vendor.release());
    const auto expectedExtensions = extensions->toString();

    const auto input = binaryFromXml (*xml);
    MoogMiniAudioProcessor restored;
    const auto result = restored.restoreState (input.getData(), static_cast<int> (input.getSize()));
    const auto first = serialiseProcessorStateBytes (restored);
    auto firstXml = serialiseProcessorState (restored);
    test.expect (result.code == StateContract::RestoreCode::success,
                 "safe v2 extension state must restore successfully");
    test.expect (input == first,
                 "canonical v2 serialize-parse-serialize must be byte-identical");
    test.expect (firstXml != nullptr
                     && firstXml->getChildByName ("extensions") != nullptr
                     && firstXml->getChildByName ("extensions")->toString() == expectedExtensions,
                 "safe extension subtree must be preserved byte-for-byte");

    MoogMiniAudioProcessor repeated;
    const auto repeatedResult = repeated.restoreState (first.getData(), static_cast<int> (first.getSize()));
    test.expect (repeatedResult.code == StateContract::RestoreCode::success
                     && first == serialiseProcessorStateBytes (repeated),
                 "repeated native v2 round trips must be byte-deterministic");
}

std::unique_ptr<juce::XmlElement> copyStateXml (const juce::XmlElement& xml)
{
    return juce::parseXML (xml.toString());
}

void testStateV2FailuresAreAtomic (TestContext& test)
{
    constexpr auto codes = std::to_array<std::pair<StateContract::RestoreCode,
                                                     std::string_view>> ({
        { StateContract::RestoreCode::success, "success"sv },
        { StateContract::RestoreCode::successMigratedV0, "success_migrated_v0"sv },
        { StateContract::RestoreCode::malformedData, "malformed_data"sv },
        { StateContract::RestoreCode::wrongRoot, "wrong_root"sv },
        { StateContract::RestoreCode::missingStateVersion, "missing_state_version"sv },
        { StateContract::RestoreCode::invalidStateVersion, "invalid_state_version"sv },
        { StateContract::RestoreCode::nonIntegerStateVersion, "non_integer_state_version"sv },
        { StateContract::RestoreCode::negativeStateVersion, "negative_state_version"sv },
        { StateContract::RestoreCode::futureStateVersion, "future_state_version"sv },
        { StateContract::RestoreCode::unsupportedStateVersion, "unsupported_state_version"sv },
        { StateContract::RestoreCode::missingRequiredProperty, "missing_required_property"sv },
        { StateContract::RestoreCode::missingRequiredChild, "missing_required_child"sv },
        { StateContract::RestoreCode::duplicateRequiredChild, "duplicate_required_child"sv },
        { StateContract::RestoreCode::duplicateParameterId, "duplicate_parameter_id"sv },
        { StateContract::RestoreCode::missingParameterId, "missing_parameter_id"sv },
        { StateContract::RestoreCode::unknownParameterId, "unknown_parameter_id"sv },
        { StateContract::RestoreCode::invalidNumericValue, "invalid_numeric_value"sv },
        { StateContract::RestoreCode::nonFiniteValue, "non_finite_value"sv },
        { StateContract::RestoreCode::outOfRangeValue, "out_of_range_value"sv },
        { StateContract::RestoreCode::invalidDiscreteValue, "invalid_discrete_value"sv },
        { StateContract::RestoreCode::unsafeExtensionStructure, "unsafe_extension_structure"sv },
        { StateContract::RestoreCode::unexpectedProperty, "unexpected_property"sv },
        { StateContract::RestoreCode::unexpectedChild, "unexpected_child"sv },
        { StateContract::RestoreCode::invalidCompatibility, "invalid_compatibility"sv },
        { StateContract::RestoreCode::invalidUi, "invalid_ui"sv },
        { StateContract::RestoreCode::invalidRecreation, "invalid_recreation"sv },
        { StateContract::RestoreCode::invalidMigrationLog, "invalid_migration_log"sv }
    });
    std::set<std::string> stableCodes;
    for (const auto& [code, expected] : codes)
    {
        const auto actual = StateContract::stableCode (code);
        test.expect (actual == expected, "every restore result must expose its exact stable code");
        test.expect (stableCodes.emplace (actual).second,
                     "every restore result stable code must be unique");
    }

    MoogMiniAudioProcessor sentinel;
    setParameter (sentinel, "filterCutoff", 0.75f, test);
    setParameter (sentinel, "tune", 0.8f, test);
    const auto validSentinel = serialiseProcessorStateBytes (sentinel);
    const auto success = sentinel.restoreState (validSentinel.getData(),
                                                static_cast<int> (validSentinel.getSize()));
    test.expect (success.code == StateContract::RestoreCode::success
                     && ! sentinel.shouldAutoLoadLastPreset(),
                 "sentinel must first complete one successful host restore");
    const auto beforeFailure = serialiseProcessorStateBytes (sentinel);
    const auto contourBefore = sentinel.getContourContract();

    const auto baseline = serialiseProcessorState (sentinel);
    test.expect (baseline != nullptr, "failure corpus baseline must serialize");
    if (baseline == nullptr)
        return;

    const auto expectRejected = [&] (std::unique_ptr<juce::XmlElement> candidate,
                                      StateContract::RestoreCode expected,
                                      std::string_view description)
    {
        test.expect (candidate != nullptr, std::string { description } + " XML must be constructible");
        if (candidate == nullptr)
            return;
        const auto binary = binaryFromXml (*candidate);
        const auto result = sentinel.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        test.expect (result.code == expected,
                     std::string { description } + " must return the exact stable error code");
        test.expect (serialiseProcessorStateBytes (sentinel) == beforeFailure,
                     std::string { description } + " must leave every live serialized byte unchanged");
        test.expect (! sentinel.shouldAutoLoadLastPreset()
                         && sentinel.getContourContract() == contourBefore,
                     std::string { description }
                         + " must preserve the previous successful lifecycle and metadata");
    };

    const auto malformed = sentinel.restoreState ("not-a-juce-state", 16);
    test.expect (malformed.code == StateContract::RestoreCode::malformedData
                     && serialiseProcessorStateBytes (sentinel) == beforeFailure,
                 "malformed binary/XML must fail atomically");
    const auto truncated = sentinel.restoreState (beforeFailure.getData(),
                                                  static_cast<int> (beforeFailure.getSize() / 2));
    test.expect (truncated.code == StateContract::RestoreCode::malformedData
                     && serialiseProcessorStateBytes (sentinel) == beforeFailure,
                 "truncated binary must fail atomically");

    expectRejected (std::make_unique<juce::XmlElement> ("wrongRoot"),
                    StateContract::RestoreCode::wrongRoot, "wrong root");

    auto missingVersion = copyStateXml (*baseline);
    missingVersion->removeAttribute ("stateVersion");
    expectRejected (std::move (missingVersion), StateContract::RestoreCode::missingStateVersion,
                    "missing version");
    for (const auto& [version, expected, description] :
         std::to_array<std::tuple<const char*, StateContract::RestoreCode, const char*>> ({
             { "abc", StateContract::RestoreCode::invalidStateVersion, "invalid version" },
             { "2.5", StateContract::RestoreCode::nonIntegerStateVersion, "noninteger version" },
             { "-1", StateContract::RestoreCode::negativeStateVersion, "negative version" },
             { "1", StateContract::RestoreCode::unsupportedStateVersion, "unsupported past version" },
             { "3", StateContract::RestoreCode::futureStateVersion, "future version" }
         }))
    {
        auto candidate = copyStateXml (*baseline);
        candidate->setAttribute ("stateVersion", version);
        expectRejected (std::move (candidate), expected, description);
    }

    auto missingProperty = copyStateXml (*baseline);
    missingProperty->removeAttribute ("engineVersion");
    expectRejected (std::move (missingProperty),
                    StateContract::RestoreCode::missingRequiredProperty,
                    "missing required property");
    auto unexpectedProperty = copyStateXml (*baseline);
    unexpectedProperty->setAttribute ("unknownProperty", "reject");
    expectRejected (std::move (unexpectedProperty),
                    StateContract::RestoreCode::unexpectedProperty,
                    "unexpected v2 property");
    auto unexpectedChild = copyStateXml (*baseline);
    unexpectedChild->addChildElement (new juce::XmlElement ("unknownChild"));
    expectRejected (std::move (unexpectedChild), StateContract::RestoreCode::unexpectedChild,
                    "unexpected v2 child");

    auto invalidCompatibility = copyStateXml (*baseline);
    invalidCompatibility->getChildByName ("compatibility")
        ->setAttribute ("contourContract", "invalidContours");
    expectRejected (std::move (invalidCompatibility),
                    StateContract::RestoreCode::invalidCompatibility,
                    "invalid compatibility metadata");
    auto invalidUi = copyStateXml (*baseline);
    invalidUi->getChildByName ("ui")->setAttribute ("viewMode", "invalidView");
    expectRejected (std::move (invalidUi), StateContract::RestoreCode::invalidUi,
                    "invalid UI metadata");
    auto invalidRecreation = copyStateXml (*baseline);
    invalidRecreation->getChildByName ("recreation")->setAttribute ("enabled", "2");
    expectRejected (std::move (invalidRecreation),
                    StateContract::RestoreCode::invalidRecreation,
                    "invalid recreation metadata");
    auto invalidMigrationLog = copyStateXml (*baseline);
    auto logEntry = std::make_unique<juce::XmlElement> ("ENTRY");
    logEntry->setAttribute ("from", "0");
    logEntry->setAttribute ("to", "2");
    logEntry->setAttribute ("action", "unexpectedNativeEntry");
    logEntry->setAttribute ("warningCode", "unexpected.native.entry");
    invalidMigrationLog->getChildByName ("migrationLog")
        ->addChildElement (logEntry.release());
    expectRejected (std::move (invalidMigrationLog),
                    StateContract::RestoreCode::invalidMigrationLog,
                    "invalid native migration log");

    auto missingChild = copyStateXml (*baseline);
    missingChild->removeChildElement (missingChild->getChildByName ("migrationLog"), true);
    expectRejected (std::move (missingChild), StateContract::RestoreCode::missingRequiredChild,
                    "missing reserved child");
    auto duplicateChild = copyStateXml (*baseline);
    duplicateChild->addChildElement (
        new juce::XmlElement (*duplicateChild->getChildByName ("ui")));
    expectRejected (std::move (duplicateChild), StateContract::RestoreCode::duplicateRequiredChild,
                    "duplicate reserved child");

    auto duplicateParameter = copyStateXml (*baseline);
    auto* duplicateParameters = duplicateParameter->getChildByName ("parameters");
    duplicateParameters->addChildElement (
        new juce::XmlElement (*duplicateParameters->getFirstChildElement()));
    expectRejected (std::move (duplicateParameter), StateContract::RestoreCode::duplicateParameterId,
                    "duplicate parameter");
    auto missingParameter = copyStateXml (*baseline);
    auto* missingParameters = missingParameter->getChildByName ("parameters");
    missingParameters->removeChildElement (missingParameters->getFirstChildElement(), true);
    expectRejected (std::move (missingParameter), StateContract::RestoreCode::missingParameterId,
                    "missing parameter");
    auto unknownParameter = copyStateXml (*baseline);
    findParameterXml (*unknownParameter, "osc1Waveform")->setAttribute ("id", "unknown.direct");
    expectRejected (std::move (unknownParameter), StateContract::RestoreCode::unknownParameterId,
                    "unknown direct v2 parameter");

    for (const auto& [id, value, expected, description] :
         std::to_array<std::tuple<const char*, const char*, StateContract::RestoreCode, const char*>> ({
             { "filterCutoff", "NaN", StateContract::RestoreCode::nonFiniteValue, "NaN value" },
             { "filterCutoff", "Inf", StateContract::RestoreCode::nonFiniteValue, "infinite value" },
             { "filterCutoff", "0.5junk", StateContract::RestoreCode::invalidNumericValue, "trailing-junk value" },
             { "filterCutoff", "2.0", StateContract::RestoreCode::outOfRangeValue, "out-of-range value" },
             { "osc1Waveform", "1.5", StateContract::RestoreCode::invalidDiscreteValue, "non-discrete choice" },
             { "output.mainEnabled", "0.5", StateContract::RestoreCode::invalidDiscreteValue, "non-boolean value" }
         }))
    {
        auto candidate = copyStateXml (*baseline);
        findParameterXml (*candidate, id)->setAttribute ("value", value);
        expectRejected (std::move (candidate), expected, description);
    }

    auto unsafeExtension = copyStateXml (*baseline);
    unsafeExtension->getChildByName ("extensions")
        ->addChildElement (new juce::XmlElement ("parameters"));
    expectRejected (std::move (unsafeExtension),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "reserved extension nesting");

    auto overlongName = copyStateXml (*baseline);
    overlongName->getChildByName ("extensions")->addChildElement (
        new juce::XmlElement (juce::String::repeatedString (
            "n", StateContract::maxExtensionNameBytes + 1)));
    expectRejected (std::move (overlongName),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "overlong extension name");

    auto overlongValue = copyStateXml (*baseline);
    auto valueNode = std::make_unique<juce::XmlElement> ("safeNode");
    valueNode->setAttribute (
        "payload", juce::String::repeatedString ("v", StateContract::maxExtensionValueBytes + 1));
    overlongValue->getChildByName ("extensions")->addChildElement (valueNode.release());
    expectRejected (std::move (overlongValue),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "overlong extension value");

    auto tooManyNodes = copyStateXml (*baseline);
    auto* nodeContainer = tooManyNodes->getChildByName ("extensions");
    for (int index = 0; index <= StateContract::maxExtensionNodes; ++index)
        nodeContainer->addChildElement (new juce::XmlElement ("safeNode"));
    expectRejected (std::move (tooManyNodes),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "extension node-count limit");

    auto overTotalBudget = copyStateXml (*baseline);
    auto budgetNode = std::make_unique<juce::XmlElement> ("safeNode");
    const auto maximumValue = juce::String::repeatedString (
        "v", StateContract::maxExtensionValueBytes);
    const auto valuesToExceedBudget = StateContract::maxExtensionTotalValueBytes
                                   / StateContract::maxExtensionValueBytes + 1;
    for (int index = 0; index < valuesToExceedBudget; ++index)
        budgetNode->setAttribute ("value" + juce::String { index }, maximumValue);
    overTotalBudget->getChildByName ("extensions")->addChildElement (budgetNode.release());
    expectRejected (std::move (overTotalBudget),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "extension total-value budget");

    auto deepestFault = copyStateXml (*baseline);
    auto* cursor = deepestFault->getChildByName ("extensions");
    for (int depth = 0; depth <= StateContract::maxExtensionDepth; ++depth)
    {
        auto* next = new juce::XmlElement ("safeNode");
        cursor->addChildElement (next);
        cursor = next;
    }
    expectRejected (std::move (deepestFault),
                    StateContract::RestoreCode::unsafeExtensionStructure,
                    "deepest extension boundary fault");

    auto lastParameterFault = copyStateXml (*baseline);
    auto* parameters = lastParameterFault->getChildByName ("parameters");
    parameters->getChildElement (parameters->getNumChildElements() - 1)
        ->setAttribute ("value", "0.5");
    expectRejected (std::move (lastParameterFault),
                    StateContract::RestoreCode::invalidDiscreteValue,
                    "last-parameter fault");

    const auto processorSource = legacyFixtureFile ("Source/PluginProcessor.cpp").loadFileAsString();
    const auto processStart = processorSource.indexOf ("void MoogMiniAudioProcessor::processBlock");
    const auto stateStart = processorSource.indexOf ("void MoogMiniAudioProcessor::getStateInformation");
    const auto processSource = processorSource.substring (processStart, stateStart);
    test.expect (processStart >= 0 && stateStart > processStart
                     && ! processSource.contains ("StateContract")
                     && ! processSource.contains ("copyState")
                     && ! processSource.contains ("replaceState")
                     && ! processSource.contains ("Xml"),
                 "processBlock must contain no state/XML/copy/restore operation");
}

void testMidiSampleZeroSmoke (TestContext& test)
{
    MoogMiniAudioProcessor processor;
    setParameter (processor, "osc1OnOff", 1.0f, test);
    setParameter (processor, "osc1Vol", 1.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);
    setParameter (processor, "filterCutoff", 1.0f, test);
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 512);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    processor.processBlock (buffer, midi);

    test.expect (isFinite (buffer), "sample-zero MIDI note-on render must be finite");
    test.expect (absoluteSum (buffer) > 0.01,
                 "sample-zero MIDI note-on under the existing oscillator patch must be non-silent");
}

void testRealtimeSmoke (TestContext& test)
{
    MoogMiniAudioProcessor processor;
    setParameter (processor, "a440HzOnOff", 1.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);

    for (const auto blockSize : { 0, 1, 128, 511 })
    {
        processor.setRateAndBufferSizeDetails (48000.0, blockSize);
        processor.prepareToPlay (48000.0, blockSize);
        juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), blockSize);
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        test.expect (isFinite (buffer), "zero and bounded realtime smoke buffers must be finite");
        processor.releaseResources();
        processor.reset();
    }
}

int runMode (std::string_view mode)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (mode.starts_with ("capture-"))
        return captureLegacyFixture (mode);

    TestContext test;

    if (mode == "unit")
        testUnitContract (test);
    else if (mode == "state")
        testStateSmoke (test);
    else if (mode == "state-v2")
    {
        testStateV2Contract (test);
        testStateV2Migration (test);
        testStateV2ExtensionRoundTrip (test);
        testStateV2FailuresAreAtomic (test);
    }
    else if (mode == "fixtures")
        testLegacyParameterFixtures (test);
    else if (mode == "registry")
        testParameterRegistry (test);
    else if (mode == "dsp")
        testProcessingContract (test);
    else if (mode == "midi")
        testMidiSampleZeroSmoke (test);
    else if (mode == "realtime")
        testRealtimeSmoke (test);
    else if (mode == "host")
    {
        testGeneratedWrapperContract (test);
        MoogMiniAudioProcessor processor;
        const auto topologyReady = testDefaultBusContract (processor, test);
        testVST3InputRole (processor, test);
        if (topologyReady)
        {
            testLayoutTable (processor, test);
            testExternalInputRouting (test);
        }
        else
        {
            test.expect (false,
                         "layout and routing tables require the exact declared bus topology");
        }
    }
    else
    {
        std::cerr << "Unknown or missing ModelDTests mode: " << mode << '\n';
        return 2;
    }

    return test.result();
}
} // namespace

int main (int argc, char* argv[])
{
    return argc == 2 ? runMode (argv[1]) : runMode ("");
}
