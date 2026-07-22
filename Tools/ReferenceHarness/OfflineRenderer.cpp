#include "OfflineRenderer.h"

#include "Oscillator.h"
#include "PluginProcessor.h"
#include "ReferenceData.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

namespace ReferenceHarness {
namespace {

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

std::string parameterId (const ParameterRegistry::Key key)
{
    return std::string { ParameterRegistry::descriptor (key).id };
}

std::string hashBytes (const void* data, const size_t size)
{
    return juce::SHA256 { data, size }.toHexString().toStdString();
}

std::string hashFloats (const std::vector<float>& values)
{
    return hashBytes (values.data(), values.size() * sizeof (float));
}

std::map<std::string, std::string> fixtureInputHashes (const RenderFixture& fixture)
{
    std::map<std::string, std::string> hashes {
        { "state", fixture.stateSha256 },
    };
    if (fixture.input.kind == InputKind::wav)
        hashes.emplace ("audio", fixture.input.sha256);
    return hashes;
}

juce::var stringArray (const std::vector<std::string>& strings)
{
    juce::Array<juce::var> array;
    for (const auto& value : strings)
        array.add (juce::String { value });
    return array;
}

juce::var integerArray (const std::vector<int>& values)
{
    juce::Array<juce::var> array;
    for (const auto value : values)
        array.add (value);
    return array;
}

juce::var stringMap (const std::map<std::string, std::string>& values)
{
    auto object = std::make_unique<juce::DynamicObject>();
    for (const auto& [key, value] : values)
        object->setProperty (juce::Identifier { key }, juce::String { value });
    return juce::var { object.release() };
}

juce::var controlTraceJson (const RenderResult& result)
{
    juce::Array<juce::var> points;
    for (const auto& point : result.controlTrace) {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("sample", static_cast<juce::int64> (point.sample));
        object->setProperty ("parameterId", juce::String { parameterId (point.key) });
        object->setProperty ("normalizedValue", static_cast<double> (point.normalizedValue));
        object->setProperty ("physicalValue", static_cast<double> (point.physicalValue));
        points.add (juce::var { object.release() });
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schema", "model-d.control-trace.v1");
    root->setProperty ("points", points);
    return juce::var { root.release() };
}

juce::var eventTraceJson (const RenderResult& result)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schema", "model-d.event-trace.v1");
    root->setProperty ("events", stringArray (result.eventTrace));
    return juce::var { root.release() };
}

struct InputAudio {
    juce::AudioBuffer<float> wav;
};

LoadResult<InputAudio> prepareInput (const RenderFixture& fixture)
{
    InputAudio prepared;
    if (fixture.input.kind != InputKind::wav)
        return { std::move (prepared), {} };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (fixture.input.file) };
    if (reader == nullptr)
        return failure<InputAudio> ("render.input-format", "WAV input cannot be opened");
    if (reader->sampleRate != fixture.config.sampleRate)
        return failure<InputAudio> ("render.input-rate", "WAV input sample rate must match the render");
    if (reader->numChannels == 0 || reader->numChannels > 2
        || reader->lengthInSamples < static_cast<juce::int64> (fixture.config.totalSamples))
        return failure<InputAudio> ("render.input-length",
                                    "WAV input must have one or two channels and cover the render");

