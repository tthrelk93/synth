#include "RequirementReporter.h"

#include "OfflineRenderer.h"
#include "ReferenceData.h"
#include "SourceIdentity.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <utility>

namespace ReferenceHarness {
namespace {

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

std::vector<std::string> splitMatrixRow (const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    bool escaped = false;
    for (const auto character : line) {
        if (escaped) {
            cell.push_back (character);
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
        } else if (character == '|') {
            cells.push_back (cell);
            cell.clear();
        } else {
            cell.push_back (character);
        }
    }
    cells.push_back (cell);
    return cells;
}

std::string trim (const std::string& value)
{
    const auto first = value.find_first_not_of (" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of (" \t\r\n");
    return value.substr (first, last - first + 1);
}

bool isRequirementId (const std::string& value)
{
    const auto dash = value.find ('-');
    if (dash < 2 || dash > 4 || value.size() != dash + 4)
        return false;
    return std::all_of (value.begin(), value.begin() + static_cast<std::ptrdiff_t> (dash),
                        [] (const auto character) { return character >= 'A' && character <= 'Z'; })
        && std::all_of (value.begin() + static_cast<std::ptrdiff_t> (dash + 1), value.end(),
                        [] (const auto character) { return character >= '0' && character <= '9'; });
}

struct MatrixRow {
    std::string id;
    std::string owner;
    std::vector<std::string> verification;
};

std::vector<std::string> splitCommaList (const std::string& value)
{
    std::vector<std::string> values;
    size_t offset = 0;
    while (offset <= value.size()) {
        const auto comma = value.find (',', offset);
        values.push_back (trim (value.substr (
            offset, comma == std::string::npos ? std::string::npos : comma - offset)));
        if (comma == std::string::npos)
            break;
        offset = comma + 1;
    }
    return values;
}

LoadResult<std::vector<MatrixRow>> readMatrixRows (const juce::File& matrixFile)
{
    if (! matrixFile.existsAsFile())
        return failure<std::vector<MatrixRow>> (
            "requirement.matrix-missing", "traceability matrix does not exist");
    std::vector<MatrixRow> rows;
    std::set<std::string> unique;
    for (const auto& juceLine : juce::StringArray::fromLines (matrixFile.loadFileAsString())) {
        const auto line = juceLine.toStdString();
        if (! line.starts_with ("| "))
            continue;
        const auto cells = splitMatrixRow (line);
        if (cells.size() < 10)
            continue;
        const auto id = trim (cells[1]);
        if (! isRequirementId (id))
            continue;
        if (! unique.insert (id).second)
            return failure<std::vector<MatrixRow>> (
                "requirement.matrix-duplicate", "traceability matrix requirement IDs must be unique");
        const auto owner = trim (cells[4]);
        const auto verification = splitCommaList (trim (cells[cells.size() - 2]));
        if (owner.empty() || verification.empty()
            || std::any_of (verification.begin(), verification.end(), [] (const auto& code) {
                   return code.empty();
               }))
            return failure<std::vector<MatrixRow>> (
                "requirement.matrix-fields", "matrix owner and verification fields are required");
        rows.push_back ({ id, owner, verification });
    }
    if (rows.empty())
        return failure<std::vector<MatrixRow>> (
            "requirement.matrix-empty", "traceability matrix has no requirement data rows");
    return { std::move (rows), {} };
}

bool readString (const juce::DynamicObject& object, const char* name, std::string& destination)
{
    const auto value = object.getProperty (name);
    if (! value.isString() || value.toString().isEmpty())
        return false;
    destination = value.toString().toStdString();
    return true;
}

bool readStringArray (const juce::DynamicObject& object,
                      const char* name,
                      std::vector<std::string>& destination,
                      const bool allowEmpty)
{
    const auto* values = object.getProperty (name).getArray();
    if (values == nullptr || (! allowEmpty && values->isEmpty()))
        return false;
    std::set<std::string> unique;
    for (const auto& value : *values) {
        if (! value.isString() || value.toString().isEmpty())
            return false;
        const auto item = value.toString().toStdString();
        if (! unique.insert (item).second)
            return false;
        destination.push_back (item);
    }
    return true;
}

std::set<std::string> acceptanceGateIds (const AcceptanceManifest& acceptance)
{
    std::set<std::string> ids;
    const auto add = [&] (const std::vector<GateDefinition>& gates) {
        for (const auto& gate : gates)
            ids.insert (gate.id);
    };
    add (acceptance.hardSoftware);
    add (acceptance.published);
    add (acceptance.derivedSoftware);
    add (acceptance.measuredHardware);
    add (acceptance.performance);
    return ids;
}

std::vector<const GateDefinition*> acceptanceGates (const AcceptanceManifest& acceptance)
{
    std::vector<const GateDefinition*> gates;
    const auto add = [&] (const std::vector<GateDefinition>& section) {
        for (const auto& gate : section)
            gates.push_back (&gate);
    };
    add (acceptance.hardSoftware);
    add (acceptance.published);
    add (acceptance.derivedSoftware);
    add (acceptance.measuredHardware);
    add (acceptance.performance);
    return gates;
}

LoadResult<bool> validateAgainstMatrix (
    const std::span<const RequirementDefinition> requirements,
    const juce::File& matrixFile,
    const bool requireOrder)
{
    const auto matrix = readMatrixRows (matrixFile);
    if (! matrix.ok())
        return { std::nullopt, matrix.diagnostics };
    std::map<std::string, const RequirementDefinition*> byId;
    for (const auto& requirement : requirements) {
        if (! byId.emplace (requirement.id, &requirement).second)
            return failure<bool> ("requirement.duplicate-id", "requirement IDs must be unique");
    }
    if (requirements.size() != matrix.value->size())
        return failure<bool> ("requirement.set", "requirement map must exactly match matrix IDs");
    for (size_t index = 0; index < matrix.value->size(); ++index) {
        const auto& matrixRow = (*matrix.value)[index];
        const auto found = byId.find (matrixRow.id);
        if (found == byId.end()
            || (requireOrder && requirements[index].id != matrixRow.id))
            return failure<bool> (
                "requirement.set", "requirement map must exactly match matrix IDs and order");
        if (found->second->owner != matrixRow.owner
            || found->second->verification != matrixRow.verification)
            return failure<bool> (
                "requirement.matrix-fields",
                "requirement owner and verification must exactly match the matrix row");
    }
    return { true, {} };
}

LoadResult<bool> validateStaticGateReciprocity (
    const std::span<const RequirementDefinition> requirements,
    const AcceptanceManifest& acceptance)
{
    std::map<std::string, std::vector<std::string>> expected;
    for (const auto& requirement : requirements)
        expected.emplace (requirement.id, std::vector<std::string> {});
    for (const auto* gate : acceptanceGates (acceptance)) {
        std::set<std::string> unique;
        if (gate->requirements.empty())
            return failure<bool> (
                "requirement.gate-reciprocity", "acceptance gates require owners");
        for (const auto& requirementId : gate->requirements) {
            const auto found = expected.find (requirementId);
            if (found == expected.end() || ! unique.insert (requirementId).second)
                return failure<bool> (
                    "requirement.gate-reciprocity",
                    "acceptance gate owners must be unique mapped requirement IDs");
            found->second.push_back (gate->id);
        }
    }
    for (const auto& requirement : requirements)
        if (requirement.gateIds != expected.at (requirement.id))
            return failure<bool> (
                "requirement.gate-reciprocity",
                "requirement gate IDs must exactly match acceptance ownership");
    return { true, {} };
}

juce::var stringArray (const std::vector<std::string>& values)
{
    juce::Array<juce::var> array;
    for (const auto& value : values)
        array.add (juce::String { value });
    return array;
}

struct ResolvedArtifact {
    juce::File file;
    std::string reportPath;
};

class TemporaryDirectory final {
public:
    TemporaryDirectory()
    {
        const auto temporaryRoot = juce::File::getSpecialLocation (juce::File::tempDirectory);
        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto candidate = temporaryRoot.getChildFile (
                "model-d-release-verify-" + juce::Uuid {}.toString());
            std::error_code error;
            if (std::filesystem::create_directory (
                    candidate.getFullPathName().toStdString(), error)) {
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

using CandidateEvidence = std::pair<std::string, std::string>;

struct AuthoritativeCandidate {
    std::vector<CandidateEvidence> evidence;
    std::vector<GateResult> gates;
};

LoadResult<AuthoritativeCandidate> buildAuthoritativeCandidateEvidence (
    const juce::File& sourceRoot,
    const FixtureIndex& index,
    const AcceptanceManifest& acceptance)
{
    TemporaryDirectory temporary;
    if (! temporary.isOwned())
        return failure<AuthoritativeCandidate> (
            "release.candidate-evidence", "temporary candidate root could not be created");
    AuthoritativeCandidate candidate;
    const auto registryEvidence = writeRegistryMetricEvidence (
        sourceRoot, temporary.directory.getChildFile ("metrics.json"), "metrics.json");
    if (! registryEvidence.ok())
        return { std::nullopt, registryEvidence.diagnostics };
    candidate.evidence.emplace_back (
        "candidate/metrics.json", registryEvidence.value->artifactSha256);
    const auto rendersRoot = temporary.directory.getChildFile ("renders");
    if (! rendersRoot.createDirectory())
        return failure<AuthoritativeCandidate> (
            "release.candidate-evidence", "temporary renders root could not be created");
    for (const auto& indexedFixture : index.renderFixtures) {
        const auto fixture = loadIndexedRenderFixture (sourceRoot, indexedFixture);
        if (! fixture.ok())
            return { std::nullopt, fixture.diagnostics };
        const auto rendered = renderFixture (*fixture.value);
        if (! rendered.ok())
            return { std::nullopt, rendered.diagnostics };
        const auto files = writeCandidateArtifacts (
            *fixture.value, *rendered.value, rendersRoot);
        if (! files.ok())
            return { std::nullopt, files.diagnostics };
        for (const auto& file : *files.value) {
            const auto relative = file.getRelativePathFrom (temporary.directory).toStdString();
            if (relative.empty() || relative.starts_with (".."))
                return failure<AuthoritativeCandidate> (
                    "release.candidate-evidence",
                    "authoritative candidate artifact escaped its temporary root");
            candidate.evidence.emplace_back ("candidate/" + relative, sha256File (file));
        }
    }
    const auto gates = buildF0GateResults (
        sourceRoot, index, acceptance,
        std::span<const GateMetricEvidence> { &*registryEvidence.value, 1 });
    if (! gates.ok())
        return { std::nullopt, gates.diagnostics };
    candidate.gates = std::move (*gates.value);
    return { std::move (candidate), {} };
}

std::optional<std::string> boundedRelativePath (const juce::File& root,
                                                const juce::File& file)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::canonical (
        root.getFullPathName().toStdString(), error);
    if (error)
        return std::nullopt;
    const auto canonicalFile = std::filesystem::canonical (
        file.getFullPathName().toStdString(), error);
    if (error)
        return std::nullopt;
    const auto relative = canonicalFile.lexically_relative (canonicalRoot);
    if (relative.empty() || relative == "." || *relative.begin() == std::filesystem::path { ".." })
        return std::nullopt;
    return relative.generic_string();
}

LoadResult<ResolvedArtifact> resolveArtifact (const juce::File& sourceRoot,
                                               const juce::File& candidateRoot,
                                               const std::string& path)
{
    const juce::File supplied { path };
    if (juce::File::isAbsolutePath (path)) {
        if (const auto relative = boundedRelativePath (candidateRoot, supplied); relative.has_value())
            return { ResolvedArtifact { supplied, "candidate/" + *relative }, {} };
        if (const auto relative = boundedRelativePath (sourceRoot, supplied); relative.has_value())
            return { ResolvedArtifact { supplied, *relative }, {} };
        return failure<ResolvedArtifact> (
            "requirement.artifact-path", "gate artifact must be beneath source or candidate root");
    }
    const auto source = resolveBoundedRegularFile (sourceRoot, path);
    if (source.ok())
        return { ResolvedArtifact { *source.value, path }, {} };
    const auto candidate = resolveBoundedRegularFile (candidateRoot, path);
    if (candidate.ok())
        return { ResolvedArtifact { *candidate.value, "candidate/" + path }, {} };
    return failure<ResolvedArtifact> (
        "requirement.artifact-path", "gate artifact must be beneath source or candidate root");
}

void appendUnique (std::vector<std::string>& values, const std::string& value)
{
    if (std::find (values.begin(), values.end(), value) == values.end())
        values.push_back (value);
}

juce::var metricJson (const MetricResult& metric)
{
    auto analyzer = std::make_unique<juce::DynamicObject>();
    analyzer->setProperty ("id", juce::String { metric.analyzer.id });
    analyzer->setProperty ("version", metric.analyzer.version);
    auto settings = std::make_unique<juce::DynamicObject>();
    for (const auto& [name, value] : metric.settings)
        settings->setProperty (juce::Identifier { name }, juce::String { value });
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("allowance", metric.allowance);
    object->setProperty ("analyzer", juce::var { analyzer.release() });
    object->setProperty ("finite", metric.finite);
    object->setProperty ("metric", juce::String { metric.metric });
    object->setProperty ("settings", juce::var { settings.release() });
    object->setProperty ("unit", juce::String { metric.unit });
    object->setProperty ("value", metric.value);
    return juce::var { object.release() };
}

juce::var gateJson (const GateResult& gate)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty ("artifactPath", juce::String { gate.artifactPath });
    object->setProperty ("artifactSha256", juce::String { gate.artifactSha256 });
    object->setProperty ("id", juce::String { gate.id });
    object->setProperty ("metric", gate.metric.has_value()
                                    ? metricJson (*gate.metric)
                                    : juce::var {});
    object->setProperty ("reason", juce::String { gate.reasonCode });
    object->setProperty ("requirements", stringArray (gate.requirements));
    object->setProperty ("status", juce::String { statusName (gate.status) });
    return juce::var { object.release() };
}

juce::var gateArrayJson (const std::span<const GateResult> gateResults)
{
    juce::Array<juce::var> gates;
    for (const auto& gate : gateResults)
        gates.add (gateJson (gate));
    return gates;
}

std::optional<Status> parseStatus (const std::string& value)
{
    if (value == "pass")
        return Status::pass;
    if (value == "fail")
        return Status::fail;
    if (value == "not-run")
        return Status::notRun;
    if (value == "awaiting-approved-reference")
        return Status::awaitingApprovedReference;
    return std::nullopt;
}

bool readPossiblyEmptyString (const juce::DynamicObject& object,
                              const char* name,
                              std::string& destination)
{
    const auto value = object.getProperty (name);
    if (! value.isString())
        return false;
    destination = value.toString().toStdString();
    return true;
}

LoadResult<std::optional<MetricResult>> parseMetric (const juce::var& value)
{
    if (value.isVoid())
        return { std::optional<MetricResult> {}, {} };
    const auto* object = value.getDynamicObject();
    const auto* analyzer = object == nullptr
                             ? nullptr
                             : object->getProperty ("analyzer").getDynamicObject();
    const auto* settings = object == nullptr
                             ? nullptr
                             : object->getProperty ("settings").getDynamicObject();
    MetricResult metric;
    if (object == nullptr || analyzer == nullptr || settings == nullptr
        || ! readString (*analyzer, "id", metric.analyzer.id)
        || ! analyzer->getProperty ("version").isInt()
        || ! readString (*object, "metric", metric.metric)
        || ! readString (*object, "unit", metric.unit)
        || ! object->getProperty ("finite").isBool()
        || (! object->getProperty ("value").isDouble()
            && ! object->getProperty ("value").isInt())
        || (! object->getProperty ("allowance").isDouble()
            && ! object->getProperty ("allowance").isInt()))
        return failure<std::optional<MetricResult>> (
            "release.report-gates", "gate metric payload is malformed");
    metric.analyzer.version = static_cast<int> (analyzer->getProperty ("version"));
    metric.value = static_cast<double> (object->getProperty ("value"));
    metric.allowance = static_cast<double> (object->getProperty ("allowance"));
    metric.finite = static_cast<bool> (object->getProperty ("finite"));
    if (! std::isfinite (metric.value) || ! std::isfinite (metric.allowance))
        return failure<std::optional<MetricResult>> (
            "release.report-gates", "gate metric values must be finite");
    for (const auto& property : settings->getProperties()) {
        if (! property.value.isString() || property.value.toString().isEmpty())
            return failure<std::optional<MetricResult>> (
                "release.report-gates", "gate metric settings must be strings");
        metric.settings.emplace (property.name.toString().toStdString(),
                                 property.value.toString().toStdString());
    }
    return { std::optional<MetricResult> { std::move (metric) }, {} };
}

LoadResult<std::vector<GateResult>> parseGatePayload (const juce::var& value)
{
    const auto* values = value.getArray();
    if (values == nullptr || values->isEmpty())
        return failure<std::vector<GateResult>> (
            "release.report-gates", "report gate payload must be nonempty");
    std::vector<GateResult> gates;
    gates.reserve (static_cast<size_t> (values->size()));
    std::set<std::string> ids;
    for (const auto& gateValue : *values) {
        const auto* object = gateValue.getDynamicObject();
        GateResult gate;
        std::string status;
        if (object == nullptr || ! readString (*object, "id", gate.id)
            || ! ids.insert (gate.id).second
            || ! readString (*object, "reason", gate.reasonCode)
            || ! readString (*object, "status", status)
            || ! readStringArray (*object, "requirements", gate.requirements, false)
            || ! readPossiblyEmptyString (*object, "artifactPath", gate.artifactPath)
            || ! readPossiblyEmptyString (*object, "artifactSha256", gate.artifactSha256))
            return failure<std::vector<GateResult>> (
                "release.report-gates", "report gate payload is malformed");
        const auto parsedStatus = parseStatus (status);
        if (! parsedStatus.has_value())
            return failure<std::vector<GateResult>> (
                "release.report-gates", "report gate status is unsupported");
        gate.status = *parsedStatus;
        const auto metric = parseMetric (object->getProperty ("metric"));
        if (! metric.ok())
            return { std::nullopt, metric.diagnostics };
        gate.metric = std::move (*metric.value);
        gates.push_back (std::move (gate));
    }
    return { std::move (gates), {} };
}

juce::var reportJson (const RequirementReport& report)
{
    std::map<std::string, int> counts {
        { "awaiting-approved-reference", 0 }, { "fail", 0 }, { "not-run", 0 }, { "pass", 0 },
    };
    juce::Array<juce::var> rows;
    for (const auto& requirement : report.requirements) {
        ++counts[statusName (requirement.status)];
        juce::Array<juce::var> artifacts;
        for (size_t index = 0; index < requirement.definition.artifactPaths.size(); ++index) {
            auto artifact = std::make_unique<juce::DynamicObject>();
            artifact->setProperty ("path", juce::String {
                requirement.definition.artifactPaths[index] });
            artifact->setProperty ("sha256", juce::String { requirement.artifactHashes[index] });
            artifacts.add (juce::var { artifact.release() });
        }
        auto row = std::make_unique<juce::DynamicObject>();
        row->setProperty ("artifacts", artifacts);
        row->setProperty ("gateIds", stringArray (requirement.definition.gateIds));
        row->setProperty ("id", juce::String { requirement.definition.id });
        row->setProperty ("owner", juce::String { requirement.definition.owner });
        row->setProperty ("reasons", stringArray (requirement.reasons));
        row->setProperty ("status", juce::String { statusName (requirement.status) });
        row->setProperty ("verification", stringArray (requirement.definition.verification));
        rows.add (juce::var { row.release() });
    }
    auto countObject = std::make_unique<juce::DynamicObject>();
    for (const auto& [name, count] : counts)
        countObject->setProperty (juce::Identifier { name }, count);
    auto provenance = std::make_unique<juce::DynamicObject>();
    provenance->setProperty ("architecture", juce::String { report.architecture });
    provenance->setProperty ("buildType", juce::String { report.buildType });
    provenance->setProperty ("platform", juce::String { report.platform });
    provenance->setProperty ("sourceCommit", juce::String { report.sourceCommit });
    provenance->setProperty ("sourceContent", juce::String { report.sourceContent });
    provenance->setProperty ("sourceDirty", report.sourceDirty);
    provenance->setProperty ("sourceTree", juce::String { report.sourceTree });
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("analyzerVersion", report.analyzerVersion);
    root->setProperty ("counts", juce::var { countObject.release() });
    root->setProperty ("fixtureVersion", report.fixtureVersion);
    root->setProperty ("gates", gateArrayJson (report.gateResults));
    root->setProperty ("manifestVersion", report.manifestVersion);
    root->setProperty ("provenance", juce::var { provenance.release() });
    root->setProperty ("releaseReady", report.releaseReady);
    root->setProperty ("requirements", rows);
    root->setProperty ("schema", "model-d.requirements-report.v1");
    return juce::var { root.release() };
}

} // namespace

LoadResult<bool> validateRequirementSet (
    const std::span<const RequirementDefinition> requirements,
    const juce::File& matrixFile)
{
    return validateAgainstMatrix (requirements, matrixFile, true);
}

LoadResult<std::vector<RequirementDefinition>> loadRequirementMap (
    const juce::File& sourceRoot,
    const juce::File& mapFile,
    const juce::File& matrixFile,
    const AcceptanceManifest& acceptance)
{
    if (! mapFile.existsAsFile())
        return failure<std::vector<RequirementDefinition>> (
            "requirement.missing", "requirement map does not exist");
    juce::var parsed;
    if (juce::JSON::parse (mapFile.loadFileAsString(), parsed).failed())
        return failure<std::vector<RequirementDefinition>> (
            "requirement.parse", "requirement map is not valid JSON");
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr
        || root->getProperty ("schema").toString() != "model-d.requirement-map.v1"
        || static_cast<int> (root->getProperty ("manifestVersion")) != 1)
        return failure<std::vector<RequirementDefinition>> (
            "requirement.schema", "requirement map schema/version is unsupported");
    const auto* rows = root->getProperty ("requirements").getArray();
    if (rows == nullptr)
        return failure<std::vector<RequirementDefinition>> (
            "requirement.shape", "requirement map rows must be an array");

    static const std::set<std::string> validVerification {
        "CR", "DOC", "HR", "HV", "LI", "OR", "RT", "SEC", "ST", "UI", "UT",
    };
    const auto validGates = acceptanceGateIds (acceptance);
    std::vector<RequirementDefinition> requirements;
    requirements.reserve (static_cast<size_t> (rows->size()));
    std::set<std::string> ids;
    for (const auto& row : *rows) {
        const auto* object = row.getDynamicObject();
        RequirementDefinition definition;
        std::string status;
        if (object == nullptr || ! readString (*object, "id", definition.id))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.id", "requirement ID is required");
        if (! ids.insert (definition.id).second)
            return failure<std::vector<RequirementDefinition>> (
                "requirement.duplicate-id", "requirement IDs must be unique");
        if (! readString (*object, "owner", definition.owner))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.owner", "each requirement needs exactly one owner");
        if (! readStringArray (*object, "verification", definition.verification, false))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.verification", "verification codes must be nonempty and unique");
        if (std::any_of (definition.verification.begin(), definition.verification.end(),
                         [&] (const auto& code) { return ! validVerification.contains (code); }))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.verification-code", "requirement verification code is unknown");
        if (! readStringArray (*object, "gateIds", definition.gateIds, true))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.gates", "requirement gates must be a unique string array");
        if (std::any_of (definition.gateIds.begin(), definition.gateIds.end(),
                         [&] (const auto& gate) { return ! validGates.contains (gate); }))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.gate-mapping", "requirement references an unknown acceptance gate");
        if (! readString (*object, "status", status))
            return failure<std::vector<RequirementDefinition>> (
                "requirement.status", "requirement status is required");
        if (status == "pass")
            definition.status = Status::pass;
        else if (status == "fail")
            definition.status = Status::fail;
        else if (status == "not-run")
            definition.status = Status::notRun;
        else if (status == "awaiting-approved-reference")
            definition.status = Status::awaitingApprovedReference;
        else
            return failure<std::vector<RequirementDefinition>> (
                "requirement.status", "requirement status is unsupported");

        const auto* artifacts = object->getProperty ("artifacts").getArray();
        if (artifacts == nullptr)
            return failure<std::vector<RequirementDefinition>> (
                "requirement.artifacts", "requirement artifacts must be an array");
        std::set<std::string> paths;
        for (const auto& artifactValue : *artifacts) {
            const auto* artifact = artifactValue.getDynamicObject();
            std::string path;
            std::string hash;
            if (artifact == nullptr || ! readString (*artifact, "path", path)
                || ! readString (*artifact, "sha256", hash) || hash.size() != 64)
                return failure<std::vector<RequirementDefinition>> (
                    "requirement.artifact", "artifact path and SHA-256 are required");
            if (! paths.insert (path).second)
                return failure<std::vector<RequirementDefinition>> (
                    "requirement.artifact", "artifact paths must be unique");
            const auto resolved = resolveBoundedRegularFile (sourceRoot, path);
            if (! resolved.ok() || sha256File (*resolved.value) != hash)
                return failure<std::vector<RequirementDefinition>> (
                    "requirement.artifact-hash", "artifact must exist and match SHA-256");
            definition.artifactPaths.push_back (path);
            definition.artifactSha256.push_back (hash);
        }
        if (definition.status == Status::pass && definition.artifactPaths.empty())
            return failure<std::vector<RequirementDefinition>> (
                "requirement.pass-artifact", "passing requirements need hashed evidence");
        requirements.push_back (std::move (definition));
    }

    const auto validSet = validateRequirementSet (requirements, matrixFile);
    if (! validSet.ok())
        return { std::nullopt, validSet.diagnostics };
    const auto reciprocal = validateStaticGateReciprocity (requirements, acceptance);
    if (! reciprocal.ok())
        return { std::nullopt, reciprocal.diagnostics };
    return { std::move (requirements), {} };
}

LoadResult<std::vector<GateResult>> buildF0GateResults (
    const juce::File& sourceRoot,
    const FixtureIndex& index,
    const AcceptanceManifest& acceptance,
    const std::span<const GateMetricEvidence> metricEvidence)
{
    const auto smoothing = expandSmoothingFixtures (sourceRoot, index, acceptance);
    if (! smoothing.ok())
        return { std::nullopt, smoothing.diagnostics };
    const auto analyzers = AnalyzerRegistry::withFoundationAnalyzers();
    const std::span<const SmoothingEvidence> noEvidence;
    auto gates = evaluateAcceptance (
        acceptance, *smoothing.value, noEvidence, analyzers, metricEvidence);
    if (! gates.ok())
        return gates;
    if (ParameterRegistry::descriptors().size() != 48)
        return failure<std::vector<GateResult>> (
            "requirement.registry-gate", "exact 48-descriptor registry gate is unavailable");
    return gates;
}

LoadResult<RequirementReport> buildRequirementReport (
    const juce::File& sourceRoot,
    const juce::File& candidateRoot,
    const std::span<const RequirementDefinition> requirements,
    const std::span<const GateResult> gateResults,
    const AcceptanceManifest& acceptance)
{
    const auto matrixFile = sourceRoot.getChildFile (
        "docs/remediation/01-traceability-matrix.md");
    const auto validSet = validateAgainstMatrix (requirements, matrixFile, false);
    if (! validSet.ok())
        return { std::nullopt, validSet.diagnostics };
    const auto reciprocal = validateStaticGateReciprocity (requirements, acceptance);
    if (! reciprocal.ok())
        return { std::nullopt, reciprocal.diagnostics };
    const auto matrix = readMatrixRows (matrixFile);
    if (! matrix.ok())
        return { std::nullopt, matrix.diagnostics };

    std::map<std::string, RequirementDefinition> definitionsById;
    for (const auto& definition : requirements)
        definitionsById.emplace (definition.id, definition);
    const auto staticGateIds = acceptanceGateIds (acceptance);
    std::map<std::string, const GateDefinition*> staticGates;
    for (const auto* gate : acceptanceGates (acceptance))
        staticGates.emplace (gate->id, gate);

    RequirementReport report;
    const auto& sourceIdentity = builtSourceIdentity();
    report.sourceCommit = sourceIdentity.commit;
    report.sourceTree = sourceIdentity.tree;
    report.sourceContent = sourceIdentity.content;
    report.sourceDirty = sourceIdentity.dirty;
    report.buildType = SYNTH_CONFIGURED_BUILD_TYPE;
    report.platform = SYNTH_CONFIGURED_PLATFORM;
    report.architecture = SYNTH_CONFIGURED_ARCHITECTURE;
    report.gateResults.assign (gateResults.begin(), gateResults.end());

    std::map<std::string, const GateResult*> resultsById;
    for (auto& gate : report.gateResults) {
        if (gate.id.empty() || gate.reasonCode.empty()
            || ! resultsById.emplace (gate.id, &gate).second)
            return failure<RequirementReport> (
                "requirement.duplicate-gate-result",
                "gate result IDs and reasons must be nonempty and unique");
        std::set<std::string> owners;
        if (gate.requirements.empty())
            return failure<RequirementReport> (
                "requirement.gate-requirement", "every gate result requires owners");
        for (const auto& requirementId : gate.requirements)
            if (! definitionsById.contains (requirementId)
                || ! owners.insert (requirementId).second)
                return failure<RequirementReport> (
                    "requirement.gate-requirement",
                    "gate result owners must be unique mapped requirement IDs");
        if (const auto staticGate = staticGates.find (gate.id);
            staticGate != staticGates.end()
            && gate.requirements != staticGate->second->requirements)
            return failure<RequirementReport> (
                "requirement.gate-reciprocity",
                "static gate result owners must match acceptance exactly");
        if (gate.status == Status::pass) {
            if (gate.artifactPath.empty() || gate.artifactSha256.size() != 64)
                return failure<RequirementReport> (
                    "requirement.gate-artifact", "passing gate needs a hashed artifact");
            const auto artifact = resolveArtifact (
                sourceRoot, candidateRoot, gate.artifactPath);
            if (! artifact.ok() || sha256File (artifact.value->file) != gate.artifactSha256)
                return failure<RequirementReport> (
                    "requirement.gate-artifact", "passing gate artifact is missing or changed");
            gate.artifactPath = artifact.value->reportPath;
        } else if (! gate.artifactPath.empty() || ! gate.artifactSha256.empty()) {
            return failure<RequirementReport> (
                "requirement.gate-artifact", "non-passing gates cannot claim artifacts");
        }
    }
    for (const auto& gateId : staticGateIds)
        if (! resultsById.contains (gateId))
            return failure<RequirementReport> (
                "requirement.gate-result-missing", "required acceptance gate has no result");

    report.requirements.reserve (requirements.size());
    for (const auto& matrixRow : *matrix.value) {
        const auto& definition = definitionsById.at (matrixRow.id);
        if (definition.artifactPaths.size() != definition.artifactSha256.size())
            return failure<RequirementReport> (
                "requirement.artifact", "artifact paths and hashes must have equal length");
        RequirementResult result;
        result.definition = definition;
        for (const auto& gate : report.gateResults)
            if (! staticGateIds.contains (gate.id)
                && std::find (gate.requirements.begin(), gate.requirements.end(), definition.id)
                       != gate.requirements.end())
                result.definition.gateIds.push_back (gate.id);
        for (size_t index = 0; index < definition.artifactPaths.size(); ++index) {
            const auto& path = definition.artifactPaths[index];
            const auto resolved = path.starts_with ("candidate/")
                                ? resolveBoundedRegularFile (candidateRoot, path.substr (10))
                                : resolveBoundedRegularFile (sourceRoot, path);
            if (! resolved.ok() || sha256File (*resolved.value) != definition.artifactSha256[index])
                return failure<RequirementReport> (
                    "requirement.artifact-hash", "mapped artifact changed after map validation");
            result.artifactHashes.push_back (definition.artifactSha256[index]);
        }

        bool anyFail = definition.status == Status::fail;
        bool anyAwaiting = definition.status == Status::awaitingApprovedReference;
        bool anyNotRun = false;
        for (const auto& gateId : result.definition.gateIds) {
            const auto* gate = resultsById.at (gateId);
            if (gate->status == Status::fail)
                anyFail = true;
            else if (gate->status == Status::awaitingApprovedReference)
                anyAwaiting = true;
            else if (gate->status == Status::notRun)
                anyNotRun = true;
            appendUnique (result.reasons, gate->reasonCode);
            if (gate->status == Status::pass
                && std::find (result.definition.artifactPaths.begin(),
                              result.definition.artifactPaths.end(), gate->artifactPath)
                       == result.definition.artifactPaths.end()) {
                result.definition.artifactPaths.push_back (gate->artifactPath);
                result.definition.artifactSha256.push_back (gate->artifactSha256);
                result.artifactHashes.push_back (gate->artifactSha256);
            }
        }

        if (anyFail) {
            result.status = Status::fail;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.declared-fail");
        } else if (anyAwaiting) {
            result.status = Status::awaitingApprovedReference;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.awaiting-approved-reference");
        } else if (anyNotRun) {
            result.status = Status::notRun;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.not-run");
        } else if (! result.definition.gateIds.empty()
                   && ! result.definition.artifactPaths.empty()) {
            result.status = Status::pass;
            result.reasons = { "requirement.gates-pass" };
        } else if (definition.status == Status::pass
                   && ! result.definition.artifactPaths.empty()) {
            result.status = Status::pass;
            result.reasons = { "requirement.evidence-pass" };
        } else {
            result.status = Status::notRun;
            result.reasons = { "requirement.not-run" };
        }
        report.requirements.push_back (std::move (result));
    }
    report.releaseReady = ! report.requirements.empty()
        && std::all_of (report.requirements.begin(), report.requirements.end(), [] (const auto& row) {
               return row.status == Status::pass;
           });
    return { std::move (report), {} };
}

LoadResult<juce::File> writeRequirementReport (const RequirementReport& report,
                                                const juce::File& candidateDirectory)
{
    if (candidateDirectory.exists())
        return failure<juce::File> (
            "requirement.output-exists", "requirement report candidate must be new");
    if (! candidateDirectory.createDirectory())
        return failure<juce::File> (
            "requirement.output-create", "requirement report candidate cannot be created");
    const auto reportFile = candidateDirectory.getChildFile ("requirements-report.json");
    if (! reportFile.replaceWithText (
            canonicalJson (reportJson (report)), false, false, "\n")) {
        candidateDirectory.deleteRecursively();
        return failure<juce::File> (
            "requirement.output-write", "requirement report could not be written");
    }
    return { reportFile, {} };
}

LoadResult<bool> verifyReleaseReady (const juce::File& reportFile)
{
    if (! reportFile.existsAsFile())
        return failure<bool> ("release.report-missing", "requirement report does not exist");
    juce::var parsed;
    const auto text = reportFile.loadFileAsString();
    if (juce::JSON::parse (text, parsed).failed())
        return failure<bool> ("release.report-parse", "requirement report is not valid JSON");
    if (text != canonicalJson (parsed))
        return failure<bool> ("release.report-canonical", "requirement report is not canonical JSON");
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr || ! root->getProperty ("schema").isString()
        || root->getProperty ("schema").toString() != "model-d.requirements-report.v1"
        || ! root->getProperty ("manifestVersion").isInt()
        || static_cast<int> (root->getProperty ("manifestVersion")) != 1
        || ! root->getProperty ("fixtureVersion").isInt()
        || static_cast<int> (root->getProperty ("fixtureVersion")) != 1
        || ! root->getProperty ("analyzerVersion").isInt()
        || static_cast<int> (root->getProperty ("analyzerVersion")) != 1
        || ! root->getProperty ("releaseReady").isBool())
        return failure<bool> ("release.report-schema", "requirement report schema is unsupported");

