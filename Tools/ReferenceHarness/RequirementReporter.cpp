#include "RequirementReporter.h"

#include "ReferenceData.h"

#include <algorithm>
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

LoadResult<std::vector<std::string>> readMatrixIds (const juce::File& matrixFile)
{
    if (! matrixFile.existsAsFile())
        return failure<std::vector<std::string>> (
            "requirement.matrix-missing", "traceability matrix does not exist");
    std::vector<std::string> ids;
    std::set<std::string> unique;
    for (const auto& juceLine : juce::StringArray::fromLines (matrixFile.loadFileAsString())) {
        const auto line = juceLine.toStdString();
        if (! line.starts_with ("| "))
            continue;
        const auto cells = splitMatrixRow (line);
        if (cells.size() < 4)
            continue;
        const auto id = trim (cells[1]);
        if (! isRequirementId (id))
            continue;
        if (! unique.insert (id).second)
            return failure<std::vector<std::string>> (
                "requirement.matrix-duplicate", "traceability matrix requirement IDs must be unique");
        ids.push_back (id);
    }
    if (ids.empty())
        return failure<std::vector<std::string>> (
            "requirement.matrix-empty", "traceability matrix has no requirement data rows");
    return { std::move (ids), {} };
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

std::map<std::string, GateClassification> acceptanceGateClasses (
    const AcceptanceManifest& acceptance)
{
    std::map<std::string, GateClassification> classes;
    const auto add = [&] (const std::vector<GateDefinition>& gates) {
        for (const auto& gate : gates)
            classes.emplace (gate.id, gate.classification);
    };
    add (acceptance.hardSoftware);
    add (acceptance.published);
    add (acceptance.derivedSoftware);
    add (acceptance.measuredHardware);
    add (acceptance.performance);
    return classes;
}

void appendUnique (std::vector<std::string>& values, const std::string& value)
{
    if (std::find (values.begin(), values.end(), value) == values.end())
        values.push_back (value);
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
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("analyzerVersion", report.analyzerVersion);
    root->setProperty ("counts", juce::var { countObject.release() });
    root->setProperty ("fixtureVersion", report.fixtureVersion);
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
    const auto matrix = readMatrixIds (matrixFile);
    if (! matrix.ok())
        return { std::nullopt, matrix.diagnostics };
    std::vector<std::string> ids;
    std::set<std::string> unique;
    ids.reserve (requirements.size());
    for (const auto& requirement : requirements) {
        if (! unique.insert (requirement.id).second)
            return failure<bool> ("requirement.duplicate-id", "requirement IDs must be unique");
        if (requirement.owner.empty())
            return failure<bool> ("requirement.owner", "each requirement needs exactly one owner");
        if (requirement.verification.empty())
            return failure<bool> (
                "requirement.verification", "each requirement needs verification codes");
        ids.push_back (requirement.id);
    }
    if (ids != *matrix.value)
        return failure<bool> (
            "requirement.set", "requirement map must exactly match matrix IDs and order");
    return { true, {} };
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
    return { std::move (requirements), {} };
}

LoadResult<RequirementReport> buildRequirementReport (
    const juce::File& sourceRoot,
    const juce::File& candidateRoot,
    const std::span<const RequirementDefinition> requirements,
    const std::span<const GateResult> gateResults,
    const AcceptanceManifest& acceptance)
{
    std::map<std::string, const GateResult*> resultsById;
    for (const auto& gate : gateResults)
        if (! resultsById.emplace (gate.id, &gate).second)
            return failure<RequirementReport> (
                "requirement.duplicate-gate-result", "gate result IDs must be unique");
    const auto gateClasses = acceptanceGateClasses (acceptance);

    RequirementReport report;
    report.sourceCommit = SYNTH_SOURCE_COMMIT;
    report.buildType = SYNTH_CONFIGURED_BUILD_TYPE;
    report.platform = SYNTH_CONFIGURED_PLATFORM;
    report.architecture = SYNTH_CONFIGURED_ARCHITECTURE;
    report.requirements.reserve (requirements.size());
    for (const auto& definition : requirements) {
        if (definition.artifactPaths.size() != definition.artifactSha256.size())
            return failure<RequirementReport> (
                "requirement.artifact", "artifact paths and hashes must have equal length");
        RequirementResult result;
        result.definition = definition;
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
        bool awaitingHardware = definition.status == Status::awaitingApprovedReference;
        bool anyNotRun = false;
        for (const auto& gateId : definition.gateIds) {
            const auto gate = resultsById.find (gateId);
            if (gate == resultsById.end())
                return failure<RequirementReport> (
                    "requirement.gate-result-missing", "required acceptance gate has no result");
            const auto* gateResult = gate->second;
            if (gateResult->status == Status::fail) {
                anyFail = true;
                appendUnique (result.reasons, gateResult->reasonCode);
            } else if (gateResult->status == Status::awaitingApprovedReference) {
                const auto classification = gateClasses.find (gateId);
                if (classification == gateClasses.end())
                    return failure<RequirementReport> (
                        "requirement.gate-mapping", "required acceptance gate is unclassified");
                if (classification->second == GateClassification::measuredHardware)
                    awaitingHardware = true;
                else
                    anyNotRun = true;
                appendUnique (result.reasons, gateResult->reasonCode);
            } else if (gateResult->status == Status::notRun) {
                anyNotRun = true;
                appendUnique (result.reasons, gateResult->reasonCode);
            } else {
                if (gateResult->artifactPath.empty() || gateResult->artifactSha256.size() != 64)
                    return failure<RequirementReport> (
                        "requirement.gate-artifact", "passing gate needs a hashed artifact");
                const auto artifact = resolveArtifact (
                    sourceRoot, candidateRoot, gateResult->artifactPath);
                if (! artifact.ok()
                    || sha256File (artifact.value->file) != gateResult->artifactSha256)
                    return failure<RequirementReport> (
                        "requirement.gate-artifact", "passing gate artifact is missing or changed");
                if (std::find (result.definition.artifactPaths.begin(),
                               result.definition.artifactPaths.end(),
                               artifact.value->reportPath)
                    == result.definition.artifactPaths.end()) {
                    result.definition.artifactPaths.push_back (artifact.value->reportPath);
                    result.definition.artifactSha256.push_back (gateResult->artifactSha256);
                    result.artifactHashes.push_back (gateResult->artifactSha256);
                }
            }
        }

        if (anyFail) {
            result.status = Status::fail;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.declared-fail");
        } else if (awaitingHardware) {
            result.status = Status::awaitingApprovedReference;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.awaiting-approved-reference");
        } else if (anyNotRun) {
            result.status = Status::notRun;
            if (result.reasons.empty())
                result.reasons.push_back ("requirement.not-run");
        } else if (! definition.gateIds.empty()
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
    if (root == nullptr
        || root->getProperty ("schema").toString() != "model-d.requirements-report.v1"
        || static_cast<int> (root->getProperty ("manifestVersion")) != 1
        || static_cast<int> (root->getProperty ("fixtureVersion")) != 1
        || static_cast<int> (root->getProperty ("analyzerVersion")) != 1)
        return failure<bool> ("release.report-schema", "requirement report schema is unsupported");
    const auto* rows = root->getProperty ("requirements").getArray();
    if (rows == nullptr || rows->size() != 127)
        return failure<bool> ("release.report-set", "requirement report must contain 127 rows");

    bool allPass = true;
    std::string firstOpen;
    std::set<std::string> ids;
    std::vector<std::string> orderedIds;
    for (const auto& rowValue : *rows) {
        const auto* row = rowValue.getDynamicObject();
        std::string id;
        std::string status;
        if (row == nullptr || ! readString (*row, "id", id)
            || ! readString (*row, "status", status) || ! ids.insert (id).second)
            return failure<bool> ("release.report-row", "requirement report row is malformed");
        orderedIds.push_back (id);
        const auto* artifacts = row->getProperty ("artifacts").getArray();
        const auto* reasons = row->getProperty ("reasons").getArray();
        if (artifacts == nullptr || reasons == nullptr || reasons->isEmpty())
            return failure<bool> ("release.report-row", "requirement row evidence is malformed");
        if (status == "pass" && artifacts->isEmpty())
            return failure<bool> ("release.pass-artifact", "passing requirement has no artifact");
        if (status != "pass" && status != "fail" && status != "not-run"
            && status != "awaiting-approved-reference")
            return failure<bool> ("release.report-status", "requirement row status is unsupported");
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
                if (! candidate.ok())
                    return failure<bool> (
                        "release.report-artifact", "candidate artifact is missing or unbounded");
                resolved = *candidate.value;
            } else {
                const auto source = resolveBoundedRegularFile (
                    juce::File { SYNTH_SOURCE_ROOT }, path);
                if (! source.ok())
                    return failure<bool> (
                        "release.report-artifact", "source artifact is missing or unbounded");
                resolved = *source.value;
            }
            if (sha256File (resolved) != hash)
                return failure<bool> ("release.report-artifact", "report artifact hash changed");
        }
        if (status != "pass" && firstOpen.empty()) {
            const auto reason = (*reasons)[0].toString().toStdString();
            firstOpen = id + " " + status + " " + reason;
        }
        allPass = allPass && status == "pass";
    }
    const auto matrix = readMatrixIds (
        juce::File { SYNTH_SOURCE_ROOT }.getChildFile (
            "docs/remediation/01-traceability-matrix.md"));
    if (! matrix.ok() || orderedIds != *matrix.value)
        return failure<bool> (
            "release.report-set", "requirement report IDs must exactly match matrix order");
    if (static_cast<bool> (root->getProperty ("releaseReady")) != allPass)
        return failure<bool> ("release.report-ready", "releaseReady disagrees with row statuses");
    if (! allPass)
        return failure<bool> ("release.not-ready", firstOpen);
    return { true, {} };
}

} // namespace ReferenceHarness
