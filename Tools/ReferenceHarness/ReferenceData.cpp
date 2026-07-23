#include "ReferenceData.h"

#include "AnalyzerRegistry.h"
#include "ParameterRegistry.h"
#include "StateContract.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <limits>
#include <set>
#include <tuple>
#include <utility>

namespace ReferenceHarness {
namespace {

constexpr auto fixtureSchema = "model-d.fixture-index.v1";
constexpr auto fixtureVersion = 1;
constexpr auto frozenArtifactCount = 9;
constexpr auto renderFixtureSchema = "model-d.render-fixture.v1";
constexpr std::uint64_t maximumRenderSamples = 96'000u * 60u;
// The renderer must never ask the processor for a block larger than its public
// fixed-capacity stage-buffer contract.
constexpr int maximumRenderBlockSize = 2048;
constexpr double maximumInputMagnitude = 1.0;
constexpr int fixtureAnalysisRequestVersion = 1;

struct ExpectedFrozenArtifact {
    const char* id;
    const char* relativePath;
    const char* sha256;
    std::initializer_list<const char*> requirements;
};

const std::array<ExpectedFrozenArtifact, frozenArtifactCount> expectedFrozenArtifacts {{
    { "legacy-parameter-inventory", "Tests/fixtures/parameters/legacy-parameter-inventory.json",
      "7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d", { "PAR-001" } },
    { "parameter-registry-v2", "Tests/fixtures/parameters/parameter-registry-v2.json",
      "2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284",
      { "PAR-001", "PAR-002", "PAR-003", "PAR-006", "PAR-007" } },
    { "parameter-snapshot-v2", "Tests/fixtures/parameters/parameter-snapshot-v2.json",
      "cbfefbf2e918818fed1fd6b340ca4c015981d6e020080a7b71bbfd006e398f16", { "PAR-009" } },
    { "contour-conversion-trace", "Tests/fixtures/state/contour-routing-conversion-trace.json",
      "ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c", { "PAR-004" } },
    { "legacy-default-state", "Tests/fixtures/state/legacy-default-state.xml",
      "07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21", { "PAR-003", "PAR-005" } },
    { "legacy-representative-state", "Tests/fixtures/state/legacy-representative-state.xml",
      "e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa", { "PAR-003", "PAR-005" } },
    { "native-default-state-v2", "Tests/fixtures/state/native-default-state-v2.xml",
      "ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507", { "PAR-003", "PAR-005" } },
    { "migrated-default-state-v2", "Tests/fixtures/state/migrated-default-state-v2.xml",
      "4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958", { "PAR-003", "PAR-005" } },
    { "migrated-representative-v2", "Tests/fixtures/state/migrated-representative-state-v2.xml",
      "f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40", { "PAR-003", "PAR-005" } },
}};

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

bool isLowercaseSha256 (const std::string& hash)
{
    return hash.size() == 64
        && std::all_of (hash.begin(), hash.end(), [] (const unsigned char character) {
               return std::isdigit (character) != 0
                   || (character >= 'a' && character <= 'f');
           });
}

bool metricBelongsToAnalyzer (const std::string_view analyzer,
                              const std::string_view metric)
{
    if (analyzer == "signal.stats.v1")
        return metric == "sample-count" || metric == "finite-count"
            || metric == "minimum" || metric == "maximum"
            || metric == "peak-absolute" || metric == "mean" || metric == "rms"
            || metric == "maximum-first-difference";
    if (analyzer == "control.step.v1")
        return metric == "first-change-sample" || metric == "settled-sample"
            || metric == "monotonic" || metric == "overshoot"
            || metric == "maximum-per-sample-movement"
            || metric == "floating-allowance";
    if (analyzer == "audio.click.v1")
        return metric == "maximum-first-difference" || metric == "pre-rms"
            || metric == "post-rms" || metric == "peak-over-steady-state"
            || metric == "finite-count";
    return false;
}

double physicalValueFor (const ParameterRegistry::Key key, const double normalized)
{
    const auto& descriptor = ParameterRegistry::descriptor (key);
    const juce::NormalisableRange<float> range {
        descriptor.rangeStart, descriptor.rangeEnd, descriptor.rangeInterval,
        descriptor.rangeSkew, descriptor.symmetricSkew,
    };
    return static_cast<double> (range.convertFrom0to1 (static_cast<float> (normalized)));
}

bool hasDisallowedPathSyntax (std::string_view relativePath, std::string& code)
{
    if (relativePath.empty()) {
        code = "path.empty";
        return true;
    }

    if (relativePath.find ('\\') != std::string_view::npos) {
        code = "path.backslash";
        return true;
    }

    if (juce::File::isAbsolutePath (juce::String::fromUTF8 (relativePath.data(),
                                                             static_cast<int> (relativePath.size())))
        || (relativePath.size() >= 2 && std::isalpha (static_cast<unsigned char> (relativePath[0])) != 0
            && relativePath[1] == ':')
        || relativePath.starts_with ("//")) {
        code = "path.absolute";
        return true;
    }

    size_t start = 0;
    while (start <= relativePath.size()) {
        const auto end = relativePath.find ('/', start);
        const auto component = relativePath.substr (start, end - start);
        if (component.empty() || component == ".") {
            code = "path.component";
            return true;
        }
        if (component == "..") {
            code = "path.traversal";
            return true;
        }
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }

    return false;
}

juce::var canonicalizeJson (const juce::var& value)
{
    if (const auto* array = value.getArray()) {
        juce::Array<juce::var> canonicalArray;
        canonicalArray.ensureStorageAllocated (array->size());
        for (const auto& element : *array)
            canonicalArray.add (canonicalizeJson (element));
        return canonicalArray;
    }

    if (const auto* object = value.getDynamicObject()) {
        std::vector<std::pair<juce::String, juce::var>> properties;
        const auto& sourceProperties = object->getProperties();
        properties.reserve (static_cast<size_t> (sourceProperties.size()));
        for (int index = 0; index < sourceProperties.size(); ++index)
            properties.emplace_back (sourceProperties.getName (index).toString(),
                                     sourceProperties.getValueAt (index));

        std::sort (properties.begin(), properties.end(), [] (const auto& left, const auto& right) {
            return left.first.compare (right.first) < 0;
        });

        auto canonicalObject = std::make_unique<juce::DynamicObject>();
        for (const auto& [name, property] : properties)
            canonicalObject->setProperty (name, canonicalizeJson (property));
        return juce::var { canonicalObject.release() };
    }

    return value;
}

const juce::var* requiredProperty (const juce::DynamicObject& object, const char* name)
{
    return object.getProperties().getVarPointer (juce::Identifier { name });
}

bool readRequiredString (const juce::DynamicObject& object,
                         const char* name,
                         std::string& destination)
{
    const auto* property = requiredProperty (object, name);
    if (property == nullptr || ! property->isString() || property->toString().isEmpty())
        return false;
    destination = property->toString().toStdString();
    return true;
}

bool readUnsignedInteger (const juce::DynamicObject& object,
                          const char* name,
                          std::uint64_t& destination)
{
    const auto* property = requiredProperty (object, name);
    if (property == nullptr || (! property->isInt() && ! property->isInt64()))
        return false;
    const auto value = static_cast<juce::int64> (*property);
    if (value < 0)
        return false;
    destination = static_cast<std::uint64_t> (value);
    return true;
}

bool readFiniteNumber (const juce::DynamicObject& object,
                       const char* name,
                       double& destination)
{
    const auto* property = requiredProperty (object, name);
    if (property == nullptr
        || (! property->isInt() && ! property->isInt64() && ! property->isDouble()))
        return false;
    destination = static_cast<double> (*property);
    return std::isfinite (destination);
}

bool readNonemptyStringArray (const juce::DynamicObject& object,
                              const char* name,
                              std::vector<std::string>& destination)
{
    const auto* property = requiredProperty (object, name);
    const auto* array = property == nullptr ? nullptr : property->getArray();
    if (array == nullptr || array->isEmpty())
        return false;
    destination.clear();
    destination.reserve (static_cast<size_t> (array->size()));
    for (const auto& entry : *array) {
        if (! entry.isString() || entry.toString().isEmpty())
            return false;
        destination.push_back (entry.toString().toStdString());
    }
    return true;
}

std::optional<ParameterRegistry::Key> parameterKeyForId (const std::string_view id)
{
    const auto descriptors = ParameterRegistry::descriptors();
    for (size_t index = 0; index < descriptors.size(); ++index)
        if (descriptors[index].id == id)
            return static_cast<ParameterRegistry::Key> (index);
    return std::nullopt;
}

std::optional<double> stateParameterValue (const juce::XmlElement& state,
                                           const std::string_view id)
{
    const auto* parameters = state.getChildByName ("parameters");
    for (auto* parameter = parameters == nullptr ? nullptr
                                                  : parameters->getFirstChildElement();
         parameter != nullptr; parameter = parameter->getNextElement())
        if (parameter->hasTagName ("PARAM")
            && parameter->getStringAttribute ("id").toStdString() == id)
            return parameter->getDoubleAttribute ("value");
    return std::nullopt;
}

bool readArtifact (const juce::var& value, IndexedArtifact& artifact)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr
        || ! readRequiredString (*object, "id", artifact.id)
        || ! readRequiredString (*object, "relativePath", artifact.relativePath)
        || ! readRequiredString (*object, "sha256", artifact.sha256))
        return false;