    const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
    const auto matrix = readMatrixRows (sourceRoot.getChildFile (
        "docs/remediation/01-traceability-matrix.md"));
    if (! matrix.ok())
        return { std::nullopt, matrix.diagnostics };
    const auto analyzers = AnalyzerRegistry::withFoundationAnalyzers();
    const auto index = loadFixtureIndex (
        sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
    if (! index.ok())
        return { std::nullopt, index.diagnostics };
    const auto acceptance = loadAcceptanceManifest (
        sourceRoot, sourceRoot.getChildFile ("Tests/reference/acceptance-v1.json"), analyzers);
    if (! acceptance.ok())
        return { std::nullopt, acceptance.diagnostics };
    auto requirements = loadRequirementMap (
        sourceRoot, sourceRoot.getChildFile ("Tests/reference/requirement-map.json"),
        sourceRoot.getChildFile ("docs/remediation/01-traceability-matrix.md"),
        *acceptance.value);
    if (! requirements.ok())
        return { std::nullopt, requirements.diagnostics };
    const auto authoritativeCandidate = buildAuthoritativeCandidateEvidence (
        sourceRoot, *index.value, *acceptance.value);
    if (! authoritativeCandidate.ok())
        return { std::nullopt, authoritativeCandidate.diagnostics };
    const auto& authoritativeGates = authoritativeCandidate.value->gates;
    const auto submittedGates = parseGatePayload (root->getProperty ("gates"));
    if (! submittedGates.ok())
        return { std::nullopt, submittedGates.diagnostics };
    auto reportFormGates = authoritativeGates;
    for (auto& gate : reportFormGates)
        if (gate.status == Status::pass && gate.artifactPath == "metrics.json")
            gate.artifactPath = "candidate/metrics.json";
    if (canonicalJson (gateArrayJson (*submittedGates.value))
        != canonicalJson (gateArrayJson (reportFormGates)))
        return failure<bool> (
            "release.gate-evidence", "submitted gate evidence is not authoritative");

    const auto* counts = root->getProperty ("counts").getDynamicObject();
    const auto* provenance = root->getProperty ("provenance").getDynamicObject();
    const auto* rows = root->getProperty ("requirements").getArray();
    if (counts == nullptr || provenance == nullptr || rows == nullptr
        || rows->size() != static_cast<int> (matrix.value->size()))
        return failure<bool> ("release.report-set", "requirement report set is malformed");
    const auto exactProvenance = [&] (const char* name, const char* expected) {
        const auto value = provenance->getProperty (name);
        return value.isString() && value.toString() == expected;
    };
    const auto& sourceIdentity = builtSourceIdentity();
    const auto dirty = provenance->getProperty ("sourceDirty");
    if (! exactProvenance ("sourceCommit", sourceIdentity.commit.c_str())
        || ! exactProvenance ("sourceTree", sourceIdentity.tree.c_str())
        || ! exactProvenance ("sourceContent", sourceIdentity.content.c_str())
        || ! dirty.isBool() || static_cast<bool> (dirty) != sourceIdentity.dirty
        || ! exactProvenance ("buildType", SYNTH_CONFIGURED_BUILD_TYPE)
        || ! exactProvenance ("platform", SYNTH_CONFIGURED_PLATFORM)
        || ! exactProvenance ("architecture", SYNTH_CONFIGURED_ARCHITECTURE))
        return failure<bool> (
            "release.report-mismatch", "report provenance is not the current build provenance");

    std::map<std::string, int> submittedCounts {
        { "awaiting-approved-reference", 0 }, { "fail", 0 }, { "not-run", 0 }, { "pass", 0 },
    };
    for (auto& [name, count] : submittedCounts) {
        const auto value = counts->getProperty (juce::Identifier { name });
        if (! value.isInt() || static_cast<int> (value) < 0)
            return failure<bool> (
                "release.report-mismatch", "report counts must be nonnegative integers");
        count = static_cast<int> (value);
    }

    std::map<std::string, RequirementDefinition*> definitionsById;
    for (auto& definition : *requirements.value)
        definitionsById.emplace (definition.id, &definition);
    std::map<std::string, int> actualSubmittedCounts {
        { "awaiting-approved-reference", 0 }, { "fail", 0 }, { "not-run", 0 }, { "pass", 0 },
    };
    bool submittedAllPass = true;
    std::vector<std::string> expectedCandidatePaths;
    expectedCandidatePaths.push_back ("candidate/metrics.json");
    for (const auto& fixture : index.value->renderFixtures) {
        const auto prefix = "candidate/renders/" + fixture.id + "/";
        for (const auto* name : { "render.json", "control-trace.json", "event-trace.json",
                                  "main.wav", "phones.wav", "metrics.json" })
            expectedCandidatePaths.push_back (prefix + name);
    }
    std::vector<CandidateEvidence> submittedCandidateEvidence;
    std::set<std::string> uniqueCandidatePaths;
    for (size_t indexPosition = 0; indexPosition < matrix.value->size(); ++indexPosition) {
        const auto* row = (*rows)[static_cast<int> (indexPosition)].getDynamicObject();
        std::string id;
        std::string owner;
        std::string status;
        std::vector<std::string> verification;
        std::vector<std::string> gateIds;
        std::vector<std::string> reasons;
        if (row == nullptr || ! readString (*row, "id", id)
            || id != (*matrix.value)[indexPosition].id)
            return failure<bool> (
                "release.report-set", "requirement report IDs must exactly match matrix order");
        if (! readString (*row, "owner", owner)
            || ! readString (*row, "status", status)
            || ! readStringArray (*row, "verification", verification, false)
            || ! readStringArray (*row, "gateIds", gateIds, true)
            || ! readStringArray (*row, "reasons", reasons, false))
            return failure<bool> ("release.report-row", "requirement report row is malformed");
        const auto parsedStatus = parseStatus (status);
        if (! parsedStatus.has_value())
            return failure<bool> ("release.report-status", "requirement row status is unsupported");
        ++actualSubmittedCounts[status];
        submittedAllPass = submittedAllPass && *parsedStatus == Status::pass;
        const auto* artifacts = row->getProperty ("artifacts").getArray();
        if (artifacts == nullptr)
            return failure<bool> ("release.report-row", "requirement artifacts must be an array");
        if (*parsedStatus == Status::pass && artifacts->isEmpty())
            return failure<bool> ("release.pass-artifact", "passing requirement has no artifact");
        for (const auto& artifactValue : *artifacts) {
            const auto* artifact = artifactValue.getDynamicObject();
            std::string path;
            std::string hash;
            if (artifact == nullptr || ! readString (*artifact, "path", path)
                || ! readString (*artifact, "sha256", hash) || hash.size() != 64)
                return failure<bool> ("release.report-artifact", "report artifact is malformed");
            juce::File resolved;
            if (path.starts_with ("candidate/")) {
                const auto candidate = resolveBoundedRegularFile (
                    reportFile.getParentDirectory(), path.substr (10));
                const auto isCandidateInventory = id == "TST-001";
                const auto isRegistryGateArtifact = path == "candidate/metrics.json"
                    && std::find (gateIds.begin(), gateIds.end(), "hard.registry.count")
                           != gateIds.end();
                if (! candidate.ok()
                    || (! isCandidateInventory && ! isRegistryGateArtifact)
                    || (isCandidateInventory
                        && ! uniqueCandidatePaths.insert (path).second))
                    return failure<bool> (
                        "release.report-mismatch",
                        "candidate evidence placement is not authoritative");
                resolved = *candidate.value;
                if (isCandidateInventory)
                    submittedCandidateEvidence.emplace_back (path, hash);
            } else {
                const auto source = resolveBoundedRegularFile (sourceRoot, path);
                if (! source.ok())
                    return failure<bool> (
                        "release.report-artifact", "source artifact is missing or unbounded");
                resolved = *source.value;
            }
            if (sha256File (resolved) != hash)
                return failure<bool> ("release.report-artifact", "report artifact hash changed");
        }
    }
    std::vector<std::string> submittedCandidatePaths;
    for (const auto& [path, hash] : submittedCandidateEvidence) {
        static_cast<void> (hash);
        submittedCandidatePaths.push_back (path);
    }
    if (submittedCandidatePaths != expectedCandidatePaths
        || submittedCounts != actualSubmittedCounts
        || static_cast<bool> (root->getProperty ("releaseReady")) != submittedAllPass)
        return failure<bool> (
            "release.report-mismatch",
            "candidate evidence, counts, or readiness disagree with authoritative inputs");

    if (submittedCandidateEvidence != authoritativeCandidate.value->evidence)
        return failure<bool> (
            "release.report-mismatch",
            "candidate evidence does not match an authoritative renderer replay");
    auto* candidateDefinition = definitionsById.at ("TST-001");
    for (const auto& [path, hash] : authoritativeCandidate.value->evidence) {
        candidateDefinition->artifactPaths.push_back (path);
        candidateDefinition->artifactSha256.push_back (hash);
    }

    const auto rebuilt = buildRequirementReport (
        sourceRoot, reportFile.getParentDirectory(), *requirements.value,
        authoritativeGates, *acceptance.value);
    if (! rebuilt.ok())
        return { std::nullopt, rebuilt.diagnostics };
    if (text != canonicalJson (reportJson (*rebuilt.value)))
        return failure<bool> (
            "release.report-mismatch", "submitted report does not match authoritative reduction");
    if (! rebuilt.value->releaseReady) {
        const auto firstOpen = std::find_if (
            rebuilt.value->requirements.begin(), rebuilt.value->requirements.end(),
            [] (const auto& requirement) { return requirement.status != Status::pass; });
        return failure<bool> (
            "release.not-ready",
            firstOpen->definition.id + " " + statusName (firstOpen->status) + " "
                + firstOpen->reasons.front());
    }
    return { true, {} };
}

} // namespace ReferenceHarness
