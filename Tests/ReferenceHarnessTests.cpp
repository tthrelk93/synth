#include "ReferenceData.h"
#include "OfflineRenderer.h"
#include "Acceptance.h"
#include "AnalyzerRegistry.h"
#include "PluginProcessor.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <limits>

namespace {

constexpr auto sourceRootPath = SYNTH_SOURCE_ROOT;

class TemporaryDirectory final {
public:
    explicit TemporaryDirectory (const juce::String& prefix)
    {
        const auto temporaryRoot = juce::File::getSpecialLocation (juce::File::tempDirectory);
        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto candidate = temporaryRoot.getChildFile (
                prefix + "-" + juce::Uuid {}.toString());
            std::error_code error;
            if (std::filesystem::create_directory (candidate.getFullPathName().toStdString(), error)) {
                directory = candidate;
                ownsDirectory = true;
                return;
            }
        }
    }

    ~TemporaryDirectory()
    {
        if (ownsDirectory)
            directory.deleteRecursively();
    }

    bool isOwned() const noexcept { return ownsDirectory; }

    juce::File directory;

private:
    bool ownsDirectory = false;
};

constexpr std::array frozenFixturePaths {
    "Tests/fixtures/parameters/legacy-parameter-inventory.json",
    "Tests/fixtures/parameters/parameter-registry-v2.json",
    "Tests/fixtures/parameters/parameter-snapshot-v2.json",
    "Tests/fixtures/state/contour-routing-conversion-trace.json",
    "Tests/fixtures/state/legacy-default-state.xml",
    "Tests/fixtures/state/legacy-representative-state.xml",
    "Tests/fixtures/state/native-default-state-v2.xml",
    "Tests/fixtures/state/migrated-default-state-v2.xml",
    "Tests/fixtures/state/migrated-representative-state-v2.xml",
    "Tests/reference/fixtures/native-v2-foundation.json",
    "Tests/reference/fixtures/migrated-v2-foundation.json",
    "Tests/reference/fixtures/legacy-contour-foundation.json",
    "Tests/reference/fixtures/par-006/none-step-v1.json",
    "Tests/reference/fixtures/par-006/gain-control-step-v1.json",
    "Tests/reference/fixtures/par-006/control-step-v1.json",
    "Tests/reference/fixtures/par-006/dedicated-pitch-step-v1.json",
    "Tests/reference/fixtures/par-006/dedicated-cutoff-step-v1.json",
    "Tests/reference/fixtures/par-006/dedicated-glide-step-v1.json",
    "Tests/reference/fixtures/par-006/contour-stage-step-v1.json",
};

class ReferenceHarnessTest final : public juce::UnitTest {
public:
    ReferenceHarnessTest()
        : juce::UnitTest ("ModelDReferenceManifestContract", "manifest")
    {
    }

    void runTest() override
    {
        const auto sourceRoot = juce::File { sourceRootPath };
        const auto index = ReferenceHarness::loadFixtureIndex (
            sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
        beginTest ("the live frozen fixture index validates");
        expect (index.ok(), "fixture index must validate");
        if (index.value.has_value())
            expect (index.value->frozenArtifacts.size() == 9,
                    "fixture index must contain all nine frozen artifacts");

        beginTest ("canonical JSON sorts object properties and retains arrays");
        const auto unorderedJson = juce::JSON::fromString (
            R"({"b":[{"b":2,"a":1}],"a":{"z":2,"x":1}})");
        expectEquals (ReferenceHarness::canonicalJson (unorderedJson),
                      juce::String { R"({"a":{"x":1,"z":2},"b":[{"a":1,"b":2}]})" } + "\n");

        beginTest ("same-prefix temporary roots are atomically owned and distinct");
        const TemporaryDirectory firstTemporaryRoot { "model-d-reference-harness-collision" };
        const TemporaryDirectory secondTemporaryRoot { "model-d-reference-harness-collision" };
        expect (firstTemporaryRoot.isOwned(), "first temporary root must be created atomically");
        expect (secondTemporaryRoot.isOwned(), "second temporary root must be created atomically");
        expect (firstTemporaryRoot.directory != secondTemporaryRoot.directory,
                "same-prefix temporary roots must never share a cleanup target");

        beginTest ("bounded paths report stable negative diagnostics");
        const TemporaryDirectory temporaryRoot { "model-d-reference-harness-contract" };
        expect (temporaryRoot.isOwned(), "bounded-path temporary root must be created atomically");
        if (! temporaryRoot.isOwned())
            return;
        const auto& temporaryDirectory = temporaryRoot.directory;
        const auto safeFile = temporaryDirectory.getChildFile ("safe.txt");
        safeFile.replaceWithText ("safe");

        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "/safe.txt"),
                          "path.absolute");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "../safe.txt"),
                          "path.traversal");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "safe\\\\txt"),
                          "path.backslash");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "missing.txt"),
                          "path.missing");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "."),
                          "path.component");

        const auto directory = temporaryDirectory.getChildFile ("directory");
        directory.createDirectory();
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "directory"),
                          "path.non-regular");

        const TemporaryDirectory outsideRoot { "model-d-reference-harness-outside" };
        expect (outsideRoot.isOwned(), "symlink target root must be created atomically");
        if (! outsideRoot.isOwned())
            return;
        const auto outside = outsideRoot.directory.getChildFile ("outside.txt");
        outside.replaceWithText ("outside");
        const auto link = temporaryDirectory.getChildFile ("escape-link");
        std::error_code symlinkError;
        std::filesystem::create_symlink (outside.getFullPathName().toStdString(),
                                         link.getFullPathName().toStdString(), symlinkError);
        expect (! symlinkError, "symlink fixture must be creatable");
        if (! symlinkError)
            expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryDirectory, "escape-link"),
                              "path.escape");

        beginTest ("fixture-index mutations report stable diagnostics");
        const auto temporaryIndex = temporaryDirectory.getChildFile ("fixture-index-v1.json");
        const auto originalIndex = sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json")
                                       .loadFileAsString();
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d",
                                  "0ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d"),
                               "index.sha256");
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "legacy-parameter-inventory", "parameter-registry-v2"),
                               "index.duplicate-id");
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "Tests/fixtures/parameters/legacy-parameter-inventory.json",
                                  "Tests/fixtures/parameters/parameter-registry-v2.json"),
                               "index.duplicate-path");
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "model-d.fixture-index.v1", "model-d.fixture-index.v999"),
                               "index.schema");
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "legacy-parameter-inventory", "unexpected-frozen-artifact"),
                               "index.frozen-set");
        expectIndexDiagnostic (temporaryIndex, originalIndex.replaceFirstOccurrenceOf (
                                  "[\"PAR-001\"]", "[\"PAR-002\"]"),
                               "index.frozen-set");

        beginTest ("semantic probes reject live-contract drift");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/parameters/parameter-registry-v2.json",
                             "2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284",
                             "\"id\":\"osc1Waveform\"", "\"id\":\"driftedOscillator\"",
                             "semantic.registry");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/parameters/legacy-parameter-inventory.json",
                             "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d",
                             "\"id\":\"osc1Waveform\"", "\"id\":\"driftedOscillator\"",
                             "semantic.inventory");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/parameters/legacy-parameter-inventory.json",
                             "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d",
                             "\"version_hint\":0", "\"version_hint\":1",
                             "semantic.inventory");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/parameters/parameter-snapshot-v2.json",
                             "cbfefbf2e918818fed1fd6b340ca4c015981d6e020080a7b71bbfd006e398f16",
                             "\"id\":\"osc1Waveform\"", "\"id\":\"driftedOscillator\"",
                             "semantic.snapshot");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/state/native-default-state-v2.xml",
                             "ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507",
                             "stateVersion=\"2\"", "stateVersion=\"3\"",
                             "semantic.state");
        expectSemanticProbe (sourceRoot,
                             "Tests/fixtures/state/contour-routing-conversion-trace.json",
                             "ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c",
                             "\"marker\":\"canonicalContours\"",
                             "\"marker\":\"legacyCrossedContours\"",
                             "semantic.contour");
    }

