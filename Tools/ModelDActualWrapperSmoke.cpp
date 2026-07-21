#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr double validationSampleRate = 48000.0;
constexpr int blockSize = 128;

class ValidationError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct Options
{
    juce::File plugin;
    int repeatCount = 0;
    std::uint64_t seed = 0;
    juce::File report;
    juce::File log;
    std::string configuration;
    std::string os;
    std::string architecture;
};

struct RepeatResult
{
    std::string instanceId;
    std::map<std::string, std::string> checks {
        { "scan", "not-started" },
        { "instance", "not-started" },
        { "identity", "not-started" },
        { "buses", "not-started" },
        { "processing", "not-started" },
        { "state", "not-started" },
        { "editor", "not-started" },
        { "teardown", "not-started" }
    };
    std::vector<std::string> notes;
    std::string status = "not-started";
    std::string failure;
};

using ChannelSet = juce::AudioChannelSet;
using BusesLayout = juce::AudioProcessor::BusesLayout;

[[noreturn]] void fail (std::string_view message)
{
    throw ValidationError (std::string { message });
}

void require (bool condition, std::string_view message)
{
    if (! condition)
        fail (message);
}

int parsePositiveInteger (const std::string& text)
{
    size_t consumed = 0;
    long long value = 0;
    try
    {
        value = std::stoll (text, &consumed, 10);
    }
    catch (const std::exception&)
    {
        fail ("--repeat must be a positive integer");
    }

    if (consumed != text.size() || value <= 0 || value > 1000)
        fail ("--repeat must be a positive integer no greater than 1000");
    return static_cast<int> (value);
}

std::uint64_t parseSeed (const std::string& text)
{
    size_t consumed = 0;
    unsigned long long value = 0;
    try
    {
        value = std::stoull (text, &consumed, 0);
    }
    catch (const std::exception&)
    {
        fail ("--seed must be an unsigned integer");
    }

    if (consumed != text.size())
        fail ("--seed must be an unsigned integer");
    return static_cast<std::uint64_t> (value);
}

Options parseOptions (int argc, char* argv[])
{
    const std::array requiredNames {
        "--plugin", "--repeat", "--seed", "--report", "--log",
        "--config", "--os", "--arch"
    };
    std::map<std::string, std::string> values;

    if (argc != static_cast<int> (requiredNames.size() * 2 + 1))
        fail ("expected --plugin, --repeat, --seed, --report, --log, --config, --os, and --arch");

    for (int index = 1; index < argc; index += 2)
    {
        const std::string name { argv[index] };
        const std::string value { argv[index + 1] };
        require (std::find (requiredNames.begin(), requiredNames.end(), name)
                     != requiredNames.end(),
                 "unknown command-line option: " + name);
        require (! value.empty(), "empty value for command-line option: " + name);
        require (values.emplace (name, value).second,
                 "duplicate command-line option: " + name);
    }

    for (const auto* name : requiredNames)
        require (values.contains (name), "missing command-line option: " + std::string { name });

    Options result;
    result.plugin = juce::File { values["--plugin"] };
    result.repeatCount = parsePositiveInteger (values["--repeat"]);
    result.seed = parseSeed (values["--seed"]);
    result.report = juce::File { values["--report"] };
    result.log = juce::File { values["--log"] };
    result.configuration = values["--config"];
    result.os = values["--os"];
    result.architecture = values["--arch"];

    require (result.plugin.exists(), "staged VST3 path does not exist");
    require (result.plugin.hasFileExtension ("vst3"),
             "staged plug-in path must have the .vst3 extension");
    require (result.report != result.log, "report and log paths must be distinct");
    require (! result.report.isDirectory() && ! result.log.isDirectory(),
             "report and log paths must name files");
    return result;
}

std::string canonicalSeed (std::uint64_t seed)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::nouppercase << seed;
    return stream.str();
}