    prepared.wav.setSize (static_cast<int> (reader->numChannels),
                          static_cast<int> (fixture.config.totalSamples), false, true, false);
    if (! reader->read (&prepared.wav, 0, prepared.wav.getNumSamples(), 0, true, true))
        return failure<InputAudio> ("render.input-read", "WAV input cannot be decoded");
    return { std::move (prepared), {} };
}

float inputSample (const RenderFixture& fixture,
                   const InputAudio& prepared,
                   const int channel,
                   const std::uint64_t sample)
{
    switch (fixture.input.kind) {
        case InputKind::silence: return 0.0f;
        case InputKind::dc: return static_cast<float> (fixture.input.value);
        case InputKind::sine:
            return static_cast<float> (fixture.input.value
                * std::sin (2.0 * juce::MathConstants<double>::pi * fixture.input.frequencyHz
                            * static_cast<double> (sample) / fixture.config.sampleRate));
        case InputKind::wav:
            return prepared.wav.getSample (std::min (channel, prepared.wav.getNumChannels() - 1),
                                           static_cast<int> (sample));
    }
    return 0.0f;
}

struct OrderedEvent {
    std::uint64_t sample = 0;
    std::uint32_t sequence = 0;
    const AutomationEvent* automation = nullptr;
    const MidiEvent* midi = nullptr;
};

std::vector<OrderedEvent> orderedEvents (const RenderFixture& fixture)
{
    std::vector<OrderedEvent> events;
    events.reserve (fixture.automation.size() + fixture.midi.size());
    for (const auto& event : fixture.automation)
        events.push_back ({ event.sample, event.sequence, &event, nullptr });
    for (const auto& event : fixture.midi)
        events.push_back ({ event.sample, event.sequence, nullptr, &event });
    std::stable_sort (events.begin(), events.end(), [] (const auto& left, const auto& right) {
        return std::tie (left.sample, left.sequence) < std::tie (right.sample, right.sequence);
    });
    return events;
}

std::string automationTrace (const AutomationEvent& event)
{
    return "automation:" + std::to_string (event.sample) + ":"
         + std::to_string (event.sequence) + ":" + parameterId (event.key) + ":"
         + juce::String { event.normalizedValue, 9 }.toStdString();
}

std::string midiTrace (const MidiEvent& event)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string bytes;
    bytes.reserve (event.bytes.size() * 2);
    for (const auto byte : event.bytes) {
        bytes.push_back (hex[byte >> 4]);
        bytes.push_back (hex[byte & 0x0f]);
    }
    return "midi:" + std::to_string (event.sample) + ":"
         + std::to_string (event.sequence) + ":" + bytes;
}