    const auto* requirements = requiredProperty (*object, "requirements");
    const auto* requirementArray = requirements == nullptr ? nullptr : requirements->getArray();
    if (requirementArray == nullptr || requirementArray->isEmpty())
        return false;

    artifact.requirements.clear();
    artifact.requirements.reserve (static_cast<size_t> (requirementArray->size()));
    for (const auto& requirement : *requirementArray) {
        if (! requirement.isString() || requirement.toString().isEmpty())
            return false;
        artifact.requirements.push_back (requirement.toString().toStdString());
    }

    return true;
}

bool readArtifactSection (const juce::DynamicObject& root,
                          const char* name,
                          std::vector<IndexedArtifact>& artifacts)
{
    const auto* section = requiredProperty (root, name);
    const auto* array = section == nullptr ? nullptr : section->getArray();
    if (array == nullptr)
        return false;

    artifacts.clear();
    artifacts.reserve (static_cast<size_t> (array->size()));
    for (const auto& value : *array) {
        IndexedArtifact artifact;
        if (! readArtifact (value, artifact))
            return false;
        artifacts.push_back (std::move (artifact));
    }
    return true;
}

std::optional<Diagnostic> semanticFailure (const char* code, const char* message)
{
    return Diagnostic { code, message };
}

std::optional<Diagnostic> validateDescriptorFixture (
    const juce::File& file,
    const char* schema,
    const char* entriesName,
    std::span<const ParameterRegistry::Descriptor> expectedDescriptors,
    const bool requireVersionHint,
    const char* code)
{
    juce::var parsed;
    if (juce::JSON::parse (file.loadFileAsString(), parsed).failed())
        return semanticFailure (code, "parameter fixture must be valid JSON");

    const auto* root = parsed.getDynamicObject();
    const auto* schemaProperty = root == nullptr ? nullptr : requiredProperty (*root, "schema");
    const auto* entries = root == nullptr ? nullptr : requiredProperty (*root, entriesName);
    const auto* entryArray = entries == nullptr ? nullptr : entries->getArray();
    if (schemaProperty == nullptr || ! schemaProperty->isString()
        || schemaProperty->toString() != schema || entryArray == nullptr
        || entryArray->size() != static_cast<int> (expectedDescriptors.size()))
        return semanticFailure (code, "parameter fixture must match the live descriptor count and schema");

    for (size_t position = 0; position < expectedDescriptors.size(); ++position) {
        const auto* entry = entryArray->getReference (static_cast<int> (position)).getDynamicObject();
        const auto* index = entry == nullptr ? nullptr : requiredProperty (*entry, "index");
        const auto* id = entry == nullptr ? nullptr : requiredProperty (*entry, "id");
        if (index == nullptr || ! index->isInt() || static_cast<int> (*index) != static_cast<int> (position)
            || id == nullptr || ! id->isString()
            || id->toString().toStdString() != expectedDescriptors[position].id)
            return semanticFailure (code, "parameter fixture descriptor order does not match the live registry");

        if (requireVersionHint) {
            const auto* versionHint = requiredProperty (*entry, "version_hint");
            if (versionHint == nullptr || ! versionHint->isInt()
                || static_cast<int> (*versionHint) != expectedDescriptors[position].versionHint)
                return semanticFailure (code, "parameter fixture version hints do not match the live registry");
        }
    }

    return std::nullopt;
}

std::optional<Diagnostic> validateParameterFixtures (const juce::File& sourceRoot)
{
    const auto descriptors = ParameterRegistry::descriptors();
    if (descriptors.size() != ParameterRegistry::parameterCount)
        return semanticFailure ("semantic.registry", "live parameter registry must contain exactly 48 descriptors");

    if (const auto diagnostic = validateDescriptorFixture (
            sourceRoot.getChildFile ("Tests/fixtures/parameters/parameter-registry-v2.json"),
            "model-d.parameter-registry.v2", "parameters", descriptors, true, "semantic.registry");
        diagnostic.has_value())
        return diagnostic;

    std::vector<ParameterRegistry::Descriptor> legacyDescriptors;
    legacyDescriptors.reserve (descriptors.size());
    for (const auto& descriptor : descriptors)
        if (descriptor.versionHint == 0)
            legacyDescriptors.push_back (descriptor);
    if (const auto diagnostic = validateDescriptorFixture (
            sourceRoot.getChildFile ("Tests/fixtures/parameters/legacy-parameter-inventory.json"),
            "model-d.legacy-parameter-inventory.v1", "parameters", legacyDescriptors, true, "semantic.inventory");
        diagnostic.has_value())
        return diagnostic;

    return validateDescriptorFixture (
        sourceRoot.getChildFile ("Tests/fixtures/parameters/parameter-snapshot-v2.json"),
        "model-d.parameter-snapshot.v2", "fields", descriptors, false, "semantic.snapshot");
}

const char* contourContractName (const StateContract::ContourContract contract)
{
    return contract == StateContract::ContourContract::legacyCrossedContours
             ? "legacyCrossedContours"
             : "canonicalContours";
}

std::optional<Diagnostic> validateStateFixture (
    const juce::File& file,
    const bool isLegacy,
    const StateContract::ContourContract expectedContour = StateContract::ContourContract::canonicalContours)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return semanticFailure ("semantic.state", "state fixture must be valid XML");

    if (isLegacy) {
        if (! xml->hasTagName ("Parameters") || xml->hasAttribute ("stateVersion"))
            return semanticFailure ("semantic.state", "legacy state fixture must retain the Parameters root");
        return std::nullopt;
    }

    if (! xml->hasTagName ("modelDState")
        || xml->getStringAttribute ("stateVersion") != juce::String (StateContract::currentVersion))
        return semanticFailure ("semantic.state", "v2 state fixture must retain the current modelDState root and version");

    const auto* compatibility = xml->getChildByName ("compatibility");
    if (compatibility == nullptr
        || compatibility->getStringAttribute ("contourContract") != contourContractName (expectedContour))
        return semanticFailure ("semantic.state", "v2 state fixture has an incompatible contour marker");
    return std::nullopt;
}