std::string aggregateSha256 (const juce::File& product)
{
    if (product.existsAsFile())
        return juce::SHA256 (product).toHexString().toStdString();

    juce::Array<juce::File> files;
    product.findChildFiles (files, juce::File::findFiles, true);
    require (! files.isEmpty(), "staged VST3 contains no payload files");
    std::sort (files.begin(), files.end(), [&product] (const auto& left, const auto& right)
    {
        return left.getRelativePathFrom (product) < right.getRelativePathFrom (product);
    });

    juce::String aggregateInput;
    for (const auto& file : files)
    {
        auto relative = file.getRelativePathFrom (product).replaceCharacter ('\\', '/');
        aggregateInput << juce::SHA256 (file).toHexString() << "  " << relative << "\n";
    }
    const auto utf8 = aggregateInput.toUTF8();
    return juce::SHA256 (utf8.getAddress(), utf8.sizeInBytes() - 1)
        .toHexString().toStdString();
}

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

juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& processor,
                                              const juce::String& name)
{
    for (auto* parameter : processor.getParameters())
        if (parameter != nullptr && parameter->getName (256) == name)
            return parameter;
    return nullptr;
}

void setParameter (juce::AudioProcessor& processor,
                   const juce::String& name,
                   float value)
{
    auto* parameter = findParameter (processor, name);
    require (parameter != nullptr,
             "host parameter is missing: " + name.toStdString());
    parameter->setValueNotifyingHost (value);
}

void setParameterAtIndex (juce::AudioProcessor& processor,
                          int index,
                          const juce::String& expectedName,
                          float value)
{
    const auto& parameters = processor.getParameters();
    require (juce::isPositiveAndBelow (index, parameters.size()),
             "known-patch host parameter index is missing");
    auto* parameter = parameters[index];
    require (parameter != nullptr && parameter->getName (256) == expectedName,
             "known-patch host parameter order changed at index "
                 + std::to_string (index));
    parameter->setValueNotifyingHost (value);
}

void verifyIdentity (const juce::PluginDescription& description,
                     juce::AudioPluginInstance& instance)
{
    require (description.name == SYNTH_EXPECTED_PLUGIN_NAME,
             "scan found an unexpected plug-in name");
    require (description.pluginFormatName == "VST3",
             "scan description is not VST3");
    require (description.isInstrument,
             "scan description does not identify an instrument");
    require (instance.acceptsMidi(), "hosted instance must accept MIDI");
    require (! instance.producesMidi(), "hosted instance must not produce MIDI");
    require (! instance.isMidiEffect(), "hosted instance must not be a MIDI effect");
}