LoadResult<RenderResult> renderPattern (const RenderFixture& fixture,
                                        const InputAudio& preparedInput,
                                        const std::vector<int>& pattern)
{
    MoogMiniAudioProcessor processor;
    const auto stateXml = juce::XmlDocument::parse (fixture.stateFile);
    if (stateXml == nullptr)
        return failure<RenderResult> ("render.state-read", "validated state XML cannot be read");
    juce::MemoryBlock stateBytes;
    juce::AudioProcessor::copyXmlToBinary (*stateXml, stateBytes);
    const auto restored = processor.restoreState (
        stateBytes.getData(), static_cast<int> (stateBytes.getSize()));
    if (! restored.succeeded())
        return failure<RenderResult> ("render.state-restore", "validated state failed to restore");
    if (processor.getContourContract() != fixture.expectedContourContract)
        return failure<RenderResult> ("render.contour", "restored contour contract does not match fixture");
    const auto initialSnapshot = processor.captureParameterSnapshot ({});
    if (! initialSnapshot.coherent
        || initialSnapshot.contourContract != fixture.expectedContourContract)
        return failure<RenderResult> ("render.snapshot", "restored parameter snapshot is incoherent");

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    if (! processor.setBusesLayout (layout))
        return failure<RenderResult> ("render.buses", "fixture bus layout is unsupported");

    const auto maximumBlock = *std::max_element (pattern.begin(), pattern.end());
    processor.setRateAndBufferSizeDetails (fixture.config.sampleRate, maximumBlock);
    processor.prepareToPlay (fixture.config.sampleRate, maximumBlock);

    RenderResult result;
    result.sampleRate = fixture.config.sampleRate;
    result.mainChannels = processor.getChannelCountOfBus (false, 0);
    result.phonesChannels = processor.getChannelCountOfBus (false, 1);
    result.blockPattern = pattern;
    result.main.reserve (static_cast<size_t> (fixture.config.totalSamples)
                         * static_cast<size_t> (result.mainChannels));
    result.phones.reserve (static_cast<size_t> (fixture.config.totalSamples)
                           * static_cast<size_t> (result.phonesChannels));
    result.reproducibility.sourceCommit = SYNTH_SOURCE_COMMIT;
    result.reproducibility.juceCommit = SYNTH_RESOLVED_JUCE_COMMIT;
    result.reproducibility.buildType = SYNTH_CONFIGURED_BUILD_TYPE;
    result.reproducibility.platform = SYNTH_CONFIGURED_PLATFORM;
    result.reproducibility.architecture = SYNTH_CONFIGURED_ARCHITECTURE;
    result.reproducibility.fixtureSha256 = sha256File (fixture.fixtureFile);
    result.reproducibility.inputHashes = fixtureInputHashes (fixture);
    result.reproducibility.seed = fixture.config.seed;

    const auto events = orderedEvents (fixture);
    size_t eventIndex = 0;
    size_t patternIndex = 0;
    std::uint64_t position = 0;
    while (position < fixture.config.totalSamples) {
        const auto hostBlockEnd = std::min (
            fixture.config.totalSamples,
            position + static_cast<std::uint64_t> (pattern[patternIndex % pattern.size()]));
        ++patternIndex;

        while (position < hostBlockEnd) {
            juce::MidiBuffer midiBuffer;
            while (eventIndex < events.size() && events[eventIndex].sample == position) {
                const auto& ordered = events[eventIndex++];
                if (ordered.automation != nullptr) {
                    const auto& automation = *ordered.automation;
                    auto* parameter = processor.getPreparedParameter (automation.key);
                    if (parameter == nullptr) {
                        processor.releaseResources();
                        processor.reset();
                        return failure<RenderResult> ("render.parameter",
                                                      "validated parameter is unavailable");
                    }
                    parameter->setValueNotifyingHost (automation.normalizedValue);
                    result.controlTrace.push_back ({
                        automation.sample,
                        automation.key,
                        parameter->getValue(),
                        parameter->convertFrom0to1 (parameter->getValue()),
                    });
                    result.eventTrace.push_back (automationTrace (automation));
                } else if (ordered.midi != nullptr) {
                    const auto& midi = *ordered.midi;
                    midiBuffer.addEvent (
                        juce::MidiMessage { midi.bytes.data(), static_cast<int> (midi.bytes.size()), 0.0 },
                        0);
                    result.eventTrace.push_back (midiTrace (midi));
                }
            }

            const auto nextEvent = eventIndex < events.size()
                                     ? events[eventIndex].sample
                                     : fixture.config.totalSamples;
            const auto boundary = std::min ({ hostBlockEnd, nextEvent, fixture.config.totalSamples });
            const auto subBlockSize = static_cast<int> (boundary - position);
            if (subBlockSize <= 0) {
                processor.releaseResources();
                processor.reset();
                return failure<RenderResult> ("render.boundary", "render event boundary did not advance");
            }

            juce::AudioBuffer<float> buffer (
                std::max (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels()),
                subBlockSize);
            buffer.clear();
            auto externalInput = processor.getBusBuffer (buffer, true, 0);
            bool nonFiniteInput = false;
            for (int sample = 0; sample < subBlockSize; ++sample)
                for (int channel = 0; channel < externalInput.getNumChannels(); ++channel) {
                    const auto value = inputSample (
                        fixture, preparedInput, channel,
                        position + static_cast<std::uint64_t> (sample));
                    nonFiniteInput = nonFiniteInput || ! std::isfinite (value);
                    externalInput.setSample (channel, sample, value);
                }
            if (nonFiniteInput) {
                processor.releaseResources();
                processor.reset();
                return failure<RenderResult> ("render.non-finite-input",
                                              "render input contains a non-finite sample");
            }

            processor.processBlock (buffer, midiBuffer);
            const auto main = processor.getBusBuffer (buffer, false, 0);
            const auto phones = processor.getBusBuffer (buffer, false, 1);
            bool nonFiniteOutput = false;
            for (int sample = 0; sample < subBlockSize; ++sample) {
                for (int channel = 0; channel < main.getNumChannels(); ++channel) {
                    const auto value = main.getSample (channel, sample);
                    nonFiniteOutput = nonFiniteOutput || ! std::isfinite (value);
                    result.main.push_back (value);
                }
                for (int channel = 0; channel < phones.getNumChannels(); ++channel) {
                    const auto value = phones.getSample (channel, sample);
                    nonFiniteOutput = nonFiniteOutput || ! std::isfinite (value);
                    result.phones.push_back (value);
                }
            }
            if (nonFiniteOutput) {
                processor.releaseResources();
                processor.reset();
                return failure<RenderResult> ("render.non-finite-output",
                                              "processor output contains a non-finite sample");
            }
            position = boundary;
        }
    }

    processor.releaseResources();
    processor.reset();
    result.reproducibility.outputHashes.emplace ("main", hashFloats (result.main));
    result.reproducibility.outputHashes.emplace ("phones", hashFloats (result.phones));
    const auto control = canonicalJson (controlTraceJson (result));
    result.reproducibility.outputHashes.emplace (
        "control", hashBytes (control.toRawUTF8(), static_cast<size_t> (control.getNumBytesAsUTF8())));
    const auto event = canonicalJson (eventTraceJson (result));
    result.reproducibility.outputHashes.emplace (
        "event", hashBytes (event.toRawUTF8(), static_cast<size_t> (event.getNumBytesAsUTF8())));
    return { std::move (result), {} };
}