std::optional<Diagnostic> validateStateFixtures (const juce::File& sourceRoot)
{
    const auto validate = [&] (const char* path,
                               const bool isLegacy,
                               const StateContract::ContourContract contour) {
        return validateStateFixture (sourceRoot.getChildFile (path), isLegacy, contour);
    };

    if (const auto diagnostic = validate ("Tests/fixtures/state/legacy-default-state.xml", true,
                                          StateContract::ContourContract::legacyCrossedContours);
        diagnostic.has_value())
        return diagnostic;
    if (const auto diagnostic = validate ("Tests/fixtures/state/legacy-representative-state.xml", true,
                                          StateContract::ContourContract::legacyCrossedContours);
        diagnostic.has_value())
        return diagnostic;
    if (const auto diagnostic = validate ("Tests/fixtures/state/native-default-state-v2.xml", false,
                                          StateContract::ContourContract::canonicalContours);
        diagnostic.has_value())
        return diagnostic;
    if (const auto diagnostic = validate ("Tests/fixtures/state/migrated-default-state-v2.xml", false,
                                          StateContract::ContourContract::legacyCrossedContours);
        diagnostic.has_value())
        return diagnostic;
    return validate ("Tests/fixtures/state/migrated-representative-state-v2.xml", false,
                     StateContract::ContourContract::legacyCrossedContours);
}

std::optional<Diagnostic> validateContourFixture (const juce::File& sourceRoot)
{
    const auto file = sourceRoot.getChildFile ("Tests/fixtures/state/contour-routing-conversion-trace.json");
    juce::var parsed;
    if (juce::JSON::parse (file.loadFileAsString(), parsed).failed())
        return semanticFailure ("semantic.contour", "contour fixture must be valid JSON");

    const auto* root = parsed.getDynamicObject();
    const auto* schema = root == nullptr ? nullptr : requiredProperty (*root, "schema");
    const auto* canonical = root == nullptr ? nullptr : requiredProperty (*root, "canonicalContours");
    const auto* legacy = root == nullptr ? nullptr : requiredProperty (*root, "legacyCrossedContours");
    const auto* conversion = root == nullptr ? nullptr : requiredProperty (*root, "confirmedConversion");
    const auto* conversionObject = conversion == nullptr ? nullptr : conversion->getDynamicObject();
    const auto* marker = conversionObject == nullptr ? nullptr : requiredProperty (*conversionObject, "marker");
    if (schema == nullptr || ! schema->isString() || schema->toString() != "model-d-contour-routing-trace-v1"
        || canonical == nullptr || canonical->getDynamicObject() == nullptr
        || legacy == nullptr || legacy->getDynamicObject() == nullptr
        || marker == nullptr || ! marker->isString()
        || marker->toString() != contourContractName (StateContract::ContourContract::canonicalContours))
        return semanticFailure ("semantic.contour", "contour trace must retain canonical and legacy compatibility markers");
    return std::nullopt;
}

std::optional<Diagnostic> validateLiveContracts (const juce::File& sourceRoot)
{
    if (const auto diagnostic = validateParameterFixtures (sourceRoot); diagnostic.has_value())
        return diagnostic;
    if (const auto diagnostic = validateStateFixtures (sourceRoot); diagnostic.has_value())
        return diagnostic;
    return validateContourFixture (sourceRoot);
}

} // namespace

std::string statusName (const Status status)
{
    switch (status) {
        case Status::pass: return "pass";
        case Status::fail: return "fail";
        case Status::notRun: return "not-run";
        case Status::awaitingApprovedReference: return "awaiting-approved-reference";
    }

    return "unknown";
}

std::string sha256File (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};
    return juce::SHA256 { file }.toHexString().toStdString();
}

