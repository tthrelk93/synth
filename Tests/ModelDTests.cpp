#include "PluginProcessor.h"
#include "PresetManager.h"

#include "ParameterRegistry.h"
#include "ParameterSnapshotCapture.h"

#include "PitchDomain.h"
#include "PitchWheelSlider.h"

#include "ParameterBinding.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#define SYNTH_STRINGIFY_IMPL(value) #value
#define SYNTH_STRINGIFY(value) SYNTH_STRINGIFY_IMPL(value)

namespace
{
using namespace std::literals;

#if ! JUCE_MAC
class ConditionalDispatchLoopStopper final : private juce::Timer
{
public:
    ConditionalDispatchLoopStopper (std::function<bool()> completionCondition,
                                    double watchdogMilliseconds)
        : condition (std::move (completionCondition)),
          deadline (juce::Time::getMillisecondCounterHiRes() + watchdogMilliseconds)
    {
        startTimer (5);
    }

    bool completed() const noexcept
    {
        return conditionWasMet;
    }

private:
    void timerCallback() override
    {
        conditionWasMet = condition();

        if (! conditionWasMet
            && juce::Time::getMillisecondCounterHiRes() < deadline)
            return;

        stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

    std::function<bool()> condition;
    double deadline;
    bool conditionWasMet = false;
};
#endif

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

juce::MemoryBlock binaryFromRawXmlText (const juce::String& xml)
{
    juce::MemoryBlock result;
    juce::MemoryOutputStream output { result, false };
    output.writeInt (0x21324356);
    output.writeInt (xml.getNumBytesAsUTF8());
    output.write (xml.toRawUTF8(), static_cast<std::size_t> (xml.getNumBytesAsUTF8()));
    output.writeByte (0);
    return result;
}

juce::MemoryBlock serialiseProcessorStateBytes (MoogMiniAudioProcessor& processor)
{
    juce::MemoryBlock result;
    processor.getStateInformation (result);
    return result;
}

juce::XmlElement* findExactChildXml (juce::XmlElement& root, std::string_view tagName)
{
    for (auto* child = root.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
        if (std::string_view { child->getTagName().toRawUTF8() } == tagName)
            return child;
    return nullptr;
}

juce::XmlElement* findParameterXml (juce::XmlElement& root, std::string_view id)
{
    auto* parameters = root.getTagName() == "parameters" || root.getTagName() == "Parameters"
                         ? &root : findExactChildXml (root, "parameters");
    if (parameters == nullptr)
        return nullptr;
    for (auto* child = parameters->getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
        if (child->getTagName() == "PARAM"
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

std::atomic<float>* rawParameterForTest (MoogMiniAudioProcessor& processor,
                                         ParameterRegistry::Key key)
{
    const auto id = ParameterRegistry::descriptor (key).id;
    return processor.apvts.getRawParameterValue (
        juce::String { id.data(), id.size() });
}

std::array<float, ParameterRegistry::parameterCount> snapshotPhysicalValues (
    const MoogMiniAudioProcessor::ParameterSnapshot& snapshot)
{
    return {
        static_cast<float> (snapshot.oscillator1.waveform),
        static_cast<float> (snapshot.oscillator2.waveform),
        static_cast<float> (snapshot.oscillator3.waveform),
        static_cast<float> (snapshot.oscillator1.range),
        static_cast<float> (snapshot.oscillator2.range),
        static_cast<float> (snapshot.oscillator3.range),
        static_cast<float> (snapshot.oscillator1.level),
        static_cast<float> (snapshot.oscillator2.level),
        static_cast<float> (snapshot.oscillator3.level),
        static_cast<float> (snapshot.masterTune),
        static_cast<float> (snapshot.oscillator2.detune),
        static_cast<float> (snapshot.oscillator3.detune),
        snapshot.filterCutoff,
        static_cast<float> (snapshot.filterEmphasis),
        static_cast<float> (snapshot.filterContour),
        static_cast<float> (snapshot.outputLevel),
        static_cast<float> (snapshot.externalInputLevel),
        static_cast<float> (snapshot.glide),
        static_cast<float> (snapshot.modulationMix),
        snapshot.contours.filterAttack,
        snapshot.contours.filterDecay,
        snapshot.contours.loudnessAttack,
        snapshot.contours.loudnessDecay,
        snapshot.contours.filterSustain * 10.0f,
        static_cast<float> (snapshot.noiseLevel),
        snapshot.contours.loudnessSustain * 10.0f,
        static_cast<float> (snapshot.phonesLevel),
        snapshot.feedback,
        snapshot.modulationWheel,
        snapshot.pitchWheel,
        snapshot.oscillator1.enabled ? 1.0f : 0.0f,
        snapshot.oscillator2.enabled ? 1.0f : 0.0f,
        snapshot.oscillator3.enabled ? 1.0f : 0.0f,
        snapshot.a440Enabled ? 1.0f : 0.0f,
        snapshot.oscillator3KeyboardControl ? 1.0f : 0.0f,
        snapshot.oscillatorModulationEnabled ? 1.0f : 0.0f,
        snapshot.noiseEnabled ? 1.0f : 0.0f,
        snapshot.externalInputEnabled ? 1.0f : 0.0f,
        snapshot.pinkNoise ? 1.0f : 0.0f,
        snapshot.filterModulationEnabled ? 1.0f : 0.0f,
        snapshot.keyboardControl1 ? 1.0f : 0.0f,
        snapshot.keyboardControl2 ? 1.0f : 0.0f,
        snapshot.contours.decayEnabled ? 1.0f : 0.0f,
        snapshot.glideEnabled ? 1.0f : 0.0f,
        static_cast<float> (snapshot.priority),
        static_cast<float> (snapshot.trigger),
        snapshot.mainOutputEnabled ? 1.0f : 0.0f,
        snapshot.phonesOutputEnabled ? 1.0f : 0.0f
    };
}

float expectedSanitizedValue (const ParameterRegistry::Descriptor& descriptor,
                              float value)
{
    if (! std::isfinite (value))
        value = descriptor.physicalDefault;
    value = std::clamp (value, descriptor.rangeStart, descriptor.rangeEnd);
    if (descriptor.kind == ParameterRegistry::Kind::choice)
        return std::round (value);
    if (descriptor.kind == ParameterRegistry::Kind::boolean)
        return value >= 0.5f ? 1.0f : 0.0f;
    return value;
}

std::string_view priorityModeName (MoogMiniAudioProcessor::PriorityMode mode)
{
    switch (mode)
    {
        case MoogMiniAudioProcessor::PriorityMode::low: return "low";
        case MoogMiniAudioProcessor::PriorityMode::high: return "high";
        case MoogMiniAudioProcessor::PriorityMode::last: return "last";
    }
    return "invalid";
}

std::string_view triggerModeName (MoogMiniAudioProcessor::TriggerMode mode)
{
    return mode == MoogMiniAudioProcessor::TriggerMode::multi ? "multi" : "single";
}

juce::String makeParameterSnapshotV2Fixture()
{
    MoogMiniAudioProcessor processor;
    const auto descriptors = ParameterRegistry::descriptors();
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto key = static_cast<ParameterRegistry::Key> (index);
        const auto& descriptor = descriptors[index];
        float value = 0.0f;
        if (descriptor.kind == ParameterRegistry::Kind::floating)
            value = static_cast<float> (index + 1) / 50.0f;
        else if (descriptor.kind == ParameterRegistry::Kind::choice)
            value = static_cast<float> ((index * 3 + 1)
                      % static_cast<std::size_t> (descriptor.rangeEnd + 1.0f));
        else
            value = (index % 2u) == 0u ? 1.0f : 0.0f;
        rawParameterForTest (processor, key)->store (value, std::memory_order_relaxed);
    }
    const auto distinct = processor.captureParameterSnapshot ({});
    const auto distinctValues = snapshotPhysicalValues (distinct);
    const auto beforeInvalid = processor.getInvalidParameterValueCount();
    rawParameterForTest (processor, ParameterRegistry::Key::filterCutoff)
        ->store (std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);
    rawParameterForTest (processor, ParameterRegistry::Key::keyboardPriorityMode)
        ->store (std::numeric_limits<float>::infinity(), std::memory_order_relaxed);
    rawParameterForTest (processor, ParameterRegistry::Key::outputMainEnabled)
        ->store (-std::numeric_limits<float>::infinity(), std::memory_order_relaxed);
    const auto sanitized = processor.captureParameterSnapshot (distinct);

    auto root = juce::DynamicObject::Ptr { new juce::DynamicObject };
    root->setProperty ("schema", "model-d.parameter-snapshot.v2");
    juce::Array<juce::var> fields;
    fields.ensureStorageAllocated (static_cast<int> (descriptors.size()));
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        auto field = juce::DynamicObject::Ptr { new juce::DynamicObject };
        field->setProperty ("index", static_cast<int> (index));
        field->setProperty ("id", juce::String { descriptors[index].id.data(),
                                                  descriptors[index].id.size() });
        field->setProperty ("physical", distinctValues[index]);
        fields.add (juce::var { field.get() });
    }
    root->setProperty ("fields", juce::var { fields });
    root->setProperty ("priority", juce::String { priorityModeName (distinct.priority).data(),
                                                   priorityModeName (distinct.priority).size() });
    root->setProperty ("trigger", juce::String { triggerModeName (distinct.trigger).data(),
                                                  triggerModeName (distinct.trigger).size() });
    root->setProperty ("contourContract", "canonicalContours");
    auto diagnostic = juce::DynamicObject::Ptr { new juce::DynamicObject };
    diagnostic->setProperty (
        "invalidCountDelta",
        static_cast<juce::int64> (processor.getInvalidParameterValueCount() - beforeInvalid));
    diagnostic->setProperty ("filterCutoff", sanitized.filterCutoff);
    diagnostic->setProperty ("priority",
                             juce::String { priorityModeName (sanitized.priority).data(),
                                            priorityModeName (sanitized.priority).size() });
    diagnostic->setProperty ("mainOutputEnabled", sanitized.mainOutputEnabled);
    root->setProperty ("sanitization", juce::var { diagnostic.get() });
    return juce::JSON::toString (juce::var { root.get() },
                                 juce::JSON::FormatOptions {}
                                     .withSpacing (juce::JSON::Spacing::none)) + "\n";
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
juce::String makeContourRoutingTraceFixture();

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

    test.expect (state->getTagName() == "Parameters",
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
        test.expect (child->getTagName() == "PARAM",
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

juce::String makeContourRoutingTraceFixture()
{
    const ContourRouting::ContourControls controls {
        0.11f, 0.22f, 0.3f, 0.66f, 0.77f, 0.9f, true
    };
    const auto settingsObject = [] (const ContourRouting::ContourSettings& settings)
    {
        auto result = juce::DynamicObject::Ptr { new juce::DynamicObject };
        result->setProperty ("attack", settings.attack);
        result->setProperty ("decay", settings.decay);
        result->setProperty ("sustain", settings.sustain);
        return juce::var { result.get() };
    };
    const auto routedObject = [&] (const ContourRouting::RoutedContours& routed)
    {
        auto result = juce::DynamicObject::Ptr { new juce::DynamicObject };
        result->setProperty ("physicalFilter", settingsObject (routed.filter));
        result->setProperty ("physicalLoudnessVCA", settingsObject (routed.loudness));
        return juce::var { result.get() };
    };

    auto legacyXml = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-representative-state.xml"));
    if (legacyXml == nullptr)
        return {};
    for (const auto& [id, value] : std::to_array<std::pair<const char*, const char*>> ({
             { "filterAttackTimeKnob", "0.11" }, { "filterDecayTimeKnob", "0.22" },
             { "filterSustainKnob", "3" }, { "loudnessAttackTimeKnob", "0.66" },
             { "loudnessDecayTimeKnob", "0.77" }, { "loudnessSustainLevelKnob", "9" },
             { "decaySwitch", "1" } }))
        findParameterXml (*legacyXml, id)->setAttribute ("value", value);
    const auto legacyBytes = binaryFromXml (*legacyXml);
    MoogMiniAudioProcessor processor;
    if (! processor.restoreState (legacyBytes.getData(), static_cast<int> (legacyBytes.getSize())).succeeded())
        return {};
    const auto before = processor.captureContourSnapshot ({});
    const auto beforeRouted = ContourRouting::route (before.contract, before.controls);
    const auto conversion = processor.convertLegacyContours (
        { true, MoogMiniAudioProcessor::HostAutomationKnowledge::unknown });
    const auto after = processor.captureContourSnapshot (before);
    const auto afterRouted = ContourRouting::route (after.contract, after.controls);

    auto root = juce::DynamicObject::Ptr { new juce::DynamicObject };
    root->setProperty ("schema", "model-d-contour-routing-trace-v1");
    auto controlsObject = juce::DynamicObject::Ptr { new juce::DynamicObject };
    controlsObject->setProperty ("filterAttackTimeKnob", controls.filterAttack);
    controlsObject->setProperty ("filterDecayTimeKnob", controls.filterDecay);
    controlsObject->setProperty ("filterSustainKnob", controls.filterSustain);
    controlsObject->setProperty ("loudnessAttackTimeKnob", controls.loudnessAttack);
    controlsObject->setProperty ("loudnessDecayTimeKnob", controls.loudnessDecay);
    controlsObject->setProperty ("loudnessSustainLevelKnob", controls.loudnessSustain);
    root->setProperty ("distinctControls", juce::var { controlsObject.get() });
    root->setProperty ("canonicalContours", routedObject (ContourRouting::route (
        StateContract::ContourContract::canonicalContours, controls)));
    root->setProperty ("legacyCrossedContours", routedObject (beforeRouted));
    auto convertedObject = juce::DynamicObject::Ptr { new juce::DynamicObject };
    convertedObject->setProperty ("marker", "canonicalContours");
    convertedObject->setProperty ("result", juce::String { conversion.codeString().data(),
                                                            conversion.codeString().size() });
    convertedObject->setProperty ("warning", juce::String { conversion.warningString().data(),
                                                             conversion.warningString().size() });
    convertedObject->setProperty ("logAction", "convertLegacyContours");
    convertedObject->setProperty ("logWarningCode", "legacy.hostAutomationNotRewritten");
    convertedObject->setProperty ("swappedStaticValues", routedObject ({
        { after.controls.filterAttack, after.controls.filterDecay, after.controls.filterSustain },
        { after.controls.loudnessAttack, after.controls.loudnessDecay, after.controls.loudnessSustain }
    }));
    convertedObject->setProperty ("physicalTraceAfter", routedObject (afterRouted));
    root->setProperty ("confirmedConversion", juce::var { convertedObject.get() });
    return juce::JSON::toString (juce::var { root.get() },
                                 juce::JSON::FormatOptions {}
                                     .withSpacing (juce::JSON::Spacing::none)) + "\n";
}

int captureLegacyFixture (std::string_view mode)
{
    if (mode == "capture-parameter-snapshot-v2")
    {
        std::cout << makeParameterSnapshotV2Fixture().toStdString();
        return 0;
    }
    if (mode == "capture-registry-v2")
    {
        std::cout << makeRegistryV2Export().toStdString();
        return 0;
    }
    if (mode == "capture-contour-routing-trace")
    {
        const auto fixture = makeContourRoutingTraceFixture();
        if (fixture.isEmpty())
            return 1;
        std::cout << fixture.toStdString();
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

    test.expect (state->getTagName() == "modelDState",
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
            test.expect (parameter != nullptr && parameter->getTagName() == "PARAM"
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
        constexpr auto preciseToken = "0.123456789012345678901234567890123456789";
        constexpr auto largeToken = "123456789012345678901234567890.125";
        auto unknown = std::make_unique<juce::XmlElement> ("PARAM");
        unknown->setAttribute ("id", "vendor.preciseValue");
        unknown->setAttribute ("value", preciseToken);
        unknownLegacy->addChildElement (unknown.release());
        auto large = std::make_unique<juce::XmlElement> ("PARAM");
        large->setAttribute ("id", "vendor.largeValue");
        large->setAttribute ("value", largeToken);
        unknownLegacy->addChildElement (large.release());
        const auto binary = binaryFromXml (*unknownLegacy);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto output = serialiseProcessorState (migrated);
        const auto* extensions = output == nullptr ? nullptr : output->getChildByName ("extensions");
        const auto* legacyParameters = extensions == nullptr ? nullptr
                                                              : extensions->getChildByName ("legacyParameters");
        juce::String precisePreserved;
        juce::String largePreserved;
        if (legacyParameters != nullptr)
            for (auto* child = legacyParameters->getFirstChildElement(); child != nullptr;
                 child = child->getNextElement())
            {
                const auto id = child->getStringAttribute ("id");
                if (id == "vendor.preciseValue")
                    precisePreserved = child->getStringAttribute ("value");
                if (id == "vendor.largeValue")
                    largePreserved = child->getStringAttribute ("value");
            }
        test.expect (result.succeeded() && precisePreserved == preciseToken
                         && largePreserved == largeToken,
                     "unknown v0 numeric tokens must survive exactly after XML decoding");

        findParameterXml (*unknownLegacy, "vendor.preciseValue")->setAttribute ("value", "NaN");
        const auto unsafeBinary = binaryFromXml (*unknownLegacy);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (unsafeBinary.getData(),
                                           static_cast<int> (unsafeBinary.getSize())).code
                         == StateContract::RestoreCode::nonFiniteValue,
                     "non-finite unknown v0 PARAM must be rejected");
    }

    const auto makeUnknownBoundaryState = [&] (int unknownCount)
    {
        auto state = juce::parseXML (legacyFixtureFile (
            "Tests/fixtures/state/legacy-default-state.xml"));
        if (state == nullptr)
            return state;
        for (int index = 0; index < unknownCount; ++index)
        {
            auto parameter = std::make_unique<juce::XmlElement> ("PARAM");
            parameter->setAttribute ("id", "vendor.node" + juce::String { index });
            parameter->setAttribute ("value", "1");
            state->addChildElement (parameter.release());
        }
        return state;
    };
    auto nodeBoundary = makeUnknownBoundaryState (StateContract::maxExtensionNodes - 1);
    if (nodeBoundary != nullptr)
    {
        const auto binary = binaryFromXml (*nodeBoundary);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto saved = serialiseProcessorStateBytes (migrated);
        MoogMiniAudioProcessor reloaded;
        const auto reload = reloaded.restoreState (saved.getData(), static_cast<int> (saved.getSize()));
        test.expect (result.succeeded() && reload.succeeded()
                         && saved == serialiseProcessorStateBytes (reloaded),
                     "maximum-sized v0 extension node set must self-reload deterministically");
    }
    auto nodeOver = makeUnknownBoundaryState (StateContract::maxExtensionNodes);
    if (nodeOver != nullptr)
    {
        const auto binary = binaryFromXml (*nodeOver);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (binary.getData(), static_cast<int> (binary.getSize())).code
                         == StateContract::RestoreCode::unsafeExtensionStructure,
                     "one-over v0 extension node set must reject before success");
    }

    auto tokenBoundary = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (tokenBoundary != nullptr)
    {
        const auto maximumToken = "0." + juce::String::repeatedString (
            "0", StateContract::maxExtensionValueBytes - 2);
        auto parameter = std::make_unique<juce::XmlElement> ("PARAM");
        parameter->setAttribute ("id", "vendor.maximumToken");
        parameter->setAttribute ("value", maximumToken);
        tokenBoundary->addChildElement (parameter.release());
        const auto binary = binaryFromXml (*tokenBoundary);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto saved = serialiseProcessorStateBytes (migrated);
        MoogMiniAudioProcessor reloaded;
        const auto reload = reloaded.restoreState (saved.getData(), static_cast<int> (saved.getSize()));
        test.expect (result.succeeded() && reload.succeeded(),
                     "maximum-length unknown v0 numeric token must self-reload");

        findParameterXml (*tokenBoundary, "vendor.maximumToken")->setAttribute (
            "value", maximumToken + "0");
        const auto overBinary = binaryFromXml (*tokenBoundary);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (overBinary.getData(),
                                           static_cast<int> (overBinary.getSize())).code
                         == StateContract::RestoreCode::unsafeExtensionStructure,
                     "one-over unknown v0 numeric token must reject before parsing");
    }

    auto nameBoundary = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (nameBoundary != nullptr)
    {
        const auto maximumId = juce::String::repeatedString (
            "n", StateContract::maxExtensionNameBytes);
        auto parameter = std::make_unique<juce::XmlElement> ("PARAM");
        parameter->setAttribute ("id", maximumId);
        parameter->setAttribute ("value", "1");
        nameBoundary->addChildElement (parameter.release());
        const auto binary = binaryFromXml (*nameBoundary);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto saved = serialiseProcessorStateBytes (migrated);
        MoogMiniAudioProcessor reloaded;
        const auto reload = reloaded.restoreState (saved.getData(), static_cast<int> (saved.getSize()));
        test.expect (result.succeeded() && reload.succeeded(),
                     "maximum-length unknown v0 ID must self-reload");

        findParameterXml (*nameBoundary, maximumId.toStdString())
            ->setAttribute ("id", maximumId + "n");
        const auto overBinary = binaryFromXml (*nameBoundary);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (overBinary.getData(),
                                           static_cast<int> (overBinary.getSize())).code
                         == StateContract::RestoreCode::unsafeExtensionStructure,
                     "one-over unknown v0 ID must reject before success");
    }

    const auto makeUnknownValueBudgetState = [&] (int unknownCount)
    {
        auto state = juce::parseXML (legacyFixtureFile (
            "Tests/fixtures/state/legacy-default-state.xml"));
        if (state == nullptr)
            return state;
        const auto maximumToken = "0." + juce::String::repeatedString (
            "0", StateContract::maxExtensionValueBytes - 2);
        for (int index = 0; index < unknownCount; ++index)
        {
            auto parameter = std::make_unique<juce::XmlElement> ("PARAM");
            parameter->setAttribute ("id", "v" + juce::String { index });
            parameter->setAttribute ("value", maximumToken);
            state->addChildElement (parameter.release());
        }
        return state;
    };
    auto valueBudgetBoundary = makeUnknownValueBudgetState (15);
    if (valueBudgetBoundary != nullptr)
    {
        const auto binary = binaryFromXml (*valueBudgetBoundary);
        MoogMiniAudioProcessor migrated;
        const auto result = migrated.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        const auto saved = serialiseProcessorStateBytes (migrated);
        MoogMiniAudioProcessor reloaded;
        const auto reload = reloaded.restoreState (saved.getData(), static_cast<int> (saved.getSize()));
        test.expect (result.succeeded() && reload.succeeded(),
                     "bounded v0 aggregate extension values must self-reload");
    }
    auto valueBudgetOver = makeUnknownValueBudgetState (16);
    if (valueBudgetOver != nullptr)
    {
        const auto binary = binaryFromXml (*valueBudgetOver);
        MoogMiniAudioProcessor rejected;
        test.expect (rejected.restoreState (binary.getData(), static_cast<int> (binary.getSize())).code
                         == StateContract::RestoreCode::unsafeExtensionStructure,
                     "over-budget v0 aggregate extension values must reject before success");
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
                 "safe extension semantics and property/child order must be preserved");

    MoogMiniAudioProcessor repeated;
    const auto repeatedResult = repeated.restoreState (first.getData(), static_cast<int> (first.getSize()));
    test.expect (repeatedResult.code == StateContract::RestoreCode::success
                     && first == serialiseProcessorStateBytes (repeated),
                 "repeated native v2 round trips must be byte-deterministic");

    MoogMiniAudioProcessor lexicalSource;
    const auto lexicalXml = serialiseProcessorState (lexicalSource);
    if (lexicalXml != nullptr)
    {
        auto rawText = lexicalXml->toString (juce::XmlElement::TextFormat().singleLine());
        const auto noncanonicalExtension =
            "<extensions><vendorData first=\"A&#38;B\" second=\"&quot;x&quot;\"></vendorData></extensions>";
        rawText = rawText.replace ("<extensions/>", noncanonicalExtension)
                         .replace ("<extensions />", noncanonicalExtension);
        test.expect (rawText.contains ("A&#38;B"),
                     "lexical extension test must retain its noncanonical entity spelling");
        const auto rawBinary = binaryFromRawXmlText (rawText);
        MoogMiniAudioProcessor canonicalized;
        const auto canonicalizedResult = canonicalized.restoreState (
            rawBinary.getData(), static_cast<int> (rawBinary.getSize()));
        const auto canonicalBytes = serialiseProcessorStateBytes (canonicalized);
        const auto canonicalXml = serialiseProcessorState (canonicalized);
        const auto* canonicalExtensions = canonicalXml == nullptr ? nullptr
                                                                   : canonicalXml->getChildByName ("extensions");
        const auto* canonicalVendor = canonicalExtensions == nullptr ? nullptr
                                                                      : canonicalExtensions->getChildByName ("vendorData");
        test.expect (canonicalizedResult.succeeded() && canonicalVendor != nullptr
                         && canonicalVendor->getStringAttribute ("first") == "A&B"
                         && canonicalVendor->getStringAttribute ("second") == "\"x\""
                         && rawBinary != canonicalBytes,
                     "JUCE lexical normalization must preserve decoded extension semantics");
        MoogMiniAudioProcessor canonicalReload;
        const auto canonicalReloadResult = canonicalReload.restoreState (
            canonicalBytes.getData(), static_cast<int> (canonicalBytes.getSize()));
        test.expect (canonicalReloadResult.succeeded()
                         && canonicalBytes == serialiseProcessorStateBytes (canonicalReload),
                     "extension bytes must stabilize after the first JUCE canonicalization");
    }
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
    auto legacySentinel = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    test.expect (legacySentinel != nullptr, "migrated rollback sentinel must parse");
    if (legacySentinel == nullptr)
        return;
    legacySentinel->setAttribute ("presetName", "Rollback Legacy");
    auto rollbackUnknown = std::make_unique<juce::XmlElement> ("PARAM");
    rollbackUnknown->setAttribute ("id", "vendor.rollbackToken");
    rollbackUnknown->setAttribute ("value", "0.1234567890123456789");
    legacySentinel->addChildElement (rollbackUnknown.release());
    const auto validSentinel = binaryFromXml (*legacySentinel);
    const auto success = sentinel.restoreState (validSentinel.getData(),
                                                static_cast<int> (validSentinel.getSize()));
    test.expect (success.code == StateContract::RestoreCode::successMigratedV0
                     && ! sentinel.shouldAutoLoadLastPreset()
                     && sentinel.getContourContract()
                            == StateContract::ContourContract::legacyCrossedContours,
                 "sentinel must first complete one successful migrated v0 host restore");
    const auto beforeFailure = serialiseProcessorStateBytes (sentinel);
    const auto contourBefore = sentinel.getContourContract();

    const auto baseline = serialiseProcessorState (sentinel);
    test.expect (baseline != nullptr, "failure corpus baseline must serialize");
    if (baseline == nullptr)
        return;
    const auto* baselineCompatibility = baseline->getChildByName ("compatibility");
    const auto* baselineMigrationLog = baseline->getChildByName ("migrationLog");
    const auto* baselineExtensions = baseline->getChildByName ("extensions");
    test.expect (baselineCompatibility != nullptr
                     && baselineCompatibility->getStringAttribute ("sourceHash").isNotEmpty()
                     && baselineMigrationLog != nullptr
                     && baselineMigrationLog->getNumChildElements() != 0
                     && baselineExtensions != nullptr
                     && baselineExtensions->getNumChildElements() != 0,
                 "rollback sentinel must carry non-empty hash, migration log, and extensions");

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
    expectRejected (std::make_unique<juce::XmlElement> ("MODELDSTATE"),
                    StateContract::RestoreCode::wrongRoot,
                    "case-mismatched canonical root");
    expectRejected (std::make_unique<juce::XmlElement> ("parameters"),
                    StateContract::RestoreCode::wrongRoot,
                    "case-mismatched legacy root");

    auto missingVersion = copyStateXml (*baseline);
    missingVersion->removeAttribute ("stateVersion");
    expectRejected (std::move (missingVersion), StateContract::RestoreCode::missingStateVersion,
                    "missing version");
    for (const auto& [version, expected, description] :
         std::to_array<std::tuple<const char*, StateContract::RestoreCode, const char*>> ({
             { "abc", StateContract::RestoreCode::invalidStateVersion, "invalid version" },
             { "2.5", StateContract::RestoreCode::nonIntegerStateVersion, "noninteger version" },
             { "-1", StateContract::RestoreCode::negativeStateVersion, "negative version" },
             { "-99999999999999999999999999999999999999999999999999999999999999999999999999999999",
               StateContract::RestoreCode::negativeStateVersion, "huge negative version" },
             { "1", StateContract::RestoreCode::unsupportedStateVersion, "unsupported past version" },
             { "99999999999999999999999999999999999999999999999999999999999999999999999999999999",
               StateContract::RestoreCode::futureStateVersion, "huge future version" },
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
    invalidMigrationLog->getChildByName ("migrationLog")->getFirstChildElement()
        ->setAttribute ("action", "invalid action");
    expectRejected (std::move (invalidMigrationLog),
                    StateContract::RestoreCode::invalidMigrationLog,
                    "invalid migrated-state migration log");
    auto lowercaseMigrationEntry = copyStateXml (*baseline);
    lowercaseMigrationEntry->getChildByName ("migrationLog")->getFirstChildElement()
        ->setTagName ("entry");
    expectRejected (std::move (lowercaseMigrationEntry),
                    StateContract::RestoreCode::invalidMigrationLog,
                    "case-mismatched migration ENTRY");

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
    auto lowercaseV2Parameter = copyStateXml (*baseline);
    lowercaseV2Parameter->getChildByName ("parameters")->getFirstChildElement()
        ->setTagName ("param");
    expectRejected (std::move (lowercaseV2Parameter), StateContract::RestoreCode::unexpectedChild,
                    "case-mismatched v2 PARAM");

    auto lowercaseLegacyParameter = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    if (lowercaseLegacyParameter != nullptr)
    {
        lowercaseLegacyParameter->getFirstChildElement()->setTagName ("param");
        const auto binary = binaryFromXml (*lowercaseLegacyParameter);
        const auto result = sentinel.restoreState (binary.getData(), static_cast<int> (binary.getSize()));
        test.expect (result.code == StateContract::RestoreCode::unexpectedChild
                         && serialiseProcessorStateBytes (sentinel) == beforeFailure,
                     "case-mismatched v0 PARAM must reject atomically as unexpected_child");
    }

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

void testStatePublicationBoundary (TestContext& test)
{
    const auto headerSource = legacyFixtureFile ("Source/PluginProcessor.h").loadFileAsString();
    const auto processorSource = legacyFixtureFile ("Source/PluginProcessor.cpp").loadFileAsString();
    test.expect (headerSource.contains ("mutable juce::CriticalSection statePublicationLock")
                     && headerSource.contains ("std::atomic<bool> restoredStateFromHost")
                     && headerSource.contains (
                         "std::atomic<StateContract::ContourContract> contourContractCache"),
                 "state publication must declare one common lock plus atomic lifecycle metadata");

    const auto getterStart = processorSource.indexOf (
        "StateContract::ContourContract MoogMiniAudioProcessor::getContourContract");
    const auto getterEnd = processorSource.indexOf (
        getterStart, "bool MoogMiniAudioProcessor::shouldAutoLoadLastPreset");
    const auto getterSource = processorSource.substring (getterStart, getterEnd);
    test.expect (getterStart >= 0 && getterEnd > getterStart
                     && getterSource.contains ("contourContractCache.load")
                     && ! getterSource.contains ("canonicalState")
                     && ! getterSource.contains ("ValueTree")
                     && ! getterSource.contains ("juce::String")
                     && ! getterSource.contains ("StateContract::contourContract"),
                 "realtime contour metadata getter must be an atomic-cache read only");

    MoogMiniAudioProcessor nativeSource;
    setParameter (nativeSource, "filterCutoff", 0.8f, test);
    const auto nativeBytes = serialiseProcessorStateBytes (nativeSource);
    auto legacyXml = juce::parseXML (legacyFixtureFile (
        "Tests/fixtures/state/legacy-representative-state.xml"));
    test.expect (legacyXml != nullptr, "publication stress legacy source must parse");
    if (legacyXml == nullptr)
        return;
    legacyXml->setAttribute ("presetName", "Publication Stress");
    auto unknown = std::make_unique<juce::XmlElement> ("PARAM");
    unknown->setAttribute ("id", "vendor.publicationToken");
    unknown->setAttribute ("value", "0.1234567890123456789");
    legacyXml->addChildElement (unknown.release());
    const auto migratedBytes = binaryFromXml (*legacyXml);

    MoogMiniAudioProcessor shared;
    const auto initial = shared.restoreState (nativeBytes.getData(),
                                              static_cast<int> (nativeBytes.getSize()));
    test.expect (initial.succeeded(), "publication stress initial restore must succeed");
    std::atomic<int> failures { 0 };
    std::atomic<bool> start { false };
    std::thread reader ([&]
    {
        while (! start.load (std::memory_order_acquire))
            std::this_thread::yield();
        for (int iteration = 0; iteration < 300; ++iteration)
        {
            juce::MemoryBlock state;
            shared.getStateInformation (state);
            const auto parsed = StateContract::parseAndPrepare (
                state.getData(), static_cast<int> (state.getSize()));
            if (! parsed.result.succeeded())
                failures.fetch_add (1, std::memory_order_relaxed);
            static_cast<void> (shared.getContourContract());
            static_cast<void> (shared.shouldAutoLoadLastPreset());
        }
    });
    start.store (true, std::memory_order_release);
    for (int iteration = 0; iteration < 300; ++iteration)
    {
        const auto& bytes = iteration % 2 == 0 ? migratedBytes : nativeBytes;
        if (! shared.restoreState (bytes.getData(), static_cast<int> (bytes.getSize())).succeeded())
            failures.fetch_add (1, std::memory_order_relaxed);
    }
    reader.join();
    const auto finalBytes = serialiseProcessorStateBytes (shared);
    const auto finalParsed = StateContract::parseAndPrepare (
        finalBytes.getData(), static_cast<int> (finalBytes.getSize()));
    test.expect (failures.load (std::memory_order_relaxed) == 0
                     && finalParsed.result.succeeded(),
                 "concurrent restore/save publication must expose only complete valid snapshots");
}

void testContourTask3BContract (TestContext& test)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    const auto routingHeader = sourceRoot.getChildFile ("Source/ContourRouting.h");
    const auto routingSource = sourceRoot.getChildFile ("Source/ContourRouting.cpp");
    const auto processorHeader = sourceRoot.getChildFile ("Source/PluginProcessor.h").loadFileAsString();
    const auto processorSource = sourceRoot.getChildFile ("Source/PluginProcessor.cpp").loadFileAsString();
    const auto editorSource = sourceRoot.getChildFile ("Source/PluginEditor.cpp").loadFileAsString();

    test.expect (routingHeader.existsAsFile() && routingSource.existsAsFile(),
                 "Task 3B must provide one shared production ContourRouting adapter");
    const auto traceFixture = sourceRoot.getChildFile (
        "Tests/fixtures/state/contour-routing-conversion-trace.json");
    test.expect (traceFixture.existsAsFile()
                     && traceFixture.loadFileAsString() == makeContourRoutingTraceFixture(),
                 "production contour routing/conversion capture must equal its fixture byte-for-byte");
    test.expect (processorHeader.contains ("ContourSnapshot")
                     && processorHeader.contains ("ContourConversionRequest")
                     && processorHeader.contains ("undoContourConversion"),
                 "processor must expose typed contour snapshot/conversion/undo seams");
    test.expect (processorHeader.contains ("stateGeneration")
                     && processorSource.contains ("captureContourSnapshot"),
                 "contour publication must use a generation-bracketed bounded snapshot");
    test.expect (processorSource.contains ("convertLegacyContours")
                     && processorSource.contains ("host_automation_not_rewritten"),
                 "legacy static conversion must be explicit and retain the stable automation warning");
    test.expect (processorSource.contains ("ContourRouting::route")
                     && processorSource.contains ("setEnvelopeSettings (normalizedToMilliseconds (routed.filter.attack)")
                     && processorSource.contains ("setContourEnvelopeSettings (normalizedToMilliseconds (routed.loudness.attack)"),
                 "processBlock must route both physical contour ports through the shared adapter");
    test.expect (editorSource.contains ("setSignalFlowContourControl")
                     && editorSource.contains ("captureParameterSnapshot"),
                 "Signal Flow contour reads and writes must choose semantic IDs dynamically");
    test.expect (! editorSource.contains (
                     "callbacks.setContourAttack = [setParam](float value) { setParam(\"loudnessAttackTimeKnob\""),
                 "Signal Flow filter contour callbacks must not constructor-capture crossed IDs");
    test.expect (routingHeader.loadFileAsString().contains ("mapStoredControls")
                     && editorSource.contains ("ContourRouting::mapStoredControls")
                     && ! editorSource.contains (
                         "const auto routedContours = ContourRouting::route (signalFlowContourSnapshot.contract"),
                 "Signal Flow readback must map stored semantic controls without Decay gating");

    const ContourRouting::ContourControls controls {
        0.11f, 0.22f, 0.3f, 0.66f, 0.77f, 0.9f, true
    };
    const auto canonical = ContourRouting::route (
        StateContract::ContourContract::canonicalContours, controls);
    const auto legacy = ContourRouting::route (
        StateContract::ContourContract::legacyCrossedContours, controls);
    const auto close = [] (float left, float right) { return std::abs (left - right) < 1.0e-6f; };
    test.expect (close (canonical.filter.attack, 0.11f)
                     && close (canonical.filter.decay, 0.22f)
                     && close (canonical.filter.sustain, 0.3f)
                     && close (canonical.loudness.attack, 0.66f)
                     && close (canonical.loudness.decay, 0.77f)
                     && close (canonical.loudness.sustain, 0.9f),
                 "canonical adapter truth table must route all six distinct controls exactly");
    test.expect (close (legacy.filter.attack, 0.66f)
                     && close (legacy.filter.decay, 0.77f)
                     && close (legacy.filter.sustain, 0.9f)
                     && close (legacy.loudness.attack, 0.11f)
                     && close (legacy.loudness.decay, 0.22f)
                     && close (legacy.loudness.sustain, 0.3f),
                 "legacy adapter truth table must retain the exact crossed physical routing");
    auto noDecay = controls;
    noDecay.decayEnabled = false;
    const auto noDecayRouted = ContourRouting::route (
        StateContract::ContourContract::legacyCrossedContours, noDecay);
    test.expect (noDecayRouted.filter.sustain == 1.0f
                     && noDecayRouted.loudness.sustain == 1.0f,
                 "disabled Decay must retain sustain 1.0 on both physical ports");
    const auto canonicalStored = ContourRouting::mapStoredControls (
        StateContract::ContourContract::canonicalContours, noDecay);
    const auto legacyStored = ContourRouting::mapStoredControls (
        StateContract::ContourContract::legacyCrossedContours, noDecay);
    test.expect (close (canonicalStored.filter.sustain, 0.3f)
                     && close (canonicalStored.loudness.sustain, 0.9f),
                 "canonical disabled-Decay display mapping must retain distinct stored sustains");
    test.expect (close (legacyStored.filter.sustain, 0.9f)
                     && close (legacyStored.loudness.sustain, 0.3f),
                 "legacy disabled-Decay display mapping must retain crossed distinct stored sustains");

    constexpr std::array stages { ContourRouting::Stage::attack,
                                  ContourRouting::Stage::decay,
                                  ContourRouting::Stage::sustain };
    for (const auto stage : stages)
    {
        const auto canonicalFilter = ContourRouting::parameterFor (
            StateContract::ContourContract::canonicalContours,
            ContourRouting::SemanticContour::filter, stage);
        const auto canonicalLoudness = ContourRouting::parameterFor (
            StateContract::ContourContract::canonicalContours,
            ContourRouting::SemanticContour::loudness, stage);
        const auto legacyFilter = ContourRouting::parameterFor (
            StateContract::ContourContract::legacyCrossedContours,
            ContourRouting::SemanticContour::filter, stage);
        const auto legacyLoudness = ContourRouting::parameterFor (
            StateContract::ContourContract::legacyCrossedContours,
            ContourRouting::SemanticContour::loudness, stage);
        test.expect (canonicalFilter == legacyLoudness
                         && canonicalLoudness == legacyFilter,
                     "dynamic overlay semantic mapping must cross every stage only in legacy mode");
    }

    const auto makeDistinctLegacy = [&]
    {
        auto xml = juce::parseXML (legacyFixtureFile (
            "Tests/fixtures/state/legacy-representative-state.xml"));
        if (xml != nullptr)
        {
            xml->setAttribute ("presetName", "Contour Conversion Retention");
            for (const auto& [id, value] : std::to_array<std::pair<const char*, const char*>> ({
                     { "filterAttackTimeKnob", "0.11" }, { "filterDecayTimeKnob", "0.22" },
                     { "filterSustainKnob", "3" }, { "loudnessAttackTimeKnob", "0.66" },
                     { "loudnessDecayTimeKnob", "0.77" }, { "loudnessSustainLevelKnob", "9" },
                     { "decaySwitch", "1" } }))
                findParameterXml (*xml, id)->setAttribute ("value", value);
            auto retained = std::make_unique<juce::XmlElement> ("PARAM");
            retained->setAttribute ("id", "vendor.contourRetention");
            retained->setAttribute ("value", "0.3141592653589793");
            xml->addChildElement (retained.release());
        }
        return xml;
    };
    const auto renderTrace = [] (MoogMiniAudioProcessor& processor)
    {
        processor.setRateAndBufferSizeDetails (48000.0, 1);
        processor.prepareToPlay (48000.0, 1);
        juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 1);
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        processor.releaseResources();
        return processor.getContourTrace ({});
    };
    const auto setDistinct = [&] (MoogMiniAudioProcessor& processor)
    {
        setParameter (processor, "filterAttackTimeKnob", 0.11f, test);
        setParameter (processor, "filterDecayTimeKnob", 0.22f, test);
        setParameter (processor, "filterSustainKnob", 0.3f, test);
        setParameter (processor, "loudnessAttackTimeKnob", 0.66f, test);
        setParameter (processor, "loudnessDecayTimeKnob", 0.77f, test);
        setParameter (processor, "loudnessSustainLevelKnob", 0.9f, test);
        setParameter (processor, "decaySwitch", 1.0f, test);
    };

    MoogMiniAudioProcessor native;
    setDistinct (native);
    const auto nativeTrace = renderTrace (native);
    test.expect (nativeTrace.contract == StateContract::ContourContract::canonicalContours
                     && close (nativeTrace.routed.filter.attack, 0.11f)
                     && close (nativeTrace.routed.loudness.attack, 0.66f),
                 "new/native v2 must use canonical routing in the actual processor path");

    const auto distinctLegacyXml = makeDistinctLegacy();
    test.expect (distinctLegacyXml != nullptr, "synthetic distinct v0 contour state must parse");
    if (distinctLegacyXml == nullptr)
        return;
    const auto distinctLegacyBytes = binaryFromXml (*distinctLegacyXml);
    MoogMiniAudioProcessor migrated;
    const auto migration = migrated.restoreState (distinctLegacyBytes.getData(),
                                                   static_cast<int> (distinctLegacyBytes.getSize()));
    const auto legacyTrace = renderTrace (migrated);
    test.expect (migration.succeeded()
                     && legacyTrace.contract == StateContract::ContourContract::legacyCrossedContours
                     && close (legacyTrace.routed.filter.attack, 0.66f)
                     && close (legacyTrace.routed.loudness.attack, 0.11f)
                     && close (physicalParameterValue (migrated, "filterAttackTimeKnob"), 0.11f),
                 "synthetic v0 must retain stored IDs and exact frozen legacy physical routing");

    MoogMiniAudioProcessor canonicalAutomation;
    setDistinct (canonicalAutomation);
    setParameter (canonicalAutomation, "filterAttackTimeKnob", 0.15f, test);
    auto canonicalAutomationTrace = canonicalAutomation.getContourTrace ({});
    test.expect (close (canonicalAutomationTrace.routed.filter.attack, 0.15f)
                     && close (canonicalAutomationTrace.routed.loudness.attack, 0.66f),
                 "canonical Filter-ID host automation must change only the physical Filter port");
    setParameter (canonicalAutomation, "loudnessAttackTimeKnob", 0.73f, test);
    canonicalAutomationTrace = canonicalAutomation.getContourTrace ({});
    test.expect (close (canonicalAutomationTrace.routed.filter.attack, 0.15f)
                     && close (canonicalAutomationTrace.routed.loudness.attack, 0.73f),
                 "canonical Loudness-ID host automation must change only the physical VCA port");
    MoogMiniAudioProcessor legacyAutomation;
    legacyAutomation.restoreState (distinctLegacyBytes.getData(),
                                   static_cast<int> (distinctLegacyBytes.getSize()));
    setParameter (legacyAutomation, "filterAttackTimeKnob", 0.15f, test);
    auto legacyAutomationTrace = legacyAutomation.getContourTrace ({});
    test.expect (close (legacyAutomationTrace.routed.filter.attack, 0.66f)
                     && close (legacyAutomationTrace.routed.loudness.attack, 0.15f),
                 "legacy Filter-ID host automation must change only the old physical VCA port");
    setParameter (legacyAutomation, "loudnessAttackTimeKnob", 0.73f, test);
    legacyAutomationTrace = legacyAutomation.getContourTrace ({});
    test.expect (close (legacyAutomationTrace.routed.filter.attack, 0.73f)
                     && close (legacyAutomationTrace.routed.loudness.attack, 0.15f),
                 "legacy Loudness-ID host automation must change only the old physical Filter port");
    for (const auto fixture : { "Tests/fixtures/state/legacy-default-state.xml",
                                "Tests/fixtures/state/legacy-representative-state.xml" })
    {
        const auto xml = juce::parseXML (legacyFixtureFile (fixture));
        const auto bytes = xml == nullptr ? juce::MemoryBlock {} : binaryFromXml (*xml);
        MoogMiniAudioProcessor restored;
        test.expect (xml != nullptr
                         && restored.restoreState (bytes.getData(), static_cast<int> (bytes.getSize())).succeeded()
                         && restored.getContourContract()
                                == StateContract::ContourContract::legacyCrossedContours,
                     "each immutable v0 fixture must restore in legacy crossed mode");
    }

    MoogMiniAudioProcessor overlayNative;
    overlayNative.setSignalFlowContourControl (ContourRouting::SemanticContour::filter,
                                               ContourRouting::Stage::attack, 0.41f);
    overlayNative.setSignalFlowContourControl (ContourRouting::SemanticContour::loudness,
                                               ContourRouting::Stage::decay, 0.82f);
    test.expect (close (physicalParameterValue (overlayNative, "filterAttackTimeKnob"), 0.41f)
                     && close (physicalParameterValue (overlayNative, "loudnessDecayTimeKnob"), 0.82f),
                 "canonical Signal Flow writes must target canonical stable IDs");
    migrated.setSignalFlowContourControl (ContourRouting::SemanticContour::filter,
                                          ContourRouting::Stage::attack, 0.42f);
    migrated.setSignalFlowContourControl (ContourRouting::SemanticContour::loudness,
                                          ContourRouting::Stage::decay, 0.83f);
    test.expect (close (physicalParameterValue (migrated, "loudnessAttackTimeKnob"), 0.42f)
                     && close (physicalParameterValue (migrated, "filterDecayTimeKnob"), 0.83f),
                 "legacy Signal Flow writes must dynamically target crossed stable IDs");

    MoogMiniAudioProcessor conversion;
    test.expect (conversion.restoreState (distinctLegacyBytes.getData(),
                                          static_cast<int> (distinctLegacyBytes.getSize())).succeeded(),
                 "conversion source must restore");
    const auto beforeBytes = serialiseProcessorStateBytes (conversion);
    const auto beforeXml = std::unique_ptr<juce::XmlElement> (
        MoogMiniAudioProcessor::getXmlFromBinary (beforeBytes.getData(),
                                                  static_cast<int> (beforeBytes.getSize())));
    const auto beforeGeneration = conversion.getStateGeneration();
    const auto beforeSnapshot = conversion.captureContourSnapshot ({});
    const auto beforeRouted = ContourRouting::route (beforeSnapshot.contract, beforeSnapshot.controls);
    const auto cancelled = conversion.convertLegacyContours();
    test.expect (cancelled.codeString() == "cancelled"sv
                     && cancelled.warningString() == "host_automation_not_rewritten"sv
                     && beforeBytes == serialiseProcessorStateBytes (conversion)
                     && beforeGeneration == conversion.getStateGeneration(),
                 "default conversion request must cancel with warning and no mutation/generation advance");
    const auto converted = conversion.convertLegacyContours (
        { true, MoogMiniAudioProcessor::HostAutomationKnowledge::unknown });
    const auto convertedBytes = serialiseProcessorStateBytes (conversion);
    const auto convertedSnapshot = conversion.captureContourSnapshot (beforeSnapshot);
    const auto afterRouted = ContourRouting::route (convertedSnapshot.contract,
                                                    convertedSnapshot.controls);
    test.expect (converted.codeString() == "converted"sv
                     && converted.warningString() == "host_automation_not_rewritten"sv
                     && conversion.getStateGeneration() == beforeGeneration + 2
                     && conversion.getContourContract()
                            == StateContract::ContourContract::canonicalContours
                     && close (physicalParameterValue (conversion, "filterAttackTimeKnob"), 0.66f)
                     && close (physicalParameterValue (conversion, "loudnessAttackTimeKnob"), 0.11f)
                     && close (beforeRouted.filter.attack, afterRouted.filter.attack)
                     && close (beforeRouted.loudness.decay, afterRouted.loudness.decay),
                 "confirmed conversion must swap exactly the static pairs and preserve physical trace");
    const auto convertedXml = serialiseProcessorState (conversion);
    const auto* convertedLog = convertedXml == nullptr ? nullptr
                                                        : convertedXml->getChildByName ("migrationLog");
    const auto* conversionEntry = convertedLog == nullptr || convertedLog->getNumChildElements() == 0
                                    ? nullptr
                                    : convertedLog->getChildElement (
                                          convertedLog->getNumChildElements() - 1);
    test.expect (conversionEntry != nullptr
                     && conversionEntry->getIntAttribute ("from") == 2
                     && conversionEntry->getIntAttribute ("to") == 2
                     && conversionEntry->getStringAttribute ("action") == "convertLegacyContours"
                     && conversionEntry->getStringAttribute ("warningCode")
                            == "legacy.hostAutomationNotRewritten",
                 "converted state must append the exact deterministic provenance entry");
    const auto* beforeCompatibility = beforeXml == nullptr ? nullptr
                                                            : beforeXml->getChildByName ("compatibility");
    const auto* afterCompatibility = convertedXml == nullptr ? nullptr
                                                              : convertedXml->getChildByName ("compatibility");
    test.expect (beforeXml != nullptr && convertedXml != nullptr
                     && beforeXml->getStringAttribute ("engineVersion")
                            == convertedXml->getStringAttribute ("engineVersion")
                     && beforeXml->getStringAttribute ("calibrationProfileId")
                            == convertedXml->getStringAttribute ("calibrationProfileId")
                     && beforeCompatibility != nullptr && afterCompatibility != nullptr
                     && beforeCompatibility->getStringAttribute ("sourceVersion")
                            == afterCompatibility->getStringAttribute ("sourceVersion")
                     && beforeCompatibility->getStringAttribute ("migratedAtVersion")
                            == afterCompatibility->getStringAttribute ("migratedAtVersion")
                     && beforeCompatibility->getStringAttribute ("sourceHash")
                            == afterCompatibility->getStringAttribute ("sourceHash")
                     && beforeXml->getChildByName ("ui")->toString()
                            == convertedXml->getChildByName ("ui")->toString()
                     && beforeXml->getChildByName ("recreation")->toString()
                            == convertedXml->getChildByName ("recreation")->toString()
                     && beforeXml->getChildByName ("extensions")->toString()
                            == convertedXml->getChildByName ("extensions")->toString()
                     && findParameterXml (*beforeXml, "filterCutoff")->getStringAttribute ("value")
                            == findParameterXml (*convertedXml, "filterCutoff")->getStringAttribute ("value"),
                 "conversion must preserve source identity, extensions, UI, recreation, engine/calibration and unrelated parameters");
    const auto* beforeLog = beforeXml == nullptr ? nullptr : beforeXml->getChildByName ("migrationLog");
    bool priorLogPreserved = beforeLog != nullptr && convertedLog != nullptr
                          && convertedLog->getNumChildElements()
                                 == beforeLog->getNumChildElements() + 1;
    if (priorLogPreserved)
        for (int index = 0; index < beforeLog->getNumChildElements(); ++index)
            priorLogPreserved = priorLogPreserved
                && beforeLog->getChildElement (index)->toString()
                       == convertedLog->getChildElement (index)->toString();
    test.expect (priorLogPreserved,
                 "conversion must retain every prior migration-log entry in order");
    MoogMiniAudioProcessor reload;
    test.expect (reload.restoreState (convertedBytes.getData(),
                                     static_cast<int> (convertedBytes.getSize())).succeeded()
                     && serialiseProcessorStateBytes (reload) == convertedBytes,
                 "converted source-0 canonical state must validate and reload deterministically");
    const auto undo = conversion.undoContourConversion();
    test.expect (undo.codeString() == "undo_restored"sv
                     && serialiseProcessorStateBytes (conversion) == beforeBytes
                     && conversion.getContourContract()
                            == StateContract::ContourContract::legacyCrossedContours,
                 "one-level undo must restore exact complete pre-conversion bytes and marker");
    const auto afterUndoBytes = serialiseProcessorStateBytes (conversion);
    const auto secondUndoGeneration = conversion.getStateGeneration();
    test.expect (conversion.undoContourConversion().codeString() == "no_undo_available"sv
                     && afterUndoBytes == serialiseProcessorStateBytes (conversion)
                     && secondUndoGeneration == conversion.getStateGeneration(),
                 "second undo must be a stable mutation-free no-op");

    MoogMiniAudioProcessor canonicalNoOp;
    const auto nativePreparation = StateContract::prepareLegacyContourConversion (
        StateContract::makeNativeState (canonicalNoOp.apvts.copyState()));
    test.expect (nativePreparation.result.code == StateContract::RestoreCode::invalidCompatibility,
                 "conversion preparation must reject a non-legacy source before mutation");
    const auto canonicalBytes = serialiseProcessorStateBytes (canonicalNoOp);
    const auto canonicalGeneration = canonicalNoOp.getStateGeneration();
    test.expect (canonicalNoOp.convertLegacyContours (
                     { true, MoogMiniAudioProcessor::HostAutomationKnowledge::knownPresent })
                         .codeString() == "already_canonical"sv
                     && canonicalBytes == serialiseProcessorStateBytes (canonicalNoOp)
                     && canonicalGeneration == canonicalNoOp.getStateGeneration(),
                 "canonical conversion must return already_canonical without mutation");

    MoogMiniAudioProcessor knownAbsent;
    knownAbsent.restoreState (distinctLegacyBytes.getData(), static_cast<int> (distinctLegacyBytes.getSize()));
    test.expect (knownAbsent.convertLegacyContours (
                     { true, MoogMiniAudioProcessor::HostAutomationKnowledge::knownAbsent })
                         .warningString() == "none"sv,
                 "known-absent confirmation may omit the UI automation warning");
    const auto knownAbsentXml = serialiseProcessorState (knownAbsent);
    const auto* knownAbsentLog = knownAbsentXml == nullptr ? nullptr
                                                           : knownAbsentXml->getChildByName ("migrationLog");
    const auto* knownAbsentFinal = knownAbsentLog == nullptr || knownAbsentLog->getNumChildElements() == 0
                                     ? nullptr
                                     : knownAbsentLog->getChildElement (
                                           knownAbsentLog->getNumChildElements() - 1);
    test.expect (knownAbsentFinal != nullptr
                     && knownAbsentFinal->getStringAttribute ("warningCode")
                            == "legacy.hostAutomationNotRewritten",
                 "known-absent conversion must still retain deterministic conversion history");
    MoogMiniAudioProcessor knownPresent;
    knownPresent.restoreState (distinctLegacyBytes.getData(), static_cast<int> (distinctLegacyBytes.getSize()));
    test.expect (knownPresent.convertLegacyContours (
                     { true, MoogMiniAudioProcessor::HostAutomationKnowledge::knownPresent })
                         .warningString() == "host_automation_not_rewritten"sv,
                 "known-present confirmation must retain the stable automation warning");
    const auto externalBytes = serialiseProcessorStateBytes (native);
    knownAbsent.restoreState (externalBytes.getData(), static_cast<int> (externalBytes.getSize()));
    test.expect (knownAbsent.undoContourConversion().codeString() == "no_undo_available"sv,
                 "later successful external restore must clear pending conversion undo");

    auto malformedConverted = copyStateXml (*convertedXml);
    auto* malformedLog = malformedConverted->getChildByName ("migrationLog");
    malformedLog->removeChildElement (
        malformedLog->getChildElement (malformedLog->getNumChildElements() - 1), true);
    const auto malformedBytes = binaryFromXml (*malformedConverted);
    MoogMiniAudioProcessor atomicSentinel;
    atomicSentinel.restoreState (distinctLegacyBytes.getData(), static_cast<int> (distinctLegacyBytes.getSize()));
    const auto sentinelBytes = serialiseProcessorStateBytes (atomicSentinel);
    test.expect (atomicSentinel.restoreState (malformedBytes.getData(),
                                             static_cast<int> (malformedBytes.getSize())).code
                     == StateContract::RestoreCode::invalidMigrationLog
                     && serialiseProcessorStateBytes (atomicSentinel) == sentinelBytes,
                 "source-0 canonical provenance without conversion history must reject atomically");
    auto duplicateConverted = copyStateXml (*convertedXml);
    auto duplicateNamedEntry = std::make_unique<juce::XmlElement> ("ENTRY");
    duplicateNamedEntry->setAttribute ("from", 0);
    duplicateNamedEntry->setAttribute ("to", 2);
    duplicateNamedEntry->setAttribute ("action", "convertLegacyContours");
    duplicateNamedEntry->setAttribute ("warningCode", "legacy.contourRoutingPreserved");
    duplicateConverted->getChildByName ("migrationLog")->prependChildElement (
        duplicateNamedEntry.release());
    const auto duplicateConvertedBytes = binaryFromXml (*duplicateConverted);
    test.expect (atomicSentinel.restoreState (
                     duplicateConvertedBytes.getData(),
                     static_cast<int> (duplicateConvertedBytes.getSize())).code
                     == StateContract::RestoreCode::invalidMigrationLog
                     && serialiseProcessorStateBytes (atomicSentinel) == sentinelBytes,
                 "converted provenance must reject any additional conversion-named history");
    auto legacyWithConversionHistory = copyStateXml (*serialiseProcessorState (conversion));
    auto* legacyCompatibility = legacyWithConversionHistory->getChildByName ("compatibility");
    legacyCompatibility->setAttribute ("contourContract", "legacyCrossedContours");
    auto exactConversionEntry = std::make_unique<juce::XmlElement> ("ENTRY");
    exactConversionEntry->setAttribute ("from", 2);
    exactConversionEntry->setAttribute ("to", 2);
    exactConversionEntry->setAttribute ("action", "convertLegacyContours");
    exactConversionEntry->setAttribute ("warningCode", "legacy.hostAutomationNotRewritten");
    legacyWithConversionHistory->getChildByName ("migrationLog")
        ->addChildElement (exactConversionEntry.release());
    const auto legacyHistoryBytes = binaryFromXml (*legacyWithConversionHistory);
    test.expect (atomicSentinel.restoreState (legacyHistoryBytes.getData(),
                                             static_cast<int> (legacyHistoryBytes.getSize())).code
                     == StateContract::RestoreCode::invalidMigrationLog
                     && serialiseProcessorStateBytes (atomicSentinel) == sentinelBytes,
                 "legacy marker must reject conversion history atomically");

    const auto captureStart = processorSource.indexOf (
        "MoogMiniAudioProcessor::ContourSnapshot MoogMiniAudioProcessor::captureContourSnapshot");
    const auto captureEnd = processorSource.indexOf (
        captureStart, "MoogMiniAudioProcessor::ContourSnapshot MoogMiniAudioProcessor::getSignalFlowContourSnapshot");
    const auto captureSource = processorSource.substring (captureStart, captureEnd);
    test.expect (captureStart >= 0 && captureEnd > captureStart
                     && captureSource.contains ("captureParameterSnapshot")
                     && ! captureSource.contains ("juce::String")
                     && ! captureSource.contains ("getRawParameterValue")
                     && ! captureSource.contains ("ValueTree")
                     && ! captureSource.contains ("ScopedLock")
                     && ! captureSource.contains ("while"),
                 "audio contour snapshot must delegate to the complete prepared bounded reader");

    MoogMiniAudioProcessor nativeGenerationSource;
    setDistinct (nativeGenerationSource);
    const auto nativeGenerationBytes = serialiseProcessorStateBytes (nativeGenerationSource);
    auto legacyGenerationXml = copyStateXml (*distinctLegacyXml);
    for (const auto& [id, value] : std::to_array<std::pair<const char*, const char*>> ({
             { "filterAttackTimeKnob", "0.14" }, { "filterDecayTimeKnob", "0.25" },
             { "filterSustainKnob", "4" }, { "loudnessAttackTimeKnob", "0.68" },
             { "loudnessDecayTimeKnob", "0.79" }, { "loudnessSustainLevelKnob", "8" } }))
        findParameterXml (*legacyGenerationXml, id)->setAttribute ("value", value);
    const auto legacyGenerationBytes = binaryFromXml (*legacyGenerationXml);
    MoogMiniAudioProcessor concurrent;
    concurrent.restoreState (nativeGenerationBytes.getData(),
                             static_cast<int> (nativeGenerationBytes.getSize()));
    auto fallback = concurrent.captureContourSnapshot ({});
    std::atomic<int> mixedSnapshots { 0 };
    std::atomic<bool> beginCapture { false };
    std::thread captureReader ([&]
    {
        while (! beginCapture.load (std::memory_order_acquire))
            std::this_thread::yield();
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            const auto snapshot = concurrent.captureContourSnapshot (fallback);
            if (! snapshot.usedFallback)
                fallback = snapshot;
            const auto isNative = snapshot.contract
                                   == StateContract::ContourContract::canonicalContours
                && close (snapshot.controls.filterAttack, 0.11f)
                && close (snapshot.controls.filterDecay, 0.22f)
                && close (snapshot.controls.filterSustain, 0.3f)
                && close (snapshot.controls.loudnessAttack, 0.66f)
                && close (snapshot.controls.loudnessDecay, 0.77f)
                && close (snapshot.controls.loudnessSustain, 0.9f);
            const auto isLegacy = snapshot.contract
                                   == StateContract::ContourContract::legacyCrossedContours
                && close (snapshot.controls.filterAttack, 0.14f)
                && close (snapshot.controls.filterDecay, 0.25f)
                && close (snapshot.controls.filterSustain, 0.4f)
                && close (snapshot.controls.loudnessAttack, 0.68f)
                && close (snapshot.controls.loudnessDecay, 0.79f)
                && close (snapshot.controls.loudnessSustain, 0.8f);
            if ((! isNative && ! isLegacy) || (snapshot.generation & 1u) != 0u)
                mixedSnapshots.fetch_add (1, std::memory_order_relaxed);
        }
    });
    beginCapture.store (true, std::memory_order_release);
    for (int iteration = 0; iteration < 300; ++iteration)
    {
        const auto& bytes = iteration % 2 == 0 ? legacyGenerationBytes : nativeGenerationBytes;
        concurrent.restoreState (bytes.getData(), static_cast<int> (bytes.getSize()));
    }
    captureReader.join();
    test.expect (mixedSnapshots.load (std::memory_order_relaxed) == 0,
                 "bounded concurrent contour captures must return only complete generations or coherent fallback");
}

void testPreparedSnapshotContract (TestContext& test)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    const auto captureUtility = sourceRoot.getChildFile (
        "Source/ParameterSnapshotCapture.h");
    const auto captureUtilitySource = captureUtility.loadFileAsString();
    const auto registryHeader = sourceRoot.getChildFile ("Source/ParameterRegistry.h")
                                    .loadFileAsString();
    const auto processorHeader = sourceRoot.getChildFile ("Source/PluginProcessor.h")
                                     .loadFileAsString();
    const auto processorSource = sourceRoot.getChildFile ("Source/PluginProcessor.cpp")
                                     .loadFileAsString();
    const auto planSource = sourceRoot.getChildFile (
        "docs/superpowers/plans/2026-07-21-workstream-03-par-009.md")
                                .loadFileAsString();

    test.expect (registryHeader.contains ("enum class Key")
                     && registryHeader.contains ("Key::count")
                     && registryHeader.contains ("descriptor (Key key) noexcept"),
                 "PAR-009 requires a checked typed key in frozen descriptor order");
    test.expect (processorHeader.contains ("ParameterSnapshot")
                     && processorHeader.contains ("captureParameterSnapshot")
                     && processorHeader.contains ("preparedParameterHandles")
                     && processorHeader.contains ("invalidParameterValueCount"),
                 "processor must expose one complete typed prepared snapshot and diagnostic");
    test.expect (captureUtilitySource.contains ("maximumAttempts = 3")
                     && processorSource.contains ("buildTypedSnapshot")
                     && processorSource.contains ("captureParameterSnapshot"),
                 "complete snapshot capture must use three bounded generation-bracketed attempts");
    test.expect (captureUtility.existsAsFile()
                     && processorSource.contains ("ParameterSnapshotCapture::capture"),
                 "production capture must delegate to the deterministic header-only three-attempt utility");

    const auto processStart = processorSource.indexOf (
        "void MoogMiniAudioProcessor::processBlock");
    const auto processEnd = processorSource.indexOf (
        processStart, "float MoogMiniAudioProcessor::calculateGlideRate");
    const auto processBlockSource = processorSource.substring (processStart, processEnd);
    const auto firstCompleteCapture = processBlockSource.indexOf ("captureParameterSnapshot");
    test.expect (processStart >= 0 && processEnd > processStart
                     && firstCompleteCapture >= 0
                     && processBlockSource.indexOf (
                            firstCompleteCapture + 1, "captureParameterSnapshot") < 0
                     && ! processBlockSource.contains ("getRawParameterValue")
                     && ! processBlockSource.contains ("getParameter")
                     && ! processBlockSource.contains ("juce::String"),
                 "processBlock must capture once and contain no render-time parameter lookup/string");
    test.expect (processBlockSource.contains ("const juce::ScopedLock lock(midiCriticalSection)")
                     && planSource.contains ("Workstream 05-owned")
                     && planSource.contains ("does not claim that the complete function is lock-free"),
                 "PAR-009 must preserve and accurately scope the inherited Workstream 05 MIDI queue lock");

    const auto fixture = sourceRoot.getChildFile (
        "Tests/fixtures/parameters/parameter-snapshot-v2.json");
    test.expect (fixture.existsAsFile()
                     && fixture.loadFileAsString() == makeParameterSnapshotV2Fixture()
                     ,
                 "prepared snapshot production capture fixture must exist");

    static_assert (ParameterRegistry::parameterCount == 48);
    static_assert (std::is_trivially_copyable_v<MoogMiniAudioProcessor::ParameterSnapshot>);
    const auto descriptors = ParameterRegistry::descriptors();
    test.expect (descriptors.size() == ParameterRegistry::parameterCount,
                 "typed snapshot must retain exact frozen 48-descriptor coverage");
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto key = static_cast<ParameterRegistry::Key> (index);
        const auto expectedId = index < legacyParameterIds.size()
                              ? legacyParameterIds[index]
                              : newParameterIds[index - legacyParameterIds.size()];
        test.expect (ParameterRegistry::index (key) == index
                         && ParameterRegistry::descriptor (key).id == expectedId,
                     "every Key ordinal must resolve the exact immutable fixture ID");
    }

    MoogMiniAudioProcessor processor;
    auto fallback = processor.captureParameterSnapshot ({});
    const auto defaults = snapshotPhysicalValues (fallback);
    test.expect (fallback.coherent && ! fallback.usedFallback
                     && fallback.contourContract
                            == StateContract::ContourContract::canonicalContours,
                 "new processor must expose one coherent canonical native snapshot");
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto key = static_cast<ParameterRegistry::Key> (index);
        const auto* ranged = processor.getPreparedParameter (key);
        test.expect (ranged != nullptr
                         && equalsStringView (ranged->getParameterID(), descriptors[index].id)
                         && std::abs (defaults[index] - descriptors[index].physicalDefault) < 1.0e-6f,
                     "all prepared handles and native physical defaults must match the registry");
    }

    std::array<float, ParameterRegistry::parameterCount> distinctInput {};
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto& descriptor = descriptors[index];
        float value = 0.0f;
        if (descriptor.kind == ParameterRegistry::Kind::floating)
            value = static_cast<float> (index + 1) / 50.0f;
        else if (descriptor.kind == ParameterRegistry::Kind::choice)
            value = static_cast<float> ((index * 3 + 1)
                      % static_cast<std::size_t> (descriptor.rangeEnd + 1.0f));
        else
            value = (index % 2u) == 0u ? 1.0f : 0.0f;
        distinctInput[index] = value;
        rawParameterForTest (processor, static_cast<ParameterRegistry::Key> (index))
            ->store (value, std::memory_order_relaxed);
    }
    const auto distinct = processor.captureParameterSnapshot (fallback);
    const auto distinctOutput = snapshotPhysicalValues (distinct);
    bool distinctMappedExactly = distinct.coherent && ! distinct.usedFallback;
    for (std::size_t index = 0; index < descriptors.size(); ++index)
        distinctMappedExactly = distinctMappedExactly
            && std::abs (distinctOutput[index]
                         - expectedSanitizedValue (descriptors[index], distinctInput[index])) < 1.0e-6f;
    test.expect (distinctMappedExactly,
                 "all 48 distinct physical inputs must map exactly once into typed fields");

    const auto diagnosticBeforeFinite = processor.getInvalidParameterValueCount();
    for (std::size_t index = 0; index < descriptors.size(); ++index)
        rawParameterForTest (processor, static_cast<ParameterRegistry::Key> (index))
            ->store (std::numeric_limits<float>::max(), std::memory_order_relaxed);
    const auto positiveExtreme = processor.captureParameterSnapshot (distinct);
    const auto positiveValues = snapshotPhysicalValues (positiveExtreme);
    bool positiveClamped = true;
    for (std::size_t index = 0; index < descriptors.size(); ++index)
        positiveClamped = positiveClamped
            && std::isfinite (positiveValues[index])
            && std::abs (positiveValues[index] - descriptors[index].rangeEnd) < 1.0e-6f;

    for (std::size_t index = 0; index < descriptors.size(); ++index)
        rawParameterForTest (processor, static_cast<ParameterRegistry::Key> (index))
            ->store (-std::numeric_limits<float>::max(), std::memory_order_relaxed);
    const auto negativeExtreme = processor.captureParameterSnapshot (positiveExtreme);
    const auto negativeValues = snapshotPhysicalValues (negativeExtreme);
    bool negativeClamped = true;
    for (std::size_t index = 0; index < descriptors.size(); ++index)
        negativeClamped = negativeClamped
            && std::isfinite (negativeValues[index])
            && std::abs (negativeValues[index] - descriptors[index].rangeStart) < 1.0e-6f;
    test.expect (positiveClamped && negativeClamped
                     && processor.getInvalidParameterValueCount() == diagnosticBeforeFinite,
                 "finite extremes must clamp every field without incrementing invalid diagnostics");

    const auto diagnosticBeforeNonFinite = processor.getInvalidParameterValueCount();
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto invalid = index % 3u == 0u
                           ? std::numeric_limits<float>::quiet_NaN()
                           : index % 3u == 1u
                               ? std::numeric_limits<float>::infinity()
                               : -std::numeric_limits<float>::infinity();
        rawParameterForTest (processor, static_cast<ParameterRegistry::Key> (index))
            ->store (invalid, std::memory_order_relaxed);
    }
    const auto sanitized = processor.captureParameterSnapshot (negativeExtreme);
    const auto sanitizedValues = snapshotPhysicalValues (sanitized);
    bool defaultsRestored = true;
    for (std::size_t index = 0; index < descriptors.size(); ++index)
        defaultsRestored = defaultsRestored && std::isfinite (sanitizedValues[index])
            && std::abs (sanitizedValues[index] - descriptors[index].physicalDefault) < 1.0e-6f;
    test.expect (defaultsRestored
                     && processor.getInvalidParameterValueCount() - diagnosticBeforeNonFinite
                            == ParameterRegistry::parameterCount
                     && static_cast<int> (sanitized.priority) >= 0
                     && static_cast<int> (sanitized.priority) <= 2
                     && static_cast<int> (sanitized.trigger) >= 0
                     && static_cast<int> (sanitized.trigger) <= 1,
                 "NaN and infinities must default all fields, count exactly, and keep enums valid");