bool hasNonFiniteValues (const RenderResult& result)
{
    const auto nonFinite = [] (const auto value) { return ! std::isfinite (value); };
    if (std::any_of (result.main.begin(), result.main.end(), nonFinite)
        || std::any_of (result.phones.begin(), result.phones.end(), nonFinite))
        return true;
    return std::any_of (result.controlTrace.begin(), result.controlTrace.end(), [] (const auto& point) {
        return ! std::isfinite (point.normalizedValue) || ! std::isfinite (point.physicalValue);
    });
}

bool sameControlTrace (const std::vector<ControlTracePoint>& left,
                       const std::vector<ControlTracePoint>& right)
{
    return left.size() == right.size()
        && std::equal (left.begin(), left.end(), right.begin(), [] (const auto& a, const auto& b) {
               return a.sample == b.sample && a.key == b.key
                   && a.normalizedValue == b.normalizedValue
                   && a.physicalValue == b.physicalValue;
           });
}

std::optional<Diagnostic> validateCandidateResults (
    const RenderFixture& fixture,
    const std::span<const RenderResult> results)
{
    if (results.size() != fixture.config.blockPatterns.size())
        return Diagnostic { "output.result-count",
                            "candidate results must cover every declared block pattern" };
    if (results.empty())
        return Diagnostic { "output.results", "candidate render results are empty" };

    const auto fixtureHash = sha256File (fixture.fixtureFile);
    const auto expectedInputHashes = fixtureInputHashes (fixture);
    const auto& canonical = results.front();
    for (size_t index = 0; index < results.size(); ++index) {
        const auto& result = results[index];
        if (result.blockPattern != fixture.config.blockPatterns[index])
            return Diagnostic { "output.pattern",
                                "candidate result block patterns must match fixture order" };
        const auto mainSize = static_cast<size_t> (fixture.config.totalSamples)
                            * static_cast<size_t> (std::max (result.mainChannels, 0));
        const auto phonesSize = static_cast<size_t> (fixture.config.totalSamples)
                              * static_cast<size_t> (std::max (result.phonesChannels, 0));
        if (result.sampleRate != fixture.config.sampleRate
            || result.mainChannels <= 0 || result.phonesChannels <= 0
            || result.main.size() != mainSize || result.phones.size() != phonesSize
            || result.reproducibility.seed != fixture.config.seed
            || result.reproducibility.fixtureSha256 != fixtureHash)
            return Diagnostic { "output.invariant",
                                "candidate result metadata and channel sizes must match the fixture" };
        if (result.reproducibility.sourceCommit != SYNTH_SOURCE_COMMIT
            || result.reproducibility.juceCommit != SYNTH_RESOLVED_JUCE_COMMIT
            || result.reproducibility.buildType != SYNTH_CONFIGURED_BUILD_TYPE
            || result.reproducibility.platform != SYNTH_CONFIGURED_PLATFORM
            || result.reproducibility.architecture != SYNTH_CONFIGURED_ARCHITECTURE)
            return Diagnostic { "output.provenance",
                                "candidate result provenance must match the configured build" };
        if (result.reproducibility.inputHashes != expectedInputHashes)
            return Diagnostic { "output.input-hash",
                                "candidate input hashes must exactly match the fixture inputs" };
        if (hasNonFiniteValues (result))
            return Diagnostic { "output.non-finite",
                                "candidate results must contain only finite audio and control values" };
    }

    for (size_t index = 1; index < results.size(); ++index) {
        const auto& result = results[index];
        if (result.main != canonical.main || result.phones != canonical.phones
            || ! sameControlTrace (result.controlTrace, canonical.controlTrace)
            || result.eventTrace != canonical.eventTrace
            || result.reproducibility.inputHashes != canonical.reproducibility.inputHashes
            || result.reproducibility.outputHashes != canonical.reproducibility.outputHashes
            || result.reproducibility.sourceCommit != canonical.reproducibility.sourceCommit
            || result.reproducibility.juceCommit != canonical.reproducibility.juceCommit
            || result.reproducibility.buildType != canonical.reproducibility.buildType
            || result.reproducibility.platform != canonical.reproducibility.platform
            || result.reproducibility.architecture != canonical.reproducibility.architecture)
            return Diagnostic { "output.divergence",
                                "candidate results diverge across declared block patterns" };
    }

    for (const auto& result : results) {
        const auto control = canonicalJson (controlTraceJson (result));
        const auto event = canonicalJson (eventTraceJson (result));
        const std::map<std::string, std::string> expectedHashes {
            { "main", hashFloats (result.main) },
            { "phones", hashFloats (result.phones) },
            { "control", hashBytes (control.toRawUTF8(),
                                     static_cast<size_t> (control.getNumBytesAsUTF8())) },
            { "event", hashBytes (event.toRawUTF8(),
                                   static_cast<size_t> (event.getNumBytesAsUTF8())) },
        };
        for (const auto& [name, expected] : expectedHashes) {
            const auto actual = result.reproducibility.outputHashes.find (name);
            if (actual == result.reproducibility.outputHashes.end() || actual->second != expected)
                return Diagnostic { "output.hash",
                                    "candidate result hashes must match their exact bytes" };
        }
    }
    return std::nullopt;
}

