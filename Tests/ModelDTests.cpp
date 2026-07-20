#include "PluginProcessor.h"
#include "PresetManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#define SYNTH_STRINGIFY_IMPL(value) #value
#define SYNTH_STRINGIFY(value) SYNTH_STRINGIFY_IMPL(value)

namespace
{
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

        const auto parameterCount = static_cast<size_t> (parameters.size());
        test.expect (parameterCount == valuesBeforeEditor.size(),
                     "editor construction must not change the parameter inventory");
        for (size_t index = 0; index < valuesBeforeEditor.size() && index < parameterCount; ++index)
        {
            test.expect (std::abs (parameters[index]->getValue() - valuesBeforeEditor[index]) < 1.0e-6f,
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
    TestContext test;

    if (mode == "unit")
        testUnitContract (test);
    else if (mode == "state")
        testStateSmoke (test);
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