    const auto captureStart = processorSource.indexOf (
        "MoogMiniAudioProcessor::ParameterSnapshot MoogMiniAudioProcessor::captureParameterSnapshot");
    const auto captureEnd = processorSource.indexOf (
        captureStart, "juce::RangedAudioParameter* MoogMiniAudioProcessor::getPreparedParameter");
    const auto captureSource = processorSource.substring (captureStart, captureEnd);
    const auto builderStart = processorSource.indexOf (
        "MoogMiniAudioProcessor::ParameterSnapshot MoogMiniAudioProcessor::buildTypedSnapshot");
    const auto builderSource = processorSource.substring (builderStart, captureStart);
    test.expect (captureStart >= 0 && captureEnd > captureStart
                     && captureSource.contains ("ParameterSnapshotCapture::capture")
                     && captureSource.contains ("std::memory_order_acquire")
                     && captureSource.contains ("std::memory_order_relaxed")
                     && captureUtilitySource.contains ("fallback.coherent ? fallback")
                     && captureUtilitySource.contains ("result.usedFallback = true")
                     && ! captureSource.contains ("getRawParameterValue")
                     && ! captureSource.contains ("getParameter (")
                     && ! captureSource.contains ("juce::String")
                     && ! captureSource.contains ("ValueTree")
                     && ! captureSource.contains ("ScopedLock")
                     && ! captureSource.contains ("while")
                     && ! captureSource.contains ("new ")
                     && ! captureUtilitySource.contains ("std::function")
                     && ! captureUtilitySource.contains ("while")
                     && ! captureUtilitySource.contains ("new "),
                 "snapshot capture must be exactly-three-attempt, prepared, bounded and realtime-safe");
    test.expect (builderStart >= 0 && captureStart > builderStart
                     && processorHeader.contains ("std::uint64_t generation) const noexcept")
                     && ! builderSource.contains ("getRawParameterValue")
                     && ! builderSource.contains ("getParameter (")
                     && ! builderSource.contains ("juce::String")
                     && ! builderSource.contains ("ValueTree")
                     && ! builderSource.contains ("ScopedLock")
                     && ! builderSource.contains ("while")
                     && ! builderSource.contains ("new "),
                 "typed snapshot builder must be noexcept and lookup/string/tree/allocation/lock-free");