void verifyBuses (juce::AudioPluginInstance& instance,
                  std::vector<std::string>& notes)
{
    require (instance.getBusCount (true) == 1,
             "hosted instance must declare exactly one input bus");
    require (instance.getBusCount (false) == 2,
             "hosted instance must declare exactly two output buses");

    const auto* external = instance.getBus (true, 0);
    const auto* main = instance.getBus (false, 0);
    const auto* phones = instance.getBus (false, 1);
    require (external != nullptr && external->getName() == "External Input",
             "input bus 0 must be named External Input");
    require (main != nullptr && main->getName() == "Main Output",
             "output bus 0 must be named Main Output");
    require (phones != nullptr && phones->getName() == "Phones/Cue",
             "output bus 1 must be named Phones/Cue");
    require (external != nullptr && ! external->isEnabledByDefault(),
             "External Input must be disabled by default");
    require (main != nullptr && main->isEnabledByDefault(),
             "Main Output must be enabled by default");
    require (phones != nullptr && ! phones->isEnabledByDefault(),
             "Phones/Cue must be disabled by default");
    require (external != nullptr && external->getDefaultLayout() == ChannelSet::stereo(),
             "External Input default layout must be stereo");
    require (main != nullptr && main->getDefaultLayout() == ChannelSet::stereo(),
             "Main Output default layout must be stereo");
    require (phones != nullptr && phones->getDefaultLayout() == ChannelSet::stereo(),
             "Phones/Cue default layout must be stereo");

    const std::array mainLayouts { ChannelSet::mono(), ChannelSet::stereo() };
    const std::array optionalLayouts {
        ChannelSet::disabled(), ChannelSet::mono(), ChannelSet::stereo()
    };
    for (const auto& mainLayout : mainLayouts)
        for (const auto& externalLayout : optionalLayouts)
            for (const auto& phonesLayout : optionalLayouts)
            {
                const auto requested = makeLayout (mainLayout, externalLayout, phonesLayout);
                require (instance.setBusesLayout (
                             requested),
                         "one of the 18 required hosted layouts was rejected");
                const auto actual = instance.getBusesLayout();
                require (actual.inputBuses.size() == 1
                             && actual.outputBuses.size() == 2
                             && actual.getChannelSet (true, 0) == externalLayout
                             && actual.getChannelSet (false, 0) == mainLayout
                             && actual.getChannelSet (false, 1) == phonesLayout,
                         "hosted wrapper did not apply an accepted layout exactly");
            }

    struct RejectedLayout
    {
        const char* name;
        BusesLayout layout;
    };
    const std::vector<RejectedLayout> rejected {
        { "disabled-main", makeLayout (ChannelSet::disabled(), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "surround-main", makeLayout (ChannelSet::create5point1(), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "discrete-main", makeLayout (ChannelSet::discreteChannels (3), ChannelSet::disabled(), ChannelSet::disabled()) },
        { "surround-external", makeLayout (ChannelSet::stereo(), ChannelSet::create5point1(), ChannelSet::disabled()) },
        { "discrete-external", makeLayout (ChannelSet::stereo(), ChannelSet::discreteChannels (3), ChannelSet::disabled()) },
        { "surround-phones", makeLayout (ChannelSet::stereo(), ChannelSet::disabled(), ChannelSet::create5point1()) },
        { "discrete-phones", makeLayout (ChannelSet::stereo(), ChannelSet::disabled(), ChannelSet::discreteChannels (3)) }
    };
    const auto defaultLayout = makeLayout (ChannelSet::stereo(),
                                           ChannelSet::disabled(),
                                           ChannelSet::disabled());
    for (const auto& entry : rejected)
    {
        const auto applied = instance.setBusesLayout (entry.layout);
        const std::string_view name { entry.name };
        const auto isFixedTopologyCanonicalization = name == "disabled-main";
        if (! applied || ! isFixedTopologyCanonicalization)
        {
            require (! applied,
                     std::string { "rejected hosted layout was accepted: " } + entry.name);
            continue;
        }

        const auto actual = instance.getBusesLayout();
        require (instance.getBusCount (true) == 1
                     && instance.getBusCount (false) == 2
                     && actual.inputBuses.size() == 1
                     && actual.outputBuses.size() == 2,
                 std::string { "VST3 canonicalization changed the fixed topology: " }
                     + entry.name);
        if (name == "disabled-main")
            require (actual.getChannelSet (false, 0).isDisabled(),
                     "VST3 main-bus deactivation did not disable Main Output");
        notes.push_back (std::string { "VST3 fixed-topology canonicalization: " }
                         + entry.name);
        require (instance.setBusesLayout (defaultLayout),
                 "could not restore default layout after VST3 canonicalization");
    }

    // Missing/extra BusesLayout vectors violate AudioProcessor's public
    // setBusesLayout precondition. Probe those four frozen cases through the
    // host-facing fixed-topology add/remove operations instead.
    require (! instance.removeBus (true),
             "hosted wrapper accepted the missing-input topology");
    require (! instance.removeBus (false),
             "hosted wrapper accepted the missing-phones topology");
    require (! instance.addBus (true),
             "hosted wrapper accepted the extra-input topology");
    require (! instance.addBus (false),
             "hosted wrapper accepted the extra-output topology");
    require (instance.getBusCount (true) == 1 && instance.getBusCount (false) == 2,
             "hosted add/remove probes changed the fixed bus topology");
    notes.push_back ("missing/extra bus vectors rejected through fixed-topology add/remove APIs");

    require (instance.setBusesLayout (defaultLayout),
             "could not restore the default hosted bus layout");
}

void verifyProcessing (juce::AudioPluginInstance& instance)
{
    instance.setRateAndBufferSizeDetails (validationSampleRate, blockSize);
    instance.prepareToPlay (validationSampleRate, blockSize);

    juce::AudioBuffer<float> first (instance.getTotalNumOutputChannels(), blockSize);
    juce::AudioBuffer<float> second (instance.getTotalNumOutputChannels(), blockSize);
    juce::MidiBuffer noMidi;
    first.clear();
    second.clear();
    instance.processBlock (first, noMidi);
    instance.processBlock (second, noMidi);
    require (isFinite (first) && isFinite (second),
             "no-MIDI hosted render must be finite");
    require (absoluteSum (first) == 0.0 && absoluteSum (second) == 0.0,
             "no-MIDI hosted render must be deterministically silent");

    setParameter (instance, "Oscillator 1 On/Off", 1.0f);
    setParameter (instance, "Oscillator 1 Volume", 1.0f);
    // outputVolKnob is the first of several legacy parameters with the same
    // display name. Its frozen host-registry position selects it unambiguously.
    setParameterAtIndex (instance, 15, "Output Volume", 1.0f);
    setParameter (instance, "Filter Cutoff Frequency", 1.0f);

    juce::AudioBuffer<float> noteOutput (instance.getTotalNumOutputChannels(), blockSize);
    noteOutput.clear();
    juce::MidiBuffer noteMidi;
    noteMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    instance.processBlock (noteOutput, noteMidi);
    require (isFinite (noteOutput), "sample-zero note-on render must be finite");
    require (absoluteSum (noteOutput) > 0.01,
             "sample-zero note-on under the known patch must be non-silent");
}

void verifyState (juce::AudioPluginInstance& instance)
{
    auto* parameter = findParameter (instance, "Filter Cutoff Frequency");
    require (parameter != nullptr, "state-test host parameter is missing");
    const auto savedValue = parameter->getValue();
    juce::MemoryBlock state;
    instance.getStateInformation (state);
    require (state.getSize() > 0, "hosted wrapper state did not serialize");

    const auto perturbed = savedValue > 0.5f ? 0.125f : 0.875f;
    parameter->setValueNotifyingHost (perturbed);
    require (std::abs (parameter->getValue() - savedValue) > 0.1f,
             "host parameter did not accept the state-test perturbation");
    instance.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    parameter = findParameter (instance, "Filter Cutoff Frequency");
    require (parameter != nullptr
                 && std::abs (parameter->getValue() - savedValue) < 1.0e-6f,
             "host parameter value did not return after state restoration");
}

class EditorHostWindow final : public juce::DocumentWindow
{
public:
    EditorHostWindow()
        : DocumentWindow ("Model D wrapper host",
                          juce::Colours::black,
                          DocumentWindow::allButtons,
                          true)
    {
        setResizable (true, true);
    }

    void closeButtonPressed() override {}
};

void drainHostWindowEvents()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
}

void verifyEditor (juce::AudioPluginInstance& instance)
{
    require (instance.hasEditor(), "hosted wrapper must expose an editor");
    std::unique_ptr<juce::AudioProcessorEditor> editor { instance.createEditorIfNeeded() };
    require (editor != nullptr, "hosted wrapper editor creation failed");

    // Plug-in editors are embedded in a host-owned top-level window. Exercise
    // that topology and drain hide/destroy events before releasing the editor,
    // so X11 cannot repaint a peer after its drawable has been destroyed.
    {
        EditorHostWindow hostWindow;
        hostWindow.setContentNonOwned (editor.get(), true);
        hostWindow.centreWithSize (hostWindow.getWidth(), hostWindow.getHeight());
        hostWindow.setVisible (true);
        require (editor->isShowing(), "hosted wrapper editor did not become showing");
        drainHostWindowEvents();
        hostWindow.setVisible (false);
        drainHostWindowEvents();
        hostWindow.clearContentComponent();
    }
    drainHostWindowEvents();
    editor.reset();
    drainHostWindowEvents();
    require (instance.getActiveEditor() == nullptr,
             "hosted wrapper retained the closed editor");
}

template <typename Function>
void runCheck (RepeatResult& result,
               std::vector<std::string>& logLines,
               const std::string& name,
               Function&& function)
{
    try
    {
        function();
        result.checks[name] = "pass";
        logLines.push_back (result.instanceId + " check=" + name + " status=pass");
    }
    catch (const std::exception& error)
    {
        result.checks[name] = "fail";
        logLines.push_back (result.instanceId + " check=" + name
                            + " status=fail reason=" + error.what());
        throw;
    }
}

RepeatResult runRepeat (const Options& options,
                        int repeatIndex,
                        std::vector<std::string>& logLines)
{
    RepeatResult result;
    result.instanceId = "instance-" + std::to_string (repeatIndex + 1);
    std::unique_ptr<juce::AudioPluginInstance> instance;
    bool prepared = false;

    try
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        runCheck (result, logLines, "scan", [&]
        {
            format.findAllTypesForFile (descriptions, options.plugin.getFullPathName());
            require (descriptions.size() == 1,
                     "scan must find exactly one plug-in description");
            require (descriptions[0]->name == SYNTH_EXPECTED_PLUGIN_NAME
                         && descriptions[0]->isInstrument,
                     "scan did not find exactly the expected instrument description");
        });

        juce::String creationError;
        runCheck (result, logLines, "instance", [&]
        {
            instance = format.createInstanceFromDescription (*descriptions[0],
                                                              validationSampleRate, blockSize,
                                                              creationError);
            require (instance != nullptr,
                     "VST3 instantiation failed: " + creationError.toStdString());
            std::ostringstream address;
            address << static_cast<const void*> (instance.get());
            logLines.push_back (result.instanceId + " event=created address=" + address.str());
        });
        runCheck (result, logLines, "identity", [&]
        {
            verifyIdentity (*descriptions[0], *instance);
        });
        runCheck (result, logLines, "buses", [&]
        {
            verifyBuses (*instance, result.notes);
            for (const auto& note : result.notes)
                logLines.push_back (result.instanceId + " note=" + note);
        });
        runCheck (result, logLines, "processing", [&]
        {
            prepared = true;
            verifyProcessing (*instance);
        });
        runCheck (result, logLines, "state", [&] { verifyState (*instance); });
        runCheck (result, logLines, "editor", [&] { verifyEditor (*instance); });
        runCheck (result, logLines, "teardown", [&]
        {
            if (prepared)
            {
                instance->releaseResources();
                logLines.push_back (result.instanceId + " event=resources-released");
            }
            require (instance->getActiveEditor() == nullptr,
                     "editor remained active during teardown");
            instance.reset();
            logLines.push_back (result.instanceId + " event=destroyed");
        });
        result.status = "pass";
    }
    catch (const std::exception& error)
    {
        if (instance != nullptr)
        {
            if (auto* editor = instance->getActiveEditor())
            {
                editor->setVisible (false);
                delete editor;
            }
            if (prepared)
                instance->releaseResources();
            instance.reset();
            logLines.push_back (result.instanceId + " event=destroyed-after-failure");
        }
        result.status = "fail";
        result.failure = error.what();
    }
    return result;
}

