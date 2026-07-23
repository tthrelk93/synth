#include "ReferenceData.h"
#include "OfflineRenderer.h"
#include "Acceptance.h"
#include "AnalyzerRegistry.h"
#include "RequirementReporter.h"
#include "SourceIdentity.h"
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

        beginTest ("isolated Git identities reject dirty builds, dirty trees, and stale heads");
        const TemporaryDirectory gitRoot { "model-d-source-identity" };
        expect (gitRoot.isOwned(), "source identity Git root must be created atomically");
        if (! gitRoot.isOwned())
            return;
        const auto runGit = [&] (std::initializer_list<const char*> arguments) {
            juce::StringArray command;
            const auto executable = ReferenceHarness::buildGitExecutable();
            command.add (juce::String::fromUTF8 (
                executable.data(), static_cast<int> (executable.size())));
            command.add ("-C");
            command.add (gitRoot.directory.getFullPathName());
            for (const auto* argument : arguments)
                command.add (argument);
            juce::ChildProcess process;
            if (! process.start (command, juce::ChildProcess::wantStdOut
                                          | juce::ChildProcess::wantStdErr))
                return false;
            process.readAllProcessOutput();
            return process.getExitCode() == 0;
        };
        const auto tracked = gitRoot.directory.getChildFile ("tracked.txt");
        expect (runGit ({ "init", "--initial-branch=main" })
                    && runGit ({ "config", "user.name", "Model D Contract" })
                    && runGit ({ "config", "user.email", "model-d@example.invalid" })
                    && tracked.replaceWithText ("clean\n", false, false, "\n")
                    && runGit ({ "add", "tracked.txt" })
                    && runGit ({ "commit", "-m", "clean identity" }),
                "isolated source identity repository must initialize and commit");
        const auto cleanIdentity = ReferenceHarness::inspectSourceIdentity (
            gitRoot.directory, ReferenceHarness::buildGitExecutable());
        expect (cleanIdentity.ok() && ! cleanIdentity.value->dirty,
                "clean repository identity must inspect as clean");
        if (! cleanIdentity.value.has_value())
            return;
        expect (ReferenceHarness::validateAuthoritativeSourceIdentity (
                    *cleanIdentity.value, *cleanIdentity.value).ok(),
                "matching clean build/current identities must pass");

        auto malformedBuiltContent = *cleanIdentity.value;
        malformedBuiltContent.content = std::string (64, 'g');
        expectDiagnostic (ReferenceHarness::validateAuthoritativeSourceIdentity (
                              malformedBuiltContent, *cleanIdentity.value),
                          "source.identity-built");

        auto malformedCurrentContent = *cleanIdentity.value;
        malformedCurrentContent.content = std::string (64, 'A');
        expectDiagnostic (ReferenceHarness::validateAuthoritativeSourceIdentity (
                              *cleanIdentity.value, malformedCurrentContent),
                          "source.identity-current");

        expect (tracked.replaceWithText ("dirty build bytes\n", false, false, "\n"),
                "dirty-build fixture must change tracked bytes");
        const auto dirtyBuildIdentity = ReferenceHarness::inspectSourceIdentity (
            gitRoot.directory, ReferenceHarness::buildGitExecutable());
        expect (dirtyBuildIdentity.ok() && dirtyBuildIdentity.value->dirty,
                "dirty-build identity must record dirty tracked bytes");
        expect (runGit ({ "checkout", "--", "tracked.txt" }),
                "dirty-build fixture must restore its tracked file");
        const auto restoredIdentity = ReferenceHarness::inspectSourceIdentity (
            gitRoot.directory, ReferenceHarness::buildGitExecutable());
        if (dirtyBuildIdentity.value.has_value() && restoredIdentity.value.has_value())
            expectDiagnostic (ReferenceHarness::validateAuthoritativeSourceIdentity (
                                  *dirtyBuildIdentity.value, *restoredIdentity.value),
                              "source.identity-built-dirty");

        expect (tracked.replaceWithText ("dirty current bytes\n", false, false, "\n"),
                "current-dirty fixture must change tracked bytes");
        const auto dirtyCurrentIdentity = ReferenceHarness::inspectSourceIdentity (
            gitRoot.directory, ReferenceHarness::buildGitExecutable());
        if (dirtyCurrentIdentity.value.has_value())
            expectDiagnostic (ReferenceHarness::validateAuthoritativeSourceIdentity (
                                  *cleanIdentity.value, *dirtyCurrentIdentity.value),
                              "source.identity-current-dirty");
        expect (runGit ({ "checkout", "--", "tracked.txt" }),
                "current-dirty fixture must restore its tracked file");

        expect (tracked.replaceWithText ("new committed bytes\n", false, false, "\n")
                    && runGit ({ "add", "tracked.txt" })
                    && runGit ({ "commit", "-m", "new head" }),
                "stale-head fixture must create a successor commit");
        const auto successorIdentity = ReferenceHarness::inspectSourceIdentity (
            gitRoot.directory, ReferenceHarness::buildGitExecutable());
        expect (successorIdentity.ok() && ! successorIdentity.value->dirty,
                "successor identity must inspect as clean");
        if (successorIdentity.value.has_value())
            expectDiagnostic (ReferenceHarness::validateAuthoritativeSourceIdentity (
                                  *cleanIdentity.value, *successorIdentity.value),
                              "source.identity-stale");

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
        const auto expectRegistryDescriptorDrift = [&] (const juce::String& needle,
                                                         const juce::String& replacement) {
            expectSemanticProbe (
                sourceRoot,
                "Tests/fixtures/parameters/parameter-registry-v2.json",
                "2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284",
                needle, replacement, "semantic.registry");
        };
        expectRegistryDescriptorDrift ("\"index\":0", "\"index\":48");
        expectRegistryDescriptorDrift (
            "\"semantic_key\":\"oscillator_1_waveform\"",
            "\"semantic_key\":\"drifted_semantic_key\"");
        expectRegistryDescriptorDrift ("\"version_hint\":0", "\"version_hint\":2");
        expectRegistryDescriptorDrift (
            "\"display_name\":\"Oscillator 1 Waveform\"",
            "\"display_name\":\"Drifted Display Name\"");
        expectRegistryDescriptorDrift ("\"short_label\":\"\"",
                                       "\"short_label\":\"Osc 1\"");
        expectRegistryDescriptorDrift ("\"unit_key\":\"choice\"",
                                       "\"unit_key\":\"normalized\"");
        expectRegistryDescriptorDrift ("\"kind\":\"choice\"", "\"kind\":\"float\"");
        expectRegistryDescriptorDrift ("\"range_start\":0.0", "\"range_start\":0.25");
        expectRegistryDescriptorDrift ("\"range_end\":5.0", "\"range_end\":4.0");
        expectRegistryDescriptorDrift ("\"range_interval\":1.0",
                                       "\"range_interval\":0.5");
        expectRegistryDescriptorDrift ("\"range_skew\":1.0", "\"range_skew\":2.0");
        expectRegistryDescriptorDrift ("\"symmetric_skew\":false",
                                       "\"symmetric_skew\":true");
        expectRegistryDescriptorDrift ("\"physical_default\":0.0",
                                       "\"physical_default\":1.0");
        expectRegistryDescriptorDrift (
            "\"choice_values\":[\"Triangle\",\"Sharktooth\"",
            "\"choice_values\":[\"Sharktooth\",\"Triangle\"");
        expectRegistryDescriptorDrift ("\"mapping_key\":\"indexedChoice\"",
                                       "\"mapping_key\":\"linear\"");
        expectRegistryDescriptorDrift ("\"automatable\":true",
                                       "\"automatable\":false");
        expectRegistryDescriptorDrift ("\"smoothing_class\":\"none\"",
                                       "\"smoothing_class\":\"gainControl\"");
        expectRegistryDescriptorDrift ("\"persistence_scope\":\"apvtsState\"",
                                       "\"persistence_scope\":\"unspecified\"");
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
        expectMetric (five, "first-change-sample", "samples", 10.0);
        expectMetric (five, "settled-sample", "samples", 249.0);
        expectMetric (five, "monotonic", "boolean", 1.0);

        const auto tenMs = makeRamp (10, 480, false);
        const auto ten = registry.analyze (
            "control.step.v1", AnalysisRequest { .eventSample = 10, .durationSamples = 480,
                                                   .start = 0.0, .target = 1.0,
                                                   .control = tenMs });
        expectMetric (ten, "first-change-sample", "samples", 10.0);
        expectMetric (ten, "settled-sample", "samples", 489.0);

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

        const std::array<double, 8> priorSameKeyChanges {
            0.4, 0.9, 0.1, 0.4, 0.6, 0.8, 0.8, 0.8,
        };
        const auto eventLocal = registry.analyze (
            "control.step.v1",
            AnalysisRequest { .eventSample = 4, .durationSamples = 2,
                              .start = 0.4, .target = 0.8,
                              .control = priorSameKeyChanges });
        expectMetric (eventLocal, "first-change-sample", "samples", 4.0);
        expectMetric (eventLocal, "settled-sample", "samples", 5.0);
        expectMetric (eventLocal, "monotonic", "boolean", 1.0);
        expectMetric (eventLocal, "overshoot", "normalized", 0.0);
        expectMetric (eventLocal, "maximum-per-sample-movement",
                      "normalized/sample", 0.2);
        if (five.value.has_value())
            expect (five.value->front().settings == std::map<std::string, std::string> {
                        { "allowance", "8*epsilon*max(1,travel)" },
                        { "first-change", "first sample at/after event outside start allowance" },
                        { "ideal-increment", "travel/duration-or-travel-for-zero-duration" },
                        { "ramp-origin", "first ideal increment occurs at event sample" },
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

        beginTest ("fixture analysis deinterleaves the requested audio channel");
        RenderFixture stereoFixture;
        stereoFixture.id = "stereo-click";
        stereoFixture.fixtureRelativePath = "Tests/reference/fixtures/stereo-click.json";
        stereoFixture.config.sampleRate = 48000.0;
        stereoFixture.config.totalSamples = 4;
        stereoFixture.config.blockPatterns = { { 4 } };
        FixtureAnalysisRequest rightClick;
        rightClick.id = "right-click";
        rightClick.version = 1;
        rightClick.analyzer = { "audio.click.v1", 1 };
        rightClick.metric = "maximum-first-difference";
        rightClick.inputKind = AnalysisInputKind::audio;
        rightClick.audioTap = AudioTap::main;
        rightClick.channel = 1;
        rightClick.eventSample = 1;
        rightClick.windowSamples = 2;
        auto leftClick = rightClick;
        leftClick.id = "left-click";
        leftClick.channel = 0;
        stereoFixture.analysisRequests = { rightClick, leftClick };
        RenderResult stereoRender;
        stereoRender.sampleRate = 48000.0;
        stereoRender.mainChannels = 2;
        stereoRender.phonesChannels = 2;
        stereoRender.main = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        stereoRender.phones = stereoRender.main;
        stereoRender.blockPattern = { 4 };
        const std::array stereoResults { stereoRender };
        const auto stereoMetrics = analyzeFixtureMetrics (stereoFixture, stereoResults);
        expect (stereoMetrics.ok() && stereoMetrics.value->size() == 2,
                "both channel requests must produce one selected metric");
        if (stereoMetrics.value.has_value() && stereoMetrics.value->size() == 2) {
            expectWithinAbsoluteError ((*stereoMetrics.value)[0].metric.value, 0.0, 0.0,
                                       "the quiet right channel must have no click");
            expectWithinAbsoluteError ((*stereoMetrics.value)[1].metric.value, 1.0, 0.0,
                                       "the clicked left channel must retain its transition");
            expect ((*stereoMetrics.value)[0].provenance.requestId == "right-click"
                        && (*stereoMetrics.value)[1].provenance.requestId == "left-click",
                    "metric provenance must retain stable request identity");
        }

        beginTest ("fixture analysis reconstructs dense control samples from exact indices");
        RenderFixture denseFixture;
        denseFixture.id = "dense-control";
        denseFixture.fixtureRelativePath = "Tests/reference/fixtures/dense-control.json";
        denseFixture.config.sampleRate = 48000.0;
        denseFixture.config.totalSamples = 8;
        denseFixture.config.blockPatterns = { { 8 } };
        FixtureAnalysisRequest denseRequest;
        denseRequest.id = "filter-dense-count";
        denseRequest.version = 1;
        denseRequest.analyzer = { "signal.stats.v1", 1 };
        denseRequest.metric = "sample-count";
        denseRequest.inputKind = AnalysisInputKind::control;
        denseRequest.parameterKey = ParameterRegistry::Key::filterCutoff;
        denseRequest.parameterId = "filterCutoff";
        denseRequest.controlDomain = ControlDomain::normalized;
        denseRequest.eventSample = 2;
        denseRequest.windowSamples = 6;
        denseRequest.start = 0.5;
        denseFixture.analysisRequests = { denseRequest };
        RenderResult denseRender;
        denseRender.sampleRate = 48000.0;
        denseRender.mainChannels = 2;
        denseRender.phonesChannels = 2;
        denseRender.main.assign (16, 0.0f);
        denseRender.phones.assign (16, 0.0f);
        denseRender.controlTrace = {
            { 2, ParameterRegistry::Key::filterCutoff, 0.25f, 0.25f },
            { 6, ParameterRegistry::Key::filterCutoff, 0.75f, 0.75f },
        };
        denseRender.blockPattern = { 8 };
        const std::array denseResults { denseRender };
        const auto denseMetrics = analyzeFixtureMetrics (denseFixture, denseResults);
        expect (denseMetrics.ok() && denseMetrics.value->size() == 1,
                "dense control request must produce one selected metric");
        if (denseMetrics.value.has_value() && denseMetrics.value->size() == 1)
            expectWithinAbsoluteError (denseMetrics.value->front().metric.value, 8.0, 0.0,
                                       "sparse points must reconstruct the full sample domain");

        beginTest ("dense control analysis does not replay pre-origin changes over the effective start");
        auto eventLocalFixture = denseFixture;
        eventLocalFixture.id = "event-local-control";
        eventLocalFixture.fixtureRelativePath =
            "Tests/reference/fixtures/event-local-control.json";
        auto eventLocalRequest = denseRequest;
        eventLocalRequest.id = "event-local-minimum";
        eventLocalRequest.metric = "minimum";
        eventLocalRequest.eventSample = 4;
        eventLocalRequest.windowSamples = 4;
        eventLocalRequest.start = 0.4;
        eventLocalFixture.analysisRequests = { eventLocalRequest };
        auto eventLocalRender = denseRender;
        eventLocalRender.controlTrace = {
            { 1, ParameterRegistry::Key::filterCutoff, 0.9f, 0.9f },
            { 2, ParameterRegistry::Key::filterCutoff, 0.1f, 0.1f },
            { 3, ParameterRegistry::Key::filterCutoff, 0.4f, 0.4f },
            { 4, ParameterRegistry::Key::filterCutoff, 0.6f, 0.6f },
            { 5, ParameterRegistry::Key::filterCutoff, 0.8f, 0.8f },
        };
        const std::array eventLocalResults { eventLocalRender };
        const auto eventLocalMetrics = analyzeFixtureMetrics (
            eventLocalFixture, eventLocalResults);
        expect (eventLocalMetrics.ok() && eventLocalMetrics.value->size() == 1,
                "event-local dense control request must produce its selected metric");
        if (eventLocalMetrics.value.has_value() && eventLocalMetrics.value->size() == 1)
            expectWithinAbsoluteError (eventLocalMetrics.value->front().metric.value,
                                       0.4, 1.0e-7,
                                       "pre-origin events must not overwrite the derived effective start");

        beginTest ("finite extreme analyzer inputs never produce non-finite metrics");
        const auto maximumDouble = std::numeric_limits<double>::max();
        const std::array<double, 4> maximumConstant {
            maximumDouble, maximumDouble, maximumDouble, maximumDouble,
        };
        const auto maximumStats = registry.analyze (
            "signal.stats.v1", AnalysisRequest { .control = maximumConstant });
        expect (maximumStats.ok(), "a finite DBL_MAX constant has representable statistics");
        expectMetric (maximumStats, "mean", "amplitude", maximumDouble);
        expectMetric (maximumStats, "rms", "amplitude", maximumDouble);
        if (maximumStats.value.has_value()) {
            const auto maximumJson = metricResultsJson (*maximumStats.value);
            expect (! maximumJson.containsIgnoreCase ("nan")
                        && ! maximumJson.containsIgnoreCase ("infinity")
                        && ! maximumJson.containsIgnoreCase ("null"),
                    "representable DBL_MAX statistics must serialize as finite numbers");
        }

        const auto expectOverflowFailure = [&] (
            const LoadResult<std::vector<MetricResult>>& result,
            const juce::String& context) {
            expectDiagnostic (result, "analyzer.overflow");
            expect (result.value.has_value(), context + " must retain deterministic failure metrics");
            if (! result.value.has_value())
                return;
            for (const auto& resultMetric : *result.value)
                expect (std::isfinite (resultMetric.value)
                            && std::isfinite (resultMetric.allowance)
                            && ! resultMetric.finite,
                        context + " failure metrics must be finite-valued and marked invalid");
            const auto firstJson = metricResultsJson (*result.value);
            const auto secondJson = metricResultsJson (*result.value);
            expectEquals (firstJson, secondJson, context + " JSON must be deterministic");
            expect (! firstJson.containsIgnoreCase ("nan")
                        && ! firstJson.containsIgnoreCase ("infinity")
                        && ! firstJson.containsIgnoreCase ("null"),
                    context + " JSON must contain no non-finite placeholder");
        };

        const std::array<double, 2> mixedExtremes { maximumDouble, -maximumDouble };
        expectOverflowFailure (
            registry.analyze ("signal.stats.v1",
                              AnalysisRequest { .control = mixedExtremes }),
            "mixed DBL_MAX signal");

        const auto adjacentMaximum = std::nextafter (maximumDouble, 0.0);
        const std::array<double, 4> adjacentControl {
            maximumDouble, maximumDouble, adjacentMaximum, adjacentMaximum,
        };
        const auto adjacentStep = registry.analyze (
            "control.step.v1",
            AnalysisRequest { .eventSample = 2,
                              .start = maximumDouble,
                              .target = adjacentMaximum,
                              .control = adjacentControl });
        expect (adjacentStep.ok(),
                "adjacent finite extreme endpoints have representable control metrics");
        if (adjacentStep.value.has_value())
            for (const auto& resultMetric : *adjacentStep.value)
                expect (std::isfinite (resultMetric.value)
                            && std::isfinite (resultMetric.allowance)
                            && resultMetric.finite,
                        "representable extreme control metrics must remain finite and valid");

        const std::array<double, 2> oppositeEndpointControl {
            -maximumDouble, maximumDouble,
        };
        expectOverflowFailure (
            registry.analyze (
                "control.step.v1",
                AnalysisRequest { .eventSample = 1,
                                  .start = -maximumDouble,
                                  .target = maximumDouble,
                                  .control = oppositeEndpointControl }),
            "opposite extreme control endpoints");
        const std::array<double, 3> extremeMovementControl {
            0.0, maximumDouble, -maximumDouble,
        };
        expectOverflowFailure (
            registry.analyze (
                "control.step.v1",
                AnalysisRequest { .eventSample = 1,
                                  .start = 0.0,
                                  .target = 0.0,
                                  .control = extremeMovementControl }),
            "extreme adjacent control samples");

        const auto maximumFloat = std::numeric_limits<float>::max();
        const std::array<float, 3> extremeAudio {
            maximumFloat, -maximumFloat, maximumFloat,
        };
        const auto extremeClick = registry.analyze (
            "audio.click.v1",
            AnalysisRequest { .eventSample = 1,
                              .durationSamples = std::numeric_limits<std::uint64_t>::max(),
                              .audio = extremeAudio });
        expect (extremeClick.ok(),
                "finite float extremes and a saturated click window have representable metrics");
        if (extremeClick.value.has_value())
            for (const auto& resultMetric : *extremeClick.value)
                expect (std::isfinite (resultMetric.value)
                            && std::isfinite (resultMetric.allowance)
                            && resultMetric.finite,
                        "extreme audio metrics must remain finite and valid");
        expectMetric (extremeClick, "maximum-first-difference", "amplitude/sample",
                      2.0 * static_cast<double> (maximumFloat));

        const std::vector<MetricResult> forgedNonFinite {
            { { "signal.stats.v1", 1 }, "rms", "amplitude",
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::quiet_NaN(), true, {} },
        };
        const auto sanitizedJson = metricResultsJson (forgedNonFinite);
        expect (! sanitizedJson.containsIgnoreCase ("nan")
                    && ! sanitizedJson.containsIgnoreCase ("infinity")
                    && ! sanitizedJson.containsIgnoreCase ("null")
                    && sanitizedJson.contains (R"("finite":false)"),
                "serialization must enforce finite numeric fields and invalidate bad metrics");

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
        const std::array<double, 3> finiteControl { 0.0, 1.0, 1.0 };
        for (const auto& invalidRequest : {
                 AnalysisRequest { .eventSample = 1,
                                   .start = std::numeric_limits<double>::infinity(),
                                   .target = 1.0,
                                   .control = finiteControl },
                 AnalysisRequest { .eventSample = 1,
                                   .start = 0.0,
                                   .target = std::numeric_limits<double>::quiet_NaN(),
                                   .control = finiteControl },
             }) {
            const auto invalidControl = registry.analyze ("control.step.v1", invalidRequest);
            expectDiagnostic (invalidControl, "analyzer.non-finite");
            expect (invalidControl.value.has_value(),
                    "invalid control requests must retain deterministic failure metrics");
            if (! invalidControl.value.has_value())
                continue;
            for (const auto& metric : *invalidControl.value)
                expect (std::isfinite (metric.value) && std::isfinite (metric.allowance),
                        "every failed control metric field must remain finite");
            const auto firstJson = metricResultsJson (*invalidControl.value);
            const auto secondJson = metricResultsJson (*invalidControl.value);
            expectEquals (firstJson, secondJson,
                          "failed control metric JSON must be byte-deterministic");
            expect (! firstJson.containsIgnoreCase ("nan")
                        && ! firstJson.containsIgnoreCase ("infinity")
                        && ! firstJson.containsIgnoreCase ("null"),
                    "failed control metric JSON must not encode non-finite placeholders");
        }

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
        const auto approvedWithOpenEvidence = acceptanceText
            .replaceFirstOccurrenceOf (R"("status": "draft")", R"("status": "approved")")
            .replaceFirstOccurrenceOf (
                R"("published": [])",
                R"("published": [{"id":"published.probe","classification":"published","status":"not-run","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"source":"approved primary source","sourceVersion":"revision 2","page":"1"}])")
            .replaceFirstOccurrenceOf (
                R"("measuredHardware": [])",
                R"("measuredHardware": [{"id":"hardware.probe","classification":"measured-hardware","status":"awaiting-approved-reference","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"referenceStatus":"approved","referenceSet":"approved set","bandArtifact":"band.json","rawSha256":["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],"instrument":"calibrated interface","environment":"controlled studio","captureChain":"synth to interface","repetitionCount":3,"repetitionStatistic":"arithmetic mean","uncertaintyMethod":"sample standard deviation","uncertaintyValue":0.25,"approver":"Hardware Reviewer","approvalDate":"2026-07-22"}])")
            .replaceFirstOccurrenceOf (
                R"("performance": [])",
                R"("performance": [{"id":"performance.probe","classification":"performance","status":"not-run","requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"targetSystem":"approved system","budgetBasis":"approved budget","rationale":"approved rationale","reviewStatus":"approved","reviewer":"Performance Reviewer","reviewDate":"2026-07-22"}])");
        expectAcceptanceDiagnostic (sourceRoot, registry, approvedWithOpenEvidence,
                                    "acceptance.incomplete");
        expectAcceptanceDiagnostic (
            sourceRoot, registry,
            approvedWithOpenEvidence.replaceFirstOccurrenceOf (
                R"("status": "not-run")", R"("status": "fail")"),
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
                                        R"("value": 0.005)", R"("value": 0.006)"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("unit": "seconds")", R"("unit": "samples")"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("metric": "settled-sample")",
                                        R"("metric": "first-change-sample")"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (
            sourceRoot, registry,
            acceptanceText
                .replaceFirstOccurrenceOf (R"("analyzer": "control.step.v1")",
                                           R"("analyzer": "signal.stats.v1")")
                .replaceFirstOccurrenceOf (R"("metric": "settled-sample")",
                                           R"("metric": "sample-count")"),
            "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("requirements": ["PAR-006", "TST-006"])",
                                        R"("requirements": ["PAR-006"])"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (
            sourceRoot, registry,
            acceptanceText.replaceFirstOccurrenceOf (
                R"("id": "par006.gain-control.duration",
      "classification": "derived-software",
      "status": "not-run")",
                R"("id": "par006.gain-control.duration",
      "classification": "derived-software",
      "status": "fail")"),
            "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "5 ms linear-amplitude software safety ramp",
                                        "5 ms normalized-domain ramp"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "10 ms owner-declared control-domain software safety ramp",
                                        "10 ms unspecified ramp"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "+1 sample after ceil(duration * sampleRate)",
                                        "+2 samples after floor(duration * sampleRate)"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "approved exact-step policy", "nonempty but wrong basis"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        "2026-07-22 user-approved design", "unspecified review"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("reviewDate": "2026-07-22")",
                                        R"("reviewDate": "2026-07-23")"),
                                    "acceptance.derived-policy");
        expectAcceptanceDiagnostic (sourceRoot, registry,
                                    acceptanceText.replaceFirstOccurrenceOf (
                                        R"("status": "not-run")", R"("status": "pass")"),
                                    "acceptance.pass-artifact");

        beginTest ("reviewed policies may execute while global approval requires complete evidence");
        const auto artifactPath = juce::String { "Tests/reference/acceptance-v1.json" };
        const auto artifactHash = juce::String { sha256File (acceptanceFile) };
        const auto passArtifact = juce::String {
            R"("status": "pass", "artifactPath": ")" }
                                + artifactPath + R"(", "artifactSha256": ")"
                                + artifactHash + R"(")";
        const auto completeApproved = acceptanceText
            .replaceFirstOccurrenceOf (R"("status": "draft")", R"("status": "approved")")
            .replace (R"("status": "not-run")", passArtifact)
            .replaceFirstOccurrenceOf (
                R"("published": [])",
                juce::String { R"("published": [{"id":"published.probe","classification":"published",)" }
                    + passArtifact
                    + R"(,"requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"source":"approved primary source","sourceVersion":"revision 2","page":"1"}])")
            .replaceFirstOccurrenceOf (
                R"("measuredHardware": [])",
                juce::String { R"("measuredHardware": [{"id":"hardware.probe","classification":"measured-hardware",)" }
                    + passArtifact
                    + R"(,"requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"referenceStatus":"approved","referenceSet":"approved set","bandArtifact":"band.json","rawSha256":["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],"instrument":"calibrated interface","environment":"controlled studio","captureChain":"synth to interface","repetitionCount":3,"repetitionStatistic":"arithmetic mean","uncertaintyMethod":"sample standard deviation","uncertaintyValue":0.25,"approver":"Hardware Reviewer","approvalDate":"2026-07-22"}])")
            .replaceFirstOccurrenceOf (
                R"("performance": [])",
                juce::String { R"("performance": [{"id":"performance.probe","classification":"performance",)" }
                    + passArtifact
                    + R"(,"requirements":["TST-006"],"analyzer":"signal.stats.v1","analyzerVersion":1,"metric":"sample-count","unit":"count","value":1,"targetSystem":"approved system","budgetBasis":"approved budget","rationale":"approved rationale","reviewStatus":"approved","reviewer":"Performance Reviewer","reviewDate":"2026-07-22"}])");
        const TemporaryDirectory approvedRoot { "model-d-acceptance-approved" };
        expect (approvedRoot.isOwned(), "approved manifest root must be owned");
        if (! approvedRoot.isOwned())
            return;
        const auto approvedFile = approvedRoot.directory.getChildFile ("acceptance.json");
        approvedFile.replaceWithText (completeApproved);
        const auto approvedManifest = loadAcceptanceManifest (sourceRoot, approvedFile, registry);
        expect (approvedManifest.ok(),
                "all five complete evidence sections may produce a globally approved manifest");
        if (approvedManifest.value.has_value()) {
            expect (approvedManifest.value->published.front().published.has_value()
                        && approvedManifest.value->published.front().published->sourceVersion
                               == "revision 2",
                    "published provenance must load into its typed record");
            expect (approvedManifest.value->measuredHardware.front().measuredHardware.has_value()
                        && approvedManifest.value->measuredHardware.front()
                               .measuredHardware->rawSha256.size() == 1,
                    "hardware provenance must load typed raw capture hashes");
            expect (approvedManifest.value->performance.front().performance.has_value()
                        && approvedManifest.value->performance.front().performance->reviewStatus
                               == "approved",
                    "performance provenance must load its typed review record");
        }

        const auto expectTypedProvenanceFailure = [&] (const juce::String& needle,
                                                       const juce::String& replacement,
                                                       const std::string_view code) {
            expectAcceptanceDiagnostic (
                sourceRoot, registry,
                completeApproved.replaceFirstOccurrenceOf (needle, replacement), code);
        };
        for (const auto& [needle, replacement] : std::array<std::pair<const char*, const char*>, 3> {{
                 { R"("source":"approved primary source",)", R"("source":"",)" },
                 { R"("sourceVersion":"revision 2",)", R"("sourceVersion":"",)" },
                 { R"("page":"1")", R"("page":"")" },
             }})
            expectTypedProvenanceFailure (needle, replacement, "acceptance.source");
        for (const auto& [needle, replacement] : std::array<std::pair<const char*, const char*>, 14> {{
                 { R"("referenceStatus":"approved",)", R"("referenceStatus":"pending",)" },
                 { R"("referenceSet":"approved set",)", R"("referenceSet":"",)" },
                 { R"("bandArtifact":"band.json",)", R"("bandArtifact":"",)" },
                 { R"("rawSha256":["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],)", R"("rawSha256":[],)" },
                 { R"("instrument":"calibrated interface",)", R"("instrument":"",)" },
                 { R"("environment":"controlled studio",)", R"("environment":"",)" },
                 { R"("captureChain":"synth to interface",)", R"("captureChain":"",)" },
                 { R"("repetitionCount":3,)", R"("repetitionCount":0,)" },
                 { R"("repetitionStatistic":"arithmetic mean",)", R"("repetitionStatistic":"",)" },
                 { R"("uncertaintyMethod":"sample standard deviation",)", R"("uncertaintyMethod":"",)" },
                 { R"("uncertaintyValue":0.25,)", R"("uncertaintyValue":-0.25,)" },
                 { R"("approver":"Hardware Reviewer",)", R"("approver":"",)" },
                 { R"("approvalDate":"2026-07-22")", R"("approvalDate":"2026-02-30")" },
                 { R"("rawSha256":["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],)", R"("rawSha256":["ABC"],)" },
             }})
            expectTypedProvenanceFailure (needle, replacement, "acceptance.reference");
        for (const auto& [needle, replacement] : std::array<std::pair<const char*, const char*>, 6> {{
                 { R"("targetSystem":"approved system",)", R"("targetSystem":"",)" },
                 { R"("budgetBasis":"approved budget",)", R"("budgetBasis":"",)" },
                 { R"("rationale":"approved rationale",)", R"("rationale":"",)" },
                 { R"("reviewStatus":"approved",)", R"("reviewStatus":"pending",)" },
                 { R"("reviewer":"Performance Reviewer",)", R"("reviewer":"",)" },
                 { R"("reviewDate":"2026-07-22")", R"("reviewDate":"not-a-date")" },
             }})
            expectTypedProvenanceFailure (needle, replacement, "acceptance.performance");
        expectAcceptanceDiagnostic (
            sourceRoot, registry,
            completeApproved.replaceFirstOccurrenceOf (passArtifact, R"("status": "not-run")"),
            "acceptance.incomplete");
        expectAcceptanceDiagnostic (
            sourceRoot, registry,
            completeApproved.replaceFirstOccurrenceOf (
                R"("id": "par006.gain-control.duration",
      "classification": "derived-software",
      "status": "pass")",
                R"("id": "par006.gain-control.duration",
      "classification": "derived-software",
      "status": "fail")"),
            "acceptance.derived-policy");

        beginTest ("seven templates expand from the live registry without a duplicate ID inventory");
        const auto index = loadFixtureIndex (
            sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
        expect (index.ok(), "fixture index with smoothing templates must validate");
        if (! index.value.has_value())
            return;
        expectEquals (static_cast<int> (index.value->smoothingFixtures.size()), 7);
        if (! manifest.value.has_value())
            return;
        const auto expanded = expandSmoothingFixtures (
            sourceRoot, *index.value, *manifest.value);
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
                            && smoothingCase.status == Status::notRun,
                        "none must require evidence before its exact-step policy can pass");
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
        expectSmoothingPolicyDiagnostic (
            sourceRoot, *manifest.value,
            "Tests/reference/fixtures/par-006/gain-control-step-v1.json",
            "par006.gain-control.duration", "par006.control.duration");
        expectSmoothingPolicyDiagnostic (
            sourceRoot, *manifest.value,
            "Tests/reference/fixtures/par-006/control-step-v1.json",
            "par006.settling.allowance", "par006.none.intermediate");
        expectSmoothingPolicyDiagnostic (
            sourceRoot, *manifest.value,
            "Tests/reference/fixtures/par-006/dedicated-pitch-step-v1.json",
            "  \"analyzer\": \"control.step.v1\",",
            "  \"policyId\": \"par006.gain-control.duration\",\n"
            "  \"analyzer\": \"control.step.v1\",");

        beginTest ("declarations alone cannot produce an acceptance pass");
        if (! manifest.value.has_value())
            return;
        const auto declarationsOnly = evaluateAcceptance (
            *manifest.value, *expanded.value, {}, registry);
        expect (declarationsOnly.ok(), "draft declarations must evaluate as unavailable evidence");
        int declarationPassCount = 0;
        for (const auto& result : *declarationsOnly.value)
            declarationPassCount += result.status == Status::pass ? 1 : 0;
        expectEquals (declarationPassCount, 0,
                      "fixture declarations and fixture hashes must never fabricate a pass");

        beginTest ("a declared manifest pass is not executable evidence");
        const auto declaredPassText = acceptanceText.replaceFirstOccurrenceOf (
            R"("status": "not-run",
      "requirements": ["PAR-001", "TST-006"])",
            juce::String { R"("status": "pass",
      "artifactPath": ")" }
                + artifactPath + R"(",
      "artifactSha256": ")" + artifactHash + R"(",
      "requirements": ["PAR-001", "TST-006"])");
        const TemporaryDirectory declaredPassRoot { "model-d-acceptance-declared-pass" };
        expect (declaredPassRoot.isOwned(), "declared-pass manifest root must be owned");
        if (! declaredPassRoot.isOwned())
            return;
        const auto declaredPassFile = declaredPassRoot.directory.getChildFile ("acceptance.json");
        declaredPassFile.replaceWithText (declaredPassText);
        const auto declaredPassManifest = loadAcceptanceManifest (
            sourceRoot, declaredPassFile, registry);
        expect (declaredPassManifest.ok(), "draft declared-pass probe must validate structurally");
        if (declaredPassManifest.value.has_value()) {
            const auto declarationEvaluation = evaluateAcceptance (
                *declaredPassManifest.value, *expanded.value, {}, registry);
            expect (declarationEvaluation.ok(), "declared-pass probe must evaluate deterministically");
            if (declarationEvaluation.value.has_value()) {
                const auto* hardGate = findGate (*declarationEvaluation.value,
                                                 "hard.registry.count");
                expect (hardGate != nullptr && hardGate->status == Status::notRun
                            && hardGate->reasonCode == "acceptance.evidence-missing"
                            && ! hardGate->metric.has_value()
                            && hardGate->artifactPath.empty()
                            && hardGate->artifactSha256.empty(),
                        "a declaration-only pass must become evidence-missing without a pass claim");
                const auto metriclessPasses = std::count_if (
                    declarationEvaluation.value->begin(), declarationEvaluation.value->end(),
                    [] (const auto& gate) {
                        return gate.status == Status::pass && ! gate.metric.has_value();
                    });
                expectEquals (static_cast<int> (metriclessPasses), 0,
                              "evaluation must emit zero metricless passes");
            }
        }

        beginTest ("only a bound Task 2 candidate trace can pass");
        const TemporaryDirectory candidateRoot { "model-d-acceptance-evidence" };
        expect (candidateRoot.isOwned(), "evidence candidate root must be owned");
        if (! candidateRoot.isOwned())
            return;
        const auto stateFile = candidateRoot.directory.getChildFile ("state.xml");
        expect (sourceRoot.getChildFile ("Tests/fixtures/state/native-default-state-v2.xml")
                    .copyFileTo (stateFile),
                "real smoothing evidence must copy the validated v2 state");
        const auto fixtureFile = candidateRoot.directory.getChildFile ("fixture.json");
        fixtureFile.replaceWithText (
            juce::String { R"({"schema":"model-d.render-fixture.v1","id":"none-step-evidence","purpose":"Exercise acceptance binding for a generated no-smoothing control trace.","reviewStatus":"foundation-reviewed","state":{"kind":"hostState","path":"state.xml","sha256":")" }
            + sha256File (stateFile)
            + R"(","version":2,"contourContract":"canonicalContours"},"render":{"sampleRate":48000,"totalSamples":512,"seed":0,"blockPatterns":[[64],[17,31]]},"automation":[{"sample":0,"sequence":0,"parameterId":"a440HzOnOff","normalizedValue":0.0},{"sample":256,"sequence":1,"parameterId":"a440HzOnOff","normalizedValue":1.0}],"midi":[],"input":{"kind":"silence"},"analysisRequests":[{"requestId":"main-left-count","version":1,"analyzer":{"id":"signal.stats.v1","version":1},"metric":"sample-count","input":{"kind":"audio","tap":"main","channel":0},"event":{"originSample":0,"windowSamples":512}}],"requirements":["PAR-006"]})");
        const auto evidenceFixture = loadRenderFixture (candidateRoot.directory, fixtureFile);
        expect (evidenceFixture.ok(), "smoothing evidence fixture must load through Task 2");
        if (! evidenceFixture.value.has_value())
            return;
        const auto realRenders = renderFixture (*evidenceFixture.value);
        expect (realRenders.ok(), "smoothing evidence must render through Task 2");
        if (! realRenders.value.has_value())
            return;
        expect (realRenders.value->front().reproducibility.outputHashes.contains ("control")
                    && realRenders.value->front().reproducibility.outputHashes.contains ("event")
                    && ! realRenders.value->front().reproducibility.outputHashes.contains (
                        "control-trace.json"),
                "real Task 2 render hashes must retain their producer-defined internal keys");
        const auto candidateRendersRoot = candidateRoot.directory.getChildFile ("candidate");
        expect (candidateRendersRoot.createDirectory(),
                "smoothing candidate renders root must be created");
        const auto written = writeCandidateArtifacts (
            *evidenceFixture.value, *realRenders.value, candidateRendersRoot);
        expect (written.ok(), "smoothing evidence must use Task 2 candidate artifacts");
        if (! written.value.has_value())
            return;
        const auto candidateDirectory = candidateRendersRoot.getChildFile (
            evidenceFixture.value->id);
        const auto candidateTrace = candidateDirectory.getChildFile ("control-trace.json");
        const auto candidateHash = sha256File (candidateTrace);
        const auto noneCaseFound = std::find_if (
            expanded.value->begin(), expanded.value->end(), [] (const auto& smoothingCase) {
                return smoothingCase.parameterId == "a440HzOnOff";
            });
        expect (noneCaseFound != expanded.value->end()
                    && noneCaseFound->smoothingClass == ParameterRegistry::SmoothingClass::none,
                "real evidence parameter must use class none");
        if (noneCaseFound == expanded.value->end())
            return;
        const auto& noneCase = *noneCaseFound;

        SmoothingEvidence validEvidence;
        validEvidence.parameterId = noneCase.parameterId;
        validEvidence.fixture = *evidenceFixture.value;
        validEvidence.render = realRenders.value->front();
        validEvidence.candidateDirectory = candidateDirectory;

        const auto evaluated = evaluateAcceptance (
            *manifest.value, *expanded.value,
            std::span<const SmoothingEvidence> { &validEvidence, 1 }, registry);
        expect (evaluated.ok(), "draft F0 acceptance must evaluate honestly");
        int passCount = 0;
        int awaitingCount = 0;
        for (const auto& result : *evaluated.value) {
            passCount += result.status == Status::pass ? 1 : 0;
            awaitingCount += result.status == Status::awaitingApprovedReference ? 1 : 0;
            if (result.status == Status::pass)
                expect (! result.artifactSha256.empty(), "a pass must carry an artifact hash");
        }
        expectEquals (passCount, 1, "only the one supplied valid none case may pass");
        expect (awaitingCount > 0, "missing hardware references must remain awaiting approval");
        const auto passed = findGate (*evaluated.value,
                                      "par006." + noneCase.parameterId + ".control");
        expect (passed != nullptr && passed->metric.has_value()
                    && passed->artifactPath == candidateTrace.getFullPathName().toStdString()
                    && passed->artifactSha256 == candidateHash,
                "a pass must carry its executed metric and real candidate artifact");

        beginTest ("invalid traces, analyzer metrics, and hashes cannot pass");
        const auto expectEvidenceFailure = [&] (SmoothingEvidence evidence,
                                                const std::string_view reason) {
            const auto result = evaluateAcceptance (
                *manifest.value, *expanded.value,
                std::span<const SmoothingEvidence> { &evidence, 1 }, registry);
            expect (result.ok(), "invalid supplied evidence must produce an honest gate result");
            if (! result.value.has_value())
                return;
            const auto* gate = findGate (*result.value,
                                         "par006." + noneCase.parameterId + ".control");
            expect (gate != nullptr && gate->status == Status::fail
                        && gate->reasonCode == reason
                        && gate->artifactPath.empty() && gate->artifactSha256.empty(),
                    "invalid evidence must fail without an artifact pass claim");
        };
        auto mismatchedRender = validEvidence;
        mismatchedRender.render.controlTrace.back().normalizedValue = 0.5f;
        expectEvidenceFailure (std::move (mismatchedRender), "smoothing.trace-mismatch");
        auto mismatchedEvent = validEvidence;
        mismatchedEvent.render.eventTrace.back() += ":mismatch";
        expectEvidenceFailure (std::move (mismatchedEvent), "smoothing.trace-mismatch");
        auto mismatchedProvenance = validEvidence;
        mismatchedProvenance.render.reproducibility.sourceCommit = "mismatch";
        expectEvidenceFailure (std::move (mismatchedProvenance), "smoothing.trace-mismatch");
        auto mismatchedSourceTree = validEvidence;
        mismatchedSourceTree.render.reproducibility.sourceTree = std::string (40, '0');
        expectEvidenceFailure (std::move (mismatchedSourceTree), "smoothing.trace-mismatch");
        auto mismatchedSourceContent = validEvidence;
        mismatchedSourceContent.render.reproducibility.sourceContent = std::string (64, '0');
        expectEvidenceFailure (std::move (mismatchedSourceContent), "smoothing.trace-mismatch");
        auto mismatchedSourceDirty = validEvidence;
        mismatchedSourceDirty.render.reproducibility.sourceDirty =
            ! mismatchedSourceDirty.render.reproducibility.sourceDirty;
        expectEvidenceFailure (std::move (mismatchedSourceDirty), "smoothing.trace-mismatch");
        auto mismatchedCompilerId = validEvidence;
        mismatchedCompilerId.render.reproducibility.compilerId = "forged-compiler";
        expectEvidenceFailure (std::move (mismatchedCompilerId), "smoothing.trace-mismatch");
        auto mismatchedCompilerVersion = validEvidence;
        mismatchedCompilerVersion.render.reproducibility.compilerVersion = "0.0-forged";
        expectEvidenceFailure (std::move (mismatchedCompilerVersion), "smoothing.trace-mismatch");
        auto mismatchedFixture = validEvidence;
        mismatchedFixture.fixture.id = "mismatch";
        expectEvidenceFailure (std::move (mismatchedFixture), "smoothing.trace-mismatch");

        const auto tamperedDirectory = candidateRoot.directory.getChildFile ("tampered-candidate");
        expect (candidateDirectory.copyDirectoryTo (tamperedDirectory),
                "candidate must be copied for a tamper probe");
        const auto tamperedTrace = tamperedDirectory.getChildFile ("control-trace.json");
        tamperedTrace.replaceWithText (tamperedTrace.loadFileAsString() + " ");
        auto tamperedEvidence = validEvidence;
        tamperedEvidence.candidateDirectory = tamperedDirectory;
        expectEvidenceFailure (std::move (tamperedEvidence), "smoothing.artifact-hash");

        const auto missingDirectory = candidateRoot.directory.getChildFile ("missing-candidate");
        expect (candidateDirectory.copyDirectoryTo (missingDirectory),
                "candidate must be copied for a missing-file probe");
        expect (missingDirectory.getChildFile ("control-trace.json").deleteFile(),
                "missing-file probe must remove the copied control trace");
        auto missingEvidence = validEvidence;
        missingEvidence.candidateDirectory = missingDirectory;
        expectEvidenceFailure (std::move (missingEvidence), "smoothing.artifact-hash");

        const auto missingCompilerDirectory = candidateRoot.directory.getChildFile (
            "missing-compiler-candidate");
        expect (candidateDirectory.copyDirectoryTo (missingCompilerDirectory),
                "candidate must be copied for a missing compiler identity probe");
        const auto missingCompilerRender = missingCompilerDirectory.getChildFile ("render.json");
        juce::var missingCompilerJson;
        expect (juce::JSON::parse (missingCompilerRender.loadFileAsString(),
                                  missingCompilerJson).wasOk(),
                "missing compiler probe render manifest must parse");
        if (auto* missingCompilerRoot = missingCompilerJson.getDynamicObject())
            if (auto* reproducibility = missingCompilerRoot->getProperty (
                    "reproducibility").getDynamicObject())
                reproducibility->removeProperty ("compilerId");
        expect (missingCompilerRender.replaceWithText (
                    canonicalJson (missingCompilerJson), false, false, "\n"),
                "missing compiler probe must rewrite render manifest");
        auto missingCompilerEvidence = validEvidence;
        missingCompilerEvidence.candidateDirectory = missingCompilerDirectory;
        expectEvidenceFailure (std::move (missingCompilerEvidence), "smoothing.artifact-hash");

        const auto forgedCompilerDirectory = candidateRoot.directory.getChildFile (
            "forged-compiler-candidate");
        expect (candidateDirectory.copyDirectoryTo (forgedCompilerDirectory),
                "candidate must be copied for a forged compiler identity probe");
        const auto forgedCompilerRender = forgedCompilerDirectory.getChildFile ("render.json");
        juce::var forgedCompilerJson;
        expect (juce::JSON::parse (forgedCompilerRender.loadFileAsString(),
                                  forgedCompilerJson).wasOk(),
                "forged compiler probe render manifest must parse");
        if (auto* forgedCompilerRoot = forgedCompilerJson.getDynamicObject())
            if (auto* reproducibility = forgedCompilerRoot->getProperty (
                    "reproducibility").getDynamicObject())
                reproducibility->setProperty ("compilerVersion", "0.0-forged");
        expect (forgedCompilerRender.replaceWithText (
                    canonicalJson (forgedCompilerJson), false, false, "\n"),
                "forged compiler probe must rewrite render manifest");
        auto forgedCompilerEvidence = validEvidence;
        forgedCompilerEvidence.candidateDirectory = forgedCompilerDirectory;
        expectEvidenceFailure (std::move (forgedCompilerEvidence), "smoothing.trace-mismatch");
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
        for (size_t offset = 0; offset < durationSamples; ++offset) {
            const auto normalized = static_cast<double> (offset + 1)
                                  / static_cast<double> (durationSamples);
            values[eventSample + offset] = downward ? 1.0 - normalized : normalized;
        }
        std::fill (values.begin() + static_cast<std::ptrdiff_t> (eventSample + durationSamples - 1),
                   values.end(), downward ? 0.0 : 1.0);
        return values;
    }

    static const ReferenceHarness::GateResult* findGate (
        const std::vector<ReferenceHarness::GateResult>& results,
        const std::string_view id)
    {
        const auto found = std::find_if (results.begin(), results.end(), [&] (const auto& result) {
            return result.id == id;
        });
        return found == results.end() ? nullptr : &*found;
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

    void expectSmoothingPolicyDiagnostic (const juce::File& sourceRoot,
                                          const ReferenceHarness::AcceptanceManifest& manifest,
                                          const juce::String& relativePath,
                                          const juce::String& needle,
                                          const juce::String& replacement)
    {
        const TemporaryDirectory probeRoot { "model-d-smoothing-policy-negative" };
        expect (probeRoot.isOwned(), "smoothing policy probe root must be owned");
        if (! probeRoot.isOwned())
            return;
        for (const auto path : frozenFixturePaths) {
            const auto source = sourceRoot.getChildFile (path);
            const auto destination = probeRoot.directory.getChildFile (path);
            destination.getParentDirectory().createDirectory();
            expect (source.copyFileTo (destination),
                    "smoothing policy probe must copy every indexed fixture");
        }
        const auto fixture = probeRoot.directory.getChildFile (relativePath);
        fixture.replaceWithText (
            fixture.loadFileAsString().replaceFirstOccurrenceOf (needle, replacement));
        const auto indexPath = "Tests/reference/fixture-index-v1.json";
        const auto indexFile = probeRoot.directory.getChildFile (indexPath);
        indexFile.getParentDirectory().createDirectory();
        const auto originalHash = ReferenceHarness::sha256File (
            sourceRoot.getChildFile (relativePath));
        indexFile.replaceWithText (
            sourceRoot.getChildFile (indexPath).loadFileAsString().replaceFirstOccurrenceOf (
                originalHash, ReferenceHarness::sha256File (fixture)));
        const auto index = ReferenceHarness::loadFixtureIndex (probeRoot.directory, indexFile);
        expect (index.ok(), "policy mutation index must remain structurally valid");
        if (index.value.has_value())
            expectDiagnostic (ReferenceHarness::expandSmoothingFixtures (
                                  probeRoot.directory, *index.value, manifest),
                              "smoothing.policy");
    }
};