    const auto contour = processor.captureContourSnapshot ({});
    test.expect (std::abs (contour.controls.filterAttack - sanitized.contours.filterAttack) < 1.0e-6f
                     && contour.contract == sanitized.contourContract,
                 "Task 3B contour snapshot must delegate to complete capture semantics");

    {
        auto coherentPrior = distinct;
        coherentPrior.generation = 42;
        coherentPrior.contourContract = StateContract::ContourContract::legacyCrossedContours;
        coherentPrior.usedFallback = false;
        auto constructorInitial = fallback;
        constructorInitial.generation = 0;
        constructorInitial.contourContract = StateContract::ContourContract::canonicalContours;
        constructorInitial.usedFallback = false;
        int generationReads = 0;
        int valueReads = 0;
        int contractReads = 0;
        int builderCalls = 0;
        const std::array<std::uint64_t, 6> generations { 0, 2, 4, 6, 8, 10 };
        auto failed = ParameterSnapshotCapture::capture (
            coherentPrior, constructorInitial,
            [&]() noexcept { return generations[static_cast<std::size_t> (generationReads++)]; },
            [&]() noexcept {
                ++valueReads;
                return std::array<float, ParameterRegistry::parameterCount> {};
            },
            [&]() noexcept {
                ++contractReads;
                return StateContract::ContourContract::canonicalContours;
            },
            [&](const auto&, StateContract::ContourContract, std::uint64_t) noexcept {
                ++builderCalls;
                return constructorInitial;
            });
        const auto failedValues = snapshotPhysicalValues (failed);
        const auto priorValues = snapshotPhysicalValues (coherentPrior);
        bool priorRetained = failed.generation == coherentPrior.generation
                          && failed.contourContract == coherentPrior.contourContract
                          && failed.coherent && failed.usedFallback
                          && failedValues == priorValues;
        test.expect (generationReads == 6 && valueReads == 3 && contractReads == 3
                         && builderCalls == 0 && priorRetained,
                     "three failed equal-even attempts must return the coherent prior with payload/generation/contract retained and fallback flagged");

        auto incoherentCaller = coherentPrior;
        incoherentCaller.coherent = false;
        generationReads = 0;
        valueReads = 0;
        contractReads = 0;
        auto initialFallback = ParameterSnapshotCapture::capture (
            incoherentCaller, constructorInitial,
            [&]() noexcept { return generations[static_cast<std::size_t> (generationReads++)]; },
            [&]() noexcept {
                ++valueReads;
                return std::array<float, ParameterRegistry::parameterCount> {};
            },
            [&]() noexcept {
                ++contractReads;
                return StateContract::ContourContract::legacyCrossedContours;
            },
            [&](const auto&, StateContract::ContourContract, std::uint64_t) noexcept {
                ++builderCalls;
                return coherentPrior;
            });
        const auto initialValues = snapshotPhysicalValues (constructorInitial);
        const auto initialFallbackValues = snapshotPhysicalValues (initialFallback);
        bool initialRetained = initialFallback.generation == constructorInitial.generation
                            && initialFallback.contourContract
                                   == constructorInitial.contourContract
                            && initialFallback.coherent && initialFallback.usedFallback
                            && initialValues == initialFallbackValues;
        test.expect (generationReads == 6 && valueReads == 3 && contractReads == 3
                         && initialRetained,
                     "an incoherent caller fallback must select the coherent constructor initial after exactly three failures");

        generationReads = 0;
        valueReads = 0;
        contractReads = 0;
        builderCalls = 0;
        const std::array<std::uint64_t, 2> stableGeneration { 12, 12 };
        const auto success = ParameterSnapshotCapture::capture (
            coherentPrior, constructorInitial,
            [&]() noexcept { return stableGeneration[static_cast<std::size_t> (generationReads++)]; },
            [&]() noexcept {
                ++valueReads;
                return distinctInput;
            },
            [&]() noexcept {
                ++contractReads;
                return StateContract::ContourContract::canonicalContours;
            },
            [&](const auto&, StateContract::ContourContract contract,
                std::uint64_t generation) noexcept {
                ++builderCalls;
                auto result = distinct;
                result.generation = generation;
                result.contourContract = contract;
                result.usedFallback = false;
                return result;
            });
        test.expect (generationReads == 2 && valueReads == 1 && contractReads == 1
                         && builderCalls == 1 && success.generation == 12
                         && success.contourContract
                                == StateContract::ContourContract::canonicalContours
                         && ! success.usedFallback,
                     "equal-even utility capture must build and return the accepted generation once");
    }