private:
    template <typename T>
    void expectDiagnostic (const ReferenceHarness::LoadResult<T>& result,
                           const std::string_view code)
    {
        expect (! result.ok(), "negative case must fail");
        const auto actual = result.diagnostics.empty() ? "<none>" : result.diagnostics.front().code;
        expect (! result.diagnostics.empty() && actual == code,
                "negative case must report stable diagnostic " + std::string { code }
                    + ", got " + actual);
    }

    void expectIndexDiagnostic (const juce::File& indexFile,
                                const juce::String& contents,
                                const std::string_view code)
    {
        indexFile.replaceWithText (contents);
        expectDiagnostic (ReferenceHarness::loadFixtureIndex (
                              juce::File { sourceRootPath }, indexFile),
                          code);
    }

    void expectSemanticProbe (const juce::File& sourceRoot,
                              const juce::String& relativePath,
                              const juce::String& expectedHash,
                              const juce::String& needle,
                              const juce::String& replacement,
                              const std::string_view code)
    {
        const TemporaryDirectory probeRoot { "model-d-reference-harness-probe" };
        expect (probeRoot.isOwned(), "semantic probe root must be created atomically");
        if (! probeRoot.isOwned())
            return;
        for (const auto fixturePath : frozenFixturePaths) {
            const auto source = sourceRoot.getChildFile (fixturePath);
            const auto destination = probeRoot.directory.getChildFile (fixturePath);
            destination.getParentDirectory().createDirectory();
            expect (source.copyFileTo (destination), "semantic probe must copy each frozen fixture");
        }

        const auto fixture = probeRoot.directory.getChildFile (relativePath);
        fixture.replaceWithText (fixture.loadFileAsString().replaceFirstOccurrenceOf (needle, replacement));

        const auto originalIndex = sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json")
                                       .loadFileAsString();
        const auto probeIndex = originalIndex.replaceFirstOccurrenceOf (
            expectedHash, ReferenceHarness::sha256File (fixture));
        const auto indexFile = probeRoot.directory.getChildFile ("fixture-index-v1.json");
        indexFile.replaceWithText (probeIndex);
        expectDiagnostic (ReferenceHarness::loadFixtureIndex (probeRoot.directory, indexFile), code);
    }
};

ReferenceHarnessTest referenceHarnessTest;

class ReferenceAnalyzerTest final : public juce::UnitTest {
public:
    ReferenceAnalyzerTest()
        : juce::UnitTest ("ModelDReferenceAnalyzerContract", "analyzers")
    {
    }

    void runTest() override
    {
        using namespace ReferenceHarness;
        const auto registry = AnalyzerRegistry::withFoundationAnalyzers();

        beginTest ("foundation analyzer identities are versioned and immutable");
        for (const auto id : { "signal.stats.v1", "control.step.v1", "audio.click.v1" }) {
            const auto found = registry.find (id);
            expect (found.ok() && found.value->id == id && found.value->version == 1,
                    juce::String { id } + " must resolve at version one");
        }
        expectDiagnostic (registry.find ("signal.stats.v2"), "analyzer.unknown");

        beginTest ("signal statistics calibrate constant and impulse inputs");
        const std::array<float, 4> constant { 2.0f, 2.0f, 2.0f, 2.0f };
        const auto constantMetrics = registry.analyze (
            "signal.stats.v1", AnalysisRequest { .audio = constant });
        expect (constantMetrics.ok(), "constant statistics must analyze");
        expectMetric (constantMetrics, "sample-count", "count", 4.0);
        expectMetric (constantMetrics, "finite-count", "count", 4.0);
        expectMetric (constantMetrics, "minimum", "amplitude", 2.0);
        expectMetric (constantMetrics, "maximum", "amplitude", 2.0);
        expectMetric (constantMetrics, "peak-absolute", "amplitude", 2.0);
        expectMetric (constantMetrics, "mean", "amplitude", 2.0);
        expectMetric (constantMetrics, "rms", "amplitude", 2.0);
        expectMetric (constantMetrics, "maximum-first-difference", "amplitude/sample", 0.0);
        const std::array<float, 5> impulse { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
        const auto impulseMetrics = registry.analyze (
            "signal.stats.v1", AnalysisRequest { .audio = impulse });
        expectMetric (impulseMetrics, "mean", "amplitude", 0.2);
        expectMetric (impulseMetrics, "rms", "amplitude", std::sqrt (0.2));
        expectMetric (impulseMetrics, "maximum-first-difference", "amplitude/sample", 1.0);
        if (constantMetrics.value.has_value()) {
            const auto settings = constantMetrics.value->front().settings;
            expect (settings == std::map<std::string, std::string> {
                                   { "finite-policy", "reject-non-finite" },
                                   { "input", "audio" },
                                   { "sample-domain", "contiguous" },
                               },
                    "signal analyzer settings must be exact and explicit");
            expectEquals (metricResultsJson (*constantMetrics.value),
                          metricResultsJson (*constantMetrics.value),
                          "metric JSON must be deterministic");
        }

        beginTest ("control steps calibrate directions, policy ramps, overshoot, and settling");
        const std::array<double, 5> upward { 0.0, 0.0, 1.0, 1.0, 1.0 };
        const auto up = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 2, .start = 0.0,
                                                   .target = 1.0, .control = upward });
        expectMetric (up, "first-change-sample", "samples", 2.0);
        expectMetric (up, "settled-sample", "samples", 2.0);
        expectMetric (up, "monotonic", "boolean", 1.0);
        expectMetric (up, "overshoot", "normalized", 0.0);
        expectMetric (up, "maximum-per-sample-movement", "normalized/sample", 1.0);

