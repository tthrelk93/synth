#include "OfflineRenderer.h"

#include "Acceptance.h"
#include "AnalyzerRegistry.h"
#include "Oscillator.h"
#include "PluginProcessor.h"
#include "ReferenceData.h"
#include "RequirementReporter.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
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

LoadResult<std::vector<MetricEvidenceRecord>> analyzeFixtureMetricsImpl (
    const RenderFixture& fixture,
    const std::span<const RenderResult> results)
{
    if (results.empty())
        return failure<std::vector<MetricEvidenceRecord>> (
            "metrics.render-empty", "render metrics require at least one immutable result");
    const auto analyzers = AnalyzerRegistry::withFoundationAnalyzers();
    std::vector<MetricEvidenceRecord> records;
    for (const auto& fixtureRequest : fixture.analysisRequests) {
        std::optional<juce::String> expected;
        std::vector<MetricResult> canonicalMetrics;
        for (const auto& result : results) {
            const auto registered = analyzers.find (fixtureRequest.analyzer.id);
            if (! registered.ok() || registered.value->version != fixtureRequest.analyzer.version)
                return failure<std::vector<MetricEvidenceRecord>> (
                    "metrics.request-analyzer", "fixture analysis request identity is not registered");
            if (fixtureRequest.eventSample >= fixture.config.totalSamples
                || fixtureRequest.windowSamples == 0
                || fixtureRequest.windowSamples
                       > fixture.config.totalSamples - fixtureRequest.eventSample)
                return failure<std::vector<MetricEvidenceRecord>> (
                    "metrics.request-event", "fixture analysis event window is invalid");
            std::vector<float> audio;
            std::vector<double> control;
            AnalysisRequest request;
            request.metric = fixtureRequest.metric;
            request.eventSample = fixtureRequest.eventSample;
            request.durationSamples = fixtureRequest.windowSamples - 1;
            request.start = fixtureRequest.start.value_or (0.0);
            request.target = fixtureRequest.target.value_or (0.0);
            if (fixtureRequest.inputKind == AnalysisInputKind::audio) {
                const auto& interleaved = fixtureRequest.audioTap == AudioTap::main
                                            ? result.main : result.phones;
                const auto channels = fixtureRequest.audioTap == AudioTap::main
                                    ? result.mainChannels : result.phonesChannels;
                if (channels <= 0 || fixtureRequest.channel < 0
                    || fixtureRequest.channel >= channels
                    || interleaved.size() % static_cast<size_t> (channels) != 0
                    || interleaved.size() / static_cast<size_t> (channels)
                           != fixture.config.totalSamples)
                    return failure<std::vector<MetricEvidenceRecord>> (
                        "metrics.request-audio", "fixture analysis audio tap/channel is invalid");
                audio.reserve (fixture.config.totalSamples);
                for (size_t frame = 0; frame < fixture.config.totalSamples; ++frame)
                    audio.push_back (interleaved[frame * static_cast<size_t> (channels)
                                                 + static_cast<size_t> (fixtureRequest.channel)]);
                request.audio = audio;
            } else {
                if (! fixtureRequest.start.has_value())
                    return failure<std::vector<MetricEvidenceRecord>> (
                        "metrics.request-control", "dense control analysis requires a start value");
                control.assign (fixture.config.totalSamples, *fixtureRequest.start);
                auto cursor = std::uint64_t { 0 };
                auto current = *fixtureRequest.start;
                auto sawEvent = false;
                for (const auto& point : result.controlTrace) {
                    if (point.key != fixtureRequest.parameterKey)
                        continue;
                    if (point.sample < cursor || point.sample >= fixture.config.totalSamples)
                        return failure<std::vector<MetricEvidenceRecord>> (
                            "metrics.request-control", "control trace sample indices are invalid");
                    std::fill (control.begin() + static_cast<std::ptrdiff_t> (cursor),
                               control.begin() + static_cast<std::ptrdiff_t> (point.sample),
                               current);
                    current = fixtureRequest.controlDomain == ControlDomain::normalized
                            ? static_cast<double> (point.normalizedValue)
                            : static_cast<double> (point.physicalValue);
                    cursor = point.sample;
                    sawEvent = sawEvent || point.sample == fixtureRequest.eventSample;
                }
                std::fill (control.begin() + static_cast<std::ptrdiff_t> (cursor),
                           control.end(), current);
                if (! sawEvent)
                    return failure<std::vector<MetricEvidenceRecord>> (
                        "metrics.request-control",
                        "control request event is absent from the immutable trace");
                request.control = control;
            }
            const auto analyzed = analyzers.analyze (fixtureRequest.analyzer.id, request);
            if (! analyzed.ok() || ! analyzed.value.has_value())
                return { std::nullopt, analyzed.diagnostics };
            const auto serialized = metricResultsJson (*analyzed.value);
            if (expected.has_value() && serialized != *expected)
                return failure<std::vector<MetricEvidenceRecord>> (
                    "metrics.block-divergence",
                    "declared analyzer metrics must be identical across block patterns");
            expected = serialized;
            if (canonicalMetrics.empty())
                canonicalMetrics = *analyzed.value;
        }

        MetricProvenance provenance;
        provenance.kind = MetricSubjectKind::render;
        provenance.fixtureId = fixture.id;
        provenance.fixturePath = fixture.fixtureRelativePath;
        provenance.fixtureSha256 = sha256File (fixture.fixtureFile);
        provenance.requestId = fixtureRequest.id;
        provenance.requestVersion = fixtureRequest.version;
        provenance.sampleRate = fixture.config.sampleRate;
        provenance.totalSamples = fixture.config.totalSamples;
        provenance.seed = fixture.config.seed;
        provenance.blockPatterns = fixture.config.blockPatterns;
        provenance.reproducibility = results.front().reproducibility;
        for (auto& metric : canonicalMetrics)
            records.push_back ({ std::move (metric), provenance });
    }
    return { std::move (records), {} };
}