    const auto setPhysical = [] (MoogMiniAudioProcessor& target,
                                 ParameterRegistry::Key key,
                                 float physical)
    {
        auto* parameter = target.getPreparedParameter (key);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (physical));
    };
    MoogMiniAudioProcessor nativeSource;
    for (std::size_t index = 0; index < descriptors.size(); ++index)
    {
        const auto& descriptor = descriptors[index];
        const auto value = descriptor.kind == ParameterRegistry::Kind::floating
                         ? 0.83f
                         : descriptor.kind == ParameterRegistry::Kind::choice
                             ? descriptor.rangeEnd
                             : descriptor.physicalDefault >= 0.5f ? 0.0f : 1.0f;
        setPhysical (nativeSource, static_cast<ParameterRegistry::Key> (index), value);
    }
    const auto nativeBytes = serialiseProcessorStateBytes (nativeSource);
    auto legacyXml = juce::parseXML (sourceRoot.getChildFile (
        "Tests/fixtures/state/legacy-default-state.xml"));
    test.expect (legacyXml != nullptr, "whole-state snapshot stress legacy fixture must parse");
    if (legacyXml != nullptr)
    {
        for (std::size_t index = 0; index < legacyParameterIds.size(); ++index)
        {
            const auto& descriptor = descriptors[index];
            const auto value = descriptor.kind == ParameterRegistry::Kind::floating
                             ? 0.17f : 0.0f;
            findParameterXml (*legacyXml, descriptor.id)->setAttribute ("value", value);
        }
        const auto legacyBytes = binaryFromXml (*legacyXml);
        MoogMiniAudioProcessor nativeExpectedProcessor;
        MoogMiniAudioProcessor legacyExpectedProcessor;
        const auto nativeRestore = nativeExpectedProcessor.restoreState (
            nativeBytes.getData(), static_cast<int> (nativeBytes.getSize()));
        const auto legacyRestore = legacyExpectedProcessor.restoreState (
            legacyBytes.getData(), static_cast<int> (legacyBytes.getSize()));
        const auto nativeExpected = nativeExpectedProcessor.captureParameterSnapshot ({});
        const auto legacyExpected = legacyExpectedProcessor.captureParameterSnapshot ({});
        const auto nativeExpectedValues = snapshotPhysicalValues (nativeExpected);
        const auto legacyExpectedValues = snapshotPhysicalValues (legacyExpected);
        test.expect (nativeRestore.succeeded() && legacyRestore.succeeded()
                         && nativeExpected.contourContract
                                == StateContract::ContourContract::canonicalContours
                         && legacyExpected.contourContract
                                == StateContract::ContourContract::legacyCrossedContours,
                     "distinct native and legacy whole-state restore vectors must prepare");

        MoogMiniAudioProcessor shared;
        shared.restoreState (nativeBytes.getData(), static_cast<int> (nativeBytes.getSize()));
        std::atomic<bool> begin { false };
        std::atomic<bool> writerDone { false };
        std::atomic<int> mixed { 0 };
        std::atomic<int> fallbackCount { 0 };
        std::thread reader ([&]
        {
            auto prior = shared.captureParameterSnapshot ({});
            while (! begin.load (std::memory_order_acquire))
                std::this_thread::yield();
            int captures = 0;
            do
            {
                const auto previous = prior;
                const auto snapshot = shared.captureParameterSnapshot (prior);
                if (! snapshot.usedFallback)
                    prior = snapshot;
                else
                {
                    fallbackCount.fetch_add (1, std::memory_order_relaxed);
                    const auto previousValues = snapshotPhysicalValues (previous);
                    const auto fallbackValues = snapshotPhysicalValues (snapshot);
                    bool matchesPrevious = snapshot.generation == previous.generation
                                        && snapshot.contourContract == previous.contourContract;
                    for (std::size_t index = 0; index < fallbackValues.size(); ++index)
                        matchesPrevious = matchesPrevious
                            && std::abs (fallbackValues[index] - previousValues[index]) < 1.0e-6f;
                    if (! matchesPrevious)
                        mixed.fetch_add (1, std::memory_order_relaxed);
                }
                const auto values = snapshotPhysicalValues (snapshot);
                const auto& expected = snapshot.contourContract
                                             == StateContract::ContourContract::canonicalContours
                                         ? nativeExpectedValues : legacyExpectedValues;
                bool complete = snapshot.coherent && (snapshot.generation & 1u) == 0u;
                for (std::size_t index = 0; index < values.size(); ++index)
                    complete = complete
                        && std::abs (values[index] - expected[index]) < 1.0e-6f;
                if (! complete)
                    mixed.fetch_add (1, std::memory_order_relaxed);
                ++captures;
            }
            while (! writerDone.load (std::memory_order_acquire) || captures < 5000);
        });
        begin.store (true, std::memory_order_release);
        for (int iteration = 0; iteration < 500; ++iteration)
        {
            const auto& bytes = iteration % 2 == 0 ? legacyBytes : nativeBytes;
            if (! shared.restoreState (bytes.getData(), static_cast<int> (bytes.getSize())).succeeded())
                mixed.fetch_add (1, std::memory_order_relaxed);
        }
        writerDone.store (true, std::memory_order_release);
        reader.join();
        static_cast<void> (fallbackCount.load (std::memory_order_relaxed));
        test.expect (mixed.load (std::memory_order_relaxed) == 0,
                     "alternating native/legacy restores must yield complete generations; any observed fallback must equal the prior coherent snapshot");
    }