        const std::array<double, 5> downward { 1.0, 1.0, 0.0, 0.0, 0.0 };
        const auto down = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 2, .start = 1.0,
                                                   .target = 0.0, .control = downward });
        expectMetric (down, "first-change-sample", "samples", 2.0);
        expectMetric (down, "monotonic", "boolean", 1.0);

        const auto fiveMs = makeRamp (10, 240, false);
        const auto five = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 10, .durationSamples = 240,
                                                   .start = 0.0, .target = 1.0,
                                                   .control = fiveMs });
        expectMetric (five, "first-change-sample", "samples", 11.0);
        expectMetric (five, "settled-sample", "samples", 250.0);
        expectMetric (five, "monotonic", "boolean", 1.0);

        const auto tenMs = makeRamp (10, 480, false);
        const auto ten = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 10, .durationSamples = 480,
                                                   .start = 0.0, .target = 1.0,
                                                   .control = tenMs });
        expectMetric (ten, "settled-sample", "samples", 490.0);

        const std::array<double, 6> overshoot { 0.0, 0.0, 0.5, 1.1, 1.0, 1.0 };
        const auto over = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 2, .durationSamples = 2,
                                                   .start = 0.0, .target = 1.0,
                                                   .control = overshoot });
        expectMetric (over, "monotonic", "boolean", 0.0);
        expectMetric (over, "overshoot", "normalized", 0.1);
        const std::array<double, 7> late { 0.0, 0.0, 0.3, 0.6, 0.9, 0.99, 1.0 };
        const auto lateMetrics = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 2, .durationSamples = 3,
                                                   .start = 0.0, .target = 1.0,
                                                   .control = late });
        expectMetric (lateMetrics, "settled-sample", "samples", 6.0);
        expectMetric (lateMetrics, "floating-allowance", "normalized",
                      8.0 * std::numeric_limits<double>::epsilon());
        if (five.value.has_value())
            expect (five.value->front().settings == std::map<std::string, std::string> {
                        { "allowance", "8*epsilon*max(1,travel)" },
                        { "first-change", "first sample at/after event outside start allowance" },
                        { "ideal-increment", "travel/duration-or-travel-for-zero-duration" },
                        { "settled", "first sample whose suffix remains within target allowance" },
                    },
                    "control-step settings must be exact and explicit");

        beginTest ("audio click metrics calibrate a declared event window");
        const std::array<float, 7> click { 1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 1.0f, 1.0f };
        const auto clickMetrics = registry.analyze (
            "audio.click.v1", AnalysisRequest { .eventSample = 3, .durationSamples = 1,
                                                  .audio = click });
        expectMetric (clickMetrics, "maximum-first-difference", "amplitude/sample", 1.0);
        expectMetric (clickMetrics, "pre-rms", "amplitude", 1.0);
        expectMetric (clickMetrics, "post-rms", "amplitude", 1.0);
        expectMetric (clickMetrics, "peak-over-steady-state", "dB", 0.0);
        expectMetric (clickMetrics, "finite-count", "count", 7.0);
        if (clickMetrics.value.has_value())
            expect (clickMetrics.value->front().settings == std::map<std::string, std::string> {
                        { "event-window", "inclusive transition indices event..event+duration" },
                        { "finite-policy", "reject-non-finite" },
                        { "steady-state", "mean of pre/post RMS" },
                    },
                    "audio-click settings must be exact and explicit");

        beginTest ("empty, non-finite, and unknown metric analysis fails stably without NaN JSON");
        const auto empty = registry.analyze ("signal.stats.v1", {});
        expectDiagnostic (empty, "analyzer.empty-input");
        const std::array<float, 2> nonFinite { 0.0f, std::numeric_limits<float>::quiet_NaN() };
        const auto nan = registry.analyze (
            "signal.stats.v1", AnalysisRequest { .audio = nonFinite });
        expectDiagnostic (nan, "analyzer.non-finite");
        if (nan.value.has_value()) {
            const auto json = metricResultsJson (*nan.value);
            expect (! json.containsIgnoreCase ("nan") && ! json.containsIgnoreCase ("inf"),
                    "failed metric JSON must remain numeric and deterministic");
        }
        expectDiagnostic (registry.analyze (
                              "signal.stats.v1", AnalysisRequest { .metric = "unknown",
                                                                    .audio = constant }),
                          "analyzer.unknown-metric");

        beginTest ("the draft acceptance manifest validates exact approved software policies");
        const auto sourceRoot = juce::File { sourceRootPath };
        const auto acceptanceFile = sourceRoot.getChildFile ("Tests/reference/acceptance-v1.json");
        const auto manifest = loadAcceptanceManifest (sourceRoot, acceptanceFile, registry);
        expect (manifest.ok(), "the checked-in acceptance manifest must validate");
        if (manifest.value.has_value()) {
            expect (manifest.value->status == "draft", "global acceptance must remain draft");
            expectEquals (static_cast<int> (manifest.value->derivedSoftware.size()), 4);
        }

        beginTest ("acceptance validation rejects unsupported or unproven claims");
        const auto acceptanceText = acceptanceFile.loadFileAsString();
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "model-d.acceptance.v1", "model-d.acceptance.v2"),
                                    "acceptance.schema");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("status": "draft")", R"("status": "staged")"),
                                    "acceptance.status");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("classification": "hard-software")",
                                        R"("classification": "subjective")"),
                                    "acceptance.classification");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("analyzer": "signal.stats.v1")",
                                        R"("analyzer": "signal.stats.v2")"),
                                    "acceptance.analyzer");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("analyzerVersion": 1)", R"("analyzerVersion": 2)"),
                                    "acceptance.analyzer-version");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("metric": "sample-count")", R"("metric": "mystery")"),
                                    "acceptance.metric");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("unit": "count")", R"("unit": "mystery")"),
                                    "acceptance.unit");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("id": "hard.registry.count")",
                                        R"("id": "par006.gain-control.duration")"),
                                    "acceptance.duplicate-id");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("requirements": ["PAR-001", "TST-006"])",
                                        R"("requirements": [])"),
                                    "acceptance.requirement");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("derivation": "5 ms linear-amplitude software safety ramp")",
                                        R"("derivation": "")"),
                                    "acceptance.derivation");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("published": [])",
                                        R"("published": [{"id":"bad.source","classification":"published","status":"not-run","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1}])"),
                                    "acceptance.source");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("measuredHardware": [])",
                                        R"("measuredHardware": [{"id":"bad.reference","classification":"measured-hardware","status":"awaiting-approved-reference","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1}])"),
                                    "acceptance.reference");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("performance": [])",
                                        R"("performance": [{"id":"bad.performance","classification":"performance","status":"not-run","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1}])"),
                                    "acceptance.performance");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("status": "draft")", R"("status": "approved")"),
                                    "acceptance.incomplete");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("zeroBasis": "approved exact-step policy")",
                                        R"("zeroBasis": "")"),
                                    "acceptance.zero");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("reviewStatus": "approved")",
                                        R"("reviewStatus": "draft")"),
                                    "acceptance.derived-review");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("id": "par006.gain-control.duration")",
                                        R"("id": "par006.unapproved.duration")"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("status": "not-run")", R"("status": "pass")"),
                                    "acceptance.pass-artifact");

        beginTest ("seven templates expand from the live registry without a duplicate ID inventory");
        const auto index = loadFixtureIndex (
            sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
        expect (index.ok(), "fixture index with smoothing templates must validate");
        if (! index.value.has_value())
            return;
        expectEquals (static_cast<int> (index.value->smoothingFixtures.size()), 7);
        const auto expanded = expandSmoothingFixtures (sourceRoot, *index.value);
        expect (expanded.ok(), "the seven templates must expand by registry class");
        if (! expanded.value.has_value())
            return;
        expectEquals (static_cast<int> (expanded.value->size()), 48);
        const std::map<ParameterRegistry::SmoothingClass, int> expectedCounts {
            { ParameterRegistry::SmoothingClass::none, 27 },
            { ParameterRegistry::SmoothingClass::gainControl, 7 },
            { ParameterRegistry::SmoothingClass::control, 5 },
            { ParameterRegistry::SmoothingClass::dedicatedPitch, 1 },
            { ParameterRegistry::SmoothingClass::dedicatedCutoff, 1 },
            { ParameterRegistry::SmoothingClass::dedicatedGlide, 1 },
            { ParameterRegistry::SmoothingClass::contourStage, 6 },
        };
        std::map<ParameterRegistry::SmoothingClass, int> actualCounts;
        for (size_t position = 0; position < expanded.value->size(); ++position) {
            const auto& smoothingCase = expanded.value->at (position);
            ++actualCounts[smoothingCase.smoothingClass];
            expect (smoothingCase.parameterId
                        == ParameterRegistry::descriptors()[position].id,
                    "expansion order and IDs must come directly from the registry");
            expect (smoothingCase.sampleRates == std::vector<int> { 44100, 48000, 96000 },
                    "every template must declare the exact sample-rate matrix");
        }
        expect (actualCounts == expectedCounts, "frozen registry class counts must match");
        for (const auto& smoothingCase : *expanded.value) {
            if (smoothingCase.smoothingClass == ParameterRegistry::SmoothingClass::none) {
                expect (smoothingCase.durationSeconds == 0.0
                            && smoothingCase.intermediateValues == std::optional<int> { 0 }
                            && smoothingCase.status == Status::pass,
                        "none must use the executable exact-step policy");
            } else if (smoothingCase.smoothingClass
                       == ParameterRegistry::SmoothingClass::gainControl) {
                expectWithinAbsoluteError (smoothingCase.durationSeconds, 0.005, 1.0e-15,
                                           "gainControl must retain the approved 5 ms policy");
                expect (smoothingCase.status == Status::notRun,
                        "gainControl owner DSP tap must remain not-run");
                expect (! smoothingCase.intermediateValues.has_value(),
                        "gainControl must not invent an intermediate count before its tap runs");
            } else if (smoothingCase.smoothingClass
                       == ParameterRegistry::SmoothingClass::control) {
                expectWithinAbsoluteError (smoothingCase.durationSeconds, 0.010, 1.0e-15,
                                           "control must retain the approved 10 ms policy");
                expect (smoothingCase.status == Status::notRun,
                        "control owner DSP tap must remain not-run");
                expect (! smoothingCase.intermediateValues.has_value(),
                        "control must not invent an intermediate count before its tap runs");
            } else {
                expect (smoothingCase.durationSeconds == 0.0
                            && ! smoothingCase.intermediateValues.has_value()
                            && smoothingCase.status == Status::notRun,
                        "dedicated classes must not substitute a generic ramp");
            }
        }

        beginTest ("live evaluation passes only exact-step control evidence");
        if (! manifest.value.has_value())
            return;
        const auto evaluated = evaluateAcceptance (*manifest.value, *expanded.value);
        expect (evaluated.ok(), "draft F0 acceptance must evaluate honestly");
        int passCount = 0;
        int awaitingCount = 0;
        for (const auto& result : *evaluated.value) {
            passCount += result.status == Status::pass ? 1 : 0;
            awaitingCount += result.status == Status::awaitingApprovedReference ? 1 : 0;
            if (result.status == Status::pass)
                expect (! result.artifactSha256.empty(), "a pass must carry an artifact hash");
        }
        expectEquals (passCount, 27, "only the 27 none-class control gates may pass");
        expect (awaitingCount > 0, "missing hardware references must remain awaiting approval");
    }