LoadResult<FixtureIndex> validateIndexAndRenderFixtures (const juce::File& indexPath)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    auto index = loadFixtureIndex (sourceRoot, indexPath);
    if (! index.ok())
        return index;
    for (const auto& artifact : index.value->renderFixtures) {
        const auto fixture = loadIndexedRenderFixture (sourceRoot, artifact);
        if (! fixture.ok())
            return { std::nullopt, fixture.diagnostics };
    }
    return index;
}

struct ValidatedInputs {
    FixtureIndex index;
    AcceptanceManifest acceptance;
    std::vector<RequirementDefinition> requirements;
    std::vector<SmoothingCase> smoothing;
};

LoadResult<ValidatedInputs> validateInputs (const juce::File& indexPath,
                                            const juce::File& acceptancePath,
                                            const juce::File& requirementsPath)
{
    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    const auto index = validateIndexAndRenderFixtures (indexPath);
    if (! index.ok())
        return { std::nullopt, index.diagnostics };
    const auto analyzers = AnalyzerRegistry::withFoundationAnalyzers();
    for (const auto id : { "signal.stats.v1", "control.step.v1", "audio.click.v1" }) {
        const auto analyzer = analyzers.find (id);
        if (! analyzer.ok())
            return { std::nullopt, analyzer.diagnostics };
    }
    const auto acceptance = loadAcceptanceManifest (sourceRoot, acceptancePath, analyzers);
    if (! acceptance.ok())
        return { std::nullopt, acceptance.diagnostics };
    const auto smoothing = expandSmoothingFixtures (
        sourceRoot, *index.value, *acceptance.value);
    if (! smoothing.ok())
        return { std::nullopt, smoothing.diagnostics };
    const auto requirements = loadRequirementMap (
        sourceRoot, requirementsPath,
        sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md"),
        *acceptance.value);
    if (! requirements.ok())
        return { std::nullopt, requirements.diagnostics };
    return { ValidatedInputs { *index.value, *acceptance.value,
                               *requirements.value, *smoothing.value }, {} };
}

bool appendCandidateEvidence (std::vector<RequirementDefinition>& requirements,
                              const juce::File& candidateRoot,
                              const std::vector<juce::File>& files)
{
    const auto row = std::find_if (
        requirements.begin(), requirements.end(), [] (const auto& requirement) {
            return requirement.id == "TST-001";
        });
    if (row == requirements.end())
        return false;
    for (const auto& file : files) {
        const auto relative = file.getRelativePathFrom (candidateRoot).toStdString();
        if (relative.starts_with ("..") || relative.empty())
            return false;
        row->artifactPaths.push_back ("candidate/" + relative);
        row->artifactSha256.push_back (sha256File (file));
    }
    return true;
}

void printDiagnostic (const Diagnostic& diagnostic)
{
    std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
}

} // namespace