ReferenceAnalyzerTest referenceAnalyzerTest;

class ReferencePitchTest final : public juce::UnitTest {
public:
    ReferencePitchTest()
        : juce::UnitTest ("ModelDReferencePitchContract", "pitch")
    {
    }

    void runTest() override
    {
        using namespace ReferenceHarness;
        const auto registry = AnalyzerRegistry::withFoundationAnalyzers();

        beginTest ("versioned periodic pitch analyzer calibrates across pitch and sample rate");
        const auto identity = registry.find ("audio.pitch.v1");
        expect (identity.ok() && identity.value->id == "audio.pitch.v1"
                    && identity.value->version == 1,
                "audio.pitch.v1 must resolve at version one");
        constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0 };
        constexpr std::array midiCoordinates { 36.0, 60.0, 69.0, 77.0, 96.0 };
        for (const auto sampleRate : sampleRates) {
            for (const auto midi : midiCoordinates) {
                const auto tone = makeTone (sampleRate, midi, 8192);
                const auto analyzed = registry.analyze (
                    "audio.pitch.v1",
                    AnalysisRequest {
                        .metric = "midi-semitones",
                        .sampleRate = sampleRate,
                        .audio = tone,
                    });
                expect (analyzed.ok() && analyzed.value.has_value()
                            && analyzed.value->size() == 1,
                        "each analytic tone must produce one selected pitch metric");
                if (! analyzed.value.has_value() || analyzed.value->size() != 1)
                    continue;
                const auto& result = analyzed.value->front();
                const auto error = std::abs (result.value - midi);
                logMessage ("pitch-grid sample-rate=" + juce::String { sampleRate, 0 }
                            + " midi=" + juce::String { midi, 0 }
                            + " error-semitones=" + juce::String { error, 9 });
                expect (result.metric == "midi-semitones" && result.unit == "semitones",
                        "the selected pitch coordinate must use the governed metric and unit");
                expect (result.finite && std::isfinite (result.value),
                        "calibrated pitch metrics must be finite");
                expect (error < 0.005,
                        "analytic pitch error must remain below 0.005 semitone");
            }
        }

