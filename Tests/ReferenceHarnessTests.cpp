#include "ReferenceData.h"
#include "OfflineRenderer.h"
#include "PluginProcessor.h"

#include <array>
#include <cmath>
#include <filesystem>

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
            expect (result.reproducibility.inputHashes.contains ("audio"),
                    "generated audio input hash must be recorded");
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
        const auto secondRun = ReferenceHarness::renderFixture (fixtures.front());
        expect (secondRun.ok(), "repeated native render must succeed");
        if (secondRun.value.has_value() && secondRun.value->size() == firstRun.value->size()) {
            const auto expectedMain = firstRun.value->front().reproducibility.outputHashes.at ("main");
            const auto expectedPhones = firstRun.value->front().reproducibility.outputHashes.at ("phones");
            const auto expectedControl = firstRun.value->front().reproducibility.outputHashes.at ("control");
            for (size_t pattern = 0; pattern < firstRun.value->size(); ++pattern) {
                const auto& first = firstRun.value->at (pattern);
                const auto& second = secondRun.value->at (pattern);
                expect (first.reproducibility.outputHashes.at ("main") == expectedMain
                            && second.reproducibility.outputHashes.at ("main") == expectedMain,
                        "main audio hash must be repeatable and host-block invariant");
                expect (first.reproducibility.outputHashes.at ("phones") == expectedPhones
                            && second.reproducibility.outputHashes.at ("phones") == expectedPhones,
                        "phones audio hash must be repeatable and host-block invariant");
                expect (first.reproducibility.outputHashes.at ("control") == expectedControl
                            && second.reproducibility.outputHashes.at ("control") == expectedControl,
                        "control trace hash must be repeatable and host-block invariant");
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
                                    R"("parameterId": "filterCutoff")",
                                    R"("parameterId": "unknownParameter")"),
                                "fixture.parameter");
        expectFixtureDiagnostic (sourceRoot, validText.replaceFirstOccurrenceOf (
                                    R"("normalizedValue": 0.25)",
                                    R"("normalizedValue": 1e999)"),
                                "fixture.automation-value");

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
        const auto writtenA = ReferenceHarness::writeCandidateArtifacts (
            fixtures.front(), *firstRun.value, candidateA);
        const auto writtenB = secondRun.value.has_value()
                            ? ReferenceHarness::writeCandidateArtifacts (
                                  fixtures.front(), *secondRun.value, candidateB)
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
        expect (sourceRoot.getChildFile ("Tests/fixtures/state/native-default-state-v2.xml")
                    .copyFileTo (stateDirectory.getChildFile ("native-default-state-v2.xml")),
                "negative fixture must copy its state");
        const auto fixtureFile = fixtureRoot.directory.getChildFile ("fixture.json");
        fixtureFile.replaceWithText (contents);
        expectDiagnostic (ReferenceHarness::loadRenderFixture (fixtureRoot.directory, fixtureFile), code);
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