    MoogMiniAudioProcessor renderProcessor;
    setPhysical (renderProcessor, ParameterRegistry::Key::osc1OnOff, 1.0f);
    setPhysical (renderProcessor, ParameterRegistry::Key::osc1Vol, 10.0f);
    setPhysical (renderProcessor, ParameterRegistry::Key::outputVolKnob, 10.0f);
    rawParameterForTest (renderProcessor, ParameterRegistry::Key::filterCutoff)
        ->store (std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);
    renderProcessor.setRateAndBufferSizeDetails (48000.0, 128);
    renderProcessor.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> renderBuffer (renderProcessor.getTotalNumOutputChannels(), 128);
    renderBuffer.clear();
    juce::MidiBuffer renderMidi;
    renderMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    const auto renderDiagnosticBefore = renderProcessor.getInvalidParameterValueCount();
    renderProcessor.processBlock (renderBuffer, renderMidi);
    test.expect (isFinite (renderBuffer)
                     && renderProcessor.getInvalidParameterValueCount()
                            == renderDiagnosticBefore + 1,
                 "actual processor rendering must consume the sanitized complete snapshot");
    renderProcessor.releaseResources();

    for (const auto& [path, hash] :
         std::to_array<std::pair<const char*, const char*>> ({
             { "Tests/fixtures/parameters/legacy-parameter-inventory.json", "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d" },
             { "Tests/fixtures/parameters/parameter-registry-v2.json", "2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284" },
             { "Tests/fixtures/state/legacy-default-state.xml", "07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21" },
             { "Tests/fixtures/state/legacy-representative-state.xml", "e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa" },
             { "Tests/fixtures/state/native-default-state-v2.xml", "ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507" },
             { "Tests/fixtures/state/migrated-default-state-v2.xml", "4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958" },
             { "Tests/fixtures/state/migrated-representative-state-v2.xml", "f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40" },
             { "Tests/fixtures/state/contour-routing-conversion-trace.json", "ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c" }
         }))
        expectFixtureHash (test, sourceRoot.getChildFile (path), hash, path);
}