juce::var makeReport (const Options& options,
                      const std::string& sha256,
                      const std::vector<RepeatResult>& repeats,
                      const std::string& aggregateStatus)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schema_version", 1);

    auto tool = std::make_unique<juce::DynamicObject>();
    tool->setProperty ("name", "ModelDActualWrapperSmoke");
    tool->setProperty ("version", SYNTH_VALIDATOR_TOOL_VERSION);
    root->setProperty ("tool", juce::var { tool.release() });

    auto plugin = std::make_unique<juce::DynamicObject>();
    const auto buildRoot = options.report.getParentDirectory().getParentDirectory();
    plugin->setProperty ("relative_path",
                         options.plugin.getRelativePathFrom (buildRoot).replaceCharacter ('\\', '/'));
    plugin->setProperty ("aggregate_sha256", juce::String { sha256 });
    root->setProperty ("plugin", juce::var { plugin.release() });

    auto build = std::make_unique<juce::DynamicObject>();
    build->setProperty ("configuration", juce::String { options.configuration });
    build->setProperty ("os", juce::String { options.os });
    build->setProperty ("architecture", juce::String { options.architecture });
    root->setProperty ("build", juce::var { build.release() });
    root->setProperty ("repeat_count", options.repeatCount);
    root->setProperty ("seed", juce::String { canonicalSeed (options.seed) });

    juce::Array<juce::var> repeatArray;
    for (size_t index = 0; index < repeats.size(); ++index)
    {
        const auto& repeat = repeats[index];
        auto repeatObject = std::make_unique<juce::DynamicObject>();
        repeatObject->setProperty ("repeat", static_cast<int> (index + 1));
        repeatObject->setProperty ("instance_id", juce::String { repeat.instanceId });
        auto checks = std::make_unique<juce::DynamicObject>();
        for (const auto& [name, status] : repeat.checks)
            checks->setProperty (juce::Identifier { name }, juce::String { status });
        repeatObject->setProperty ("checks", juce::var { checks.release() });
        juce::Array<juce::var> notes;
        for (const auto& note : repeat.notes)
            notes.add (juce::String { note });
        repeatObject->setProperty ("notes", juce::var { notes });
        repeatObject->setProperty ("status", juce::String { repeat.status });
        if (! repeat.failure.empty())
            repeatObject->setProperty ("failure", juce::String { repeat.failure });
        repeatArray.add (juce::var { repeatObject.release() });
    }
    root->setProperty ("repeats", juce::var { repeatArray });
    root->setProperty ("status", juce::String { aggregateStatus });
    return juce::var { root.release() };
}