private:
    template <typename T>
    void expectDiagnostic (const ReferenceHarness::LoadResult<T>& result,
                           const std::string_view code)
    {
        expect (! result.ok(), "negative case must fail");
        const auto actual = result.diagnostics.empty() ? "<none>" : result.diagnostics.front().code;
        expect (! result.diagnostics.empty() && actual == code,
                "negative case must report stable diagnostic " + std::string { code }
                    + ", got " + actual);
    }

    void expectMetric (const ReferenceHarness::LoadResult<
                           std::vector<ReferenceHarness::MetricResult>>& result,
                       const std::string_view name,
                       const std::string_view unit,
                       const double expected)
    {
        expect (result.value.has_value(), "analysis must return metric records");
        if (! result.value.has_value())
            return;
        const auto found = std::find_if (result.value->begin(), result.value->end(), [&] (const auto& metric) {
            return metric.metric == name;
        });
        expect (found != result.value->end(), "metric must be reported: " + std::string { name });
        if (found != result.value->end()) {
            expect (found->unit == unit, "metric unit must be exact: " + std::string { name });
            expectWithinAbsoluteError (found->value, expected, 1.0e-12,
                                       "metric value must match analytic calibration");
            expect (std::isfinite (found->value), "metric JSON values must always be finite");
        }
    }

    static std::vector<double> makeRamp (const size_t eventSample,
                                         const size_t durationSamples,
                                         const bool downward)
    {
        std::vector<double> values (eventSample + durationSamples + 3,
                                    downward ? 1.0 : 0.0);
        for (size_t offset = 1; offset <= durationSamples; ++offset) {
            const auto normalized = static_cast<double> (offset)
                                  / static_cast<double> (durationSamples);
            values[eventSample + offset] = downward ? 1.0 - normalized : normalized;
        }
        std::fill (values.begin() + static_cast<std::ptrdiff_t> (eventSample + durationSamples),
                   values.end(), downward ? 0.0 : 1.0);
        return values;
    }

    void expectAcceptanceDiagnostic (const juce::File& sourceRoot,
                                     const ReferenceHarness::AnalyzerRegistry& registry,
                                     const juce::String& text,
                                     const std::string_view code)
    {
        const TemporaryDirectory temporary { "model-d-acceptance-negative" };
        expect (temporary.isOwned(), "acceptance negative root must be owned");
        if (! temporary.isOwned())
            return;
        const auto file = temporary.directory.getChildFile ("acceptance.json");
        file.replaceWithText (text);
        expectDiagnostic (loadAcceptanceManifest (sourceRoot, file, registry), code);
    }
};

