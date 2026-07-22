#include "ReferenceData.h"

#include <filesystem>

namespace {

constexpr auto sourceRootPath = SYNTH_SOURCE_ROOT;

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

        beginTest ("bounded paths report stable negative diagnostics");
        const auto temporaryRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                       .getChildFile ("model-d-reference-harness-contract");
        temporaryRoot.deleteRecursively();
        temporaryRoot.createDirectory();
        const auto safeFile = temporaryRoot.getChildFile ("safe.txt");
        safeFile.replaceWithText ("safe");

        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "/safe.txt"),
                          "path.absolute");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "../safe.txt"),
                          "path.traversal");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "safe\\\\txt"),
                          "path.backslash");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "missing.txt"),
                          "path.missing");
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "."),
                          "path.component");

        const auto directory = temporaryRoot.getChildFile ("directory");
        directory.createDirectory();
        expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "directory"),
                          "path.non-regular");

        const auto outside = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("model-d-reference-harness-outside.txt");
        outside.replaceWithText ("outside");
        const auto link = temporaryRoot.getChildFile ("escape-link");
        std::error_code symlinkError;
        std::filesystem::create_symlink (outside.getFullPathName().toStdString(),
                                         link.getFullPathName().toStdString(), symlinkError);
        expect (! symlinkError, "symlink fixture must be creatable");
        if (! symlinkError)
            expectDiagnostic (ReferenceHarness::resolveBoundedRegularFile (temporaryRoot, "escape-link"),
                              "path.escape");

        beginTest ("fixture-index mutations report stable diagnostics");
        const auto temporaryIndex = temporaryRoot.getChildFile ("fixture-index-v1.json");
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

        temporaryRoot.deleteRecursively();
        outside.deleteFile();
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
};

ReferenceHarnessTest referenceHarnessTest;

} // namespace

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.runAllTests();
    for (int index = 0; index < runner.getNumResults(); ++index) {
        if (const auto* result = runner.getResult (index); result != nullptr && result->failures != 0)
            return 1;
    }
    return 0;
}