bool writeFloatWav (const juce::File& file,
                    const double sampleRate,
                    const int channels,
                    const std::vector<float>& interleaved)
{
    if (channels <= 0 || interleaved.size() % static_cast<size_t> (channels) != 0)
        return false;
    auto fileStream = file.createOutputStream();
    if (fileStream == nullptr)
        return false;
    std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate (sampleRate)
                             .withNumChannels (channels)
                             .withBitsPerSample (32)
                             .withSampleFormat (
                                 juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = format.createWriterFor (stream, options);
    if (writer == nullptr)
        return false;

    const auto frames = static_cast<int> (interleaved.size() / static_cast<size_t> (channels));
    juce::AudioBuffer<float> audio (channels, frames);
    for (int sample = 0; sample < frames; ++sample)
        for (int channel = 0; channel < channels; ++channel)
            audio.setSample (channel, sample,
                             interleaved[static_cast<size_t> (sample * channels + channel)]);
    return writer->writeFromAudioSampleBuffer (audio, 0, frames);
}

LoadResult<FixtureIndex> validateIndexAndRenderFixtures (const juce::File& indexPath)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    auto index = loadFixtureIndex (sourceRoot, indexPath);
    if (! index.ok())
        return index;
    for (const auto& artifact : index.value->renderFixtures) {
        const auto fixture = loadRenderFixture (
            sourceRoot, sourceRoot.getChildFile (artifact.relativePath));
        if (! fixture.ok())
            return { std::nullopt, fixture.diagnostics };
    }
    return index;
}

void printDiagnostic (const Diagnostic& diagnostic)
{
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
}

} // namespace

LoadResult<std::vector<RenderResult>> renderFixture (const RenderFixture& fixture)
{
    const auto input = prepareInput (fixture);
    if (! input.ok())
        return { std::nullopt, input.diagnostics };

    std::vector<RenderResult> results;
    results.reserve (fixture.config.blockPatterns.size());
    for (const auto& pattern : fixture.config.blockPatterns) {
        const auto rendered = renderPattern (fixture, *input.value, pattern);
        if (! rendered.ok())
            return { std::nullopt, rendered.diagnostics };
        results.push_back (std::move (*rendered.value));
    }
    return { std::move (results), {} };
}