        beginTest ("periodic pitch reports the complete metric and settings contract");
        const auto concertA = makeTone (48000.0, 69.0, 8192);
        const auto complete = registry.analyze (
            "audio.pitch.v1",
            AnalysisRequest { .sampleRate = 48000.0, .audio = concertA });
        expect (complete.ok() && complete.value.has_value() && complete.value->size() == 3,
                "a periodic tone must report all three pitch metrics");
        expectMetric (complete, "frequency-hz", "Hz", 440.0, 0.1);
        expectMetric (complete, "midi-semitones", "semitones", 69.0, 0.005);
        if (complete.value.has_value()) {
            const auto midi = findMetric (*complete.value, "midi-semitones");
            const auto confidence = findMetric (*complete.value, "confidence");
            expect (midi != nullptr && midi->allowance == 0.01,
                    "the governed pitch-coordinate allowance must be one cent");
            expect (confidence != nullptr && confidence->unit == "ratio"
                        && confidence->finite && confidence->value >= 0.80
                        && confidence->value <= 1.0,
                    "pitch confidence must be a finite normalized ratio above threshold");
            expect (complete.value->front().settings == std::map<std::string, std::string> {
                        { "algorithm", "normalized-autocorrelation-parabolic-v1" },
                        { "ambiguity-separation", "0.02" },
                        { "confidence-threshold", "0.80" },
                        { "lag-range", "20Hz..5000Hz" },
                        { "minimum-periods", "4" },
                        { "periodic-multiple-tolerance", "0.05" },
                    },
                    "pitch analyzer settings must be exact and explicit");
        }

