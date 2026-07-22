#pragma once

#include "ParameterRegistry.h"
#include "StateContract.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ReferenceHarness {

enum class Status { pass, fail, notRun, awaitingApprovedReference };

struct Diagnostic {
    std::string code;
    std::string message;
};

template <typename T>
struct LoadResult {
    std::optional<T> value;
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return value.has_value() && diagnostics.empty(); }
};

struct IndexedArtifact {
    std::string id;
    std::string relativePath;
    std::string sha256;
    std::vector<std::string> requirements;
};

struct FixtureIndex {
    int version = 0;
    std::string schema;
    std::vector<IndexedArtifact> frozenArtifacts;
    std::vector<IndexedArtifact> renderFixtures;
    std::vector<IndexedArtifact> smoothingFixtures;
};

struct AutomationEvent {
    std::uint64_t sample = 0;
    std::uint32_t sequence = 0;
    ParameterRegistry::Key key {};
    float normalizedValue = 0.0f;
};

struct MidiEvent {
    std::uint64_t sample = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> bytes;
};

struct RenderConfig {
    double sampleRate = 0.0;
    std::uint64_t totalSamples = 0;
    std::uint64_t seed = 0;
    std::vector<std::vector<int>> blockPatterns;
};

enum class InputKind { silence, dc, sine, wav };

struct InputDefinition {
    InputKind kind = InputKind::silence;
    double value = 0.0;
    double frequencyHz = 0.0;
    juce::File file;
    std::string sha256;
};

struct ReproducibilityInfo {
    std::string sourceCommit;
    std::string juceCommit;
    std::string buildType;
    std::string platform;
    std::string architecture;
    std::string fixtureSha256;
    std::map<std::string, std::string> inputHashes;
    std::map<std::string, std::string> outputHashes;
    std::uint64_t seed = 0;
};

struct RenderFixture {
    std::string id;
    juce::File fixtureFile;
    juce::File stateFile;
    std::string stateSha256;
    StateContract::ContourContract expectedContourContract {};
    RenderConfig config;
    std::vector<AutomationEvent> automation;
    std::vector<MidiEvent> midi;
    InputDefinition input;
    std::vector<std::string> analyzers;
    std::vector<std::string> requirements;
};

struct ControlTracePoint {
    std::uint64_t sample = 0;
    ParameterRegistry::Key key {};
    float normalizedValue = 0.0f;
    float physicalValue = 0.0f;
};

struct RenderResult {
    double sampleRate = 0.0;
    int mainChannels = 0;
    int phonesChannels = 0;
    std::vector<float> main;
    std::vector<float> phones;
    std::vector<ControlTracePoint> controlTrace;
    std::vector<std::string> eventTrace;
    std::vector<int> blockPattern;
    ReproducibilityInfo reproducibility;
};

} // namespace ReferenceHarness
