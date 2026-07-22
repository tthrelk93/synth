#pragma once

#include "ParameterRegistry.h"
#include "StateContract.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
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
    // v1 has no stochastic harness input generator, so only zero is valid.
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

struct AnalyzerIdentity {
    std::string id;
    int version = 0;
};

struct AnalysisRequest {
    std::string metric;
    std::uint64_t eventSample = 0;
    std::uint64_t durationSamples = 0;
    double start = 0.0;
    double target = 0.0;
    std::span<const double> control;
    std::span<const float> audio;
};

struct MetricResult {
    AnalyzerIdentity analyzer;
    std::string metric;
    std::string unit;
    double value = 0.0;
    double allowance = 0.0;
    bool finite = false;
    std::map<std::string, std::string> settings;
};

enum class MetricSubjectKind { render, liveRegistry };

struct MetricProvenance {
    MetricSubjectKind kind = MetricSubjectKind::render;
    std::string fixtureId;
    std::string fixtureSha256;
    double sampleRate = 0.0;
    std::uint64_t totalSamples = 0;
    std::uint64_t seed = 0;
    std::vector<std::vector<int>> blockPatterns;
    ReproducibilityInfo reproducibility;
    std::string registryPath;
    std::string registrySha256;
    std::uint64_t registryCount = 0;
};

struct MetricEvidenceRecord {
    MetricResult metric;
    MetricProvenance provenance;
};

struct GateMetricEvidence {
    std::string gateId;
    MetricEvidenceRecord record;
    juce::File artifactFile;
    std::string artifactPath;
    std::string artifactSha256;
};

enum class GateClassification {
    hardSoftware,
    published,
    derivedSoftware,
    measuredHardware,
    performance
};

struct PublishedProvenance {
    std::string source;
    std::string sourceVersion;
    std::string page;
};

struct MeasuredHardwareProvenance {
    std::string referenceStatus;
    std::string referenceSet;
    std::string bandArtifact;
    std::vector<std::string> rawSha256;
    std::string instrument;
    std::string environment;
    std::string captureChain;
    int repetitionCount = 0;
    std::string repetitionStatistic;
    std::string uncertaintyMethod;
    double uncertaintyValue = 0.0;
    std::string approver;
    std::string approvalDate;
};

struct PerformanceProvenance {
    std::string targetSystem;
    std::string budgetBasis;
    std::string rationale;
    std::string reviewStatus;
    std::string reviewer;
    std::string reviewDate;
};

struct GateDefinition {
    std::string id;
    GateClassification classification = GateClassification::hardSoftware;
    Status status = Status::notRun;
    std::vector<std::string> requirements;
    AnalyzerIdentity analyzer;
    std::string metric;
    std::string unit;
    double value = 0.0;
    double allowance = 0.0;
    std::map<std::string, std::string> provenance;
    std::optional<PublishedProvenance> published;
    std::optional<MeasuredHardwareProvenance> measuredHardware;
    std::optional<PerformanceProvenance> performance;
    std::string artifactPath;
    std::string artifactSha256;
};

struct GateResult {
    std::string id;
    Status status = Status::notRun;
    std::string reasonCode;
    std::vector<std::string> requirements;
    std::optional<MetricResult> metric;
    std::string artifactPath;
    std::string artifactSha256;
};

struct AcceptanceManifest {
    std::string schema;
    int version = 0;
    std::string status;
    std::vector<GateDefinition> hardSoftware;
    std::vector<GateDefinition> published;
    std::vector<GateDefinition> derivedSoftware;
    std::vector<GateDefinition> measuredHardware;
    std::vector<GateDefinition> performance;
};

struct SmoothingFixture {
    std::string schema;
    std::string id;
    ParameterRegistry::SmoothingClass smoothingClass = ParameterRegistry::SmoothingClass::unspecified;
    double startNormalized = 0.0;
    double endNormalized = 0.0;
    std::vector<int> sampleRates;
    std::uint64_t eventSample = 0;
    std::uint64_t analysisWindow = 0;
    double durationSeconds = 0.0;
    std::optional<int> intermediateValues;
    std::string analyzerId;
    std::string requiredTap;
    std::string ownerWorkstream;
    Status status = Status::notRun;
    std::string reasonCode;
    Status referenceStatus = Status::awaitingApprovedReference;
    std::string referenceReasonCode;
    std::optional<std::string> policyId;
    std::optional<std::string> settlingPolicyId;
    std::string relativePath;
    std::string sha256;
};

struct SmoothingCase : SmoothingFixture {
    ParameterRegistry::Key key {};
    std::string parameterId;
};

struct SmoothingEvidence {
    std::string parameterId;
    RenderFixture fixture;
    RenderResult render;
    juce::File candidateDirectory;
};

} // namespace ReferenceHarness