        beginTest ("fixture pitch analysis propagates the exact render sample rate");
        RenderFixture fixture;
        fixture.id = "pitch-sample-rate-propagation";
        fixture.fixtureRelativePath =
            "Tests/reference/fixtures/pitch-sample-rate-propagation.json";
        fixture.config.sampleRate = 44100.0;
        fixture.config.totalSamples = 8192;
        fixture.config.blockPatterns = { { 8192 } };
        FixtureAnalysisRequest pitchRequest;
        pitchRequest.id = "concert-a";
        pitchRequest.version = 1;
        pitchRequest.analyzer = { "audio.pitch.v1", 1 };
        pitchRequest.metric = "midi-semitones";
        pitchRequest.inputKind = AnalysisInputKind::audio;
        pitchRequest.audioTap = AudioTap::main;
        pitchRequest.channel = 0;
        pitchRequest.eventSample = 4096;
        pitchRequest.windowSamples = 4096;
        fixture.analysisRequests = { pitchRequest };
        RenderResult render;
        render.sampleRate = 48000.0;
        render.mainChannels = 1;
        render.main = makeTone (48000.0, 60.0, 4096);
        const auto requestedTone = makeTone (48000.0, 69.0, 4096);
        render.main.insert (render.main.end(), requestedTone.begin(), requestedTone.end());
        render.blockPattern = { 8192 };
        const std::array renders { render };
        const auto evidence = analyzeFixtureMetrics (fixture, renders);
        expect (evidence.ok() && evidence.value.has_value() && evidence.value->size() == 1,
                "fixture pitch analysis must consume its requested window and render sample rate");
        if (evidence.value.has_value() && evidence.value->size() == 1)
            expectWithinAbsoluteError (
                evidence.value->front().metric.value, 69.0, 0.005,
                "fixture pitch evidence must retain the analytic MIDI coordinate");

        beginTest ("pitch rejection diagnostics are stable and periodic multiples are accepted");
        const std::vector<float> silence (4096, 0.0f);
        const auto shortTone = makeTone (48000.0, 69.0, 32);
        const auto boundaryFrequency = 48000.0 / std::floor (48000.0 / 5000.0);
        const auto boundaryTone = makeTone (
            48000.0, 69.0 + 12.0 * std::log2 (boundaryFrequency / 440.0), 4096);
        std::vector<float> ambiguous (4096);
        for (size_t sample = 0; sample < ambiguous.size(); ++sample) {
            const auto time = static_cast<double> (sample) / 48000.0;
            ambiguous[sample] = static_cast<float> (
                std::sin (juce::MathConstants<double>::twoPi * 150.0 * time)
                + 0.2 * std::sin (juce::MathConstants<double>::twoPi * 660.0 * time));
        }
        auto nonFinite = concertA;
        nonFinite[100] = std::numeric_limits<float>::quiet_NaN();
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 0.0, .audio = concertA }),
            "analyzer.sample-rate");
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 48000.0, .audio = silence }),
            "analyzer.pitch-silence");
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 48000.0, .audio = shortTone }),
            "analyzer.pitch-window");
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 48000.0, .audio = boundaryTone }),
            "analyzer.pitch-window");
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 48000.0, .audio = ambiguous }),
            "analyzer.pitch-ambiguous");
        expectDiagnostic (registry.analyze (
            "audio.pitch.v1", AnalysisRequest { .sampleRate = 48000.0, .audio = nonFinite }),
            "analyzer.non-finite");
    }

private:
    static std::vector<float> makeTone (const double sampleRate,
                                        const double midi,
                                        const size_t sampleCount)
    {
        const auto frequency = 440.0 * std::exp2 ((midi - 69.0) / 12.0);
        std::vector<float> tone (sampleCount);
        for (size_t sample = 0; sample < tone.size(); ++sample)
            tone[sample] = static_cast<float> (
                0.75 * std::sin (juce::MathConstants<double>::twoPi
                                 * frequency * static_cast<double> (sample) / sampleRate));
        return tone;
    }

    static const ReferenceHarness::MetricResult* findMetric (
        const std::vector<ReferenceHarness::MetricResult>& metrics,
        const std::string_view name)
    {
        const auto found = std::find_if (metrics.begin(), metrics.end(), [&] (const auto& metric) {
            return metric.metric == name;
        });
        return found == metrics.end() ? nullptr : &*found;
    }

    void expectMetric (const ReferenceHarness::LoadResult<
                           std::vector<ReferenceHarness::MetricResult>>& result,
                       const std::string_view name,
                       const std::string_view unit,
                       const double expected,
                       const double tolerance)
    {
        expect (result.value.has_value(), "analysis must return pitch metric records");
        if (! result.value.has_value())
            return;
        const auto found = findMetric (*result.value, name);
        expect (found != nullptr, "pitch metric must be reported: " + std::string { name });
        if (found != nullptr) {
            expect (found->unit == unit, "pitch metric unit must be exact: " + std::string { name });
            expectWithinAbsoluteError (found->value, expected, tolerance,
                                       "pitch metric must match analytic calibration");
            expect (found->finite && std::isfinite (found->value),
                    "pitch metric must be finite and valid");
        }
    }

    template <typename T>
    void expectDiagnostic (const ReferenceHarness::LoadResult<T>& result,
                           const std::string_view code)
    {
        expect (! result.ok(), "negative pitch case must fail");
        const auto actual = result.diagnostics.empty() ? "<none>" : result.diagnostics.front().code;
        expect (! result.diagnostics.empty() && actual == code,
                "negative pitch case must report stable diagnostic " + std::string { code }
                    + ", got " + actual);
    }
};