ReferenceAnalyzerTest referenceAnalyzerTest;

class ReferenceRendererTest final : public juce::UnitTest {
public:
    ReferenceRendererTest()
        : juce::UnitTest ("ModelDReferenceRendererContract", "renderer")
    {
    }

    void runTest() override
    {
        const auto sourceRoot = juce::File { sourceRootPath };
        constexpr std::array fixturePaths {
            "Tests/reference/fixtures/native-v2-foundation.json",
            "Tests/reference/fixtures/migrated-v2-foundation.json",
            "Tests/reference/fixtures/legacy-contour-foundation.json",
        };

        beginTest ("all foundation render fixtures validate and restore their v2 state");
        std::vector<ReferenceHarness::RenderFixture> fixtures;
        for (const auto path : fixturePaths) {
            const auto loaded = ReferenceHarness::loadRenderFixture (
                sourceRoot, sourceRoot.getChildFile (path));
            expect (loaded.ok(), juce::String { path } + " must validate");
            if (! loaded.value.has_value())
                continue;

            MoogMiniAudioProcessor processor;
            const auto stateXml = juce::XmlDocument::parse (loaded.value->stateFile);
            expect (stateXml != nullptr, juce::String { path } + " state XML must parse");
            juce::MemoryBlock bytes;
            if (stateXml != nullptr)
                juce::AudioProcessor::copyXmlToBinary (*stateXml, bytes);
            const auto restored = processor.restoreState (
                bytes.getData(), static_cast<int> (bytes.getSize()));
            expect (restored.succeeded(), juce::String { path } + " state must restore");
            expect (processor.getContourContract() == loaded.value->expectedContourContract,
                    juce::String { path } + " contour marker must survive restore");
            fixtures.push_back (*loaded.value);
        }
        expectEquals (static_cast<int> (fixtures.size()), 3);
        if (fixtures.size() != 3)
            return;
        expect (fixtures.back().expectedContourContract
                    == StateContract::ContourContract::legacyCrossedContours,
                "legacy fixture must retain legacyCrossedContours");

        beginTest ("events apply at exact samples in stable same-sample sequence order");
        const auto firstRun = ReferenceHarness::renderFixture (fixtures.front());
        expect (firstRun.ok(), "native fixture must render");
        if (! firstRun.value.has_value())
            return;
        expectEquals (static_cast<int> (firstRun.value->size()), 3);
        for (const auto& result : *firstRun.value) {
            expectEquals (static_cast<int> (result.controlTrace.size()), 3);
            if (result.controlTrace.size() == 3) {
                expect (result.controlTrace[0].sample == 0
                            && result.controlTrace[0].key
                                == ParameterRegistry::Key::outputVolKnob,
                        "output volume must be the first sample-zero automation event");
                expect (result.controlTrace[1].sample == 0
                            && result.controlTrace[1].key
                                == ParameterRegistry::Key::a440HzOnOff,
                        "A-440 must be the second sample-zero automation event");
                expect (result.controlTrace[2].sample == 512
                            && result.controlTrace[2].key
                                == ParameterRegistry::Key::filterCutoff,
                        "the transition must apply exactly at sample 512");
            }
            expect (result.eventTrace.size() >= 5, "all automation and MIDI events must be traced");
            if (result.eventTrace.size() >= 3) {
                expect (result.eventTrace[0].starts_with ("automation:0:0:outputVolKnob:"),
                        "sequence zero must be traced first");
                expect (result.eventTrace[1].starts_with ("automation:0:1:a440HzOnOff:"),
                        "sequence one must be traced second");
                expect (result.eventTrace[2].starts_with ("midi:0:2:"),
                        "same-sample MIDI must retain sequence order");
            }
            expect (std::all_of (result.main.begin(), result.main.end(), [] (float sample) {
                        return std::isfinite (sample);
                    }),
                    "main samples must be finite");
            expect (std::all_of (result.phones.begin(), result.phones.end(), [] (float sample) {
                        return std::isfinite (sample);
                    }),
                    "phones samples must be finite");
            expect (result.main.size()
                        == static_cast<size_t> (result.mainChannels) * 2048u,
                    "main audio must retain its exact interleaved channel count");
            expect (result.phones.size()
                        == static_cast<size_t> (result.phonesChannels) * 2048u,
                    "phones audio must retain its exact interleaved channel count");
            expect (result.reproducibility.inputHashes
                        == std::map<std::string, std::string> {
                            { "state", fixtures.front().stateSha256 },
                        },
                    "generated input fixtures must record exactly the state input hash");
        }

        for (size_t fixtureIndex = 1; fixtureIndex < fixtures.size(); ++fixtureIndex) {
            const auto rendered = ReferenceHarness::renderFixture (fixtures[fixtureIndex]);
            expect (rendered.ok(), "each migrated/compatibility fixture must render");
            if (! rendered.value.has_value())
                continue;
            expectEquals (static_cast<int> (rendered.value->size()), 3);
            for (const auto& result : *rendered.value) {
                expect (std::all_of (result.main.begin(), result.main.end(), [] (float sample) {
                            return std::isfinite (sample);
                        }),
                        "migrated/compatibility main samples must be finite");
                expect (std::all_of (result.phones.begin(), result.phones.end(), [] (float sample) {
                            return std::isfinite (sample);
                        }),
                        "migrated/compatibility phones samples must be finite");
            }
        }

        beginTest ("repeated runs and host block patterns have identical local-policy hashes");
        std::optional<ReferenceHarness::LoadResult<std::vector<ReferenceHarness::RenderResult>>>
            nativeSecondRun;
        for (size_t fixtureIndex = 0; fixtureIndex < fixtures.size(); ++fixtureIndex) {
            const auto repeatedFirst = ReferenceHarness::renderFixture (fixtures[fixtureIndex]);
            const auto repeatedSecond = ReferenceHarness::renderFixture (fixtures[fixtureIndex]);
            expect (repeatedFirst.ok() && repeatedSecond.ok(),
                    "every foundation fixture must render twice");
            if (! repeatedFirst.value.has_value() || ! repeatedSecond.value.has_value()
                || repeatedFirst.value->size() != repeatedSecond.value->size()
                || repeatedFirst.value->empty())
                continue;
            if (fixtureIndex == 0)
                nativeSecondRun = repeatedSecond;
            for (const auto hashName : { "main", "phones", "control", "event" }) {
                const auto expected = repeatedFirst.value->front()
                                          .reproducibility.outputHashes.at (hashName);
                for (size_t pattern = 0; pattern < repeatedFirst.value->size(); ++pattern) {
                    expect (repeatedFirst.value->at (pattern)
                                    .reproducibility.outputHashes.at (hashName) == expected
                                && repeatedSecond.value->at (pattern)
                                       .reproducibility.outputHashes.at (hashName) == expected,
                            fixtures[fixtureIndex].id + " " + hashName
                                + " hash must be repeatable and host-block invariant");
                }
            }
        }

        beginTest ("render fixture validation reports stable negative diagnostics");
        const auto validText = sourceRoot.getChildFile (fixturePaths.front()).loadFileAsString();
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("kind": "hostState")", R"("kind": "preset")"),
                                "fixture.state-source");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("sampleRate": 48000)", R"("sampleRate": 32000)"),
                                "fixture.sample-rate");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("totalSamples": 2048)", R"("totalSamples": 0)"),
                                "fixture.total-samples");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("totalSamples": 2048)", R"("totalSamples": -1)"),
                                "fixture.total-samples");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("blockPatterns": [[128], [17, 31, 64, 127], [512]])",
                                    R"("blockPatterns": [])"),
                                "fixture.block-pattern");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("blockPatterns": [[128], [17, 31, 64, 127], [512]])",
                                    R"("blockPatterns": [[0]])"),
                                "fixture.block-size");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("blockPatterns": [[128], [17, 31, 64, 127], [512]])",
                                    R"("blockPatterns": [[2049]])"),
                                "fixture.block-size");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("seed": 0)", R"("seed": 1)"),
                                "fixture.seed");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("sample": 512, "sequence": 3)",
                                    R"("sample": 2049, "sequence": 3)"),
                                "fixture.event-sample");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("sample": 512, "sequence": 3)",
                                    R"("sample": 512, "sequence": 1)"),
                                "fixture.event-sequence");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("bytes": [144, 60, 100])", R"("bytes": [144, 60, 999])"),
                                "fixture.midi");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("bytes": [144, 60, 100])", R"("bytes": [144, 255, 255])"),
                                "fixture.midi");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("parameterId": "filterCutoff")",
                                    R"("parameterId": "unknownParameter")"),
                                "fixture.parameter");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("normalizedValue": 0.25)",
                                    R"("normalizedValue": 1e999)"),
                                "fixture.automation-value");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("input": {"kind": "silence"})",
                                    R"("input": {"kind": "dc", "value": 1e300})"),
                                "fixture.input-value");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("input": {"kind": "silence"})",
                                    R"("input": {"kind": "sine", "value": -1e300, "frequencyHz": 440})"),
                                "fixture.input-value");

        const auto legacyText = sourceRoot.getChildFile (fixturePaths.back()).loadFileAsString();
        expectFixtureDiagnostic (sourceRoot, legacyText.replaceFirstOccurrenceOf (
                                    R"({"sample": 0, "sequence": 2, "parameterId": "noiseOnOffSwitch", "normalizedValue": 0.0})",
                                    R"({"sample": 0, "sequence": 2, "parameterId": "noiseOnOffSwitch", "normalizedValue": 1.0})"),
                                "fixture.stochastic-state");

        const auto transition =
            R"({"sample": 512, "sequence": 3, "parameterId": "filterCutoff", "normalizedValue": 0.25})";
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    transition,
                                    juce::String { transition }
                                        + R"(,
    {"sample": 768, "sequence": 5, "parameterId": "noiseOnOffSwitch", "normalizedValue": 1.0})"),
                                "fixture.stochastic-state");
        expectFixtureValid (sourceRoot, validText.replaceFirstOccurrenceOf (
                               transition,
                               juce::String { transition }
                                   + R"(,
    {"sample": 768, "sequence": 6, "parameterId": "oscModSwitch", "normalizedValue": 0.0},
    {"sample": 768, "sequence": 5, "parameterId": "oscModSwitch", "normalizedValue": 1.0})"));
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    transition,
                                    juce::String { transition }
                                        + R"(,
    {"sample": 768, "sequence": 6, "parameterId": "filterModSwitch", "normalizedValue": 1.0},
    {"sample": 768, "sequence": 5, "parameterId": "filterModSwitch", "normalizedValue": 0.0})"),
                                "fixture.stochastic-state");

        const TemporaryDirectory wavRoot { "model-d-reference-wav-negative" };
        expect (wavRoot.isOwned(), "WAV negative root must be owned");
        if (wavRoot.isOwned()) {
            const auto stateDirectory = wavRoot.directory.getChildFile ("Tests/fixtures/state");
            stateDirectory.createDirectory();
            expect (fixtures.front().stateFile.copyFileTo (
                        stateDirectory.getChildFile ("native-default-state-v2.xml")),
                    "WAV negative fixture must copy its state");
            const auto wavFile = wavRoot.directory.getChildFile ("input.wav");
            wavFile.replaceWithData ("not-a-wave", 10);
            auto wavText = validText.replaceFirstOccurrenceOf (
                R"("input": {"kind": "silence"})",
                R"("input": {"kind": "wav", "path": "input.wav", "sha256": "0000000000000000000000000000000000000000000000000000000000000000"})");
            const auto wavFixture = wavRoot.directory.getChildFile ("fixture.json");
            wavFixture.replaceWithText (wavText);
            expectDiagnostic (ReferenceHarness::loadRenderFixture (wavRoot.directory, wavFixture),
                              "fixture.input-hash");
        }

        beginTest ("candidate output refuses an existing directory");
        const TemporaryDirectory existingOutput { "model-d-reference-existing-output" };
        expect (existingOutput.isOwned(), "existing output root must be owned");
        if (existingOutput.isOwned())
            expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), *firstRun.value, existingOutput.directory),
                              "output.exists");

        beginTest ("candidate artifacts are canonical and repeatable");
        const TemporaryDirectory candidateTemporaryRoot { "model-d-reference-candidates" };
        expect (candidateTemporaryRoot.isOwned(), "candidate temporary root must be owned");
        const auto retainedRoot = juce::SystemStats::getEnvironmentVariable (
            "MODEL_D_REFERENCE_CANDIDATE_ROOT", {});
        const auto candidateRoot = retainedRoot.isNotEmpty()
                                     ? juce::File { retainedRoot }
                                     : candidateTemporaryRoot.directory;
        if (retainedRoot.isNotEmpty())
            expect (candidateRoot.createDirectory(), "retained candidate root must be creatable");
        const auto candidateA = candidateRoot.getChildFile ("candidate-a");
        const auto candidateB = candidateRoot.getChildFile ("candidate-b");
        const auto expectRejectedCandidate = [&] (const std::vector<ReferenceHarness::RenderResult>& results,
                                                   const juce::String& name,
                                                   const std::string_view code) {
            const auto directory = candidateRoot.getChildFile (name);
            expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), results, directory),
                              code);
            expect (! directory.exists(), "invalid results must be rejected before output creation");
        };
        auto wrongCount = *firstRun.value;
        wrongCount.pop_back();
        expectRejectedCandidate (wrongCount, "wrong-count", "output.result-count");
        auto wrongPattern = *firstRun.value;
        wrongPattern[1].blockPattern = { 999 };
        expectRejectedCandidate (wrongPattern, "wrong-pattern", "output.pattern");
        auto wrongInvariant = *firstRun.value;
        wrongInvariant[1].sampleRate = 44100.0;
        expectRejectedCandidate (wrongInvariant, "wrong-invariant", "output.invariant");
        auto divergent = *firstRun.value;
        divergent[1].reproducibility.outputHashes["main"] = std::string (64, '0');
        expectRejectedCandidate (divergent, "divergent", "output.divergence");
        auto nonFinite = *firstRun.value;
        nonFinite[1].main[0] = std::numeric_limits<float>::infinity();
        expectRejectedCandidate (nonFinite, "non-finite", "output.non-finite");

        auto emptyProvenance = *firstRun.value;
        for (auto& result : emptyProvenance) {
            result.reproducibility.sourceCommit.clear();
            result.reproducibility.juceCommit.clear();
            result.reproducibility.buildType.clear();
            result.reproducibility.platform.clear();
            result.reproducibility.architecture.clear();
        }
        expectRejectedCandidate (emptyProvenance, "empty-provenance", "output.provenance");
        const auto expectWrongProvenance = [&] (
            std::string ReferenceHarness::ReproducibilityInfo::* member,
            const juce::String& name) {
            auto wrongProvenance = *firstRun.value;
            for (auto& result : wrongProvenance)
                result.reproducibility.*member = "uniformly-forged-provenance";
            expectRejectedCandidate (wrongProvenance, name, "output.provenance");
        };
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::sourceCommit,
                               "wrong-source-commit");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::juceCommit,
                               "wrong-juce-commit");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::buildType,
                               "wrong-build-type");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::platform,
                               "wrong-platform");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::architecture,
                               "wrong-architecture");

        auto wrongFixtureHash = *firstRun.value;
        for (auto& result : wrongFixtureHash)
            result.reproducibility.fixtureSha256 = std::string (64, '0');
        expectRejectedCandidate (wrongFixtureHash, "wrong-fixture-hash", "output.invariant");
        auto wrongSeed = *firstRun.value;
        for (auto& result : wrongSeed)
            result.reproducibility.seed = 1;
        expectRejectedCandidate (wrongSeed, "wrong-seed", "output.invariant");

        auto missingStateHash = *firstRun.value;
        for (auto& result : missingStateHash)
            result.reproducibility.inputHashes.clear();
        expectRejectedCandidate (missingStateHash, "missing-state-hash", "output.input-hash");
        auto wrongStateHash = *firstRun.value;
        for (auto& result : wrongStateHash)
            result.reproducibility.inputHashes = { { "state", std::string (64, '0') } };
        expectRejectedCandidate (wrongStateHash, "wrong-state-hash", "output.input-hash");
        auto unexpectedInputHash = *firstRun.value;
        for (auto& result : unexpectedInputHash)
            result.reproducibility.inputHashes.emplace ("audio", std::string (64, '0'));
        expectRejectedCandidate (unexpectedInputHash, "unexpected-input-hash", "output.input-hash");

        auto wavFixture = fixtures.front();
        wavFixture.input.kind = ReferenceHarness::InputKind::wav;
        wavFixture.input.sha256 = std::string (64, 'a');
        auto missingWavHash = *firstRun.value;
        for (auto& result : missingWavHash)
            result.reproducibility.inputHashes = {
                { "state", fixtures.front().stateSha256 },
            };
        const auto missingWavDirectory = candidateRoot.getChildFile ("missing-wav-hash");
        expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                              wavFixture, missingWavHash, missingWavDirectory),
                          "output.input-hash");
        expect (! missingWavDirectory.exists(),
                "missing WAV input hashes must be rejected before output creation");
        auto wrongWavHash = *firstRun.value;
        for (auto& result : wrongWavHash)
            result.reproducibility.inputHashes = {
                { "audio", std::string (64, '0') },
                { "state", fixtures.front().stateSha256 },
            };
        const auto wrongWavDirectory = candidateRoot.getChildFile ("wrong-wav-hash");
        expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                              wavFixture, wrongWavHash, wrongWavDirectory),
                          "output.input-hash");
        expect (! wrongWavDirectory.exists(),
                "invalid WAV input hashes must be rejected before output creation");
        const auto writtenA = ReferenceHarness::writeCandidateArtifacts (
            fixtures.front(), *firstRun.value, candidateA);
        const auto writtenB = nativeSecondRun.has_value()
                                  && nativeSecondRun->value.has_value()
                            ? ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), *nativeSecondRun->value, candidateB)
                            : ReferenceHarness::LoadResult<std::vector<juce::File>> {};
        expect (writtenA.ok() && writtenB.ok(), "both fresh candidate directories must be written");
        for (const auto name : { "render.json", "control-trace.json", "event-trace.json" })
            expect (candidateA.getChildFile (name).hasIdenticalContentTo (
                        candidateB.getChildFile (name)),
                    juce::String { name } + " must be byte-identical across repeated renders");
        for (const auto name : { "main.wav", "phones.wav" })
            expectEquals (ReferenceHarness::sha256File (candidateA.getChildFile (name)),
                          ReferenceHarness::sha256File (candidateB.getChildFile (name)),
                          juce::String { name } + " SHA-256 must match across repeated renders");
    }