void testParameterBindingContract (TestContext& test)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    const auto bindingHeader = sourceRoot.getChildFile ("Source/ParameterBinding.h");
    const auto bindingSource = sourceRoot.getChildFile ("Source/ParameterBinding.cpp");
    const auto bindingHeaderText = bindingHeader.loadFileAsString();
    const auto bindingSourceText = bindingSource.loadFileAsString();
    const auto editorHeader = sourceRoot.getChildFile ("Source/PluginEditor.h")
                                  .loadFileAsString();
    const auto editorSource = sourceRoot.getChildFile ("Source/PluginEditor.cpp")
                                  .loadFileAsString();
    const auto pitchWheelSource = sourceRoot.getChildFile ("Source/PitchWheelSlider.cpp")
                                      .loadFileAsString();

    test.expect (bindingHeader.existsAsFile() && bindingSource.existsAsFile()
                     && bindingHeaderText.contains ("class ParameterBinding")
                     && bindingHeaderText.contains ("ParameterRegistry::Key")
                     && bindingHeaderText.contains ("juce::ParameterAttachment")
                     && bindingHeaderText.contains ("DisplayMapping"),
                 "typed prepared ParameterBinding production files must exist");
    test.expect (! editorHeader.contains ("getParameterID")
                     && ! editorHeader.contains ("getNormalizedValue")
                     && ! editorHeader.contains ("getSliderValueFromNormalized")
                     && ! editorHeader.contains ("getEnumSizeLessOne")
                     && ! editorHeader.contains ("sliderHasChanged")
                     && ! editorSource.contains ("getParameterID")
                     && ! editorSource.contains ("getNormalizedValue")
                     && ! editorSource.contains ("getSliderValueFromNormalized")
                     && ! editorSource.contains ("getEnumSizeLessOne")
                     && ! editorSource.contains ("sliderHasChanged"),
                 "editor duplicate ID and normalization chains must be deleted");
    test.expect (! editorSource.contains ("audioProcessor.apvts.getRawParameterValue")
                     && ! editorSource.contains ("audioProcessor.apvts.getParameter(")
                     && ! editorSource.contains ("audioProcessor.apvts.getParameter (")
                     && ! editorSource.contains ("audioProcessor.apvts.getParameterAsValue"),
                 "editor must contain no direct APVTS parameter getter");

    const auto timerStart = editorSource.indexOf (
        "void MoogMiniAudioProcessorEditor::timerCallback()");
    const auto timerSource = timerStart >= 0 ? editorSource.substring (timerStart)
                                             : juce::String {};
    const auto capture = timerSource.indexOf ("captureParameterSnapshot");
    test.expect (timerStart >= 0 && capture >= 0
                     && timerSource.indexOf (capture + 1, "captureParameterSnapshot") < 0
                     && ! timerSource.contains ("getRawParameterValue")
                     && ! timerSource.contains ("getParameter(")
                     && ! timerSource.contains ("getParameter (")
                     && ! timerSource.contains ("getParameterAsValue")
                     && ! timerSource.contains ("getParameterID"),
                 "timer must capture one complete snapshot and perform no parameter lookup");
    test.expect (editorHeader.contains ("ParameterRegistry::Key parameterKey")
                     && editorHeader.contains ("std::vector<std::unique_ptr<ParameterBinding>>")
                     && editorSource.contains ("setValueAsCompleteGesture")
                     && editorSource.contains ("ContourRouting::mapStoredControls"),
                 "editor controls must retain typed bindings and dynamic stored contour mapping");
    const auto pitchMouseUpStart = pitchWheelSource.indexOf (
        "void PitchWheelSlider::mouseUp");
    const auto pitchMouseUpSource = pitchWheelSource.substring (pitchMouseUpStart);
    const auto pitchReset = pitchMouseUpSource.indexOf ("setValue(0.0");
    const auto pitchEnd = pitchMouseUpSource.indexOf ("    Slider::mouseUp(event)");
    test.expect (pitchMouseUpStart >= 0 && pitchReset >= 0 && pitchEnd >= 0
                     && pitchReset < pitchEnd,
                 "pitch wheel spring-back value must remain inside its single drag gesture");

    PitchWheelSlider pitchWheel;
    test.expect (pitchWheel.getMinimum() == -7.0
                     && pitchWheel.getMaximum() == 7.0
                     && pitchWheel.getInterval() == 0.01,
                 "pitch wheel component must display the centered seven-semitone range");

    struct ParameterEventListener final : juce::AudioProcessorParameter::Listener
    {
        void parameterValueChanged (int, float) override { events.emplace_back ("value"); }
        void parameterGestureChanged (int, bool starting) override
        {
            events.emplace_back (starting ? "begin" : "end");
        }
        std::vector<std::string> events;
    };

    MoogMiniAudioProcessor processor;
    const auto close = [] (float left, float right)
    {
        return std::abs (left - right) < 1.0e-5f;
    };
    const auto testSliderMapping = [&] (ParameterRegistry::Key key,
                                        double componentMinimum,
                                        double componentMaximum,
                                        double componentInterval,
                                        DisplayMapping mapping,
                                        float parameterValue,
                                        double expectedComponentValue,
                                        double componentValue,
                                        float expectedParameterValue,
                                        std::string_view description)
    {
        auto* parameter = processor.getPreparedParameter (key);
        juce::Slider slider;
        slider.setRange (componentMinimum, componentMaximum, componentInterval);
        ParameterBinding binding (key, *parameter, slider, mapping);

        parameter->setValueNotifyingHost (parameter->convertTo0to1 (parameterValue));
        test.expect (close (static_cast<float> (slider.getValue()),
                            static_cast<float> (expectedComponentValue)),
                     description);
        binding.beginGesture();
        slider.setValue (componentValue, juce::sendNotificationSync);
        binding.endGesture();
        test.expect (close (parameter->convertFrom0to1 (parameter->getValue()),
                            expectedParameterValue),
                     description);
    };

    testSliderMapping (ParameterRegistry::Key::feedbackKnob, 0.0, 10.0, 0.01,
                       DisplayMapping::componentRange, 0.25f, 2.5, 7.0, 0.7f,
                       "float binding must preserve normalized component range mapping");
    testSliderMapping (ParameterRegistry::Key::osc1Waveform, 0.0, 5.0, 1.0,
                       DisplayMapping::componentRange, 5.0f, 5.0, 2.0, 2.0f,
                       "choice binding must preserve exact endpoints and round trips");
    testSliderMapping (ParameterRegistry::Key::tune, -2.5, 2.5, 0.5,
                       DisplayMapping::componentRange, 10.0f, 2.5, -2.5, 0.0f,
                       "tune binding must preserve -2.5 through +2.5 display");
    testSliderMapping (ParameterRegistry::Key::osc2Freq, -8.0, 8.0, 1.0,
                       DisplayMapping::componentRange, 16.0f, 8.0, -8.0, 0.0f,
                       "detune binding must preserve -8 through +8 display");
    testSliderMapping (ParameterRegistry::Key::filterCutoff, -5.0, 5.0, 0.01,
                       DisplayMapping::componentRange, 1.0f, 5.0, -5.0, 0.0f,
                       "cutoff binding must preserve -5 through +5 display");
    testSliderMapping (ParameterRegistry::Key::pitchWheelValue, -7.0, 7.0, 0.01,
                       DisplayMapping::componentRange, 0.5f, 0.0, 7.0, 1.0f,
                       "Pitch Wheel binding must preserve center and positive endpoint");
    testSliderMapping (ParameterRegistry::Key::pitchWheelValue, -7.0, 7.0, 0.01,
                       DisplayMapping::componentRange, 0.0f, -7.0, 0.0, 0.5f,
                       "Pitch Wheel binding must preserve negative endpoint and center");
    testSliderMapping (ParameterRegistry::Key::filterEmphasis, 0.0, 10.0, 1.0,
                       DisplayMapping::componentRange, 10.0f, 10.0, 3.0, 3.0f,
                       "panel-index binding must preserve exact 0 through 10 steps");
    testSliderMapping (ParameterRegistry::Key::filterAttackTimeKnob,
                       10.0, 10000.0, 0.01,
                       DisplayMapping::contourTimeMilliseconds,
                       0.0f, 10.0, 5000.0, 0.5f,
                       "contour-time binding must retain physical*10000 and 10 ms clamp");

    auto* booleanParameter = processor.getPreparedParameter (
        ParameterRegistry::Key::filterModSwitch);
    juce::ToggleButton button;
    ParameterBinding buttonBinding (ParameterRegistry::Key::filterModSwitch,
                                    *booleanParameter, button);
    ParameterEventListener buttonEvents;
    booleanParameter->addListener (&buttonEvents);
    button.setToggleState (true, juce::sendNotificationSync);
    test.expect (buttonEvents.events == std::vector<std::string> { "begin", "value", "end" }
                     && booleanParameter->getValue() == 1.0f,
                 "button binding must emit one complete begin/value/end gesture");
    booleanParameter->removeListener (&buttonEvents);

    auto* continuousParameter = processor.getPreparedParameter (
        ParameterRegistry::Key::modWheelValue);
    juce::Slider continuousSlider;
    continuousSlider.setRange (0.0, 1.0, 0.0);
    ParameterBinding continuousBinding (ParameterRegistry::Key::modWheelValue,
                                        *continuousParameter, continuousSlider,
                                        DisplayMapping::componentRange);
    ParameterEventListener continuousEvents;
    continuousParameter->addListener (&continuousEvents);
    continuousBinding.beginGesture();
    continuousSlider.setValue (0.2, juce::sendNotificationSync);
    continuousSlider.setValue (0.8, juce::sendNotificationSync);
    continuousBinding.endGesture();
    test.expect (continuousEvents.events
                     == std::vector<std::string> { "begin", "value", "value", "end" },
                 "continuous binding must emit exactly begin/value(s)/end");

    continuousEvents.events.clear();

    MoogMiniAudioProcessor restoredProcessor;
    auto* restoredParameter = restoredProcessor.getPreparedParameter (
        ParameterRegistry::Key::outputVolKnob);
    juce::Slider restoredSlider;
    restoredSlider.setRange (0.0, 10.0, 1.0);
    ParameterBinding restoredBinding (ParameterRegistry::Key::outputVolKnob,
                                      *restoredParameter, restoredSlider,
                                      DisplayMapping::componentRange);
    MoogMiniAudioProcessor stateSource;
    auto* sourceParameter = stateSource.getPreparedParameter (
        ParameterRegistry::Key::outputVolKnob);
    sourceParameter->setValueNotifyingHost (sourceParameter->convertTo0to1 (7.0f));
    const auto stateBytes = serialiseProcessorStateBytes (stateSource);
    auto beginOffThreadChanges = std::make_shared<juce::WaitableEvent>();
    std::atomic<bool> restoreSucceeded { false };
    std::atomic<bool> offThreadChangesFinished { false };
    constexpr auto delayedWorkerMilliseconds = 600;
    std::thread hostUpdate ([&, beginOffThreadChanges] {
        beginOffThreadChanges->wait();
        juce::Thread::sleep (delayedWorkerMilliseconds);
        continuousParameter->setValueNotifyingHost (0.35f);
        const auto restore = restoredProcessor.restoreState (
            stateBytes.getData(), static_cast<int> (stateBytes.getSize()));
        restoreSucceeded.store (restore.succeeded(), std::memory_order_release);
        offThreadChangesFinished.store (true, std::memory_order_release);
    });
    juce::MessageManager::callAsync ([beginOffThreadChanges] {
        beginOffThreadChanges->signal();
    });

    const auto bindingUpdatesApplied = [&] {
        return offThreadChangesFinished.load (std::memory_order_acquire)
            && close (static_cast<float> (continuousSlider.getValue()), 0.35f)
            && restoredSlider.getValue() == 7.0;
    };
    constexpr auto callbackWatchdogMilliseconds = 5000.0;
    bool callbacksCompleted = false;