ReferencePitchTest referencePitchTest;

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
            expect (loaded.value->purpose.size() > 20
                        && loaded.value->reviewStatus
                               == ReferenceHarness::FixtureReviewStatus::foundationReviewed,
                    juce::String { path } + " governance metadata must load into typed fields");
            fixtures.push_back (*loaded.value);
        }
        expectEquals (static_cast<int> (fixtures.size()), 3);
        if (fixtures.size() != 3)
            return;
        expect (fixtures.back().expectedContourContract
                    == StateContract::ContourContract::legacyCrossedContours,
                "legacy fixture must retain legacyCrossedContours");

        beginTest ("indexed render fixture identity and requirement ownership are exact");
        const auto fixtureIndex = ReferenceHarness::loadFixtureIndex (
            sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
        expect (fixtureIndex.ok(), "fixture index must load for identity binding");
        if (fixtureIndex.value.has_value()) {
            const auto& indexed = fixtureIndex.value->renderFixtures.front();
            expect (ReferenceHarness::loadIndexedRenderFixture (sourceRoot, indexed).ok(),
                    "matching index and fixture identity must load");
            auto wrongId = indexed;
            wrongId.id = "different-safe-id";
            expectDiagnostic (ReferenceHarness::loadIndexedRenderFixture (sourceRoot, wrongId),
                              "fixture.index-id");
            auto wrongRequirements = indexed;
            wrongRequirements.requirements = { "TST-001" };
            expectDiagnostic (
                ReferenceHarness::loadIndexedRenderFixture (sourceRoot, wrongRequirements),
                "fixture.index-requirements");
        }

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
            expect (! result.reproducibility.compilerId.empty()
                        && ! result.reproducibility.compilerVersion.empty()
                        && ! result.reproducibility.sourceTree.empty()
                        && ! result.reproducibility.sourceContent.empty()
                        && result.reproducibility.sourceDirty
                               == ReferenceHarness::builtSourceIdentity().dirty,
                    "generated render provenance must include exact build/source identity");
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
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("signal.stats.v1")",
                                    R"("signal.stats.v2")"),
                                "fixture.analysis-analyzer");

        beginTest ("render fixture purpose and review status are mandatory governance metadata");
        constexpr auto purpose = R"(  "purpose": "Exercise native v2 state restoration, deterministic rendering, and typed audio analysis.",
)";
        constexpr auto reviewStatus = R"(  "reviewStatus": "foundation-reviewed",
)";
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (purpose, {}),
                                "fixture.purpose");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    "Exercise native v2 state restoration, deterministic rendering, and typed audio analysis.",
                                    ""),
                                "fixture.purpose");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    "Exercise native v2 state restoration, deterministic rendering, and typed audio analysis.",
                                    "TBD"),
                                "fixture.purpose");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (reviewStatus, {}),
                                "fixture.review-status");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("reviewStatus": "foundation-reviewed")",
                                    R"("reviewStatus": "accepted")"),
                                "fixture.review-status");

        beginTest ("render fixture IDs are safe single portable components");
        const auto fixtureId = R"("id": "native-v2-foundation")";
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    fixtureId, R"("id": "")"),
                                "fixture.id");
        for (const auto unsafeId : {
                 R"("id": ".")", R"("id": "..")", R"("id": "/absolute")",
                 R"("id": "nested/name")", R"("id": "nested\\name")",
                 R"("id": "C:drive")", R"("id": "C:\\drive")",
                 R"("id": "\\\\server\\share")", R"("id": "a/../escape")",
             })
            expectFixtureDiagnostic (
                sourceRoot, validText.replaceFirstOccurrenceOf (fixtureId, unsafeId),
                "fixture.id");

        beginTest ("render analysis requests are typed, versioned, and fixture-owned");
        const auto rightAudioRequest = R"JSON({"requestId": "main-right-count", "version": 1,
     "analyzer": {"id": "signal.stats.v1", "version": 1},
     "metric": "sample-count",
     "input": {"kind": "audio", "tap": "main", "channel": 1},
     "event": {"originSample": 0, "windowSamples": 2048}})JSON";
        const auto controlRequest = R"JSON({"requestId": "filter-cutoff-first-change", "version": 1,
     "analyzer": {"id": "control.step.v1", "version": 1},
     "metric": "first-change-sample",
     "input": {"kind": "control", "parameterId": "filterCutoff", "domain": "normalized"},
     "event": {"originSample": 512, "windowSamples": 1536},
     "start": 0.5, "target": 0.25})JSON";
        const auto controlText = validText.replaceFirstOccurrenceOf (
            rightAudioRequest, controlRequest);
        expectFixtureValid (sourceRoot, validText);
        expectFixtureValid (sourceRoot, controlText);

        beginTest ("control analysis endpoints come from restored state and ordered automation");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("start": 0.5, "target": 0.25)",
                                    R"("start": 0.4, "target": 0.25)"),
                                "fixture.analysis-endpoints");
        const auto unrelatedControlRequest = R"JSON({"requestId": "filter-cutoff-count", "version": 1,
     "analyzer": {"id": "signal.stats.v1", "version": 1},
     "metric": "sample-count",
     "input": {"kind": "control", "parameterId": "filterCutoff", "domain": "normalized"},
     "event": {"originSample": 1024, "windowSamples": 1024},
     "start": 0.25})JSON";
        expectFixtureDiagnostic (
            sourceRoot, validText.replaceFirstOccurrenceOf (
                            rightAudioRequest, unrelatedControlRequest),
            "fixture.analysis-event");
        const auto transition =
            R"({"sample": 512, "sequence": 3, "parameterId": "filterCutoff", "normalizedValue": 0.25})";
        const auto sameSampleFinal = controlText
            .replaceFirstOccurrenceOf (
                transition,
                juce::String { transition }
                    + R"(,
    {"sample": 512, "sequence": 5, "parameterId": "filterCutoff", "normalizedValue": 0.75})")
            .replaceFirstOccurrenceOf (
                R"("start": 0.5, "target": 0.25)",
                R"("start": 0.5, "target": 0.75)");
        expectFixtureValid (sourceRoot, sameSampleFinal);
        const auto decimalEndpoint = controlText
            .replaceFirstOccurrenceOf (
                R"("normalizedValue": 0.25)", R"("normalizedValue": 0.1)")
            .replaceFirstOccurrenceOf (
                R"("start": 0.5, "target": 0.25)",
                R"("start": 0.5, "target": 0.1)");
        expectFixtureValid (sourceRoot, decimalEndpoint);
        const auto priorSameKey = controlText
            .replaceFirstOccurrenceOf (
                transition,
                R"({"sample": 256, "sequence": 5, "parameterId": "filterCutoff", "normalizedValue": 0.1},
    {"sample": 512, "sequence": 3, "parameterId": "filterCutoff", "normalizedValue": 0.25})")
            .replaceFirstOccurrenceOf (
                R"("start": 0.5, "target": 0.25)",
                R"("start": 0.1000000001, "target": 0.25)");
        expectFixtureValid (
            sourceRoot, priorSameKey,
            static_cast<double> (static_cast<float> (0.1)));
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("requestId": "main-left-count")",
                                    R"("requestId": "../escape")"),
                                "fixture.analysis-id");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("version": 1,
     "analyzer")",
                                    R"("version": 2,
     "analyzer")"),
                                "fixture.analysis-version");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("id": "signal.stats.v1", "version": 1)",
                                    R"("id": "signal.stats.v1", "version": 2)"),
                                "fixture.analysis-analyzer");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("metric": "sample-count")",
                                    R"("metric": "unknown")"),
                                "fixture.analysis-metric");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("tap": "main")", R"("tap": "sidechain")"),
                                "fixture.analysis-input");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("channel": 0)", R"("channel": 2)"),
                                "fixture.analysis-input");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("domain": "normalized")",
                                    R"("domain": "events")"),
                                "fixture.analysis-input");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("input": {"kind": "control", "parameterId": "filterCutoff", "domain": "normalized"})",
                                    R"("input": {"kind": "control", "parameterId": "unknownParameter", "domain": "normalized"})"),
                                "fixture.analysis-input");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("originSample": 512, "windowSamples": 1536)",
                                    R"("originSample": 2048, "windowSamples": 1)"),
                                "fixture.analysis-event");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("originSample": 512, "windowSamples": 1536)",
                                    R"("originSample": 512, "windowSamples": 1537)"),
                                "fixture.analysis-event");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("start": 0.5, "target": 0.25)",
                                    R"("start": 1e999, "target": 0.25)"),
                                "fixture.analysis-endpoints");
        expectFixtureDiagnostic (sourceRoot, controlText.replaceFirstOccurrenceOf (
                                    R"("start": 0.5, "target": 0.25)",
                                    R"("start": 0.5, "target": 0.75)"),
                                "fixture.analysis-endpoints");
        auto stringOnly = juce::JSON::fromString (validText);
        auto* stringOnlyRoot = stringOnly.getDynamicObject();
        stringOnlyRoot->removeProperty ("analysisRequests");
        stringOnlyRoot->setProperty ("analyzers", juce::Array<juce::var> { "signal.stats.v1" });
        expectFixtureDiagnostic (
            sourceRoot, ReferenceHarness::canonicalJson (stringOnly),
            "fixture.analysis-requests");

        const auto legacyText = sourceRoot.getChildFile (fixturePaths.back()).loadFileAsString();
        expectFixtureDiagnostic (sourceRoot, legacyText.replaceFirstOccurrenceOf (
                                    R"({"sample": 0, "sequence": 2, "parameterId": "noiseOnOffSwitch", "normalizedValue": 0.0})",
                                    R"({"sample": 0, "sequence": 2, "parameterId": "noiseOnOffSwitch", "normalizedValue": 1.0})"),
                                "fixture.stochastic-state");

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
        if (existingOutput.isOwned()) {
            expect (existingOutput.directory.getChildFile (fixtures.front().id)
                        .createDirectory(),
                    "existing fixture output must be created");
            expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), *firstRun.value, existingOutput.directory),
                              "output.exists");
        }

        beginTest ("candidate output rejects escaped fixture destinations without remnants");
        const TemporaryDirectory containmentRoot { "model-d-reference-containment" };
        expect (containmentRoot.isOwned(), "containment root must be owned");
        if (containmentRoot.isOwned()) {
            const auto rendersRoot = containmentRoot.directory.getChildFile ("renders");
            expect (rendersRoot.createDirectory(), "containment renders root must exist");
            auto unsafeFixture = fixtures.front();
            unsafeFixture.id = "../escaped-fixture";
            const auto escaped = containmentRoot.directory.getChildFile ("escaped-fixture");
            expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                                  unsafeFixture, *firstRun.value, rendersRoot),
                              "output.destination");
            expect (! escaped.exists(),
                    "failed destination validation must leave no escaped artifact behind");
        }

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
        const auto candidateARoot = candidateRoot.getChildFile ("candidate-a");
        const auto candidateBRoot = candidateRoot.getChildFile ("candidate-b");
        expect (candidateARoot.createDirectory() && candidateBRoot.createDirectory(),
                "repeat candidate roots must be created");
        const auto candidateA = candidateARoot.getChildFile (fixtures.front().id);
        const auto candidateB = candidateBRoot.getChildFile (fixtures.front().id);
        const auto expectRejectedCandidate = [&] (const std::vector<ReferenceHarness::RenderResult>& results,
                                                   const juce::String& name,
                                                   const std::string_view code) {
            const auto root = candidateRoot.getChildFile (name);
            expect (root.createDirectory(), name + " rejection root must be created");
            const auto directory = root.getChildFile (fixtures.front().id);
            expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), results, root),
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
            result.reproducibility.sourceTree.clear();
            result.reproducibility.sourceContent.clear();
            result.reproducibility.juceCommit.clear();
            result.reproducibility.buildType.clear();
            result.reproducibility.platform.clear();
            result.reproducibility.architecture.clear();
            result.reproducibility.compilerId.clear();
            result.reproducibility.compilerVersion.clear();
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
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::sourceTree,
                               "wrong-source-tree");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::sourceContent,
                               "wrong-source-content");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::juceCommit,
                               "wrong-juce-commit");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::buildType,
                               "wrong-build-type");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::platform,
                               "wrong-platform");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::architecture,
                               "wrong-architecture");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::compilerId,
                               "wrong-compiler-id");
        expectWrongProvenance (&ReferenceHarness::ReproducibilityInfo::compilerVersion,
                               "wrong-compiler-version");
        auto wrongSourceDirty = *firstRun.value;
        for (auto& result : wrongSourceDirty)
            result.reproducibility.sourceDirty = ! result.reproducibility.sourceDirty;
        expectRejectedCandidate (wrongSourceDirty, "wrong-source-dirty", "output.provenance");

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
        const auto missingWavRoot = candidateRoot.getChildFile ("missing-wav-hash");
        expect (missingWavRoot.createDirectory(), "missing WAV root must be created");
        const auto missingWavDirectory = missingWavRoot.getChildFile (wavFixture.id);
        expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                              wavFixture, missingWavHash, missingWavRoot),
                          "output.input-hash");
        expect (! missingWavDirectory.exists(),
                "missing WAV input hashes must be rejected before output creation");
        auto wrongWavHash = *firstRun.value;
        for (auto& result : wrongWavHash)
            result.reproducibility.inputHashes = {
                { "audio", std::string (64, '0') },
                { "state", fixtures.front().stateSha256 },
            };
        const auto wrongWavRoot = candidateRoot.getChildFile ("wrong-wav-hash");
        expect (wrongWavRoot.createDirectory(), "wrong WAV root must be created");
        const auto wrongWavDirectory = wrongWavRoot.getChildFile (wavFixture.id);
        expectDiagnostic (ReferenceHarness::writeCandidateArtifacts (
                              wavFixture, wrongWavHash, wrongWavRoot),
                          "output.input-hash");
        expect (! wrongWavDirectory.exists(),
                "invalid WAV input hashes must be rejected before output creation");
        const auto writtenA = ReferenceHarness::writeCandidateArtifacts (
            fixtures.front(), *firstRun.value, candidateARoot);
        const auto writtenB = nativeSecondRun.has_value()
                                  && nativeSecondRun->value.has_value()
                            ? ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), *nativeSecondRun->value, candidateBRoot)
                            : ReferenceHarness::LoadResult<std::vector<juce::File>> {};
        expect (writtenA.ok() && writtenB.ok(), "both fresh candidate directories must be written");
        juce::var parsedCandidateRender;
        const auto candidateRenderParsed = juce::JSON::parse (
            candidateA.getChildFile ("render.json").loadFileAsString(), parsedCandidateRender);
        const auto* candidateRender = parsedCandidateRender.getDynamicObject();
        expect (candidateRenderParsed.wasOk() && candidateRender != nullptr
                    && candidateRender->getProperty ("purpose").toString().toStdString()
                           == fixtures.front().purpose
                    && candidateRender->getProperty ("reviewStatus").toString()
                           == "foundation-reviewed",
                "candidate render provenance must carry governed fixture purpose and review status");
        const auto* candidateReproducibility = candidateRender == nullptr
                                                 ? nullptr
                                                 : candidateRender->getProperty (
                                                       "reproducibility").getDynamicObject();
        expect (candidateReproducibility != nullptr
                    && candidateReproducibility->getProperty (
                           "compilerId").toString().toStdString()
                           == firstRun.value->front().reproducibility.compilerId
                    && candidateReproducibility->getProperty (
                           "compilerVersion").toString().toStdString()
                           == firstRun.value->front().reproducibility.compilerVersion
                    && candidateReproducibility->getProperty (
                           "sourceTree").toString().toStdString()
                           == firstRun.value->front().reproducibility.sourceTree
                    && candidateReproducibility->getProperty (
                           "sourceContent").toString().toStdString()
                           == firstRun.value->front().reproducibility.sourceContent
                    && candidateReproducibility->getProperty ("sourceDirty").isBool()
                    && static_cast<bool> (candidateReproducibility->getProperty (
                           "sourceDirty"))
                           == firstRun.value->front().reproducibility.sourceDirty,
                "candidate render provenance must serialize compiler and source identity");
        juce::var parsedCandidateMetrics;
        const auto candidateMetricsParsed = juce::JSON::parse (
            candidateA.getChildFile ("metrics.json").loadFileAsString(), parsedCandidateMetrics);
        const auto* candidateMetricsRoot = parsedCandidateMetrics.getDynamicObject();
        const auto* candidateMetricRecords = candidateMetricsRoot == nullptr
                                               ? nullptr
                                               : candidateMetricsRoot->getProperty ("records").getArray();
        const auto* candidateMetricRecord = candidateMetricRecords == nullptr
                                              || candidateMetricRecords->isEmpty()
                                            ? nullptr
                                            : candidateMetricRecords->getReference (0).getDynamicObject();
        const auto* candidateMetricProvenance = candidateMetricRecord == nullptr
                                                   ? nullptr
                                                   : candidateMetricRecord->getProperty (
                                                         "provenance").getDynamicObject();
        expect (candidateMetricsParsed.wasOk() && candidateMetricProvenance != nullptr
                    && candidateMetricProvenance->getProperty (
                           "fixturePurpose").toString().toStdString() == fixtures.front().purpose
                    && candidateMetricProvenance->getProperty (
                           "fixtureReviewStatus").toString() == "foundation-reviewed",
                "metric provenance must carry governed fixture purpose and review status");
        expect (candidateMetricProvenance != nullptr
                    && candidateMetricProvenance->getProperty (
                           "compilerId").toString().toStdString()
                           == firstRun.value->front().reproducibility.compilerId
                    && candidateMetricProvenance->getProperty (
                           "compilerVersion").toString().toStdString()
                           == firstRun.value->front().reproducibility.compilerVersion
                    && candidateMetricProvenance->getProperty (
                           "sourceTree").toString().toStdString()
                           == firstRun.value->front().reproducibility.sourceTree
                    && candidateMetricProvenance->getProperty (
                           "sourceContent").toString().toStdString()
                           == firstRun.value->front().reproducibility.sourceContent
                    && candidateMetricProvenance->getProperty ("sourceDirty").isBool()
                    && static_cast<bool> (candidateMetricProvenance->getProperty (
                           "sourceDirty"))
                           == firstRun.value->front().reproducibility.sourceDirty,
                "metric provenance must serialize compiler and source identity");
        for (const auto name : { "render.json", "control-trace.json", "event-trace.json",
                                 "metrics.json" })
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

    void expectFixtureValid (
        const juce::File& sourceRoot,
        const juce::String& contents,
        const std::optional<double> expectedPriorNormalized = std::nullopt)
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
        const auto loaded = ReferenceHarness::loadRenderFixture (
            fixtureRoot.directory, fixtureFile);
        expect (loaded.ok(),
                "same-sample enable then disable must leave stochastic processing off");
        if (loaded.value.has_value() && expectedPriorNormalized.has_value()) {
            const auto& request = loaded.value->analysisRequests.back();
            expect (request.start.has_value(), "control request must retain its effective start");
            MoogMiniAudioProcessor processor;
            const auto* parameter = processor.getPreparedParameter (request.parameterKey);
            expect (parameter != nullptr, "control request parameter must be prepared");
            if (request.start.has_value() && parameter != nullptr) {
                const auto normalized = parameter->convertTo0to1 (
                    parameter->convertFrom0to1 (
                        static_cast<float> (*expectedPriorNormalized)));
                const auto expected = request.controlDomain == ReferenceHarness::ControlDomain::normalized
                                        ? normalized
                                        : parameter->convertFrom0to1 (normalized);
                expectWithinAbsoluteError (
                    *request.start, static_cast<double> (expected), 0.0,
                    "loaded control start must be the processor-domain float value");
            }
            const auto rendered = ReferenceHarness::renderFixture (*loaded.value);
            const auto metrics = rendered.ok()
                                   ? ReferenceHarness::analyzeFixtureMetrics (
                                         *loaded.value, *rendered.value)
                                   : ReferenceHarness::LoadResult<std::vector<
                                         ReferenceHarness::MetricEvidenceRecord>> {};
            expect (rendered.ok() && metrics.ok(),
                    "dense control analysis must consume the derived effective endpoints");
        }
    }
};

