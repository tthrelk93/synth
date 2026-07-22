#include "Acceptance.h"

#include "ReferenceData.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <set>

namespace ReferenceHarness {
namespace {

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

const juce::var* property (const juce::DynamicObject& object, const char* name)
{
    return object.getProperties().getVarPointer (juce::Identifier { name });
}

bool readString (const juce::DynamicObject& object, const char* name, std::string& destination)
{
    const auto* value = property (object, name);
    if (value == nullptr || ! value->isString() || value->toString().isEmpty())
        return false;
    destination = value->toString().toStdString();
    return true;
}

bool readNumber (const juce::DynamicObject& object, const char* name, double& destination)
{
    const auto* value = property (object, name);
    if (value == nullptr || (! value->isInt() && ! value->isInt64() && ! value->isDouble()))
        return false;
    destination = static_cast<double> (*value);
    return std::isfinite (destination);
}

bool readRequirements (const juce::DynamicObject& object, std::vector<std::string>& destination)
{
    const auto* value = property (object, "requirements");
    const auto* array = value == nullptr ? nullptr : value->getArray();
    if (array == nullptr || array->isEmpty())
        return false;
    std::set<std::string> unique;
    for (const auto& requirement : *array) {
        if (! requirement.isString() || requirement.toString().isEmpty())
            return false;
        auto text = requirement.toString().toStdString();
        if (! unique.insert (text).second)
            return false;
        destination.push_back (std::move (text));
    }
    return true;
}

bool lowercaseSha256 (const std::string& hash)
{
    return hash.size() == 64
        && std::all_of (hash.begin(), hash.end(), [] (const unsigned char character) {
               return std::isdigit (character) != 0
                   || (character >= 'a' && character <= 'f');
           });
}

std::optional<Status> parseStatus (const std::string_view name)
{
    if (name == "pass")
        return Status::pass;
    if (name == "fail")
        return Status::fail;
    if (name == "not-run")
        return Status::notRun;
    if (name == "awaiting-approved-reference")
        return Status::awaitingApprovedReference;
    return std::nullopt;
}

std::optional<GateClassification> parseClassification (const std::string_view name)
{
    if (name == "hard-software")
        return GateClassification::hardSoftware;
    if (name == "published")
        return GateClassification::published;
    if (name == "derived-software")
        return GateClassification::derivedSoftware;
    if (name == "measured-hardware")
        return GateClassification::measuredHardware;
    if (name == "performance")
        return GateClassification::performance;
    return std::nullopt;
}

bool knownMetric (const std::string_view analyzer, const std::string_view metric)
{
    if (analyzer == "signal.stats.v1")
        return metric == "sample-count" || metric == "finite-count" || metric == "minimum"
            || metric == "maximum" || metric == "peak-absolute" || metric == "mean"
            || metric == "rms" || metric == "maximum-first-difference";
    if (analyzer == "control.step.v1")
        return metric == "first-change-sample" || metric == "settled-sample"
            || metric == "monotonic" || metric == "overshoot"
            || metric == "maximum-per-sample-movement" || metric == "floating-allowance";
    if (analyzer == "audio.click.v1")
        return metric == "maximum-first-difference" || metric == "pre-rms"
            || metric == "post-rms" || metric == "peak-over-steady-state"
            || metric == "finite-count";
    return false;
}

bool knownUnit (const std::string_view unit)
{
    constexpr std::array units {
        "count", "amplitude", "amplitude/sample", "samples", "boolean",
        "normalized", "normalized/sample", "dB", "seconds",
    };
    return std::find (units.begin(), units.end(), unit) != units.end();
}

LoadResult<GateDefinition> readGate (const juce::DynamicObject& object,
                                     const GateClassification expectedClassification,
                                     const AnalyzerRegistry& analyzers)
{
    GateDefinition gate;
    std::string classification;
    std::string status;
    if (! readString (object, "id", gate.id))
        return failure<GateDefinition> ("acceptance.id", "acceptance gate ID is required");
    if (! readString (object, "classification", classification))
        return failure<GateDefinition> (
            "acceptance.classification", "acceptance gate classification is required");
    const auto parsedClassification = parseClassification (classification);
    if (! parsedClassification.has_value() || *parsedClassification != expectedClassification)
        return failure<GateDefinition> (
            "acceptance.classification", "acceptance gate classification is unknown or in the wrong section");
    gate.classification = *parsedClassification;

    if (! readString (object, "status", status))
        return failure<GateDefinition> ("acceptance.status", "acceptance gate status is required");
    const auto parsedStatus = parseStatus (status);
    if (! parsedStatus.has_value())
        return failure<GateDefinition> ("acceptance.status", "acceptance gate status is unsupported");
    gate.status = *parsedStatus;
    if (! readRequirements (object, gate.requirements))
        return failure<GateDefinition> (
            "acceptance.requirement", "acceptance gate must name at least one unique requirement");

    if (! readString (object, "analyzer", gate.analyzer.id))
        return failure<GateDefinition> ("acceptance.analyzer", "acceptance analyzer is required");
    const auto registered = analyzers.find (gate.analyzer.id);
    if (! registered.ok())
        return failure<GateDefinition> (
            "acceptance.analyzer", "acceptance analyzer must use a registered immutable ID");
    const auto* version = property (object, "analyzerVersion");
    if (version == nullptr || ! version->isInt())
        return failure<GateDefinition> (
            "acceptance.analyzer-version", "acceptance analyzer version is required");
    gate.analyzer.version = static_cast<int> (*version);
    if (gate.analyzer.version != registered.value->version)
        return failure<GateDefinition> (
            "acceptance.analyzer-version", "acceptance analyzer version is stale or unsupported");
    if (! readString (object, "metric", gate.metric) || ! knownMetric (gate.analyzer.id, gate.metric))
        return failure<GateDefinition> (
            "acceptance.metric", "acceptance metric is unknown for the analyzer version");
    if (! readString (object, "unit", gate.unit) || ! knownUnit (gate.unit))
        return failure<GateDefinition> ("acceptance.unit", "acceptance unit is unsupported");
    if (! readNumber (object, "value", gate.value))
        return failure<GateDefinition> ("acceptance.value", "acceptance value must be finite");

    if (const auto* allowance = property (object, "allowance"); allowance != nullptr) {
        if ((! allowance->isInt() && ! allowance->isInt64() && ! allowance->isDouble())
            || ! std::isfinite (static_cast<double> (*allowance)))
            return failure<GateDefinition> (
                "acceptance.value", "acceptance allowance must be finite");
        gate.allowance = static_cast<double> (*allowance);
    }
    if (const auto* artifactPath = property (object, "artifactPath"); artifactPath != nullptr)
        gate.artifactPath = artifactPath->toString().toStdString();
    if (const auto* artifactSha = property (object, "artifactSha256"); artifactSha != nullptr)
        gate.artifactSha256 = artifactSha->toString().toStdString();
    if (gate.status == Status::pass
        && (gate.artifactPath.empty() || ! lowercaseSha256 (gate.artifactSha256)))
        return failure<GateDefinition> (
            "acceptance.pass-artifact", "passing acceptance gates require a path and SHA-256 artifact");

    return { std::move (gate), {} };
}

bool readProvenance (const juce::DynamicObject& object,
                     GateDefinition& gate,
                     const std::initializer_list<const char*> names)
{
    for (const auto* name : names) {
        std::string value;
        if (! readString (object, name, value))
            return false;
        gate.provenance.emplace (name, std::move (value));
    }
    return true;
}

LoadResult<std::vector<GateDefinition>> readSection (
    const juce::DynamicObject& root,
    const char* sectionName,
    const GateClassification classification,
    const AnalyzerRegistry& analyzers)
{
    const auto* sectionValue = property (root, sectionName);
    const auto* section = sectionValue == nullptr ? nullptr : sectionValue->getArray();
    if (section == nullptr)
        return failure<std::vector<GateDefinition>> (
            "acceptance.section", "all acceptance classification arrays are required");

    std::vector<GateDefinition> gates;
    for (const auto& value : *section) {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return failure<std::vector<GateDefinition>> (
                "acceptance.shape", "acceptance gate entries must be objects");
        auto gate = readGate (*object, classification, analyzers);
        if (! gate.value.has_value())
            return { std::nullopt, gate.diagnostics };

        switch (classification) {
            case GateClassification::hardSoftware:
                if (! readProvenance (*object, *gate.value, { "provenance" }))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.hard-provenance", "hard software gates require provenance");
                break;
            case GateClassification::published:
                if (! readProvenance (*object, *gate.value, { "source", "page" }))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.source", "published gates require source and page provenance");
                break;
            case GateClassification::derivedSoftware: {
                if (! readProvenance (*object, *gate.value,
                                     { "derivation", "reviewStatus", "reviewBasis", "reviewDate",
                                       "claimScope" }))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.derivation", "derived software gates require complete derivation provenance");
                if (gate.value->provenance["reviewStatus"] != "approved")
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.derived-review", "derived software policy must be explicitly approved");
                if (gate.value->value == 0.0) {
                    std::string zeroBasis;
                    if (! readString (*object, "zeroBasis", zeroBasis))
                        return failure<std::vector<GateDefinition>> (
                            "acceptance.zero", "an approved zero requires an explicit non-placeholder basis");
                    gate.value->provenance.emplace ("zeroBasis", std::move (zeroBasis));
                }
                break;
            }
            case GateClassification::measuredHardware:
                if (! readProvenance (*object, *gate.value,
                                     { "referenceSet", "bandArtifact", "rawSha256", "uncertainty" }))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.reference", "measured gates require reference and uncertainty provenance");
                break;
            case GateClassification::performance:
                if (! readProvenance (*object, *gate.value,
                                     { "targetSystem", "budgetBasis", "rationale" }))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.performance", "performance gates require target and budget provenance");
                break;
        }
        gates.push_back (std::move (*gate.value));
    }
    return { std::move (gates), {} };
}

bool exactDerivedPolicy (const GateDefinition& gate)
{
    if (gate.status != Status::notRun
        || gate.requirements != std::vector<std::string> { "PAR-006", "TST-006" }
        || gate.analyzer.id != "control.step.v1" || gate.analyzer.version != 1
        || gate.allowance != 0.0
        || gate.provenance.at ("reviewStatus") != "approved"
        || gate.provenance.at ("reviewBasis") != "2026-07-22 user-approved design"
        || gate.provenance.at ("reviewDate") != "2026-07-22"
        || gate.provenance.at ("claimScope")
               != "software safety policy; not a hardware measurement")
        return false;
    if (gate.id == "par006.gain-control.duration")
        return gate.value == 0.005 && gate.unit == "seconds"
            && gate.metric == "settled-sample"
            && gate.provenance.at ("derivation")
                   == "5 ms linear-amplitude software safety ramp";
    if (gate.id == "par006.control.duration")
        return gate.value == 0.010 && gate.unit == "seconds"
            && gate.metric == "settled-sample"
            && gate.provenance.at ("derivation")
                   == "10 ms owner-declared control-domain software safety ramp";
    if (gate.id == "par006.settling.allowance")
        return gate.value == 1.0 && gate.unit == "samples"
            && gate.metric == "settled-sample"
            && gate.provenance.at ("derivation")
                   == "+1 sample after ceil(duration * sampleRate)";
    if (gate.id == "par006.none.intermediate")
        return gate.value == 0.0 && gate.unit == "count"
            && gate.metric == "maximum-per-sample-movement"
            && gate.provenance.at ("derivation")
                   == "0 intermediate values for registry class none"
            && gate.provenance.contains ("zeroBasis")
            && gate.provenance.at ("zeroBasis") == "approved exact-step policy";
    return false;
}

} // namespace

LoadResult<AcceptanceManifest> loadAcceptanceManifest (
    const juce::File& sourceRoot,
    const juce::File& manifestFile,
    const AnalyzerRegistry& analyzers)
{
    if (! manifestFile.existsAsFile())
        return failure<AcceptanceManifest> (
            "acceptance.missing", "acceptance manifest does not exist");
    juce::var parsed;
    if (juce::JSON::parse (manifestFile.loadFileAsString(), parsed).failed())
        return failure<AcceptanceManifest> (
            "acceptance.parse", "acceptance manifest is not valid JSON");
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return failure<AcceptanceManifest> (
            "acceptance.shape", "acceptance manifest root must be an object");

    AcceptanceManifest manifest;
    if (! readString (*root, "schema", manifest.schema)
        || manifest.schema != "model-d.acceptance.v1")
        return failure<AcceptanceManifest> (
            "acceptance.schema", "acceptance manifest schema is unsupported");
    const auto* version = property (*root, "manifestVersion");
    if (version == nullptr || ! version->isInt() || static_cast<int> (*version) != 1)
        return failure<AcceptanceManifest> (
            "acceptance.version", "acceptance manifest version is unsupported");
    manifest.version = 1;
    if (! readString (*root, "status", manifest.status)
        || (manifest.status != "draft" && manifest.status != "approved"))
        return failure<AcceptanceManifest> (
            "acceptance.status", "global acceptance status must be draft or approved");

    const auto read = [&] (const char* name, const GateClassification classification,
                           std::vector<GateDefinition>& destination) -> std::optional<Diagnostic> {
        auto section = readSection (*root, name, classification, analyzers);
        if (! section.value.has_value())
            return section.diagnostics.front();
        destination = std::move (*section.value);
        return std::nullopt;
    };
    if (const auto diagnostic = read ("hardSoftware", GateClassification::hardSoftware,
                                      manifest.hardSoftware); diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };
    if (const auto diagnostic = read ("published", GateClassification::published,
                                      manifest.published); diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };
    if (const auto diagnostic = read ("derivedSoftware", GateClassification::derivedSoftware,
                                      manifest.derivedSoftware); diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };
    if (const auto diagnostic = read ("measuredHardware", GateClassification::measuredHardware,
                                      manifest.measuredHardware); diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };
    if (const auto diagnostic = read ("performance", GateClassification::performance,
                                      manifest.performance); diagnostic.has_value())
        return { std::nullopt, { *diagnostic } };

    std::set<std::string> identifiers;
    for (const auto* section : { &manifest.hardSoftware, &manifest.published,
                                 &manifest.derivedSoftware, &manifest.measuredHardware,
                                 &manifest.performance })
        for (const auto& gate : *section)
            if (! identifiers.insert (gate.id).second)
                return failure<AcceptanceManifest> (
                    "acceptance.duplicate-id", "acceptance gate IDs must be globally unique");

    if (manifest.derivedSoftware.size() != 4
        || ! std::all_of (manifest.derivedSoftware.begin(), manifest.derivedSoftware.end(),
                          exactDerivedPolicy))
        return failure<AcceptanceManifest> (
            "acceptance.derived-policy", "only the four approved PAR-006 software policies are permitted");

    if (manifest.status == "approved") {
        const auto sections = std::array {
            &manifest.hardSoftware,
            &manifest.published,
            &manifest.derivedSoftware,
            &manifest.measuredHardware,
            &manifest.performance,
        };
        if (std::any_of (sections.begin(), sections.end(), [] (const auto* section) {
                return section->empty()
                    || std::any_of (section->begin(), section->end(), [] (const auto& gate) {
                           return gate.status != Status::pass;
                       });
            }))
            return failure<AcceptanceManifest> (
                "acceptance.incomplete",
                "globally approved acceptance requires every evidence class and gate to pass");
    }

    for (const auto* section : { &manifest.hardSoftware, &manifest.published,
                                 &manifest.derivedSoftware, &manifest.measuredHardware,
                                 &manifest.performance }) {
        for (const auto& gate : *section) {
            if (gate.status != Status::pass)
                continue;
            const auto artifact = resolveBoundedRegularFile (sourceRoot, gate.artifactPath);
            if (! artifact.ok() || sha256File (*artifact.value) != gate.artifactSha256)
                return failure<AcceptanceManifest> (
                    "acceptance.pass-artifact", "passing gate artifact must exist and match its SHA-256");
        }
    }

    return { std::move (manifest), {} };
}

LoadResult<std::vector<SmoothingCase>> expandSmoothingFixtures (
    const juce::File& sourceRoot,
    const FixtureIndex& index,
    const AcceptanceManifest& manifest)
{
    if (index.smoothingFixtures.size() != 7)
        return failure<std::vector<SmoothingCase>> (
            "smoothing.template-count", "exactly seven smoothing templates are required");

    std::map<ParameterRegistry::SmoothingClass, SmoothingFixture> templates;
    const auto analyzers = AnalyzerRegistry::withFoundationAnalyzers();
    for (const auto& artifact : index.smoothingFixtures) {
        const auto resolved = resolveBoundedRegularFile (sourceRoot, artifact.relativePath);
        if (! resolved.ok())
            return { std::nullopt, resolved.diagnostics };
        auto loaded = loadSmoothingFixture (*resolved.value);
        if (! loaded.value.has_value())
            return { std::nullopt, loaded.diagnostics };
        if (loaded.value->id != artifact.id || artifact.requirements != std::vector<std::string> { "PAR-006" })
            return failure<std::vector<SmoothingCase>> (
                "smoothing.index", "smoothing template identity and requirement must match its index entry");
        if (! analyzers.find (loaded.value->analyzerId).ok())
            return failure<std::vector<SmoothingCase>> (
                "smoothing.analyzer", "smoothing template analyzer must be registered");
        const auto hasPolicy = [&] (const std::optional<std::string>& policy,
                                    const std::string_view expected) {
            if (! policy.has_value() || *policy != expected)
                return false;
            return std::any_of (manifest.derivedSoftware.begin(),
                                manifest.derivedSoftware.end(), [&] (const auto& gate) {
                return gate.id == expected;
            });
        };
        const auto policyMatches = [&] {
            switch (loaded.value->smoothingClass) {
                case ParameterRegistry::SmoothingClass::none:
                    return hasPolicy (loaded.value->policyId, "par006.none.intermediate")
                        && ! loaded.value->settlingPolicyId.has_value();
                case ParameterRegistry::SmoothingClass::gainControl:
                    return hasPolicy (loaded.value->policyId, "par006.gain-control.duration")
                        && hasPolicy (loaded.value->settlingPolicyId,
                                      "par006.settling.allowance");
                case ParameterRegistry::SmoothingClass::control:
                    return hasPolicy (loaded.value->policyId, "par006.control.duration")
                        && hasPolicy (loaded.value->settlingPolicyId,
                                      "par006.settling.allowance");
                case ParameterRegistry::SmoothingClass::dedicatedPitch:
                case ParameterRegistry::SmoothingClass::dedicatedCutoff:
                case ParameterRegistry::SmoothingClass::dedicatedGlide:
                case ParameterRegistry::SmoothingClass::contourStage:
                    return ! loaded.value->policyId.has_value()
                        && ! loaded.value->settlingPolicyId.has_value();
                case ParameterRegistry::SmoothingClass::unspecified:
                    return false;
            }
            return false;
        }();
        if (! policyMatches)
            return failure<std::vector<SmoothingCase>> (
                "smoothing.policy",
                "smoothing template policies must match the exact approved manifest class policies");
        loaded.value->relativePath = artifact.relativePath;
        loaded.value->sha256 = artifact.sha256;
        if (! templates.emplace (loaded.value->smoothingClass, std::move (*loaded.value)).second)
            return failure<std::vector<SmoothingCase>> (
                "smoothing.duplicate-class", "each registry smoothing class must have one template");
    }

    constexpr std::array expectedCounts { 27, 7, 5, 1, 1, 1, 6 };
    std::array<int, expectedCounts.size()> actualCounts {};
    std::vector<SmoothingCase> cases;
    cases.reserve (ParameterRegistry::parameterCount);
    for (size_t position = 0; position < ParameterRegistry::descriptors().size(); ++position) {
        const auto& descriptor = ParameterRegistry::descriptors()[position];
        const auto fixture = templates.find (descriptor.smoothing);
        if (fixture == templates.end())
            return failure<std::vector<SmoothingCase>> (
                "smoothing.coverage", "every live registry smoothing class requires one template");
        const auto countIndex = static_cast<size_t> (descriptor.smoothing)
                              - static_cast<size_t> (ParameterRegistry::SmoothingClass::none);
        if (countIndex >= actualCounts.size())
            return failure<std::vector<SmoothingCase>> (
                "smoothing.coverage", "unspecified registry smoothing classes are forbidden");
        ++actualCounts[countIndex];
        SmoothingCase smoothingCase;
        static_cast<SmoothingFixture&> (smoothingCase) = fixture->second;
        smoothingCase.key = static_cast<ParameterRegistry::Key> (position);
        smoothingCase.parameterId = std::string { descriptor.id };
        cases.push_back (std::move (smoothingCase));
    }
    if (cases.size() != ParameterRegistry::parameterCount || actualCounts != expectedCounts)
        return failure<std::vector<SmoothingCase>> (
            "smoothing.registry-count", "live smoothing class counts differ from the frozen 48-key registry");
    return { std::move (cases), {} };
}

LoadResult<std::vector<GateResult>> evaluateAcceptance (
    const AcceptanceManifest& manifest,
    const std::span<const SmoothingCase> smoothingCases,
    const std::span<const SmoothingEvidence> evidence,
    const AnalyzerRegistry& analyzers)
{
    std::vector<GateResult> results;
    const auto appendManifestSection = [&] (const std::vector<GateDefinition>& gates) {
        for (const auto& gate : gates)
            results.push_back ({ gate.id, gate.status,
                                 gate.status == Status::notRun ? "manifest-draft" : "declared-status",
                                 gate.requirements, std::nullopt,
                                 gate.artifactPath, gate.artifactSha256 });
    };
    appendManifestSection (manifest.hardSoftware);
    appendManifestSection (manifest.published);
    appendManifestSection (manifest.derivedSoftware);
    appendManifestSection (manifest.measuredHardware);
    appendManifestSection (manifest.performance);

    std::map<std::string, const SmoothingEvidence*> evidenceByParameter;
    for (const auto& item : evidence) {
        if (item.parameterId.empty()
            || ! evidenceByParameter.emplace (item.parameterId, &item).second)
            return failure<std::vector<GateResult>> (
                "smoothing.evidence-identity",
                "smoothing evidence parameter IDs must be nonempty and unique");
    }

    const auto metricNamed = [] (const std::vector<MetricResult>& metrics,
                                 const std::string_view name) -> const MetricResult* {
        const auto found = std::find_if (metrics.begin(), metrics.end(), [&] (const auto& metric) {
            return metric.metric == name;
        });
        return found == metrics.end() ? nullptr : &*found;
    };

    for (const auto& smoothingCase : smoothingCases) {
        GateResult control {
            "par006." + smoothingCase.parameterId + ".control",
            Status::notRun,
            smoothingCase.smoothingClass == ParameterRegistry::SmoothingClass::none
                ? "smoothing.evidence-missing" : smoothingCase.reasonCode,
            { "PAR-006" },
            std::nullopt, {}, {},
        };

        const auto supplied = evidenceByParameter.find (smoothingCase.parameterId);
        if (smoothingCase.smoothingClass == ParameterRegistry::SmoothingClass::none
            && supplied != evidenceByParameter.end()) {
            const auto& item = *supplied->second;
            const auto artifact = juce::File { item.candidateArtifactPath };
            const auto outputHash = item.render.reproducibility.outputHashes.find (
                "control-trace.json");
            if (item.candidateArtifactPath.empty()
                || artifact.getFileName() != "control-trace.json"
                || ! lowercaseSha256 (item.candidateArtifactSha256)
                || ! artifact.existsAsFile()
                || sha256File (artifact) != item.candidateArtifactSha256
                || outputHash == item.render.reproducibility.outputHashes.end()
                || outputHash->second != item.candidateArtifactSha256) {
                control.status = Status::fail;
                control.reasonCode = "smoothing.artifact-hash";
            } else if (std::find (smoothingCase.sampleRates.begin(),
                                 smoothingCase.sampleRates.end(),
                                 static_cast<int> (item.render.sampleRate))
                       == smoothingCase.sampleRates.end()
                       || item.render.sampleRate
                              != static_cast<double> (static_cast<int> (item.render.sampleRate))) {
                control.status = Status::fail;
                control.reasonCode = "smoothing.trace-mismatch";
            } else {
                std::vector<const ControlTracePoint*> points;
                for (const auto& point : item.render.controlTrace)
                    if (point.key == smoothingCase.key)
                        points.push_back (&point);
                std::sort (points.begin(), points.end(), [] (const auto* left, const auto* right) {
                    return left->sample < right->sample;
                });
                const auto eventPrefix = "automation:"
                                       + std::to_string (smoothingCase.eventSample) + ":";
                const auto parameterToken = ":" + smoothingCase.parameterId + ":";
                const auto hasEvent = std::any_of (
                    item.render.eventTrace.begin(), item.render.eventTrace.end(),
                    [&] (const auto& event) {
                        return event.starts_with (eventPrefix)
                            && event.find (parameterToken) != std::string::npos;
                    });
                if (points.size() != 2 || points.front()->sample >= smoothingCase.eventSample
                    || points.back()->sample != smoothingCase.eventSample || ! hasEvent
                    || ! std::isfinite (points.front()->normalizedValue)
                    || ! std::isfinite (points.back()->normalizedValue)
                    || std::abs (static_cast<double> (points.front()->normalizedValue)
                                 - smoothingCase.startNormalized)
                           > 8.0 * std::numeric_limits<double>::epsilon()) {
                    control.status = Status::fail;
                    control.reasonCode = "smoothing.trace-mismatch";
                } else {
                    std::vector<double> denseControl (
                        static_cast<size_t> (smoothingCase.eventSample + 2),
                        static_cast<double> (points.front()->normalizedValue));
                    std::fill (denseControl.begin()
                                   + static_cast<std::ptrdiff_t> (smoothingCase.eventSample),
                               denseControl.end(),
                               static_cast<double> (points.back()->normalizedValue));
                    const auto analyzed = analyzers.analyze (
                        smoothingCase.analyzerId,
                        AnalysisRequest {
                            .eventSample = smoothingCase.eventSample,
                            .durationSamples = 0,
                            .start = smoothingCase.startNormalized,
                            .target = smoothingCase.endNormalized,
                            .control = denseControl,
                        });
                    const auto validMetrics = [&] {
                        if (! analyzed.ok() || ! analyzed.value.has_value())
                            return false;
                        const auto* first = metricNamed (*analyzed.value, "first-change-sample");
                        const auto* settled = metricNamed (*analyzed.value, "settled-sample");
                        const auto* monotonic = metricNamed (*analyzed.value, "monotonic");
                        const auto* overshoot = metricNamed (*analyzed.value, "overshoot");
                        const auto* movement = metricNamed (
                            *analyzed.value, "maximum-per-sample-movement");
                        return first != nullptr && settled != nullptr && monotonic != nullptr
                            && overshoot != nullptr && movement != nullptr
                            && first->finite && settled->finite && monotonic->finite
                            && overshoot->finite && movement->finite
                            && first->value == static_cast<double> (smoothingCase.eventSample)
                            && settled->value == static_cast<double> (smoothingCase.eventSample)
                            && monotonic->value == 1.0 && overshoot->value == 0.0
                            && movement->value
                                   == std::abs (smoothingCase.endNormalized
                                                - smoothingCase.startNormalized)
                            && smoothingCase.durationSeconds == 0.0
                            && smoothingCase.intermediateValues == std::optional<int> { 0 };
                    }();
                    if (! validMetrics) {
                        control.status = Status::fail;
                        control.reasonCode = "smoothing.metric-mismatch";
                    } else {
                        const auto* settled = metricNamed (*analyzed.value, "settled-sample");
                        control.status = Status::pass;
                        control.reasonCode = "smoothing.exact-step-pass";
                        control.metric = *settled;
                        control.artifactPath = item.candidateArtifactPath;
                        control.artifactSha256 = item.candidateArtifactSha256;
                    }
                }
            }
        }
        results.push_back (std::move (control));
        results.push_back ({ "par006." + smoothingCase.parameterId + ".reference",
                             smoothingCase.referenceStatus,
                             smoothingCase.referenceReasonCode,
                             { "PAR-006" }, std::nullopt, {}, {} });
    }
    return { std::move (results), {} };
}

} // namespace ReferenceHarness