LoadResult<std::vector<MetricEvidenceRecord>> analyzeFixtureMetrics (
    const RenderFixture& fixture,
    const std::span<const RenderResult> results)
{
    return analyzeFixtureMetricsImpl (fixture, results);
}

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
    const juce::File& rendersRoot)
{
    if (! isPortableIdentifierComponent (fixture.id) || ! rendersRoot.isDirectory())
        return failure<std::vector<juce::File>> (
            "output.destination", "candidate fixture output requires a safe ID and renders root");
    std::error_code pathError;
    const auto canonicalRoot = std::filesystem::canonical (
        rendersRoot.getFullPathName().toStdString(), pathError);
    if (pathError)
        return failure<std::vector<juce::File>> (
            "output.destination", "candidate renders root cannot be canonicalized");
    const auto candidatePath = canonicalRoot / fixture.id;
    const auto canonicalCandidate = std::filesystem::weakly_canonical (candidatePath, pathError);
    const auto relativeCandidate = canonicalCandidate.lexically_relative (canonicalRoot);
    if (pathError || relativeCandidate.empty() || relativeCandidate == "."
        || std::distance (relativeCandidate.begin(), relativeCandidate.end()) != 1
        || relativeCandidate.generic_string() != fixture.id)
        return failure<std::vector<juce::File>> (
            "output.destination", "candidate fixture output must be a direct child of renders root");
    const auto newDirectory = juce::File { canonicalCandidate.string() };
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
    const auto metricsFile = newDirectory.getChildFile ("metrics.json");
    const auto renderFile = newDirectory.getChildFile ("render.json");

    if (! controlFile.replaceWithText (canonicalJson (controlTraceJson (canonical)), false, false, "\n")
        || ! eventFile.replaceWithText (canonicalJson (eventTraceJson (canonical)), false, false, "\n")
        || ! writeFloatWav (mainFile, canonical.sampleRate,
                            canonical.mainChannels, canonical.main)
        || ! writeFloatWav (phonesFile, canonical.sampleRate,
                            canonical.phonesChannels, canonical.phones))
        return failure<std::vector<juce::File>> ("output.write",
                                                 "candidate artifact could not be written");

    const auto metrics = analyzeFixtureMetrics (fixture, results);
    if (! metrics.ok()
        || ! metricsFile.replaceWithText (
            metricEvidenceJson (*metrics.value), false, false, "\n"))
        return failure<std::vector<juce::File>> (
            metrics.ok() ? "output.write" : metrics.diagnostics.front().code,
            metrics.ok() ? "candidate metric artifact could not be written"
                         : metrics.diagnostics.front().message);

    std::map<std::string, std::string> outputHashes {
        { "control-trace.json", sha256File (controlFile) },
        { "event-trace.json", sha256File (eventFile) },
        { "main.wav", sha256File (mainFile) },
        { "metrics.json", sha256File (metricsFile) },
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
    root->setProperty ("analysisRequests", analysisRequestsJson (fixture.analysisRequests));
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

    return { std::vector<juce::File> {
                 renderFile, controlFile, eventFile, mainFile, phonesFile, metricsFile,
             }, {} };
}

LoadResult<GateMetricEvidence> writeRegistryMetricEvidence (
    const juce::File& sourceRoot,
    const juce::File& metricsFile,
    std::string artifactPath)
{
    if (metricsFile.existsAsFile())
        return failure<GateMetricEvidence> (
            "metrics.exists", "registry metric artifact must not already exist");
    const auto registryPath = std::string {
        "Tests/fixtures/parameters/parameter-registry-v2.json"
    };
    const auto registryFile = resolveBoundedRegularFile (sourceRoot, registryPath);
    if (! registryFile.ok())
        return { std::nullopt, registryFile.diagnostics };
    std::vector<double> immutableRegistry;
    immutableRegistry.reserve (ParameterRegistry::descriptors().size());
    for (size_t index = 0; index < ParameterRegistry::descriptors().size(); ++index)
        immutableRegistry.push_back (static_cast<double> (index));
    const auto analyzed = AnalyzerRegistry::withFoundationAnalyzers().analyze (
        "signal.stats.v1", AnalysisRequest {
            .metric = "sample-count", .control = immutableRegistry,
        });
    if (! analyzed.ok() || ! analyzed.value.has_value() || analyzed.value->size() != 1)
        return { std::nullopt, analyzed.diagnostics };
    MetricProvenance provenance;
    provenance.kind = MetricSubjectKind::liveRegistry;
    provenance.registryPath = registryPath;
    provenance.registrySha256 = sha256File (*registryFile.value);
    provenance.registryCount = ParameterRegistry::descriptors().size();
    provenance.reproducibility.sourceCommit = SYNTH_SOURCE_COMMIT;
    GateMetricEvidence evidence;
    evidence.gateId = "hard.registry.count";
    evidence.record = { analyzed.value->front(), std::move (provenance) };
    evidence.artifactFile = metricsFile;
    evidence.artifactPath = std::move (artifactPath);
    if (! metricsFile.replaceWithText (
            metricEvidenceJson (std::span<const MetricEvidenceRecord> { &evidence.record, 1 }),
            false, false, "\n"))
        return failure<GateMetricEvidence> (
            "output.write", "registry metric artifact could not be written");
    evidence.artifactSha256 = sha256File (metricsFile);
    return { std::move (evidence), {} };
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
        const auto inputs = validateInputs (
            juce::File { arguments[2] }, juce::File { arguments[4] }, juce::File { arguments[6] });
        if (! inputs.ok()) {
            printDiagnostic (inputs.diagnostics.front());
            return 1;
        }
        if (! run)
            return 0;

        const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
        const auto output = juce::File { arguments[8] };
        if (output.exists()) {
            std::cerr << "output.exists: candidate output directory already exists\n";
            return 1;
        }
        const auto staging = output.getParentDirectory().getChildFile (
            "." + output.getFileName() + "-" + juce::Uuid {}.toString());
        if (! staging.createDirectory()) {
            std::cerr << "output.create: staging directory cannot be created\n";
            return 1;
        }
        const auto discardStaging = [&] { staging.deleteRecursively(); };
        std::vector<juce::File> candidateFiles;
        const auto registryEvidence = writeRegistryMetricEvidence (
            sourceRoot, staging.getChildFile ("metrics.json"), "metrics.json");
        if (! registryEvidence.ok()) {
            discardStaging();
            printDiagnostic (registryEvidence.diagnostics.front());
            return 1;
        }
        candidateFiles.push_back (registryEvidence.value->artifactFile);
        const auto rendersRoot = staging.getChildFile ("renders");
        if (! rendersRoot.createDirectory()) {
            discardStaging();
            std::cerr << "output.create: render directory cannot be created\n";
            return 1;
        }
        for (const auto& artifact : inputs.value->index.renderFixtures) {
            const auto fixture = loadIndexedRenderFixture (sourceRoot, artifact);
            if (! fixture.ok()) {
                discardStaging();
                printDiagnostic (fixture.diagnostics.front());
                return 1;
            }
            const auto rendered = renderFixture (*fixture.value);
            if (! rendered.ok()) {
                discardStaging();
                printDiagnostic (rendered.diagnostics.front());
                return 1;
            }
            const auto written = writeCandidateArtifacts (
                *fixture.value, *rendered.value, rendersRoot);
            if (! written.ok()) {
                discardStaging();
                printDiagnostic (written.diagnostics.front());
                return 1;
            }
            candidateFiles.insert (candidateFiles.end(),
                                   written.value->begin(), written.value->end());
        }
        auto requirements = inputs.value->requirements;
        if (! appendCandidateEvidence (requirements, staging, candidateFiles)) {
            discardStaging();
            std::cerr << "requirement.candidate-evidence: F0 artifact mapping failed\n";
            return 1;
        }
        const auto gates = buildF0GateResults (
            sourceRoot, inputs.value->index, inputs.value->acceptance,
            std::span<const GateMetricEvidence> { &*registryEvidence.value, 1 });
        if (! gates.ok()) {
            discardStaging();
            printDiagnostic (gates.diagnostics.front());
            return 1;
        }
        const auto report = buildRequirementReport (
            sourceRoot, staging, requirements, *gates.value, inputs.value->acceptance);
        if (! report.ok()) {
            discardStaging();
            printDiagnostic (report.diagnostics.front());
            return 1;
        }
        const auto writtenReport = writeRequirementReport (*report.value, output);
        if (! writtenReport.ok()) {
            discardStaging();
            printDiagnostic (writtenReport.diagnostics.front());
            return 1;
        }
        if (! registryEvidence.value->artifactFile.copyFileTo (
                output.getChildFile ("metrics.json"))
            || ! rendersRoot.copyDirectoryTo (output.getChildFile ("renders"))) {
            discardStaging();
            output.deleteRecursively();
            std::cerr << "output.write: candidate render artifacts could not be finalized\n";
            return 1;
        }
        discardStaging();
        return 0;
    }
    if (arguments[0] == "verify-release") {
        if (arguments.size() != 3 || ! exactOption (1, "--report")) {
            std::cerr << "cli.arguments: invalid command arguments\n";
            return 2;
        }
        const auto verified = verifyReleaseReady (juce::File { arguments[2] });
        if (verified.ok())
            return 0;
        if (! verified.diagnostics.empty()
            && verified.diagnostics.front().code == "release.not-ready") {
            std::cerr << "release-not-ready: " << verified.diagnostics.front().message << "\n";
            return 3;
        }
        if (! verified.diagnostics.empty())
            printDiagnostic (verified.diagnostics.front());
        return 1;
    }
    std::cerr << "cli.command: unsupported command\n";
    return 2;
}

} // namespace ReferenceHarness