bool isPortableIdentifierComponent (const std::string_view value) noexcept
{
    if (value.empty() || value.size() > 96)
        return false;
    return std::all_of (value.begin(), value.end(), [] (const unsigned char character) {
        return (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9') || character == '-';
    }) && value.front() != '-' && value.back() != '-';
}

LoadResult<juce::File> resolveBoundedRegularFile (const juce::File& root,
                                                   const std::string_view relativePath)
{
    std::string syntaxCode;
    if (hasDisallowedPathSyntax (relativePath, syntaxCode))
        return failure<juce::File> (syntaxCode, "fixture path must be a bounded relative path");

    std::error_code error;
    const auto canonicalRoot = std::filesystem::canonical (root.getFullPathName().toStdString(), error);
    if (error)
        return failure<juce::File> ("path.root", "fixture root cannot be canonicalized");

    const auto candidate = canonicalRoot / std::filesystem::path { std::string { relativePath } };
    if (! std::filesystem::exists (candidate, error) || error)
        return failure<juce::File> ("path.missing", "fixture path does not exist");
    if (! std::filesystem::is_regular_file (candidate, error) || error)
        return failure<juce::File> ("path.non-regular", "fixture path is not a regular file");

    const auto canonicalCandidate = std::filesystem::canonical (candidate, error);
    if (error)
        return failure<juce::File> ("path.missing", "fixture path cannot be canonicalized");

    const auto relativeCandidate = canonicalCandidate.lexically_relative (canonicalRoot);
    if (relativeCandidate.empty() || relativeCandidate == "."
        || *relativeCandidate.begin() == std::filesystem::path { ".." })
        return failure<juce::File> ("path.escape", "fixture path resolves outside its root");

    return { juce::File { canonicalCandidate.string() }, {} };
}

juce::String canonicalJson (const juce::var& value)
{
    const auto options = juce::JSON::FormatOptions {}
                             .withSpacing (juce::JSON::Spacing::none);
    return juce::JSON::toString (canonicalizeJson (value), options) + "\n";
}

juce::var analysisRequestsJson (const std::span<const FixtureAnalysisRequest> requests)
{
    juce::Array<juce::var> values;
    for (const auto& request : requests) {
        auto analyzer = std::make_unique<juce::DynamicObject>();
        analyzer->setProperty ("id", juce::String { request.analyzer.id });
        analyzer->setProperty ("version", request.analyzer.version);
        auto input = std::make_unique<juce::DynamicObject>();
        if (request.inputKind == AnalysisInputKind::audio) {
            input->setProperty ("channel", request.channel);
            input->setProperty ("kind", "audio");
            input->setProperty ("tap", request.audioTap == AudioTap::main ? "main" : "phones");
        } else {
            input->setProperty ("domain", request.controlDomain == ControlDomain::normalized
                                              ? "normalized" : "physical");
            input->setProperty ("kind", "control");
            input->setProperty ("parameterId", juce::String { request.parameterId });
        }
        auto event = std::make_unique<juce::DynamicObject>();
        event->setProperty ("originSample", static_cast<juce::int64> (request.eventSample));
        event->setProperty ("windowSamples", static_cast<juce::int64> (request.windowSamples));
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("analyzer", juce::var { analyzer.release() });
        object->setProperty ("event", juce::var { event.release() });
        object->setProperty ("input", juce::var { input.release() });
        object->setProperty ("metric", juce::String { request.metric });
        object->setProperty ("requestId", juce::String { request.id });
        if (request.start.has_value())
            object->setProperty ("start", *request.start);
        if (request.target.has_value())
            object->setProperty ("target", *request.target);
        object->setProperty ("version", request.version);
        values.add (juce::var { object.release() });
    }
    return juce::var { values };
}

LoadResult<FixtureIndex> loadFixtureIndex (const juce::File& sourceRoot,
                                           const juce::File& indexFile)
{
    if (! indexFile.existsAsFile())
        return failure<FixtureIndex> ("index.missing", "fixture index does not exist");

    juce::var parsed;
    if (juce::JSON::parse (indexFile.loadFileAsString(), parsed).failed())
        return failure<FixtureIndex> ("index.parse", "fixture index is not valid JSON");

    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return failure<FixtureIndex> ("index.shape", "fixture index root must be an object");

    FixtureIndex index;
    if (! readRequiredString (*root, "schema", index.schema))
        return failure<FixtureIndex> ("index.schema", "fixture index schema is required");
    if (index.schema != fixtureSchema)
        return failure<FixtureIndex> ("index.schema", "fixture index schema is unsupported");

    const auto* version = requiredProperty (*root, "version");
    if (version == nullptr || ! version->isInt() || static_cast<int> (*version) != fixtureVersion)
        return failure<FixtureIndex> ("index.version", "fixture index version is unsupported");
    index.version = static_cast<int> (*version);

    if (! readArtifactSection (*root, "frozenArtifacts", index.frozenArtifacts)
        || ! readArtifactSection (*root, "renderFixtures", index.renderFixtures)
        || ! readArtifactSection (*root, "smoothingFixtures", index.smoothingFixtures))
        return failure<FixtureIndex> ("index.artifact", "fixture index artifacts are malformed");

    if (index.frozenArtifacts.size() != frozenArtifactCount)
        return failure<FixtureIndex> ("index.frozen-count", "fixture index must contain nine frozen artifacts");

    std::set<std::string> identifiers;
    std::set<std::string> paths;
    const auto validateArtifactIdentity = [&] (const std::vector<IndexedArtifact>& artifacts) -> std::optional<Diagnostic> {
        for (const auto& artifact : artifacts) {
            if (! identifiers.insert (artifact.id).second)
                return Diagnostic { "index.duplicate-id", "fixture artifact identifiers must be unique" };
            if (! paths.insert (artifact.relativePath).second)
                return Diagnostic { "index.duplicate-path", "fixture artifact paths must be unique" };
            if (! isLowercaseSha256 (artifact.sha256))
                return Diagnostic { "index.hash-format", "fixture artifact hash must be lowercase SHA-256" };
        }
        return std::nullopt;
    };

    const auto validateArtifactBytes = [&] (const std::vector<IndexedArtifact>& artifacts) -> std::optional<Diagnostic> {
        for (const auto& artifact : artifacts) {
            const auto resolved = resolveBoundedRegularFile (sourceRoot, artifact.relativePath);
            if (! resolved.ok())
                return resolved.diagnostics.front();
            if (sha256File (*resolved.value) != artifact.sha256)
                return Diagnostic { "index.sha256", "fixture artifact hash does not match its bytes" };
        }
        return std::nullopt;
    };

    for (const auto* section : { &index.frozenArtifacts, &index.renderFixtures, &index.smoothingFixtures }) {
        if (const auto diagnostic = validateArtifactIdentity (*section); diagnostic.has_value())
            return { std::nullopt, { std::move (*diagnostic) } };
    }
    for (const auto* section : { &index.frozenArtifacts, &index.renderFixtures, &index.smoothingFixtures }) {
        if (const auto diagnostic = validateArtifactBytes (*section); diagnostic.has_value())
            return { std::nullopt, { std::move (*diagnostic) } };
    }

    if (const auto diagnostic = validateLiveContracts (sourceRoot); diagnostic.has_value())
        return { std::nullopt, { std::move (*diagnostic) } };

    for (size_t indexPosition = 0; indexPosition < expectedFrozenArtifacts.size(); ++indexPosition) {
        const auto& actual = index.frozenArtifacts[indexPosition];
        const auto& expected = expectedFrozenArtifacts[indexPosition];
        if (actual.id != expected.id
            || actual.relativePath != expected.relativePath
            || actual.sha256 != expected.sha256
            || actual.requirements.size() != expected.requirements.size())
            return failure<FixtureIndex> ("index.frozen-set",
                                          "fixture index frozen artifacts must match the approved set");

        size_t requirementPosition = 0;
        for (const auto* requirement : expected.requirements) {
            if (actual.requirements[requirementPosition++] != requirement)
                return failure<FixtureIndex> ("index.frozen-set",
                                              "fixture index frozen artifacts must match the approved set");
        }
    }

    for (const auto& indexedFixture : index.renderFixtures) {
        const auto fixture = loadIndexedRenderFixture (sourceRoot, indexedFixture);
        if (! fixture.ok())
            return { std::nullopt, fixture.diagnostics };
    }

    return { std::move (index), {} };
}

LoadResult<RenderFixture> loadRenderFixture (const juce::File& sourceRoot,
                                             const juce::File& fixtureFile)
{
    if (! fixtureFile.existsAsFile())
        return failure<RenderFixture> ("fixture.missing", "render fixture does not exist");

    juce::var parsed;
    if (juce::JSON::parse (fixtureFile.loadFileAsString(), parsed).failed())
        return failure<RenderFixture> ("fixture.parse", "render fixture is not valid JSON");
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return failure<RenderFixture> ("fixture.shape", "render fixture root must be an object");

    std::string schema;
    RenderFixture fixture;
    fixture.fixtureFile = fixtureFile;
    std::error_code fixturePathError;
    const auto canonicalSourceRoot = std::filesystem::canonical (
        sourceRoot.getFullPathName().toStdString(), fixturePathError);
    const auto canonicalFixture = std::filesystem::canonical (
        fixtureFile.getFullPathName().toStdString(), fixturePathError);
    const auto relativeFixture = canonicalFixture.lexically_relative (canonicalSourceRoot);
    if (fixturePathError || relativeFixture.empty() || relativeFixture == "."
        || *relativeFixture.begin() == std::filesystem::path { ".." })
        return failure<RenderFixture> (
            "fixture.path", "render fixture must be a regular file beneath its source root");
    fixture.fixtureRelativePath = relativeFixture.generic_string();
    if (! readRequiredString (*root, "schema", schema) || schema != renderFixtureSchema)
        return failure<RenderFixture> ("fixture.schema", "render fixture schema is unsupported");
    if (! readRequiredString (*root, "id", fixture.id)
        || ! isPortableIdentifierComponent (fixture.id))
        return failure<RenderFixture> (
            "fixture.id", "render fixture identifier must be one portable filename component");

    const auto* stateValue = requiredProperty (*root, "state");
    const auto* state = stateValue == nullptr ? nullptr : stateValue->getDynamicObject();
    std::string stateKind;
    std::string statePath;
    std::string contourContract;
    if (state == nullptr || ! readRequiredString (*state, "kind", stateKind)
        || stateKind != "hostState")
        return failure<RenderFixture> ("fixture.state-source",
                                       "render state source must be hostState");
    if (! readRequiredString (*state, "path", statePath)
        || ! readRequiredString (*state, "sha256", fixture.stateSha256)
        || ! isLowercaseSha256 (fixture.stateSha256))
        return failure<RenderFixture> ("fixture.state", "render state path and hash are required");

    const auto stateFile = resolveBoundedRegularFile (sourceRoot, statePath);
    if (! stateFile.ok())
        return { std::nullopt, stateFile.diagnostics };
    fixture.stateFile = *stateFile.value;
    if (sha256File (fixture.stateFile) != fixture.stateSha256)
        return failure<RenderFixture> ("fixture.state-hash", "render state hash does not match its bytes");

    const auto* stateVersion = requiredProperty (*state, "version");
    if (stateVersion == nullptr || ! stateVersion->isInt()
        || static_cast<int> (*stateVersion) != StateContract::currentVersion)
        return failure<RenderFixture> ("fixture.state-version", "render state version is unsupported");
    if (! readRequiredString (*state, "contourContract", contourContract)
        || (contourContract != "canonicalContours"
            && contourContract != "legacyCrossedContours"))
        return failure<RenderFixture> ("fixture.contour", "render contour contract is unsupported");
    fixture.expectedContourContract = contourContract == "legacyCrossedContours"
                                          ? StateContract::ContourContract::legacyCrossedContours
                                          : StateContract::ContourContract::canonicalContours;

    const auto stateXml = juce::XmlDocument::parse (fixture.stateFile);
    const auto* compatibility = stateXml == nullptr ? nullptr
                                                     : stateXml->getChildByName ("compatibility");
    if (stateXml == nullptr || ! stateXml->hasTagName ("modelDState")
        || stateXml->getIntAttribute ("stateVersion") != StateContract::currentVersion
        || compatibility == nullptr
        || compatibility->getStringAttribute ("contourContract") != juce::String { contourContract })
        return failure<RenderFixture> ("fixture.state-contract",
                                       "render state bytes do not match the declared v2 contour contract");

    const auto* renderValue = requiredProperty (*root, "render");
    const auto* render = renderValue == nullptr ? nullptr : renderValue->getDynamicObject();
    if (render == nullptr || ! readFiniteNumber (*render, "sampleRate", fixture.config.sampleRate)
        || (fixture.config.sampleRate != 44100.0 && fixture.config.sampleRate != 48000.0
            && fixture.config.sampleRate != 96000.0))
        return failure<RenderFixture> ("fixture.sample-rate", "render sample rate is unsupported");
    if (! readUnsignedInteger (*render, "totalSamples", fixture.config.totalSamples)
        || fixture.config.totalSamples == 0
        || fixture.config.totalSamples > maximumRenderSamples)
        return failure<RenderFixture> ("fixture.total-samples",
                                       "render length must be positive and bounded");
    if (! readUnsignedInteger (*render, "seed", fixture.config.seed)
        || fixture.config.seed != 0)
        return failure<RenderFixture> (
            "fixture.seed",
            "render-fixture v1 requires seed zero because it has no stochastic input generator");

    const auto* patternsValue = requiredProperty (*render, "blockPatterns");
    const auto* patterns = patternsValue == nullptr ? nullptr : patternsValue->getArray();
    if (patterns == nullptr || patterns->isEmpty())
        return failure<RenderFixture> ("fixture.block-pattern",
                                       "render block patterns must be nonempty");
    for (const auto& patternValue : *patterns) {
        const auto* pattern = patternValue.getArray();
        if (pattern == nullptr || pattern->isEmpty())
            return failure<RenderFixture> ("fixture.block-pattern",
                                           "each render block pattern must be nonempty");
        std::vector<int> parsedPattern;
        parsedPattern.reserve (static_cast<size_t> (pattern->size()));
        for (const auto& size : *pattern) {
            if (! size.isInt() || static_cast<int> (size) <= 0
                || static_cast<int> (size) > maximumRenderBlockSize)
                return failure<RenderFixture> ("fixture.block-size",
                                               "render block sizes must be within one stage buffer");
            parsedPattern.push_back (static_cast<int> (size));
        }
        fixture.config.blockPatterns.push_back (std::move (parsedPattern));
    }

    std::set<std::uint32_t> sequences;
    const auto readEventIdentity = [&] (const juce::DynamicObject& event,
                                        std::uint64_t& sample,
                                        std::uint32_t& sequence) -> std::optional<Diagnostic> {
        std::uint64_t parsedSequence = 0;
        if (! readUnsignedInteger (event, "sample", sample)
            || sample >= fixture.config.totalSamples)
            return Diagnostic { "fixture.event-sample", "render event sample is outside the render" };
        if (! readUnsignedInteger (event, "sequence", parsedSequence)
            || parsedSequence > std::numeric_limits<std::uint32_t>::max()
            || ! sequences.insert (static_cast<std::uint32_t> (parsedSequence)).second)
            return Diagnostic { "fixture.event-sequence", "render event sequences must be unique" };
        sequence = static_cast<std::uint32_t> (parsedSequence);
        return std::nullopt;
    };

    const auto* automationValue = requiredProperty (*root, "automation");
    const auto* automation = automationValue == nullptr ? nullptr : automationValue->getArray();
    if (automation == nullptr)
        return failure<RenderFixture> ("fixture.automation", "render automation must be an array");
    for (const auto& eventValue : *automation) {
        const auto* eventObject = eventValue.getDynamicObject();
        AutomationEvent event;
        if (eventObject == nullptr)
            return failure<RenderFixture> ("fixture.automation", "automation event must be an object");
        if (const auto diagnostic = readEventIdentity (*eventObject, event.sample, event.sequence);
            diagnostic.has_value())
            return { std::nullopt, { *diagnostic } };
        std::string parameterId;
        if (! readRequiredString (*eventObject, "parameterId", parameterId))
            return failure<RenderFixture> ("fixture.parameter", "automation parameter is required");
        const auto key = parameterKeyForId (parameterId);
        if (! key.has_value())
            return failure<RenderFixture> ("fixture.parameter", "automation parameter is unknown");
        event.key = *key;
        double normalizedValue = 0.0;
        if (! readFiniteNumber (*eventObject, "normalizedValue", normalizedValue)
            || normalizedValue < 0.0 || normalizedValue > 1.0)
            return failure<RenderFixture> ("fixture.automation-value",
                                           "automation value must be finite and normalized");
        event.normalizedValue = static_cast<float> (normalizedValue);
        fixture.automation.push_back (event);
    }

    constexpr std::array stochasticKeys {
        ParameterRegistry::Key::noiseOnOffSwitch,
        ParameterRegistry::Key::oscModSwitch,
        ParameterRegistry::Key::filterModSwitch,
    };
    std::array<bool, stochasticKeys.size()> stochasticEnabled {};
    for (size_t index = 0; index < stochasticKeys.size(); ++index)
        stochasticEnabled[index]
            = stateParameterValue (*stateXml, ParameterRegistry::descriptor (stochasticKeys[index]).id)
                  .value_or (0.0) >= 0.5;

    std::vector<const AutomationEvent*> orderedAutomation;
    orderedAutomation.reserve (fixture.automation.size());
    for (const auto& event : fixture.automation)
        orderedAutomation.push_back (&event);
    std::sort (orderedAutomation.begin(), orderedAutomation.end(), [] (const auto* left,
                                                                       const auto* right) {
        return std::tie (left->sample, left->sequence) < std::tie (right->sample, right->sequence);
    });
    const auto anyStochasticConsumerEnabled = [&] {
        return std::any_of (stochasticEnabled.begin(), stochasticEnabled.end(), [] (const auto enabled) {
            return enabled;
        });
    };
    const auto stochasticFailure = [] {
        return failure<RenderFixture> (
            "fixture.stochastic-state",
            "production random consumers must remain disabled for every rendered interval");
    };

    std::uint64_t intervalStart = 0;
    size_t eventIndex = 0;
    while (eventIndex < orderedAutomation.size()) {
        const auto boundary = orderedAutomation[eventIndex]->sample;
        if (boundary > intervalStart && anyStochasticConsumerEnabled())
            return stochasticFailure();
        while (eventIndex < orderedAutomation.size()
               && orderedAutomation[eventIndex]->sample == boundary) {
            const auto& event = *orderedAutomation[eventIndex++];
            const auto key = std::find (stochasticKeys.begin(), stochasticKeys.end(), event.key);
            if (key != stochasticKeys.end())
                stochasticEnabled[static_cast<size_t> (std::distance (stochasticKeys.begin(), key))]
                    = event.normalizedValue >= 0.5f;
        }
        intervalStart = boundary;
    }
    if (fixture.config.totalSamples > intervalStart && anyStochasticConsumerEnabled())
        return stochasticFailure();

    const auto* midiValue = requiredProperty (*root, "midi");
    const auto* midi = midiValue == nullptr ? nullptr : midiValue->getArray();
    if (midi == nullptr)
        return failure<RenderFixture> ("fixture.midi", "render MIDI must be an array");
    for (const auto& eventValue : *midi) {
        const auto* eventObject = eventValue.getDynamicObject();
        MidiEvent event;
        if (eventObject == nullptr)
            return failure<RenderFixture> ("fixture.midi", "MIDI event must be an object");
        if (const auto diagnostic = readEventIdentity (*eventObject, event.sample, event.sequence);
            diagnostic.has_value())
            return { std::nullopt, { *diagnostic } };
        const auto* bytesValue = requiredProperty (*eventObject, "bytes");
        const auto* bytes = bytesValue == nullptr ? nullptr : bytesValue->getArray();
        if (bytes == nullptr || bytes->isEmpty() || bytes->size() > 3)
            return failure<RenderFixture> ("fixture.midi", "MIDI messages must contain one to three bytes");
        for (const auto& byte : *bytes) {
            if (! byte.isInt() || static_cast<int> (byte) < 0 || static_cast<int> (byte) > 255)
                return failure<RenderFixture> ("fixture.midi", "MIDI bytes must be valid octets");
            event.bytes.push_back (static_cast<std::uint8_t> (static_cast<int> (byte)));
        }
        if (event.bytes.front() < 0x80
            || juce::MidiMessage::getMessageLengthFromFirstByte (event.bytes.front())
                != static_cast<int> (event.bytes.size()))
            return failure<RenderFixture> ("fixture.midi", "MIDI bytes must form a complete JUCE message");
        if (std::any_of (std::next (event.bytes.begin()), event.bytes.end(), [] (const auto byte) {
                return byte >= 0x80;
            }))
            return failure<RenderFixture> ("fixture.midi", "MIDI data bytes must be seven-bit values");
        const juce::MidiMessage message { event.bytes.data(), static_cast<int> (event.bytes.size()), 0.0 };
        if (message.getRawDataSize() != static_cast<int> (event.bytes.size()))
            return failure<RenderFixture> ("fixture.midi", "MIDI bytes are not accepted by JUCE");
        fixture.midi.push_back (std::move (event));
    }

    const auto* inputValue = requiredProperty (*root, "input");
    const auto* input = inputValue == nullptr ? nullptr : inputValue->getDynamicObject();
    std::string inputKind;
    if (input == nullptr || ! readRequiredString (*input, "kind", inputKind))
        return failure<RenderFixture> ("fixture.input", "render input kind is required");
    if (inputKind == "silence") {
        fixture.input.kind = InputKind::silence;
    } else if (inputKind == "dc") {
        fixture.input.kind = InputKind::dc;
        if (! readFiniteNumber (*input, "value", fixture.input.value)
            || std::abs (fixture.input.value) > maximumInputMagnitude)
            return failure<RenderFixture> (
                "fixture.input-value", "DC input must be within full-scale float audio range");
    } else if (inputKind == "sine") {
        fixture.input.kind = InputKind::sine;
        if (! readFiniteNumber (*input, "value", fixture.input.value)
            || std::abs (fixture.input.value) > maximumInputMagnitude)
            return failure<RenderFixture> (
                "fixture.input-value", "sine amplitude must be within full-scale float audio range");
        if (! readFiniteNumber (*input, "frequencyHz", fixture.input.frequencyHz)
            || fixture.input.frequencyHz <= 0.0
            || fixture.input.frequencyHz >= fixture.config.sampleRate * 0.5)
            return failure<RenderFixture> ("fixture.input", "sine input requires a valid frequency");
    } else if (inputKind == "wav") {
        fixture.input.kind = InputKind::wav;
        std::string path;
        if (! readRequiredString (*input, "path", path)
            || ! readRequiredString (*input, "sha256", fixture.input.sha256)
            || ! isLowercaseSha256 (fixture.input.sha256))
            return failure<RenderFixture> ("fixture.input", "WAV input path and hash are required");
        const auto file = resolveBoundedRegularFile (sourceRoot, path);
        if (! file.ok())
            return { std::nullopt, file.diagnostics };
        fixture.input.file = *file.value;
        if (sha256File (fixture.input.file) != fixture.input.sha256)
            return failure<RenderFixture> ("fixture.input-hash", "WAV input hash does not match its bytes");
    } else {
        return failure<RenderFixture> ("fixture.input", "render input kind is unsupported");
    }

    const auto* requestsValue = requiredProperty (*root, "analysisRequests");
    const auto* requests = requestsValue == nullptr ? nullptr : requestsValue->getArray();
    if (requests == nullptr || requests->isEmpty())
        return failure<RenderFixture> (
            "fixture.analysis-requests", "render analysis requests must be a nonempty array");
    const auto analyzerRegistry = AnalyzerRegistry::withFoundationAnalyzers();
    std::set<std::string> uniqueRequestIds;
    for (const auto& requestValue : *requests) {
        const auto* requestObject = requestValue.getDynamicObject();
        FixtureAnalysisRequest request;
        if (requestObject == nullptr
            || ! readRequiredString (*requestObject, "requestId", request.id)
            || ! isPortableIdentifierComponent (request.id)
            || ! uniqueRequestIds.insert (request.id).second)
            return failure<RenderFixture> (
                "fixture.analysis-id", "analysis request IDs must be unique portable components");
        const auto* requestVersion = requiredProperty (*requestObject, "version");
        if (requestVersion == nullptr || ! requestVersion->isInt()
            || static_cast<int> (*requestVersion) != fixtureAnalysisRequestVersion)
            return failure<RenderFixture> (
                "fixture.analysis-version", "analysis request version is unsupported");
        request.version = static_cast<int> (*requestVersion);

        const auto* analyzerValue = requiredProperty (*requestObject, "analyzer");
        const auto* analyzer = analyzerValue == nullptr ? nullptr : analyzerValue->getDynamicObject();
        const auto* analyzerVersion = analyzer == nullptr ? nullptr
                                                           : requiredProperty (*analyzer, "version");
        if (analyzer == nullptr
            || ! readRequiredString (*analyzer, "id", request.analyzer.id)
            || analyzerVersion == nullptr || ! analyzerVersion->isInt())
            return failure<RenderFixture> (
                "fixture.analysis-analyzer", "analysis analyzer identity is required");
        request.analyzer.version = static_cast<int> (*analyzerVersion);
        const auto registered = analyzerRegistry.find (request.analyzer.id);
        if (! registered.ok() || registered.value->version != request.analyzer.version)
            return failure<RenderFixture> (
                "fixture.analysis-analyzer", "analysis analyzer identity/version must be exact");
        if (! readRequiredString (*requestObject, "metric", request.metric)
            || ! metricBelongsToAnalyzer (request.analyzer.id, request.metric))
            return failure<RenderFixture> (
                "fixture.analysis-metric", "analysis metric must belong to the exact analyzer");

        const auto* inputValue = requiredProperty (*requestObject, "input");
        const auto* requestInput = inputValue == nullptr ? nullptr : inputValue->getDynamicObject();
        std::string inputKind;
        if (requestInput == nullptr || ! readRequiredString (*requestInput, "kind", inputKind))
            return failure<RenderFixture> (
                "fixture.analysis-input", "analysis input kind is required");
        if (inputKind == "audio") {
            request.inputKind = AnalysisInputKind::audio;
            std::string tap;
            const auto* channel = requiredProperty (*requestInput, "channel");
            if (! readRequiredString (*requestInput, "tap", tap)
                || (tap != "main" && tap != "phones")
                || channel == nullptr || ! channel->isInt()
                || static_cast<int> (*channel) < 0 || static_cast<int> (*channel) > 1
                || request.analyzer.id == "control.step.v1")
                return failure<RenderFixture> (
                    "fixture.analysis-input", "audio analysis requires an exact tap and channel");
            request.audioTap = tap == "phones" ? AudioTap::phones : AudioTap::main;
            request.channel = static_cast<int> (*channel);
        } else if (inputKind == "control") {
            request.inputKind = AnalysisInputKind::control;
            std::string domain;
            if (! readRequiredString (*requestInput, "parameterId", request.parameterId)
                || ! readRequiredString (*requestInput, "domain", domain)
                || (domain != "normalized" && domain != "physical")
                || request.analyzer.id == "audio.click.v1")
                return failure<RenderFixture> (
                    "fixture.analysis-input", "control analysis input is unsupported");
            const auto key = parameterKeyForId (request.parameterId);
            if (! key.has_value())
                return failure<RenderFixture> (
                    "fixture.analysis-input", "control analysis parameter is unknown");
            request.parameterKey = *key;
            request.controlDomain = domain == "physical" ? ControlDomain::physical
                                                           : ControlDomain::normalized;
        } else {
            return failure<RenderFixture> (
                "fixture.analysis-input", "analysis input kind is unsupported");
        }

        const auto* eventValue = requiredProperty (*requestObject, "event");
        const auto* analysisEvent = eventValue == nullptr ? nullptr : eventValue->getDynamicObject();
        if (analysisEvent == nullptr
            || ! readUnsignedInteger (*analysisEvent, "originSample", request.eventSample)
            || ! readUnsignedInteger (*analysisEvent, "windowSamples", request.windowSamples)
            || request.eventSample >= fixture.config.totalSamples || request.windowSamples == 0
            || request.windowSamples > fixture.config.totalSamples - request.eventSample)
            return failure<RenderFixture> (
                "fixture.analysis-event", "analysis event origin/window is outside the render");
        const auto anchored = std::any_of (
            fixture.automation.begin(), fixture.automation.end(), [&] (const auto& event) {
                return event.sample == request.eventSample;
            }) || std::any_of (fixture.midi.begin(), fixture.midi.end(), [&] (const auto& event) {
                return event.sample == request.eventSample;
            });
        if (! anchored)
            return failure<RenderFixture> (
                "fixture.analysis-event", "analysis event origin must name a rendered event sample");

        const auto readOptionalEndpoint = [&] (const char* name,
                                                std::optional<double>& destination) {
            if (requiredProperty (*requestObject, name) == nullptr)
                return true;
            double value = 0.0;
            if (! readFiniteNumber (*requestObject, name, value))
                return false;
            destination = value;
            return true;
        };
        if (! readOptionalEndpoint ("start", request.start)
            || ! readOptionalEndpoint ("target", request.target))
            return failure<RenderFixture> (
                "fixture.analysis-endpoints", "analysis endpoints must be finite");
        if (request.inputKind == AnalysisInputKind::audio
            && (request.start.has_value() || request.target.has_value()))
            return failure<RenderFixture> (
                "fixture.analysis-endpoints", "audio analysis requests do not accept endpoints");
        if (request.inputKind == AnalysisInputKind::control) {
            if (! request.start.has_value()
                || (request.analyzer.id == "control.step.v1" && ! request.target.has_value()))
                return failure<RenderFixture> (
                    "fixture.analysis-endpoints", "control analysis endpoints are incomplete");
            const auto endpointInDomain = [&] (const double endpoint) {
                if (request.controlDomain == ControlDomain::normalized)
                    return endpoint >= 0.0 && endpoint <= 1.0;
                const auto& descriptor = ParameterRegistry::descriptor (request.parameterKey);
                return endpoint >= static_cast<double> (descriptor.rangeStart)
                    && endpoint <= static_cast<double> (descriptor.rangeEnd);
            };
            if (! endpointInDomain (*request.start)
                || (request.target.has_value() && ! endpointInDomain (*request.target)))
                return failure<RenderFixture> (
                    "fixture.analysis-endpoints", "control analysis endpoints are outside the domain");
            if (request.target.has_value()) {
                const auto targetEvent = std::find_if (
                    fixture.automation.begin(), fixture.automation.end(), [&] (const auto& event) {
                        return event.sample == request.eventSample
                            && event.key == request.parameterKey;
                    });
                const auto expectedTarget = targetEvent == fixture.automation.end()
                    ? std::numeric_limits<double>::quiet_NaN()
                    : (request.controlDomain == ControlDomain::normalized
                           ? static_cast<double> (targetEvent->normalizedValue)
                           : physicalValueFor (targetEvent->key, targetEvent->normalizedValue));
                if (! std::isfinite (expectedTarget)
                    || std::abs (*request.target - expectedTarget)
                           > 8.0 * std::numeric_limits<double>::epsilon()
                                 * std::max (1.0, std::abs (expectedTarget)))
                    return failure<RenderFixture> (
                        "fixture.analysis-endpoints",
                        "control target must match automation at the analysis event");
            }
        }
        fixture.analysisRequests.push_back (std::move (request));
    }
    if (! readNonemptyStringArray (*root, "requirements", fixture.requirements))
        return failure<RenderFixture> ("fixture.requirements", "render requirements must be nonempty");

    return { std::move (fixture), {} };
}

LoadResult<RenderFixture> loadIndexedRenderFixture (
    const juce::File& sourceRoot,
    const IndexedArtifact& indexedFixture)
{
    const auto fixtureFile = resolveBoundedRegularFile (
        sourceRoot, indexedFixture.relativePath);
    if (! fixtureFile.ok())
        return { std::nullopt, fixtureFile.diagnostics };
    if (sha256File (*fixtureFile.value) != indexedFixture.sha256)
        return failure<RenderFixture> (
            "fixture.index-hash", "indexed render fixture hash does not match its bytes");
    auto fixture = loadRenderFixture (sourceRoot, *fixtureFile.value);
    if (! fixture.ok())
        return fixture;
    if (fixture.value->id != indexedFixture.id)
        return failure<RenderFixture> (
            "fixture.index-id", "render fixture ID must exactly match its index record");
    if (fixture.value->requirements != indexedFixture.requirements)
        return failure<RenderFixture> (
            "fixture.index-requirements",
            "render fixture requirements must exactly match index ownership");
    return fixture;
}

LoadResult<SmoothingFixture> loadSmoothingFixture (const juce::File& fixtureFile)
{
    if (! fixtureFile.existsAsFile())
        return failure<SmoothingFixture> ("smoothing.missing", "smoothing fixture does not exist");

    juce::var parsed;
    if (juce::JSON::parse (fixtureFile.loadFileAsString(), parsed).failed())
        return failure<SmoothingFixture> ("smoothing.parse", "smoothing fixture is not valid JSON");
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return failure<SmoothingFixture> ("smoothing.shape", "smoothing fixture root must be an object");

    SmoothingFixture fixture;
    if (! readRequiredString (*root, "schema", fixture.schema)
        || fixture.schema != "model-d.smoothing-fixture.v1")
        return failure<SmoothingFixture> ("smoothing.schema", "smoothing fixture schema is unsupported");
    if (! readRequiredString (*root, "id", fixture.id))
        return failure<SmoothingFixture> ("smoothing.id", "smoothing fixture ID is required");

    std::string className;
    if (! readRequiredString (*root, "registryClass", className))
        return failure<SmoothingFixture> ("smoothing.class", "registry smoothing class is required");
    const std::map<std::string, ParameterRegistry::SmoothingClass> classes {
        { "none", ParameterRegistry::SmoothingClass::none },
        { "gainControl", ParameterRegistry::SmoothingClass::gainControl },
        { "control", ParameterRegistry::SmoothingClass::control },
        { "dedicatedPitch", ParameterRegistry::SmoothingClass::dedicatedPitch },
        { "dedicatedCutoff", ParameterRegistry::SmoothingClass::dedicatedCutoff },
        { "dedicatedGlide", ParameterRegistry::SmoothingClass::dedicatedGlide },
        { "contourStage", ParameterRegistry::SmoothingClass::contourStage },
    };
    const auto smoothingClass = classes.find (className);
    if (smoothingClass == classes.end())
        return failure<SmoothingFixture> ("smoothing.class", "registry smoothing class is unsupported");
    fixture.smoothingClass = smoothingClass->second;

    if (! readFiniteNumber (*root, "startNormalized", fixture.startNormalized)
        || ! readFiniteNumber (*root, "endNormalized", fixture.endNormalized)
        || fixture.startNormalized < 0.0 || fixture.startNormalized > 1.0
        || fixture.endNormalized < 0.0 || fixture.endNormalized > 1.0
        || fixture.startNormalized == fixture.endNormalized)
        return failure<SmoothingFixture> (
            "smoothing.range", "smoothing start/end must be distinct normalized values");

    const auto* ratesValue = requiredProperty (*root, "sampleRates");
    const auto* rates = ratesValue == nullptr ? nullptr : ratesValue->getArray();
    constexpr std::array expectedRates { 44100, 48000, 96000 };
    if (rates == nullptr || rates->size() != static_cast<int> (expectedRates.size()))
        return failure<SmoothingFixture> (
            "smoothing.sample-rates", "smoothing sample-rate matrix must contain 44.1, 48, and 96 kHz");
    for (int index = 0; index < rates->size(); ++index) {
        const auto& rate = rates->getReference (index);
        if (! rate.isInt() || static_cast<int> (rate) != expectedRates[static_cast<size_t> (index)])
            return failure<SmoothingFixture> (
                "smoothing.sample-rates", "smoothing sample-rate matrix order is fixed");
        fixture.sampleRates.push_back (static_cast<int> (rate));
    }

    if (! readUnsignedInteger (*root, "eventSample", fixture.eventSample)
        || ! readUnsignedInteger (*root, "analysisWindow", fixture.analysisWindow)
        || fixture.analysisWindow == 0)
        return failure<SmoothingFixture> (
            "smoothing.window", "smoothing event sample and positive analysis window are required");
    if (! readFiniteNumber (*root, "durationSeconds", fixture.durationSeconds)
        || fixture.durationSeconds < 0.0)
        return failure<SmoothingFixture> (
            "smoothing.duration", "smoothing duration must be a finite nonnegative policy value");
    const auto* intermediate = requiredProperty (*root, "intermediateValues");
    if (intermediate != nullptr) {
        if (! intermediate->isInt() || static_cast<int> (*intermediate) < 0)
            return failure<SmoothingFixture> (
                "smoothing.intermediate", "declared intermediate-value count must be nonnegative");
        fixture.intermediateValues = static_cast<int> (*intermediate);
    }

    if (! readRequiredString (*root, "analyzer", fixture.analyzerId)
        || ! readRequiredString (*root, "requiredTap", fixture.requiredTap)
        || ! readRequiredString (*root, "ownerWorkstream", fixture.ownerWorkstream)
        || ! readRequiredString (*root, "reasonCode", fixture.reasonCode)
        || ! readRequiredString (*root, "referenceReasonCode", fixture.referenceReasonCode))
        return failure<SmoothingFixture> (
            "smoothing.provenance", "smoothing analyzer, tap, owner, and reason codes are required");
    const auto readOptionalString = [&] (const char* name,
                                         std::optional<std::string>& destination) {
        const auto* value = requiredProperty (*root, name);
        if (value == nullptr)
            return true;
        if (! value->isString() || value->toString().isEmpty())
            return false;
        destination = value->toString().toStdString();
        return true;
    };
    if (! readOptionalString ("policyId", fixture.policyId)
        || ! readOptionalString ("settlingPolicyId", fixture.settlingPolicyId))
        return failure<SmoothingFixture> (
            "smoothing.provenance", "declared smoothing policy IDs must be nonempty strings");

    const auto readStatus = [&] (const char* name, Status& destination) {
        std::string status;
        if (! readRequiredString (*root, name, status))
            return false;
        if (status == "pass")
            destination = Status::pass;
        else if (status == "not-run")
            destination = Status::notRun;
        else if (status == "awaiting-approved-reference")
            destination = Status::awaitingApprovedReference;
        else
            return false;
        return true;
    };
    if (! readStatus ("status", fixture.status)
        || ! readStatus ("referenceStatus", fixture.referenceStatus))
        return failure<SmoothingFixture> ("smoothing.status", "smoothing status is unsupported");

    if (fixture.smoothingClass == ParameterRegistry::SmoothingClass::none) {
        if (fixture.durationSeconds != 0.0
            || fixture.intermediateValues != std::optional<int> { 0 }
            || fixture.status != Status::notRun || fixture.referenceStatus != Status::notRun)
            return failure<SmoothingFixture> (
                "smoothing.none-policy", "none must be an exact step with no intermediate values");
    } else {
        const auto expectedDuration = fixture.smoothingClass
                                          == ParameterRegistry::SmoothingClass::gainControl
                                        ? 0.005
                                        : fixture.smoothingClass
                                              == ParameterRegistry::SmoothingClass::control
                                            ? 0.010
                                            : 0.0;
        if (fixture.durationSeconds != expectedDuration
            || fixture.status != Status::notRun
            || fixture.referenceStatus != Status::awaitingApprovedReference
            || fixture.intermediateValues.has_value())
            return failure<SmoothingFixture> (
                "smoothing.delegation", "unavailable owner paths must remain unexecuted without a generic ramp");
    }

    return { std::move (fixture), {} };
}

} // namespace ReferenceHarness
