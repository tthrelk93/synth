#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

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
    test.expect (static_cast<unsigned int> (JucePlugin_AUMainType)
                     == static_cast<unsigned int> ('aumu'),
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

void testProcessingContract (TestContext& test)
{
    testDefaultSilence (test);

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
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestContext test;
    testGeneratedWrapperContract (test);

    MoogMiniAudioProcessor processor;
    const auto topologyReady = testDefaultBusContract (processor, test);
    testVST3InputRole (processor, test);

    if (topologyReady)
    {
        testLayoutTable (processor, test);
        testProcessingContract (test);
    }
    else
    {
        test.expect (false,
                     "layout and processing tables require the exact declared bus topology");
    }

    return test.result();
}
