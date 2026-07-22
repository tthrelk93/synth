#include "ReferenceData.h"

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
    if (! readRequiredString (*root, "schema", schema) || schema != renderFixtureSchema)
        return failure<RenderFixture> ("fixture.schema", "render fixture schema is unsupported");
    if (! readRequiredString (*root, "id", fixture.id))
        return failure<RenderFixture> ("fixture.id", "render fixture identifier is required");

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

    const auto stochasticConsumerEnabled = [&] (const ParameterRegistry::Key key) {
        auto enabled = stateParameterValue (*stateXml, ParameterRegistry::descriptor (key).id)
                           .value_or (0.0) >= 0.5;
        std::vector<const AutomationEvent*> sampleZero;
        for (const auto& event : fixture.automation)
            if (event.sample == 0 && event.key == key)
                sampleZero.push_back (&event);
        std::sort (sampleZero.begin(), sampleZero.end(), [] (const auto* left, const auto* right) {
            return left->sequence < right->sequence;
        });
        for (const auto* event : sampleZero)
            enabled = event->normalizedValue >= 0.5f;
        return enabled;
    };
    if (stochasticConsumerEnabled (ParameterRegistry::Key::noiseOnOffSwitch)
        || stochasticConsumerEnabled (ParameterRegistry::Key::oscModSwitch)
        || stochasticConsumerEnabled (ParameterRegistry::Key::filterModSwitch))
        return failure<RenderFixture> (
            "fixture.stochastic-state",
            "production random consumers must be disabled by final sample-zero automation");

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

    if (! readNonemptyStringArray (*root, "analyzers", fixture.analyzers))
        return failure<RenderFixture> ("fixture.analyzers", "render analyzers must be nonempty");
    if (! readNonemptyStringArray (*root, "requirements", fixture.requirements))
        return failure<RenderFixture> ("fixture.requirements", "render requirements must be nonempty");

    return { std::move (fixture), {} };
}

} // namespace ReferenceHarness