#if JUCE_MAC
    const auto pumpDeadline = juce::Time::getMillisecondCounterHiRes()
                            + callbackWatchdogMilliseconds;
    while (! bindingUpdatesApplied()
           && juce::Time::getMillisecondCounterHiRes() < pumpDeadline)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.005, true);
    }
    callbacksCompleted = bindingUpdatesApplied();
#else
    ConditionalDispatchLoopStopper stopWhenComplete (bindingUpdatesApplied,
                                                      callbackWatchdogMilliseconds);
    juce::MessageManager::getInstance()->runDispatchLoop();
    callbacksCompleted = stopWhenComplete.completed();
#endif

    beginOffThreadChanges->signal();
    hostUpdate.join();

#if JUCE_MAC
    const auto postJoinDrainDeadline = juce::Time::getMillisecondCounterHiRes() + 1000.0;
    while (! bindingUpdatesApplied()
           && juce::Time::getMillisecondCounterHiRes() < postJoinDrainDeadline)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.005, true);
    }
    callbacksCompleted = bindingUpdatesApplied();
#endif

    test.expect (callbacksCompleted,
                 "message-thread pump must drain binding callbacks within its bounded wait");
    test.expect (continuousEvents.events == std::vector<std::string> { "value" }
                     && close (static_cast<float> (continuousSlider.getValue()), 0.35f),
                 "parameter-originated UI update must not echo a value or gesture");
    continuousParameter->removeListener (&continuousEvents);
    test.expect (restoreSucceeded.load (std::memory_order_acquire)
                     && restoredSlider.getValue() == 7.0,
                 "live binding must update after successful restore without timer polling");

    MoogMiniAudioProcessor contourProcessor;
    contourProcessor.setSignalFlowContourControl (
        ContourRouting::SemanticContour::filter, ContourRouting::Stage::attack, 0.31f);
    test.expect (close (physicalParameterValue (contourProcessor, "filterAttackTimeKnob"),
                        0.31f),
                 "canonical semantic contour write must select the filter stable ID");
    const auto legacyBytes = binaryFromXml (*juce::parseXML (sourceRoot.getChildFile (
        "Tests/fixtures/state/legacy-representative-state.xml")));
    const auto legacyRestore = contourProcessor.restoreState (
        legacyBytes.getData(), static_cast<int> (legacyBytes.getSize()));
    contourProcessor.setSignalFlowContourControl (
        ContourRouting::SemanticContour::filter, ContourRouting::Stage::attack, 0.42f);
    auto* decay = contourProcessor.getPreparedParameter (ParameterRegistry::Key::decaySwitch);
    decay->setValueNotifyingHost (0.0f);
    const auto contourSnapshot = contourProcessor.captureParameterSnapshot ({});
    const auto storedContours = ContourRouting::mapStoredControls (
        contourSnapshot.contourContract, contourSnapshot.contours);
    test.expect (legacyRestore.succeeded()
                     && close (physicalParameterValue (contourProcessor,
                                                       "loudnessAttackTimeKnob"), 0.42f)
                     && storedContours.filter.sustain != 1.0f
                     && storedContours.loudness.sustain != 1.0f,
                 "restored legacy semantic contour writes must remap dynamically and disabled-Decay display must retain stored sustains");
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

double renderProcessorPitch (const int oscillator,
                             const int offsetIndex,
                             const int midiNote,
                             TestContext& test,
                             const int rangeIndex = 3,
                             const int masterTuneIndex = 5,
                             const float pitchWheel = 0.5f,
                             const bool invalidateMasterTune = false,
                             std::uint64_t* invalidDiagnosticDelta = nullptr,
                             const bool oscillator3KeyboardControl = true,
                             const int totalSamples = 65536,
                             const int discardedSamples = 8192)
{
    constexpr double sampleRate = 48000.0;

    MoogMiniAudioProcessor processor;
    setParameter (processor, "osc1OnOff", oscillator == 1 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc2OnOff", oscillator == 2 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc3OnOff", oscillator == 3 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc1Waveform", 0.0f, test);
    setParameter (processor, "osc2Waveform", 0.0f, test);
    setParameter (processor, "osc3Waveform", 0.0f, test);
    setParameter (processor, "osc1Range", static_cast<float> (rangeIndex) / 5.0f, test);
    setParameter (processor, "osc2Range", static_cast<float> (rangeIndex) / 5.0f, test);
    setParameter (processor, "osc3Range", static_cast<float> (rangeIndex) / 5.0f, test);
    setParameter (processor, "tune", static_cast<float> (masterTuneIndex) / 10.0f, test);
    setParameter (processor, "osc2Freq",
                  static_cast<float> (oscillator == 2 ? offsetIndex : 8) / 16.0f,
                  test);
    setParameter (processor, "osc3Freq",
                  static_cast<float> (oscillator == 3 ? offsetIndex : 8) / 16.0f,
                  test);
    setParameter (processor, "pitchWheelValue", pitchWheel, test);
    setParameter (processor, "osc1Vol", oscillator == 1 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc2Vol", oscillator == 2 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc3Vol", oscillator == 3 ? 1.0f : 0.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);
    setParameter (processor, "loudnessSustainLevelKnob", 1.0f, test);
    setParameter (processor, "filterCutoff", 1.0f, test);
    setParameter (processor, "glideSwitch", 0.0f, test);
    setParameter (processor, "oscModSwitch", 0.0f, test);
    setParameter (processor, "noiseOnOffSwitch", 0.0f, test);
    setParameter (processor, "extInputVolSwitch", 0.0f, test);
    setParameter (processor, "osc3CtrlMode", oscillator3KeyboardControl ? 1.0f : 0.0f,
                  test);

    if (invalidateMasterTune)
        rawParameterForTest (processor, ParameterRegistry::Key::tune)
            ->store (std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);

    processor.setRateAndBufferSizeDetails (sampleRate, totalSamples);
    processor.prepareToPlay (sampleRate, totalSamples);
    juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), totalSamples);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, 1.0f), 0);
    const auto diagnosticBefore = processor.getInvalidParameterValueCount();
    processor.processBlock (buffer, midi);
    if (invalidDiagnosticDelta != nullptr)
        *invalidDiagnosticDelta =
            processor.getInvalidParameterValueCount() - diagnosticBefore;

    int crossings = 0;
    const auto* samples = buffer.getReadPointer (0);
    for (int sample = discardedSamples + 1; sample < totalSamples; ++sample)
        if (samples[sample - 1] <= 0.0f && samples[sample] > 0.0f)
            ++crossings;

    processor.releaseResources();
    return static_cast<double> (crossings) * sampleRate
         / static_cast<double> (totalSamples - discardedSamples);
}

double estimatePositiveCrossingFrequency (const juce::AudioBuffer<float>& buffer,
                                          const int firstSample,
                                          const int endSample)
{
    int crossings = 0;
    const auto* samples = buffer.getReadPointer (0);
    for (int sample = firstSample + 1; sample < endSample; ++sample)
        if (samples[sample - 1] <= 0.0f && samples[sample] > 0.0f)
            ++crossings;
    return static_cast<double> (crossings) * 48000.0
         / static_cast<double> (endSample - firstSample);
}

struct GlidePitchObservation {
    double source = 0.0;
    double intermediate = 0.0;
    double converged = 0.0;
    std::uint64_t invalidDiagnosticDelta = 0;
};