LoadResult<std::vector<juce::File>> writeCandidateArtifacts (
    const RenderFixture& fixture,
    const std::span<const RenderResult> results,
    const juce::File& newDirectory)
{
    if (newDirectory.exists())
        return failure<std::vector<juce::File>> ("output.exists",
                                                 "candidate output directory already exists");
    if (const auto diagnostic = validateCandidateResults (fixture, results);
        diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };
    if (! newDirectory.createDirectory())
        return failure<std::vector<juce::File>> ("output.create",
                                                 "candidate output directory cannot be created");

    const auto& canonical = results.front();
    const auto controlFile = newDirectory.getChildFile ("control-trace.json");
    const auto eventFile = newDirectory.getChildFile ("event-trace.json");
    const auto mainFile = newDirectory.getChildFile ("main.wav");
    const auto phonesFile = newDirectory.getChildFile ("phones.wav");
    const auto renderFile = newDirectory.getChildFile ("render.json");

    if (! controlFile.replaceWithText (canonicalJson (controlTraceJson (canonical)), false, false, "\n")
        || ! eventFile.replaceWithText (canonicalJson (eventTraceJson (canonical)), false, false, "\n")
        || ! writeFloatWav (mainFile, canonical.sampleRate,
                            canonical.mainChannels, canonical.main)
        || ! writeFloatWav (phonesFile, canonical.sampleRate,
                            canonical.phonesChannels, canonical.phones))
        return failure<std::vector<juce::File>> ("output.write",
                                                 "candidate artifact could not be written");

    std::map<std::string, std::string> outputHashes {
        { "control-trace.json", sha256File (controlFile) },
        { "event-trace.json", sha256File (eventFile) },
        { "main.wav", sha256File (mainFile) },
        { "phones.wav", sha256File (phonesFile) },
    };
    juce::Array<juce::var> patterns;
    for (const auto& result : results)
        patterns.add (integerArray (result.blockPattern));

    auto reproducibility = std::make_unique<juce::DynamicObject>();
    reproducibility->setProperty ("architecture", juce::String { canonical.reproducibility.architecture });
    reproducibility->setProperty ("buildType", juce::String { canonical.reproducibility.buildType });
    reproducibility->setProperty ("fixtureSha256", juce::String { canonical.reproducibility.fixtureSha256 });
    reproducibility->setProperty ("inputHashes", stringMap (canonical.reproducibility.inputHashes));
    reproducibility->setProperty ("juceCommit", juce::String { canonical.reproducibility.juceCommit });
    reproducibility->setProperty ("outputHashes", stringMap (outputHashes));
    reproducibility->setProperty ("platform", juce::String { canonical.reproducibility.platform });
    reproducibility->setProperty ("seed", static_cast<juce::int64> (canonical.reproducibility.seed));
    reproducibility->setProperty ("sourceCommit", juce::String { canonical.reproducibility.sourceCommit });

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("analyzers", stringArray (fixture.analyzers));
    root->setProperty ("blockPatterns", patterns);
    root->setProperty ("fixtureId", juce::String { fixture.id });
    root->setProperty ("mainChannels", canonical.mainChannels);
    root->setProperty ("phonesChannels", canonical.phonesChannels);
    root->setProperty ("reproducibility", juce::var { reproducibility.release() });
    root->setProperty ("requirements", stringArray (fixture.requirements));
    root->setProperty ("sampleRate", canonical.sampleRate);
    root->setProperty ("schema", "model-d.render-result.v1");
    root->setProperty ("totalSamples", static_cast<juce::int64> (fixture.config.totalSamples));
    if (! renderFile.replaceWithText (
            canonicalJson (juce::var { root.release() }), false, false, "\n"))
        return failure<std::vector<juce::File>> ("output.write",
                                                 "candidate render manifest could not be written");

    return { std::vector<juce::File> { renderFile, controlFile, eventFile, mainFile, phonesFile }, {} };
}

int runOfflineRendererCommand (const std::span<const std::string> arguments)
{
    if (arguments.empty()) {
        Oscillator oscillator;
        oscillator.setSampleRate (48000.0f);
        oscillator.setRange (Oscillator::Eight);
        oscillator.setWaveform (Oscillator::Sin);
        oscillator.start (440.0f);
        double absoluteSum = 0.0;
        for (int sample = 0; sample < 480; ++sample)
            absoluteSum += std::abs (oscillator.processNextSample (0.0f, true));
        if (! std::isfinite (absoluteSum) || absoluteSum <= 0.0) {
            std::cerr << "Offline render foundation produced invalid output\n";
            return 1;
        }
        std::cout << "Rendered 480 ModelDCore samples\n";
        return 0;
    }

    const auto exactOption = [&] (const size_t index, const char* name) {
        return index < arguments.size() && arguments[index] == name;
    };
    if (arguments[0] == "validate" || arguments[0] == "run") {
        const auto run = arguments[0] == "run";
        const auto expectedSize = run ? size_t { 9 } : size_t { 7 };
        if (arguments.size() != expectedSize
            || ! exactOption (1, "--fixture-index")
            || ! exactOption (3, "--acceptance")
            || ! exactOption (5, "--requirements")
            || (run && ! exactOption (7, "--output"))) {
            std::cerr << "cli.arguments: invalid command arguments\n";
            return 2;
        }
        const auto index = validateIndexAndRenderFixtures (juce::File { arguments[2] });
        if (! index.ok()) {
            printDiagnostic (index.diagnostics.front());
            return 1;
        }
        std::cerr << "missing-acceptance-implementation: acceptance validation is deferred to Task 3\n";
        return 1;
    }
    if (arguments[0] == "verify-release") {
        if (arguments.size() != 3 || ! exactOption (1, "--report")) {
            std::cerr << "cli.arguments: invalid command arguments\n";
            return 2;
        }
        std::cerr << "missing-requirement-implementation: release verification is deferred to Task 4\n";
        return 1;
    }
    std::cerr << "cli.command: unsupported command\n";
    return 2;
}

} // namespace ReferenceHarness