ReferenceRendererTest referenceRendererTest;

class ReferenceRequirementTest final : public juce::UnitTest {
public:
    ReferenceRequirementTest()
        : juce::UnitTest ("ModelDReferenceRequirementContract", "requirements")
    {
    }

    void runTest() override
    {
        using namespace ReferenceHarness;
        const auto sourceRoot = juce::File { sourceRootPath };
        const auto registry = AnalyzerRegistry::withFoundationAnalyzers();
        const auto acceptance = loadAcceptanceManifest (
            sourceRoot,
            sourceRoot.getChildFile ("Tests/reference/acceptance-v1.json"),
            registry);

        beginTest ("the complete requirement map matches the traceability matrix");
        expect (acceptance.ok(), "acceptance must validate before requirement mapping");
        if (! acceptance.ok())
            return;
        const auto requirements = loadRequirementMap (
            sourceRoot,
            sourceRoot.getChildFile ("Tests/reference/requirement-map.json"),
            sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md"),
            *acceptance.value);
        expect (requirements.ok(), "the checked-in requirement map must validate");
        if (requirements.ok()) {
            expectEquals (static_cast<int> (requirements.value->size()), 127,
                          "the map must contain every matrix requirement exactly once");
            expect (validateRequirementSet (
                        *requirements.value,
                        sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md")).ok(),
                    "the loaded set must remain equal to the independent matrix parse");
        }

        const auto mapText = sourceRoot.getChildFile ("Tests/reference/requirement-map.json")
                                 .loadFileAsString();
        beginTest ("requirement map rejects unsupported statuses and unproven passes");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 R"("status": "pass")", R"("status": "waived")"),
                             "requirement.status");
        expectMapDiagnostic (
            sourceRoot, *acceptance.value,
            mapText.replaceFirstOccurrenceOf (
                R"("artifacts": [
        {
          "path": "docs/remediation/evidence/workstream-02/build-foundation.md",
          "sha256": "06bb3a2c7c6759413cd2ab45c119ccd3908889374ece79abec18e38a72441b95"
        }
      ])",
                R"("artifacts": [])"),
            "requirement.pass-artifact");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 "06bb3a2c7c6759413cd2ab45c119ccd3908889374ece79abec18e38a72441b95",
                                 "00bb3a2c7c6759413cd2ab45c119ccd3908889374ece79abec18e38a72441b95"),
                             "requirement.artifact-hash");

        beginTest ("requirement map rejects unknown verification and gate identities");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (R"("HV")", R"("XX")"),
                             "requirement.verification-code");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 R"("hard.registry.count")", R"("unknown.gate")"),
                             "requirement.gate-mapping");

        beginTest ("requirement map fields exactly match matrix owners and verification order");
        expectMapDiagnostic (
            sourceRoot, *acceptance.value,
            mapText.replaceFirstOccurrenceOf (
                R"OWNER("owner": "[02 Build](02-build-packaging-host-validation.md)")OWNER",
                R"OWNER("owner": "[03 Parameters/state](03-parameter-automation-state-contract.md)")OWNER"),
            "requirement.matrix-fields");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (R"("HV")", R"("UT")"),
                             "requirement.matrix-fields");

        beginTest ("static acceptance gate ownership is exact and reciprocal");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 R"("gateIds": [
        "hard.registry.count"
      ])",
                                 R"("gateIds": [])"),
                             "requirement.gate-reciprocity");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 R"("gateIds": [])", R"("gateIds": ["hard.registry.count"])"),
                             "requirement.gate-reciprocity");

        beginTest ("requirement map rejects duplicate identities and multiple owners");
        expectMapDiagnostic (sourceRoot, *acceptance.value,
                             mapText.replaceFirstOccurrenceOf (
                                 R"("id": "BLD-002")", R"("id": "BLD-001")"),
                             "requirement.duplicate-id");
        expectMapDiagnostic (
            sourceRoot, *acceptance.value,
            mapText.replaceFirstOccurrenceOf (
                R"OWNER("owner": "[02 Build](02-build-packaging-host-validation.md)")OWNER",
                R"OWNER("owner": ["[02 Build](02-build-packaging-host-validation.md)", "duplicate"] )OWNER"),
            "requirement.owner");

        beginTest ("requirement set rejects matrix drift");
        const TemporaryDirectory matrixRoot { "model-d-requirement-matrix-drift" };
        expect (matrixRoot.isOwned(), "matrix drift root must be owned");
        if (matrixRoot.isOwned() && requirements.ok()) {
            const auto changedMatrix = matrixRoot.directory.getChildFile ("matrix.md");
            changedMatrix.replaceWithText (
                sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md")
                    .loadFileAsString().replaceFirstOccurrenceOf ("| BLD-001 |", "| BLD-999 |"));
            expectDiagnostic (validateRequirementSet (*requirements.value, changedMatrix),
                              "requirement.set");
        }

        if (! requirements.ok())
            return;
        const auto index = loadFixtureIndex (
            sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
        const auto smoothing = index.ok()
                                 ? expandSmoothingFixtures (
                                       sourceRoot, *index.value, *acceptance.value)
                                 : LoadResult<std::vector<SmoothingCase>> {};
        const std::span<const SmoothingEvidence> noEvidence;
        const TemporaryDirectory candidateRoot { "model-d-requirement-candidate-root" };
        expect (candidateRoot.isOwned(), "candidate root must be owned");
        if (! candidateRoot.isOwned())
            return;
        const auto registryEvidence = writeRegistryMetricEvidence (
            sourceRoot, candidateRoot.directory.getChildFile ("metrics.json"),
            candidateRoot.directory.getChildFile ("metrics.json")
                .getFullPathName().toStdString());
        expect (registryEvidence.ok(), "registry gate evidence must be produced by its analyzer");
        if (! registryEvidence.ok())
            return;
        beginTest ("generic metric evidence validates identity, values, provenance, and bytes");
        const auto expectMetricEvidenceFailure = [&] (
            GateMetricEvidence mutation, const juce::String& name,
            const std::string_view expectedReason,
            const AcceptanceManifest* evidenceManifest = nullptr) {
            mutation.artifactFile = candidateRoot.directory.getChildFile (
                "metric-negative-" + name + ".json");
            mutation.artifactPath = mutation.artifactFile.getFullPathName().toStdString();
            expect (mutation.artifactFile.replaceWithText (
                        metricEvidenceJson (std::span<const MetricEvidenceRecord> {
                            &mutation.record, 1 }), false, false, "\n"),
                    name + " metric mutation must be written canonically");
            mutation.artifactSha256 = sha256File (mutation.artifactFile);
            const auto result = evaluateAcceptance (
                evidenceManifest == nullptr ? *acceptance.value : *evidenceManifest,
                *smoothing.value, noEvidence, registry,
                std::span<const GateMetricEvidence> { &mutation, 1 });
            expect (result.ok(), name + " must reduce to an honest gate failure");
            if (! result.value.has_value())
                return;
            const auto gate = std::find_if (
                result.value->begin(), result.value->end(), [] (const auto& item) {
                    return item.id == "hard.registry.count";
                });
            expect (gate != result.value->end() && gate->status == Status::fail
                        && gate->reasonCode == expectedReason && ! gate->metric.has_value()
                        && gate->artifactPath.empty() && gate->artifactSha256.empty(),
                    name + " must fail without a false artifact claim");
        };
        auto staleMetric = *registryEvidence.value;
        staleMetric.record.metric.analyzer.version = 2;
        expectMetricEvidenceFailure (std::move (staleMetric), "stale-analyzer",
                                     "acceptance.metric-evidence-invalid");
        auto wrongMetric = *registryEvidence.value;
        wrongMetric.record.metric.metric = "finite-count";
        expectMetricEvidenceFailure (std::move (wrongMetric), "wrong-metric",
                                     "acceptance.metric-evidence-invalid");
        auto wrongUnit = *registryEvidence.value;
        wrongUnit.record.metric.unit = "samples";
        expectMetricEvidenceFailure (std::move (wrongUnit), "wrong-unit",
                                     "acceptance.metric-evidence-invalid");
        auto invalidFinite = *registryEvidence.value;
        invalidFinite.record.metric.finite = false;
        expectMetricEvidenceFailure (std::move (invalidFinite), "invalid-finite",
                                     "acceptance.metric-evidence-invalid");
        auto wrongSettings = *registryEvidence.value;
        wrongSettings.record.metric.settings["input"] = "audio";
        expectMetricEvidenceFailure (std::move (wrongSettings), "wrong-settings",
                                     "acceptance.metric-evidence-invalid");
        auto wrongMetricProvenance = *registryEvidence.value;
        wrongMetricProvenance.record.provenance.registryCount = 47;
        expectMetricEvidenceFailure (std::move (wrongMetricProvenance), "wrong-provenance",
                                     "acceptance.metric-evidence-invalid");
        auto wrongRegistrySourceTree = *registryEvidence.value;
        wrongRegistrySourceTree.record.provenance.reproducibility.sourceTree =
            std::string (40, '0');
        expectMetricEvidenceFailure (std::move (wrongRegistrySourceTree),
                                     "wrong-source-tree",
                                     "acceptance.metric-evidence-invalid");
        auto wrongRegistrySourceContent = *registryEvidence.value;
        wrongRegistrySourceContent.record.provenance.reproducibility.sourceContent =
            std::string (64, '0');
        expectMetricEvidenceFailure (std::move (wrongRegistrySourceContent),
                                     "wrong-source-content",
                                     "acceptance.metric-evidence-invalid");
        auto wrongRegistrySourceDirty = *registryEvidence.value;
        wrongRegistrySourceDirty.record.provenance.reproducibility.sourceDirty =
            ! wrongRegistrySourceDirty.record.provenance.reproducibility.sourceDirty;
        expectMetricEvidenceFailure (std::move (wrongRegistrySourceDirty),
                                     "wrong-source-dirty",
                                     "acceptance.metric-evidence-invalid");
        auto wrongArtifactHash = *registryEvidence.value;
        wrongArtifactHash.artifactSha256 = std::string (64, '0');
        const auto wrongHashResult = evaluateAcceptance (
            *acceptance.value, *smoothing.value, noEvidence, registry,
            std::span<const GateMetricEvidence> { &wrongArtifactHash, 1 });
        const auto wrongHashGate = std::find_if (
            wrongHashResult.value->begin(), wrongHashResult.value->end(), [] (const auto& item) {
                return item.id == "hard.registry.count";
            });
        expect (wrongHashResult.ok() && wrongHashGate != wrongHashResult.value->end()
                    && wrongHashGate->status == Status::fail
                    && wrongHashGate->reasonCode == "acceptance.metric-evidence-invalid"
                    && wrongHashGate->artifactPath.empty(),
                "a changed artifact hash must fail without an artifact claim");
        auto outOfBoundManifest = *acceptance.value;
        outOfBoundManifest.hardSoftware.front().value = 47.0;
        expectMetricEvidenceFailure (*registryEvidence.value, "out-of-bound",
                                     "acceptance.metric-out-of-bound", &outOfBoundManifest);

        beginTest ("render metric evidence replays the indexed fixture and full artifact");
        const auto indexedRender = index.ok()
            ? loadIndexedRenderFixture (sourceRoot, index.value->renderFixtures.front())
            : LoadResult<RenderFixture> {};
        const auto renderResults = indexedRender.ok()
            ? renderFixture (*indexedRender.value)
            : LoadResult<std::vector<RenderResult>> {};
        const auto renderRecords = renderResults.ok()
            ? analyzeFixtureMetrics (*indexedRender.value, *renderResults.value)
            : LoadResult<std::vector<MetricEvidenceRecord>> {};
        expect (renderRecords.ok() && renderRecords.value->size() == 2,
                "typed stereo requests must produce a multi-record artifact");
        if (! renderRecords.ok() || renderRecords.value->size() != 2)
            return;
        const auto renderMetricsDirectory = candidateRoot.directory
            .getChildFile ("renders").getChildFile ("native-v2-foundation");
        expect (renderMetricsDirectory.createDirectory(),
                "render metric directory must be created");
        const auto renderMetricsFile = renderMetricsDirectory.getChildFile ("metrics.json");
        expect (renderMetricsFile.replaceWithText (
                    metricEvidenceJson (*renderRecords.value), false, false, "\n"),
                "render metric artifact must be written canonically");
        GateMetricEvidence renderEvidence;
        renderEvidence.gateId = "hard.registry.count";
        renderEvidence.record = renderRecords.value->front();
        renderEvidence.artifactFile = renderMetricsFile;
        renderEvidence.artifactPath = "renders/native-v2-foundation/metrics.json";
        renderEvidence.artifactSha256 = sha256File (renderMetricsFile);
        auto renderManifest = *acceptance.value;
        auto& renderGateDefinition = renderManifest.hardSoftware.front();
        renderGateDefinition.analyzer = renderEvidence.record.metric.analyzer;
        renderGateDefinition.metric = renderEvidence.record.metric.metric;
        renderGateDefinition.unit = renderEvidence.record.metric.unit;
        renderGateDefinition.value = renderEvidence.record.metric.value;
        renderGateDefinition.allowance = renderEvidence.record.metric.allowance;
        const auto evaluateRenderEvidence = [&] (const GateMetricEvidence& item) {
            return evaluateAcceptance (
                renderManifest, *smoothing.value, noEvidence, registry,
                std::span<const GateMetricEvidence> { &item, 1 });
        };
        const auto renderEvaluation = evaluateRenderEvidence (renderEvidence);
        auto renderPassed = false;
        if (renderEvaluation.value.has_value()) {
            const auto renderGate = std::find_if (
                renderEvaluation.value->begin(), renderEvaluation.value->end(),
                [] (const auto& gate) { return gate.id == "hard.registry.count"; });
            renderPassed = renderGate != renderEvaluation.value->end()
                        && renderGate->status == Status::pass
                        && renderGate->reasonCode == "acceptance.metric-pass";
        }
        expect (renderEvaluation.ok() && renderPassed,
                "only authoritative indexed render evidence may pass a metric gate");
        const auto expectRenderForgery = [&] (GateMetricEvidence mutation,
                                               const juce::String& name) {
            const auto result = evaluateRenderEvidence (mutation);
            expect (result.ok(), name + " render forgery must reduce honestly");
            if (! result.value.has_value())
                return;
            const auto gate = std::find_if (
                result.value->begin(), result.value->end(),
                [] (const auto& item) { return item.id == "hard.registry.count"; });
            expect (gate != result.value->end() && gate->status == Status::fail
                        && gate->reasonCode == "acceptance.metric-evidence-invalid"
                        && ! gate->metric.has_value() && gate->artifactPath.empty(),
                    name + " render forgery must fail without an artifact claim");
        };
        auto forgedRenderPath = renderEvidence;
        forgedRenderPath.record.provenance.fixturePath =
            "Tests/reference/fixtures/migrated-v2-foundation.json";
        expectRenderForgery (std::move (forgedRenderPath), "fixture-path");
        expect (renderEvidence.record.provenance.fixturePurpose
                    == indexedRender.value->purpose
                    && renderEvidence.record.provenance.fixtureReviewStatus
                           == indexedRender.value->reviewStatus,
                "render metric evidence must retain typed fixture governance provenance");
        auto forgedRenderPurpose = renderEvidence;
        forgedRenderPurpose.record.provenance.fixturePurpose = "Forged fixture purpose";
        expectRenderForgery (std::move (forgedRenderPurpose), "fixture-purpose");
        auto forgedRenderReviewStatus = renderEvidence;
        forgedRenderReviewStatus.record.provenance.fixtureReviewStatus =
            static_cast<FixtureReviewStatus> (999);
        expectRenderForgery (std::move (forgedRenderReviewStatus), "fixture-review-status");
        auto forgedRenderCompilerId = renderEvidence;
        forgedRenderCompilerId.record.provenance.reproducibility.compilerId = "forged-compiler";
        expectRenderForgery (std::move (forgedRenderCompilerId), "compiler-id");
        auto forgedRenderCompilerVersion = renderEvidence;
        forgedRenderCompilerVersion.record.provenance.reproducibility.compilerVersion =
            "0.0-forged";
        expectRenderForgery (std::move (forgedRenderCompilerVersion), "compiler-version");
        auto forgedRenderSourceTree = renderEvidence;
        forgedRenderSourceTree.record.provenance.reproducibility.sourceTree =
            std::string (40, '0');
        expectRenderForgery (std::move (forgedRenderSourceTree), "source-tree");
        auto forgedRenderSourceContent = renderEvidence;
        forgedRenderSourceContent.record.provenance.reproducibility.sourceContent =
            std::string (64, '0');
        expectRenderForgery (std::move (forgedRenderSourceContent), "source-content");
        auto forgedRenderSourceDirty = renderEvidence;
        forgedRenderSourceDirty.record.provenance.reproducibility.sourceDirty =
            ! forgedRenderSourceDirty.record.provenance.reproducibility.sourceDirty;
        expectRenderForgery (std::move (forgedRenderSourceDirty), "source-dirty");
        auto forgedRenderRequest = renderEvidence;
        forgedRenderRequest.record.provenance.requestId = "forged-request";
        expectRenderForgery (std::move (forgedRenderRequest), "request-id");
        auto forgedRenderValue = renderEvidence;
        forgedRenderValue.record.metric.value += 1.0;
        expectRenderForgery (std::move (forgedRenderValue), "value");
        auto forgedRenderSettings = renderEvidence;
        forgedRenderSettings.record.metric.settings["input"] = "control";
        expectRenderForgery (std::move (forgedRenderSettings), "settings");
        auto forgedArtifactPath = renderEvidence;
        forgedArtifactPath.artifactPath = "renders/migrated-v2-foundation/metrics.json";
        expectRenderForgery (std::move (forgedArtifactPath), "artifact-path");
        auto forgedArtifactHash = renderEvidence;
        forgedArtifactHash.artifactSha256 = std::string (64, '0');
        expectRenderForgery (std::move (forgedArtifactHash), "artifact-hash");
        const auto forgedRenderFile = candidateRoot.directory.getChildFile (
            "forged-render-metrics.json");
        expect (forgedRenderFile.replaceWithText (
                    renderMetricsFile.loadFileAsString() + " ", false, false, "\n"),
                "forged render file must be written");
        auto forgedArtifactFile = renderEvidence;
        forgedArtifactFile.artifactFile = forgedRenderFile;
        forgedArtifactFile.artifactSha256 = sha256File (forgedRenderFile);
        expectRenderForgery (std::move (forgedArtifactFile), "artifact-file");

        const auto evaluated = smoothing.ok()
                                 ? evaluateAcceptance (
                                       *acceptance.value, *smoothing.value, noEvidence, registry,
                                       std::span<const GateMetricEvidence> {
                                           &*registryEvidence.value, 1 })
                                 : LoadResult<std::vector<GateResult>> {};
        expect (index.ok() && smoothing.ok() && evaluated.ok(),
                "authoritative F0 gate expansion must validate");
        if (! evaluated.ok())
            return;
        std::vector<GateResult> gateResults = *evaluated.value;
        for (const auto& gate : gateResults) {
            const auto isStaticDerived = std::any_of (
                acceptance.value->derivedSoftware.begin(),
                acceptance.value->derivedSoftware.end(), [&] (const auto& definition) {
                    return definition.id == gate.id;
                });
            if (gate.id.starts_with ("par006.")
                && ! isStaticDerived)
                expect (gate.requirements == std::vector<std::string> ({ "PAR-006", "TST-006" }),
                        gate.id + " must declare complete dynamic ownership");
        }
        const auto hardGate = std::find_if (
            gateResults.begin(), gateResults.end(), [] (const auto& gate) {
                return gate.id == "hard.registry.count";
            });
        expect (hardGate != gateResults.end(), "hard registry gate must exist");
        if (hardGate == gateResults.end())
            return;
        expect (hardGate->status == Status::pass && hardGate->metric.has_value()
                    && hardGate->reasonCode == "acceptance.metric-pass",
                "hard registry gate must pass through typed metric evidence");

        beginTest ("requirement reduction preserves honest mixed statuses");
        const auto report = buildRequirementReport (
            sourceRoot, candidateRoot.directory, *requirements.value, gateResults,
            *acceptance.value);
        expect (report.ok(), "mixed-status requirement report must build successfully");
        if (! report.ok())
            return;
        expectEquals (static_cast<int> (report.value->requirements.size()), 127);
        expect (! report.value->releaseReady, "F0 must remain non-release-ready");
        expectRequirement (*report.value, "BLD-001", Status::pass);
        expectRequirement (*report.value, "BLD-006", Status::notRun);
        expectRequirement (*report.value, "PAR-001", Status::pass);
        expectRequirement (*report.value, "PAR-006", Status::awaitingApprovedReference);
        expectRequirement (*report.value, "TST-006", Status::awaitingApprovedReference);
        expectRequirement (*report.value, "PIT-003", Status::awaitingApprovedReference);

        beginTest ("report building validates and restores matrix order");
        auto reversedRequirements = *requirements.value;
        std::reverse (reversedRequirements.begin(), reversedRequirements.end());
        const auto reorderedReport = buildRequirementReport (
            sourceRoot, candidateRoot.directory, reversedRequirements, gateResults,
            *acceptance.value);
        expect (reorderedReport.ok(), "valid reordered input must build");
        if (reorderedReport.ok())
            expect (reorderedReport.value->requirements.front().definition.id == "BLD-001"
                        && reorderedReport.value->requirements.back().definition.id == "IMG-008",
                    "report rows must always restore matrix order");

        beginTest ("failing gates dominate open and awaiting statuses");
        auto failingGates = gateResults;
        const auto failingGate = std::find_if (
            failingGates.begin(), failingGates.end(), [] (const auto& gate) {
                return gate.id == "par006.a440HzOnOff.control";
            });
        expect (failingGate != failingGates.end(), "PAR-006 gate must exist");
        if (failingGate != failingGates.end()) {
            failingGate->status = Status::fail;
            failingGate->reasonCode = "acceptance.metric-fail";
            const auto failingReport = buildRequirementReport (
                sourceRoot, candidateRoot.directory, *requirements.value, failingGates,
                *acceptance.value);
            expect (failingReport.ok(), "numerical failure belongs in a valid report");
            if (failingReport.ok())
            {
                expectRequirement (*failingReport.value, "PAR-006", Status::fail);
                expectRequirement (*failingReport.value, "TST-006", Status::fail);
            }
        }

        beginTest ("one dynamic unrun gate prevents a gate-complete requirement pass");
        auto oneUnrunGate = gateResults;
        const auto sharedTestArtifact = sourceRoot.getChildFile (
            "Tests/fixtures/parameters/parameter-registry-v2.json");
        const auto sharedTestHash = sha256File (sharedTestArtifact);
        for (auto& gate : oneUnrunGate) {
            gate.status = Status::pass;
            gate.reasonCode = "test.gate-pass";
            gate.artifactPath = "Tests/fixtures/parameters/parameter-registry-v2.json";
            gate.artifactSha256 = sharedTestHash;
        }
        const auto unrunGate = std::find_if (
            oneUnrunGate.begin(), oneUnrunGate.end(), [] (const auto& gate) {
                return gate.id == "par006.a440HzOnOff.control";
            });
        expect (unrunGate != oneUnrunGate.end(), "dynamic control gate must exist");
        if (unrunGate != oneUnrunGate.end()) {
            unrunGate->status = Status::notRun;
            unrunGate->reasonCode = "test.dynamic-not-run";
            unrunGate->artifactPath.clear();
            unrunGate->artifactSha256.clear();
            const auto unrunReport = buildRequirementReport (
                sourceRoot, candidateRoot.directory, *requirements.value, oneUnrunGate,
                *acceptance.value);
            expect (unrunReport.ok(), "dynamic not-run belongs in a valid report");
            if (unrunReport.ok()) {
                expectRequirement (*unrunReport.value, "PAR-006", Status::notRun);
                expectRequirement (*unrunReport.value, "TST-006", Status::notRun);
            }
        }

        beginTest ("report building rejects missing gate results and changed evidence");
        auto missingGate = gateResults;
        missingGate.erase (std::find_if (
            missingGate.begin(), missingGate.end(), [] (const auto& gate) {
                return gate.id == "hard.registry.count";
            }));
        expectDiagnostic (buildRequirementReport (
                              sourceRoot, candidateRoot.directory, *requirements.value,
                              missingGate, *acceptance.value),
                          "requirement.gate-result-missing");
        auto wrongEvidence = *requirements.value;
        wrongEvidence.front().artifactSha256.front() = std::string (64, '0');
        expectDiagnostic (buildRequirementReport (
                              sourceRoot, candidateRoot.directory, wrongEvidence,
                              gateResults, *acceptance.value),
                          "requirement.artifact-hash");

        beginTest ("canonical reports are byte-identical and release enforcement is stable");
        const TemporaryDirectory reportRoot { "model-d-requirement-report-root" };
        expect (reportRoot.isOwned(), "report root must be owned");
        if (! reportRoot.isOwned())
            return;
        const auto reportA = writeRequirementReport (
            *report.value, reportRoot.directory.getChildFile ("candidate-a"));
        const auto reportB = writeRequirementReport (
            *report.value, reportRoot.directory.getChildFile ("candidate-b"));
        expect (reportA.ok() && reportB.ok(), "two fresh report candidates must write");
        if (! reportA.ok() || ! reportB.ok())
            return;
        expect (reportA.value->hasIdenticalContentTo (*reportB.value),
                "same-commit reports must be byte-identical");
        const auto writtenJson = juce::JSON::fromString (reportA.value->loadFileAsString());
        const auto* writtenGates = writtenJson.getDynamicObject()->getProperty ("gates").getArray();
        const auto* writtenProvenance = writtenJson.getDynamicObject()
                                            ->getProperty ("provenance").getDynamicObject();
        expect (writtenGates != nullptr
                    && writtenGates->size() == static_cast<int> (gateResults.size()),
                "canonical report must retain every static and dynamic gate result");
        expect (writtenProvenance != nullptr
                    && writtenProvenance->getProperty ("sourceTree").toString().toStdString()
                           == report.value->sourceTree
                    && writtenProvenance->getProperty ("sourceContent").toString().toStdString()
                           == report.value->sourceContent
                    && writtenProvenance->getProperty ("sourceDirty").isBool()
                    && static_cast<bool> (writtenProvenance->getProperty ("sourceDirty"))
                           == report.value->sourceDirty,
                "canonical report must serialize exact build/source identity");
        expectDiagnostic (verifyReleaseReady (*reportA.value), "release.report-mismatch");

        auto falsePassJson = juce::JSON::fromString (reportA.value->loadFileAsString());
        auto* falsePassRows = falsePassJson.getDynamicObject()
                                  ->getProperty ("requirements").getArray();
        for (auto& row : *falsePassRows) {
            auto* object = row.getDynamicObject();
            if (object != nullptr && object->getProperty ("id").toString() == "BLD-006") {
                object->setProperty ("status", "pass");
                break;
            }
        }
        const auto falsePassFile = reportRoot.directory.getChildFile ("false-pass.json");
        falsePassFile.replaceWithText (canonicalJson (falsePassJson), false, false, "\n");
        expectDiagnostic (verifyReleaseReady (falsePassFile), "release.pass-artifact");
        auto wrongSetJson = juce::JSON::fromString (reportA.value->loadFileAsString());
        wrongSetJson.getDynamicObject()->getProperty ("requirements").getArray()->getReference (0)
            .getDynamicObject()->setProperty ("id", "BLD-999");
        const auto wrongSetFile = reportRoot.directory.getChildFile ("wrong-set.json");
        wrongSetFile.replaceWithText (canonicalJson (wrongSetJson), false, false, "\n");
        expectDiagnostic (verifyReleaseReady (wrongSetFile), "release.report-set");

        beginTest ("CLI validation and mixed-status runs succeed while release verification blocks");
        const auto indexPath = sourceRoot.getChildFile (
            "Tests/reference/fixture-index-v1.json").getFullPathName().toStdString();
        const auto acceptancePath = sourceRoot.getChildFile (
            "Tests/reference/acceptance-v1.json").getFullPathName().toStdString();
        const auto requirementPath = sourceRoot.getChildFile (
            "Tests/reference/requirement-map.json").getFullPathName().toStdString();
        const std::vector<std::string> validateArguments {
            "validate", "--fixture-index", indexPath,
            "--acceptance", acceptancePath,
            "--requirements", requirementPath,
        };
        expectEquals (runOfflineRendererCommand (validateArguments), 0,
                      "validate must succeed without rendering");
        const auto staleAcceptance = reportRoot.directory.getChildFile (
            "stale-analyzer-acceptance.json");
        expect (staleAcceptance.replaceWithText (
            sourceRoot.getChildFile ("Tests/reference/acceptance-v1.json")
                .loadFileAsString().replaceFirstOccurrenceOf (
                    R"("analyzerVersion": 1)", R"("analyzerVersion": 2)")));
        auto staleValidateArguments = validateArguments;
        staleValidateArguments[4] = staleAcceptance.getFullPathName().toStdString();
        expectEquals (runOfflineRendererCommand (staleValidateArguments), 1,
                      "validate must reject a stale declared analyzer version");
        const auto cliA = reportRoot.directory.getChildFile ("cli-a");
        const auto cliB = reportRoot.directory.getChildFile ("cli-b");
        const auto runArguments = [&] (const juce::File& output) {
            return std::vector<std::string> {
                "run", "--fixture-index", indexPath,
                "--acceptance", acceptancePath,
                "--requirements", requirementPath,
                "--output", output.getFullPathName().toStdString(),
            };
        };
        if (ReferenceHarness::builtSourceIdentity().dirty) {
            expectEquals (runOfflineRendererCommand (runArguments (cliA)), 1,
                          "a dirty-build binary must reject authoritative candidate generation");
            expect (! cliA.exists(),
                    "dirty-build rejection must occur before candidate output creation");
            const std::vector<std::string> dirtyVerifyArguments {
                "verify-release", "--report", reportA.value->getFullPathName().toStdString(),
            };
            expectEquals (runOfflineRendererCommand (dirtyVerifyArguments), 1,
                          "a dirty-build binary must reject authoritative release verification");
            return;
        }
        expectEquals (runOfflineRendererCommand (runArguments (cliA)), 0,
                      "run must succeed with honest open statuses");
        expectEquals (runOfflineRendererCommand (runArguments (cliB)), 0,
                      "same-commit repeated run must succeed");
        const auto cliReportA = cliA.getChildFile ("requirements-report.json");
        const auto cliReportB = cliB.getChildFile ("requirements-report.json");
        expect (cliReportA.hasIdenticalContentTo (cliReportB),
                "complete CLI reports must be byte-identical");
        expect (cliA.getChildFile ("metrics.json").existsAsFile()
                    && cliA.getChildFile ("metrics.json").hasIdenticalContentTo (
                        cliB.getChildFile ("metrics.json")),
                "run must emit a canonical byte-repeatable live-registry metrics artifact");
        for (const auto fixtureId : { "native-v2-foundation", "migrated-v2-foundation",
                                      "legacy-contour-foundation" })
            expect (cliA.getChildFile ("renders").getChildFile (fixtureId)
                        .getChildFile ("metrics.json").hasIdenticalContentTo (
                            cliB.getChildFile ("renders").getChildFile (fixtureId)
                                .getChildFile ("metrics.json")),
                    juce::String { fixtureId } + " metrics must be byte-repeatable");
        const auto cliReportPayload = juce::JSON::fromString (cliReportA.loadFileAsString());
        const auto* cliHardGate = std::find_if (
            cliReportPayload.getDynamicObject()->getProperty ("gates").getArray()->begin(),
            cliReportPayload.getDynamicObject()->getProperty ("gates").getArray()->end(),
            [] (const auto& gate) {
                return gate.getDynamicObject()->getProperty ("id").toString()
                    == "hard.registry.count";
            });
        expect (cliHardGate != cliReportPayload.getDynamicObject()
                                      ->getProperty ("gates").getArray()->end()
                    && cliHardGate->getDynamicObject()->getProperty ("status").toString() == "pass"
                    && ! cliHardGate->getDynamicObject()->getProperty ("metric").isVoid()
                    && cliHardGate->getDynamicObject()->getProperty ("artifactPath").toString()
                           == "candidate/metrics.json",
                "hard.registry.count must pass only through typed metric evidence");

        beginTest ("release verification rejects coordinated report forgery");
        expectDiagnostic (verifyReleaseReady (cliReportA), "release.not-ready");
        const auto expectReportMutation = [&] (juce::var mutation, const juce::String& name) {
            const auto file = cliA.getChildFile (name + ".json");
            file.replaceWithText (canonicalJson (mutation), false, false, "\n");
            expectDiagnostic (verifyReleaseReady (file), "release.report-mismatch");
        };
        auto wrongCounts = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongCounts.getDynamicObject()->getProperty ("counts").getDynamicObject()
            ->setProperty ("pass", 127);
        expectReportMutation (wrongCounts, "wrong-counts");
        auto wrongOwner = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongOwner.getDynamicObject()->getProperty ("requirements").getArray()->getReference (0)
            .getDynamicObject()->setProperty ("owner", "forged-owner");
        expectReportMutation (wrongOwner, "wrong-owner");
        auto wrongVerification = juce::JSON::fromString (cliReportA.loadFileAsString());
        juce::Array<juce::var> forgedVerification { "UT", "DOC" };
        wrongVerification.getDynamicObject()->getProperty ("requirements").getArray()
            ->getReference (0).getDynamicObject()->setProperty (
                "verification", forgedVerification);
        expectReportMutation (wrongVerification, "wrong-verification");
        auto wrongGateIds = juce::JSON::fromString (cliReportA.loadFileAsString());
        juce::Array<juce::var> forgedGateIds { "hard.registry.count" };
        wrongGateIds.getDynamicObject()->getProperty ("requirements").getArray()->getReference (0)
            .getDynamicObject()->setProperty ("gateIds", forgedGateIds);
        expectReportMutation (wrongGateIds, "wrong-gate-ids");
        auto wrongReasons = juce::JSON::fromString (cliReportA.loadFileAsString());
        juce::Array<juce::var> forgedReasons { "forged.reason" };
        wrongReasons.getDynamicObject()->getProperty ("requirements").getArray()->getReference (0)
            .getDynamicObject()->setProperty ("reasons", forgedReasons);
        expectReportMutation (wrongReasons, "wrong-reasons");
        auto wrongProvenance = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongProvenance.getDynamicObject()->getProperty ("provenance").getDynamicObject()
            ->setProperty ("sourceCommit", "forged-source-commit");
        expectReportMutation (wrongProvenance, "wrong-provenance");
        auto wrongSourceTree = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongSourceTree.getDynamicObject()->getProperty ("provenance").getDynamicObject()
            ->setProperty ("sourceTree", juce::String { std::string (40, '0') });
        expectReportMutation (wrongSourceTree, "wrong-source-tree");
        auto wrongSourceContent = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongSourceContent.getDynamicObject()->getProperty ("provenance").getDynamicObject()
            ->setProperty ("sourceContent", juce::String { std::string (64, '0') });
        expectReportMutation (wrongSourceContent, "wrong-source-content");
        auto wrongSourceDirty = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongSourceDirty.getDynamicObject()->getProperty ("provenance").getDynamicObject()
            ->setProperty ("sourceDirty", true);
        expectReportMutation (wrongSourceDirty, "wrong-source-dirty");

        auto wrongGateEvidence = juce::JSON::fromString (cliReportA.loadFileAsString());
        wrongGateEvidence.getDynamicObject()->getProperty ("gates").getArray()->getReference (0)
            .getDynamicObject()->setProperty ("reason", "forged.gate-reason");
        const auto wrongGateEvidenceFile = cliA.getChildFile ("wrong-gate-evidence.json");
        wrongGateEvidenceFile.replaceWithText (
            canonicalJson (wrongGateEvidence), false, false, "\n");
        expectDiagnostic (verifyReleaseReady (wrongGateEvidenceFile), "release.gate-evidence");

        auto forgedAllPass = juce::JSON::fromString (cliReportA.loadFileAsString());
        auto* forgedRoot = forgedAllPass.getDynamicObject();
        auto* forgedRows = forgedRoot->getProperty ("requirements").getArray();
        const auto copiedArtifact = forgedRows->getReference (0).getDynamicObject()
                                        ->getProperty ("artifacts");
        for (auto& row : *forgedRows) {
            auto* object = row.getDynamicObject();
            object->setProperty ("status", "pass");
            object->setProperty ("reasons", forgedReasons);
            if (object->getProperty ("artifacts").getArray()->isEmpty())
                object->setProperty ("artifacts", copiedArtifact);
        }
        auto* forgedCounts = forgedRoot->getProperty ("counts").getDynamicObject();
        forgedCounts->setProperty ("awaiting-approved-reference", 0);
        forgedCounts->setProperty ("fail", 0);
        forgedCounts->setProperty ("not-run", 0);
        forgedCounts->setProperty ("pass", 127);
        forgedRoot->setProperty ("releaseReady", true);
        expectReportMutation (forgedAllPass, "forged-all-pass");

        const auto actualReportJson = juce::JSON::fromString (cliReportA.loadFileAsString());
        juce::Array<juce::var> projectedRows;
        for (const auto& reportRow : *actualReportJson.getDynamicObject()
                                          ->getProperty ("requirements").getArray()) {
            const auto* source = reportRow.getDynamicObject();
            auto projected = std::make_unique<juce::DynamicObject>();
            projected->setProperty ("gateIds", source->getProperty ("gateIds"));
            projected->setProperty ("id", source->getProperty ("id"));
            projected->setProperty ("reasons", source->getProperty ("reasons"));
            projected->setProperty ("status", source->getProperty ("status"));
            projectedRows.add (juce::var { projected.release() });
        }
        auto projection = std::make_unique<juce::DynamicObject>();
        projection->setProperty ("requirements", projectedRows);
        projection->setProperty ("schema", "model-d.requirement-status-projection.v1");
        expectEquals (
            canonicalJson (juce::var { projection.release() }),
            sourceRoot.getChildFile ("Tests/reference/expected-f0-requirement-statuses.json")
                .loadFileAsString(),
            "CLI status/reason/gate projection must match the non-self-referential oracle");

        const auto extraEvidence = cliA.getChildFile ("forged-extra.txt");
        expect (extraEvidence.replaceWithText ("not a renderer artifact\n", false, false, "\n"));
        auto extraCandidateJson = juce::JSON::fromString (cliReportA.loadFileAsString());
        for (auto& row : *extraCandidateJson.getDynamicObject()
                              ->getProperty ("requirements").getArray()) {
            auto* object = row.getDynamicObject();
            if (object->getProperty ("id").toString() != "TST-001")
                continue;
            auto extraArtifact = std::make_unique<juce::DynamicObject>();
            extraArtifact->setProperty ("path", "candidate/forged-extra.txt");
            extraArtifact->setProperty ("sha256", juce::String { sha256File (extraEvidence) });
            object->getProperty ("artifacts").getArray()->add (
                juce::var { extraArtifact.release() });
            break;
        }
        const auto extraCandidateReport = cliA.getChildFile ("extra-candidate-report.json");
        expect (extraCandidateReport.replaceWithText (
            canonicalJson (extraCandidateJson), false, false, "\n"));
        expectDiagnostic (verifyReleaseReady (extraCandidateReport), "release.report-mismatch");

        auto missingCandidateJson = juce::JSON::fromString (cliReportA.loadFileAsString());
        for (auto& row : *missingCandidateJson.getDynamicObject()
                              ->getProperty ("requirements").getArray()) {
            auto* object = row.getDynamicObject();
            if (object->getProperty ("id").toString() == "TST-001") {
                object->getProperty ("artifacts").getArray()->clear();
                break;
            }
        }
        const auto missingCandidateReport = cliA.getChildFile ("missing-candidates.json");
        expect (missingCandidateReport.replaceWithText (
            canonicalJson (missingCandidateJson), false, false, "\n"));
        expectDiagnostic (verifyReleaseReady (missingCandidateReport), "release.report-mismatch");

        const auto forgedCandidateRoot = reportRoot.directory.getChildFile ("cli-forged");
        expect (cliA.copyDirectoryTo (forgedCandidateRoot),
                "candidate forgery fixture must copy");
        const auto forgedRender = forgedCandidateRoot.getChildFile (
            "renders/native-v2-foundation/render.json");
        expect (forgedRender.replaceWithText ("forged renderer evidence\n", false, false, "\n"));
        const auto forgedCandidateReport = forgedCandidateRoot.getChildFile (
            "requirements-report.json");
        auto forgedCandidateJson = juce::JSON::fromString (
            forgedCandidateReport.loadFileAsString());
        for (auto& row : *forgedCandidateJson.getDynamicObject()
                              ->getProperty ("requirements").getArray()) {
            auto* object = row.getDynamicObject();
            if (object->getProperty ("id").toString() != "TST-001")
                continue;
            for (auto& artifact : *object->getProperty ("artifacts").getArray()) {
                auto* artifactObject = artifact.getDynamicObject();
                if (artifactObject->getProperty ("path").toString()
                    == "candidate/renders/native-v2-foundation/render.json") {
                    artifactObject->setProperty (
                        "sha256", juce::String { sha256File (forgedRender) });
                    break;
                }
            }
            break;
        }
        expect (forgedCandidateReport.replaceWithText (
            canonicalJson (forgedCandidateJson), false, false, "\n"));
        expectDiagnostic (verifyReleaseReady (forgedCandidateReport), "release.report-mismatch");

        const auto tamperedMetricsRoot = reportRoot.directory.getChildFile (
            "cli-tampered-metrics");
        expect (cliA.copyDirectoryTo (tamperedMetricsRoot),
                "metrics tamper fixture must copy");
        const auto tamperedMetrics = tamperedMetricsRoot.getChildFile ("metrics.json");
        expect (tamperedMetrics.replaceWithText (
            tamperedMetrics.loadFileAsString() + " ", false, false, "\n"));
        expectDiagnostic (verifyReleaseReady (tamperedMetricsRoot.getChildFile (
                              "requirements-report.json")),
                          "release.report-artifact");

        const auto missingMetricsRoot = reportRoot.directory.getChildFile (
            "cli-missing-metrics");
        expect (cliA.copyDirectoryTo (missingMetricsRoot),
                "metrics missing-file fixture must copy");
        expect (missingMetricsRoot.getChildFile ("metrics.json").deleteFile(),
                "metrics missing-file fixture must remove the artifact");
        expectDiagnostic (verifyReleaseReady (missingMetricsRoot.getChildFile (
                              "requirements-report.json")),
                          "release.report-mismatch");

        const auto tamperedRenderMetricsRoot = reportRoot.directory.getChildFile (
            "cli-tampered-render-metrics");
        expect (cliA.copyDirectoryTo (tamperedRenderMetricsRoot),
                "render metrics tamper fixture must copy");
        const auto tamperedRenderMetrics = tamperedRenderMetricsRoot.getChildFile (
            "renders/native-v2-foundation/metrics.json");
        expect (tamperedRenderMetrics.replaceWithText (
            tamperedRenderMetrics.loadFileAsString() + " ", false, false, "\n"));
        expectDiagnostic (verifyReleaseReady (tamperedRenderMetricsRoot.getChildFile (
                              "requirements-report.json")),
                          "release.report-artifact");

        const std::vector<std::string> verifyArguments {
            "verify-release", "--report", cliReportA.getFullPathName().toStdString(),
        };
        expectEquals (runOfflineRendererCommand (verifyArguments),
                      3, "release verification must use exit 3 for an honest open report");
    }