GlidePitchObservation renderProcessorGlide (
    const int oscillator,
    const int rangeIndex,
    const int offsetIndex,
    const int masterTuneIndex,
    const float pitchWheel,
    const bool oscillator3KeyboardControl,
    TestContext& test)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sourceSamples = 65536;
    constexpr int glideSamples = 192000;

    MoogMiniAudioProcessor processor;
    setParameter (processor, "osc1OnOff", oscillator == 1 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc2OnOff", oscillator == 2 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc3OnOff", oscillator == 3 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc1Waveform", 0.0f, test);
    setParameter (processor, "osc2Waveform", 0.0f, test);
    setParameter (processor, "osc3Waveform", 0.0f, test);
    setParameter (processor, "osc1Range", static_cast<float> (rangeIndex) / 5.0f,
                  test);
    setParameter (processor, "osc2Range", static_cast<float> (rangeIndex) / 5.0f,
                  test);
    setParameter (processor, "osc3Range", static_cast<float> (rangeIndex) / 5.0f,
                  test);
    setParameter (processor, "tune", static_cast<float> (masterTuneIndex) / 10.0f,
                  test);
    setParameter (processor, "osc2Freq",
                  static_cast<float> (oscillator == 2 ? offsetIndex : 8) / 16.0f,
                  test);
    setParameter (processor, "osc3Freq",
                  static_cast<float> (oscillator == 3 ? offsetIndex : 8) / 16.0f,
                  test);
    setParameter (processor, "pitchWheelValue", pitchWheel, test);
    setParameter (processor, "osc1Vol", oscillator == 1 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc2Vol", oscillator == 2 ? 1.0f : 0.0f, test);
    setParameter (processor, "osc3Vol", oscillator == 3 ? 1.0f : 0.0f, test);
    setParameter (processor, "outputVolKnob", 1.0f, test);
    setParameter (processor, "loudnessSustainLevelKnob", 1.0f, test);
    setParameter (processor, "filterCutoff", 1.0f, test);
    setParameter (processor, "ctrlGlideKnob", 1.0f, test);
    setParameter (processor, "glideSwitch", 0.0f, test);
    setParameter (processor, "oscModSwitch", 0.0f, test);
    setParameter (processor, "noiseOnOffSwitch", 0.0f, test);
    setParameter (processor, "extInputVolSwitch", 0.0f, test);
    setParameter (processor, "osc3CtrlMode", oscillator3KeyboardControl ? 1.0f : 0.0f,
                  test);

    processor.setRateAndBufferSizeDetails (sampleRate, glideSamples);
    processor.prepareToPlay (sampleRate, glideSamples);
    const auto diagnosticBefore = processor.getInvalidParameterValueCount();

    juce::AudioBuffer<float> source (
        processor.getTotalNumOutputChannels(), sourceSamples);
    source.clear();
    juce::MidiBuffer sourceMidi;
    sourceMidi.addEvent (juce::MidiMessage::noteOn (1, 45, 1.0f), 0);
    processor.processBlock (source, sourceMidi);

    setParameter (processor, "glideSwitch", 1.0f, test);
    juce::AudioBuffer<float> glide (
        processor.getTotalNumOutputChannels(), glideSamples);
    glide.clear();
    juce::MidiBuffer targetMidi;
    targetMidi.addEvent (juce::MidiMessage::noteOn (1, 57, 1.0f), 0);
    processor.processBlock (glide, targetMidi);

    GlidePitchObservation result {
        .source = estimatePositiveCrossingFrequency (
            source, sourceSamples - 32768, sourceSamples),
        .intermediate = estimatePositiveCrossingFrequency (glide, 4096, 32768),
        .converged = estimatePositiveCrossingFrequency (
            glide, glideSamples - 32768, glideSamples),
        .invalidDiagnosticDelta =
            processor.getInvalidParameterValueCount() - diagnosticBefore,
    };
    processor.releaseResources();
    return result;
}

void testPitchDomainContract (TestContext& test)
{
    bool midiCoordinatesExact = true;
    bool adjacentMidiRatiosExact = true;
    PitchDomain::Checked<PitchDomain::Hertz> previousMidiHz;
    for (int midi = 17; midi <= 60; ++midi) {
        const auto note = PitchDomain::midiNote (midi);
        const auto noteHz = note.valid ? PitchDomain::toHertz (note.value)
                                       : PitchDomain::Checked<PitchDomain::Hertz> {};
        midiCoordinatesExact = midiCoordinatesExact
            && note.valid && note.value.value == midi - 69 && noteHz.valid;
        if (previousMidiHz.valid)
            adjacentMidiRatiosExact = adjacentMidiRatiosExact
                && std::abs (noteHz.value.value / previousMidiHz.value.value
                             - std::exp2 (1.0 / 12.0)) < 1.0e-12;
        previousMidiHz = noteHz;
    }
    test.expect (midiCoordinatesExact,
                 "MIDI 17 through 60 must retain exact semitone coordinates");
    test.expect (adjacentMidiRatiosExact,
                 "all 43 adjacent MIDI notes must retain equal-tempered semitone ratios");

    constexpr std::array expectedOffsets {
        -8.0, -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0,
         1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0, 8.0
    };
    for (int index = 0; index < static_cast<int> (expectedOffsets.size()); ++index) {
        const auto offset = PitchDomain::oscillatorOffset (index);
        test.expect (offset.valid && offset.value.value == expectedOffsets[index],
                     "oscillator offset must equal index minus eight");
    }

    const auto center = PitchDomain::oscillatorOffset (8);
    const auto centerHz = PitchDomain::toHertz (center.value);
    test.expect (center.valid && centerHz.valid && centerHz.value.value == 440.0,
                 "center selector must produce a unity A4 ratio");
    const auto adjacentSemitoneRatio = std::exp2 (1.0 / 12.0);
    bool adjacentSelectorRatiosExact = true;
    for (int index = 0; index < 16; ++index) {
        const auto lower = PitchDomain::oscillatorOffset (index);
        const auto upper = PitchDomain::oscillatorOffset (index + 1);
        const auto lowerHz = PitchDomain::toHertz (lower.value);
        const auto upperHz = PitchDomain::toHertz (upper.value);
        adjacentSelectorRatiosExact = adjacentSelectorRatiosExact
            && lower.valid && upper.valid && lowerHz.valid && upperHz.valid
            && std::abs (upperHz.value.value / lowerHz.value.value
                         - adjacentSemitoneRatio) < 1.0e-12;
    }
    test.expect (adjacentSelectorRatiosExact,
                 "every adjacent selector position must have one equal-tempered semitone ratio");

    constexpr std::array expectedRanges { -24.0, -12.0, 0.0, 12.0, 24.0 };
    for (int index = 1; index <= 5; ++index) {
        const auto contribution = PitchDomain::range (index);
        test.expect (contribution.valid
                         && contribution.value.mode == PitchDomain::RangeMode::musical
                         && contribution.value.semitones.value
                                == expectedRanges[static_cast<size_t> (index - 1)],
                     "musical range table must be exact");
    }
    const auto low = PitchDomain::range (0);
    test.expect (low.valid && low.value.mode == PitchDomain::RangeMode::lowFrequency,
                 "LO must remain an explicit special mode");

    constexpr std::array wheelAnchors {
        std::pair { 0.0, -7.0 },
        std::pair { 0.25, -3.5 },
        std::pair { 0.5, 0.0 },
        std::pair { 0.75, 3.5 },
        std::pair { 1.0, 7.0 },
    };
    for (const auto& [normalized, semitones] : wheelAnchors) {
        const auto bend = PitchDomain::pitchWheel (normalized);
        test.expect (bend.valid && bend.value.value == semitones,
                     "Pitch Wheel anchor must use the exact centered linear map");
    }

    bool denseWheelContract = true;
    double previous = -std::numeric_limits<double>::infinity();
    for (int point = 0; point <= 1000; ++point) {
        const auto normalized = static_cast<double> (point) / 1000.0;
        const auto bend = PitchDomain::pitchWheel (normalized);
        const auto mirror = PitchDomain::pitchWheel (1.0 - normalized);
        denseWheelContract = denseWheelContract
            && bend.valid && mirror.valid
            && bend.value.value == 14.0 * (normalized - 0.5)
            && std::abs (bend.value.value + mirror.value.value) < 1.0e-12
            && bend.value.value >= previous;
        previous = bend.value.value;
    }
    test.expect (denseWheelContract,
                 "Pitch Wheel must be linear, monotonic, centered, and symmetric");

    bool baselineMusicalCalibration = true;
    for (int rangeIndex = 1; rangeIndex <= 5; ++rangeIndex) {
        const auto selectedRange = PitchDomain::range (rangeIndex);
        const auto calibration = PitchDomain::calibration (
            PitchDomain::CalibrationProfile::baseline, selectedRange.value);
        baselineMusicalCalibration = baselineMusicalCalibration
            && selectedRange.valid && calibration.valid
            && calibration.value.value == 0.0;
    }
    const auto lowRange = PitchDomain::range (0);
    test.expect (baselineMusicalCalibration,
                 "baseline calibration must be exact zero for all musical ranges");
    test.expect (lowRange.valid
                     && ! PitchDomain::calibration (
                            PitchDomain::CalibrationProfile::baseline,
                            lowRange.value).valid,
                 "baseline calibration must reject LO");
    test.expect (! PitchDomain::pitchWheel (-0.001).valid
                     && ! PitchDomain::pitchWheel (1.001).valid
                     && ! PitchDomain::pitchWheel (
                            std::numeric_limits<double>::quiet_NaN()).valid
                     && ! PitchDomain::calibration (
                            static_cast<PitchDomain::CalibrationProfile> (255),
                            PitchDomain::range (3).value).valid,
                 "invalid wheel and calibration inputs must reject");

    const auto neutralRange = PitchDomain::range (3);
    const auto neutralTune = PitchDomain::masterTune (5);
    const auto neutralOffset = PitchDomain::oscillatorOffset (8);
    const auto neutralWheel = PitchDomain::pitchWheel (0.5);
    const auto neutralCalibration = PitchDomain::calibration (
        PitchDomain::CalibrationProfile::baseline, neutralRange.value);
    bool neutralMidiCompositionsValid = true;
    for (int midi = 0; midi <= 127; ++midi) {
        const auto note = PitchDomain::midiNote (midi);
        const auto composedNeutral = PitchDomain::compose ({
            note.value, neutralRange.value.semitones, neutralTune.value,
            neutralOffset.value, neutralWheel.value, neutralCalibration.value,
            PitchDomain::Semitones { 0.0 }
        });
        neutralMidiCompositionsValid = neutralMidiCompositionsValid
            && note.valid && neutralRange.valid && neutralTune.valid
            && neutralOffset.valid && neutralWheel.valid && neutralCalibration.valid
            && composedNeutral.valid && std::isfinite (composedNeutral.hertz.value)
            && composedNeutral.hertz.value > 0.0;
    }
    test.expect (neutralMidiCompositionsValid,
                 "MIDI 0 through 127 must compose to finite positive neutral frequencies");

    constexpr std::array expectedMasterTune {
        -2.5, -2.0, -1.5, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0, 2.5
    };
    bool masterTuneTableExact = true;
    for (int index = 0; index < static_cast<int> (expectedMasterTune.size()); ++index) {
        const auto tune = PitchDomain::masterTune (index);
        masterTuneTableExact = masterTuneTableExact
            && tune.valid
            && tune.value.value == expectedMasterTune[static_cast<size_t> (index)];
    }
    test.expect (masterTuneTableExact,
                 "master tune must expose the complete exact half-semitone table");

    PitchDomain::Contributions contributions {
        .note = { -5.0 },
        .range = { 12.0 },
        .masterTune = { 1.5 },
        .oscillatorOffset = { -3.0 },
        .pitchWheel = { 2.0 },
        .calibration = { 0.25 },
        .modulation = { -0.75 },
    };
    const auto composed = PitchDomain::compose (contributions);
    test.expect (composed.valid && composed.coordinate.value == 7.0,
                 "all pitch terms must add in semitone space");
    test.expect (composed.valid
                     && std::abs (composed.hertz.value
                                  - 440.0 * std::exp2 (7.0 / 12.0)) < 1.0e-12,
                 "composed pitch must convert once with the analytic formula");

    const auto ratio = PitchDomain::ratioToSemitones (1.25);
    test.expect (ratio.valid
                     && std::abs (std::exp2 (ratio.value.value / 12.0) - 1.25) < 1.0e-12,
                 "positive compatibility ratios must round-trip");
    test.expect (! PitchDomain::midiNote (-1).valid
                     && ! PitchDomain::midiNote (128).valid
                     && ! PitchDomain::range (-1).valid
                     && ! PitchDomain::range (6).valid
                     && ! PitchDomain::masterTune (-1).valid
                     && ! PitchDomain::oscillatorOffset (-1).valid
                     && ! PitchDomain::oscillatorOffset (17).valid
                     && ! PitchDomain::masterTune (11).valid
                     && ! PitchDomain::pitchWheel (std::numeric_limits<double>::infinity()).valid
                     && ! PitchDomain::ratioToSemitones (0.0).valid
                     && ! PitchDomain::fromHertz (
                            std::numeric_limits<double>::infinity()).valid,
                 "invalid note, range, tune, offset, wheel, and ratio inputs must reject");

    constexpr std::array wheelOutputs {
        std::pair { 0.0f, -7.0 },
        std::pair { 0.25f, -3.5 },
        std::pair { 0.5f, 0.0 },
        std::pair { 0.75f, 3.5 },
        std::pair { 1.0f, 7.0 },
    };
    for (const auto& [normalized, bend] : wheelOutputs) {
        const auto rendered = renderProcessorPitch (
            1, 8, 45, test, 3, 5, normalized);
        const auto expected =
            440.0 * std::exp2 ((45.0 + bend - 69.0) / 12.0);
        test.expect (std::abs (rendered / expected - 1.0) < 0.02,
                     "processor output must follow each governed wheel anchor");
    }

    const auto composedProcessorPitch =
        renderProcessorPitch (2, 5, 64, test, 2, 3, 0.75f);
    const auto expectedComposedMidi = 64.0 - 12.0 - 1.0 - 3.0 + 3.5;
    const auto expectedComposedFrequency =
        440.0 * std::exp2 ((expectedComposedMidi - 69.0) / 12.0);
    test.expect (
        std::abs (composedProcessorPitch / expectedComposedFrequency - 1.0) < 0.02,
        "processor pitch must apply the exact linear semitone bend once");

    struct RepresentativeOscillatorCase {
        int oscillator;
        int offsetIndex;
        int midiNote;
        int rangeIndex;
        int masterTuneIndex;
        float normalizedWheel;
        double expectedMidi;
    };
    constexpr std::array representativeOscillatorCases {
        RepresentativeOscillatorCase { 1, 8, 64, 2, 3, 0.75f, 54.5 },
        RepresentativeOscillatorCase { 2, 5, 64, 2, 3, 0.75f, 51.5 },
        RepresentativeOscillatorCase { 3, 11, 50, 4, 8, 0.25f, 63.0 },
    };
    for (const auto& representative : representativeOscillatorCases) {
        const auto rendered = renderProcessorPitch (
            representative.oscillator, representative.offsetIndex,
            representative.midiNote, test, representative.rangeIndex,
            representative.masterTuneIndex, representative.normalizedWheel);
        const auto expected = 440.0 * std::exp2 (
            (representative.expectedMidi - 69.0) / 12.0);
        test.expect (std::abs (rendered / expected - 1.0) < 0.02,
                     "each oscillator must compose its representative nonzero pitch factors");
    }

    std::uint64_t invalidDiagnosticDelta = 0;
    const auto invalidFallbackPitch = renderProcessorPitch (
        2, 8, 69, test, 3, 5, 0.5f, true, &invalidDiagnosticDelta);
    test.expect (std::isfinite (invalidFallbackPitch)
                     && std::abs (invalidFallbackPitch / 440.0 - 1.0) < 0.02
                     && invalidDiagnosticDelta == 1,
                 "invalid processor pitch input must use the declared fallback and diagnose once");

    MoogMiniAudioProcessor emptyMidiOsc3Processor;
    setParameter (emptyMidiOsc3Processor, "osc1OnOff", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "osc2OnOff", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "osc3OnOff", 1.0f, test);
    setParameter (emptyMidiOsc3Processor, "osc3Waveform", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "osc3Range", 3.0f / 5.0f, test);
    setParameter (emptyMidiOsc3Processor, "tune", 0.5f, test);
    setParameter (emptyMidiOsc3Processor, "osc3Freq", 0.5f, test);
    setParameter (emptyMidiOsc3Processor, "pitchWheelValue", 0.5f, test);
    setParameter (emptyMidiOsc3Processor, "osc3Vol", 1.0f, test);
    setParameter (emptyMidiOsc3Processor, "outputVolKnob", 1.0f, test);
    setParameter (emptyMidiOsc3Processor, "loudnessSustainLevelKnob", 1.0f, test);
    setParameter (emptyMidiOsc3Processor, "filterCutoff", 1.0f, test);
    setParameter (emptyMidiOsc3Processor, "oscModSwitch", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "noiseOnOffSwitch", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "extInputVolSwitch", 0.0f, test);
    setParameter (emptyMidiOsc3Processor, "osc3CtrlMode", 1.0f, test);
    constexpr int emptyMidiBlockSize = 128;
    emptyMidiOsc3Processor.setRateAndBufferSizeDetails (48000.0, emptyMidiBlockSize);
    emptyMidiOsc3Processor.prepareToPlay (48000.0, emptyMidiBlockSize);
    juce::AudioBuffer<float> emptyMidiOsc3Buffer (
        emptyMidiOsc3Processor.getTotalNumOutputChannels(), emptyMidiBlockSize);
    emptyMidiOsc3Buffer.clear();
    juce::MidiBuffer emptyMidi;
    const auto emptyMidiDiagnosticBefore =
        emptyMidiOsc3Processor.getInvalidParameterValueCount();
    emptyMidiOsc3Processor.processBlock (emptyMidiOsc3Buffer, emptyMidi);
    const auto emptyMidiDiagnosticDelta =
        emptyMidiOsc3Processor.getInvalidParameterValueCount() - emptyMidiDiagnosticBefore;
    test.expect (isFinite (emptyMidiOsc3Buffer) && emptyMidiDiagnosticDelta == 1,
                 "empty-MIDI keyboard-controlled Oscillator 3 must diagnose invalid pitch once");
    emptyMidiOsc3Processor.releaseResources();

    const auto osc3KeyboardDisabledPitch = renderProcessorPitch (
        3, 11, 45, test, 3, 5, 1.0f, false, nullptr, false);
    const auto expectedOsc3KeyboardDisabledPitch =
        440.0 * std::exp2 (3.0 / 12.0);
    test.expect (std::isfinite (osc3KeyboardDisabledPitch)
                     && std::abs (osc3KeyboardDisabledPitch
                                  / expectedOsc3KeyboardDisabledPitch - 1.0) < 0.02,
                 "keyboard-control-disabled Oscillator 3 must ignore note and Pitch Wheel input");

    struct GlideCase {
        int oscillator;
        int rangeIndex;
        int offsetIndex;
        int masterTuneIndex;
        float wheel;
        double sourceMidi;
        double targetMidi;
    };
    constexpr std::array glideCases {
        GlideCase { 1, 3, 8, 7, 0.75f, 49.5, 61.5 },
        GlideCase { 2, 2, 11, 7, 0.75f, 40.5, 52.5 },
        GlideCase { 3, 4, 6, 7, 0.75f, 59.5, 71.5 },
    };
    for (const auto& glideCase : glideCases) {
        const auto observed = renderProcessorGlide (
            glideCase.oscillator, glideCase.rangeIndex, glideCase.offsetIndex,
            glideCase.masterTuneIndex, glideCase.wheel, true, test);
        const auto sourceFrequency =
            440.0 * std::exp2 ((glideCase.sourceMidi - 69.0) / 12.0);
        const auto targetFrequency =
            440.0 * std::exp2 ((glideCase.targetMidi - 69.0) / 12.0);
        std::cout << "processor-glide oscillator=" << glideCase.oscillator
                  << " source-hz=" << observed.source
                  << " intermediate-hz=" << observed.intermediate
                  << " converged-hz=" << observed.converged << '\n';
        test.expect (
            std::abs (observed.source / sourceFrequency - 1.0) < 0.02,
            "musical oscillator source pitch must apply static pitch terms exactly once");
        test.expect (
            observed.intermediate > sourceFrequency * 1.05
                && observed.intermediate < targetFrequency * 0.90,
            "musical oscillator glide must expose a pitch strictly between source and target");
        test.expect (
            std::abs (observed.converged / targetFrequency - 1.0) < 0.02,
            "musical oscillator glide must converge to the composed target pitch");
        test.expect (
            observed.invalidDiagnosticDelta == 0,
            "valid musical oscillator glide must not emit invalid-composition diagnostics");
    }

    const auto keyboardDisabledGlide = renderProcessorGlide (
        3, 3, 6, 7, 1.0f, false, test);
    const auto keyboardDisabledFrequency =
        440.0 * std::exp2 ((69.0 + 1.0 - 2.0 - 69.0) / 12.0);
    test.expect (
        std::abs (keyboardDisabledGlide.source / keyboardDisabledFrequency - 1.0)
                < 0.02
            && std::abs (keyboardDisabledGlide.intermediate
                         / keyboardDisabledFrequency - 1.0) < 0.02
            && std::abs (keyboardDisabledGlide.converged
                         / keyboardDisabledFrequency - 1.0) < 0.02
            && keyboardDisabledGlide.invalidDiagnosticDelta == 0,
        "keyboard-control-disabled Oscillator 3 must remain reference-based throughout note glide");

    constexpr std::array lowFrequencyWheelCases {
        std::pair { 0.0f, -7.0 },
        std::pair { 0.5f, 0.0 },
        std::pair { 1.0f, 7.0 },
    };
    for (const auto& [normalizedWheel, bend] : lowFrequencyWheelCases) {
        std::uint64_t diagnosticDelta = 0;
        const auto rendered = renderProcessorPitch (
            1, 8, 127, test, 0, 5, normalizedWheel, false, &diagnosticDelta,
            true, 262144, 8192);
        const auto expected =
            440.0 * std::exp2 ((127.0 + bend - 69.0) / 12.0) / 256.0;
        std::cout << "processor-lo wheel=" << normalizedWheel
                  << " observed-hz=" << rendered
                  << " expected-hz=" << expected
                  << " diagnostic-delta=" << diagnosticDelta << '\n';
        test.expect (
            std::abs (rendered / expected - 1.0) < 0.01,
            "LO processor output must apply relative Pitch Wheel bend exactly once");
        test.expect (
            diagnosticDelta == 0,
            "LO processor output must bypass unsupported musical calibration without diagnostics");
    }

    const auto processorSource =
        legacyFixtureFile ("Source/PluginProcessor.cpp").loadFileAsString();
    test.expect (
        processorSource.contains (
            "if (! osc1UsesMusicalPitch)\n"
            "            osc1.setFrequency(effectiveFrequency);")
            && processorSource.contains (
                "if (! osc2UsesMusicalPitch)\n"
                "            osc2.setFrequency(effectiveFrequency);")
            && processorSource.contains (
                "if (! osc3UsesMusicalPitch)\n"
                "            osc3.setFrequency(osc3BaseFrequency);"),
        "musical processor pitch paths must bypass legacy frequency setters");
}

int runMode (std::string_view mode)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (mode.starts_with ("capture-"))
        return captureLegacyFixture (mode);

    TestContext test;

    if (mode == "unit")
        testUnitContract (test);
    else if (mode == "pitch-domain")
        testPitchDomainContract (test);
    else if (mode == "state")
        testStateSmoke (test);
    else if (mode == "state-v2")
    {
        testStateV2Contract (test);
        testStateV2Migration (test);
        testStateV2ExtensionRoundTrip (test);
        testStateV2FailuresAreAtomic (test);
        testStatePublicationBoundary (test);
    }
    else if (mode == "contours")
        testContourTask3BContract (test);
    else if (mode == "prepared-snapshot")
        testPreparedSnapshotContract (test);
    else if (mode == "parameter-binding")
        testParameterBindingContract (test);
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