private:
    template <typename T>
    void expectDiagnostic (const ReferenceHarness::LoadResult<T>& result,
                           const std::string_view code)
    {
        expect (! result.ok(), "negative case must fail");
        const auto actual = result.diagnostics.empty() ? "<none>" : result.diagnostics.front().code;
        expect (! result.diagnostics.empty() && actual == code,
                "negative case must report stable diagnostic " + std::string { code }
                    + ", got " + actual);
    }

    void expectFixtureDiagnostic (const juce::File& sourceRoot,
                                  const juce::String& contents,
                                  const std::string_view code)
    {
        const TemporaryDirectory fixtureRoot { "model-d-reference-fixture-negative" };
        expect (fixtureRoot.isOwned(), "negative fixture root must be owned");
        if (! fixtureRoot.isOwned())
            return;
        const auto stateDirectory = fixtureRoot.directory.getChildFile ("Tests/fixtures/state");
        stateDirectory.createDirectory();
        for (const auto stateName : { "native-default-state-v2.xml",
                                      "migrated-default-state-v2.xml",
                                      "migrated-representative-state-v2.xml" })
            expect (sourceRoot.getChildFile ("Tests/fixtures/state").getChildFile (stateName)
                        .copyFileTo (stateDirectory.getChildFile (stateName)),
                    "negative fixture must copy each referenced state");
        const auto fixtureFile = fixtureRoot.directory.getChildFile ("fixture.json");
        fixtureFile.replaceWithText (contents);
        expectDiagnostic (ReferenceHarness::loadRenderFixture (fixtureRoot.directory, fixtureFile), code);
    }

    void expectFixtureValid (const juce::File& sourceRoot, const juce::String& contents)
    {
        const TemporaryDirectory fixtureRoot { "model-d-reference-fixture-positive" };
        expect (fixtureRoot.isOwned(), "positive fixture root must be owned");
        if (! fixtureRoot.isOwned())
            return;
        const auto stateDirectory = fixtureRoot.directory.getChildFile ("Tests/fixtures/state");
        stateDirectory.createDirectory();
        for (const auto stateName : { "native-default-state-v2.xml",
                                      "migrated-default-state-v2.xml",
                                      "migrated-representative-state-v2.xml" })
            expect (sourceRoot.getChildFile ("Tests/fixtures/state").getChildFile (stateName)
                        .copyFileTo (stateDirectory.getChildFile (stateName)),
                    "positive fixture must copy each referenced state");
        const auto fixtureFile = fixtureRoot.directory.getChildFile ("fixture.json");
        fixtureFile.replaceWithText (contents);
        expect (ReferenceHarness::loadRenderFixture (fixtureRoot.directory, fixtureFile).ok(),
                "same-sample enable then disable must leave stochastic processing off");
    }
};

ReferenceRendererTest referenceRendererTest;

} // namespace

int main (int argc, char** argv)
{
    juce::UnitTestRunner runner;
    if (argc == 2)
        runner.runTestsInCategory (argv[1]);
    else
        runner.runAllTests();
    for (int index = 0; index < runner.getNumResults(); ++index) {
        if (const auto* result = runner.getResult (index); result != nullptr && result->failures != 0)
            return 1;
    }
    return 0;
}