private:
    template <typename T>
    void expectDiagnostic (const ReferenceHarness::LoadResult<T>& result,
                           const std::string_view code)
    {
        const auto actual = result.diagnostics.empty() ? "<none>" : result.diagnostics.front().code;
        expect (! result.ok() && ! result.diagnostics.empty() && actual == code,
                "negative case must report stable diagnostic " + std::string { code }
                    + ", got " + actual);
    }

    void expectMapDiagnostic (const juce::File& sourceRoot,
                              const ReferenceHarness::AcceptanceManifest& acceptance,
                              const juce::String& contents,
                              const std::string_view code)
    {
        const TemporaryDirectory temporary { "model-d-requirement-negative" };
        expect (temporary.isOwned(), "requirement negative root must be owned");
        if (! temporary.isOwned())
            return;
        const auto mapFile = temporary.directory.getChildFile ("requirement-map.json");
        mapFile.replaceWithText (contents);
        expectDiagnostic (ReferenceHarness::loadRequirementMap (
                              sourceRoot, mapFile,
                              sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md"),
                              acceptance),
                          code);
    }

    void expectRequirement (const ReferenceHarness::RequirementReport& report,
                            const std::string_view id,
                            const ReferenceHarness::Status status)
    {
        const auto found = std::find_if (
            report.requirements.begin(), report.requirements.end(), [&] (const auto& requirement) {
                return requirement.definition.id == id;
            });
        expect (found != report.requirements.end() && found->status == status,
                std::string { id } + " must have status " + ReferenceHarness::statusName (status));
    }
};

ReferenceRequirementTest referenceRequirementTest;

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