void writeOutputs (const Options& options,
                   const std::vector<std::string>& logLines,
                   const juce::var& report)
{
    require (options.report.getParentDirectory().createDirectory(),
             "could not create report directory");
    require (options.log.getParentDirectory().createDirectory(),
             "could not create log directory");

    juce::String logText;
    for (const auto& line : logLines)
        logText << line << "\n";
    require (options.log.replaceWithText (logText, false, false, "\n"),
             "could not write human-readable log");
    require (options.report.replaceWithText (juce::JSON::toString (report, true) + "\n",
                                             false, false, "\n"),
             "could not write JSON report");
}

int run (int argc, char* argv[])
{
    const auto options = parseOptions (argc, argv);
    require (canonicalSeed (options.seed) == "0x4d6f64656c44",
             "--seed must be the fixed validation seed 0x4d6f64656c44");

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    std::vector<std::string> logLines {
        "tool=ModelDActualWrapperSmoke version="
            + std::string { SYNTH_VALIDATOR_TOOL_VERSION },
        "plugin=" + options.plugin.getFullPathName().toStdString(),
        "repeat_count=" + std::to_string (options.repeatCount)
            + " seed=" + canonicalSeed (options.seed)
    };
    const auto sha256 = aggregateSha256 (options.plugin);
    logLines.push_back ("plugin_aggregate_sha256=" + sha256);

    std::vector<RepeatResult> repeats;
    repeats.reserve (static_cast<size_t> (options.repeatCount));
    bool passed = true;
    for (int repeat = 0; repeat < options.repeatCount; ++repeat)
    {
        repeats.push_back (runRepeat (options, repeat, logLines));
        passed = passed && repeats.back().status == "pass";
    }

    const std::string status = passed ? "pass" : "fail";
    logLines.push_back ("aggregate_status=" + status);
    writeOutputs (options, logLines, makeReport (options, sha256, repeats, status));
    std::cout << "Model D actual-wrapper smoke status=" << status
              << " repeats=" << options.repeatCount << '\n';
    return passed ? 0 : 1;
}
} // namespace

int main (int argc, char* argv[])
{
    try
    {
        return run (argc, argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << "ModelDActualWrapperSmoke: " << error.what() << '\n';
        return 2;
    }
}
