#include "Acceptance.h"

#include "OfflineRenderer.h"
#include "ReferenceData.h"
#include "SourceIdentity.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
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

bool readUnsignedInteger (const juce::DynamicObject& object,
                          const char* name,
                          std::uint64_t& destination)
{
    const auto* value = property (object, name);
    if (value == nullptr || (! value->isInt() && ! value->isInt64()))
        return false;
    const auto parsed = static_cast<juce::int64> (*value);
    if (parsed < 0)
        return false;
    destination = static_cast<std::uint64_t> (parsed);
    return true;
}

bool readInteger (const juce::DynamicObject& object, const char* name, int& destination)
{
    const auto* value = property (object, name);
    if (value == nullptr || ! value->isInt())
        return false;
    destination = static_cast<int> (*value);
    return true;
}

bool readBoolean (const juce::DynamicObject& object, const char* name, bool& destination)
{
    const auto* value = property (object, name);
    if (value == nullptr || ! value->isBool())
        return false;
    destination = static_cast<bool> (*value);
    return true;
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

bool exactStringArray (const juce::var* value, const std::vector<std::string>& expected)
{
    const auto* array = value == nullptr ? nullptr : value->getArray();
    if (array == nullptr || static_cast<size_t> (array->size()) != expected.size())
        return false;
    for (int index = 0; index < array->size(); ++index)
        if (! (*array)[index].isString()
            || (*array)[index].toString().toStdString() != expected[static_cast<size_t> (index)])
            return false;
    return true;
}

bool exactBlockPatterns (const juce::var* value,
                         const std::vector<std::vector<int>>& expected)
{
    const auto* patterns = value == nullptr ? nullptr : value->getArray();
    if (patterns == nullptr || static_cast<size_t> (patterns->size()) != expected.size())
        return false;
    for (int patternIndex = 0; patternIndex < patterns->size(); ++patternIndex) {
        const auto* pattern = (*patterns)[patternIndex].getArray();
        const auto& expectedPattern = expected[static_cast<size_t> (patternIndex)];
        if (pattern == nullptr || static_cast<size_t> (pattern->size()) != expectedPattern.size())
            return false;
        for (int sizeIndex = 0; sizeIndex < pattern->size(); ++sizeIndex)
            if (! (*pattern)[sizeIndex].isInt()
                || static_cast<int> ((*pattern)[sizeIndex])
                       != expectedPattern[static_cast<size_t> (sizeIndex)])
                return false;
    }
    return true;
}

bool exactStringMap (const juce::var* value,
                     const std::map<std::string, std::string>& expected)
{
    const auto* object = value == nullptr ? nullptr : value->getDynamicObject();
    if (object == nullptr
        || static_cast<size_t> (object->getProperties().size()) != expected.size())
        return false;
    for (const auto& [name, expectedValue] : expected) {
        const auto* actual = property (*object, name.c_str());
        if (actual == nullptr || ! actual->isString()
            || actual->toString().toStdString() != expectedValue)
            return false;
    }
    return true;
}

std::optional<ParameterRegistry::Key> keyForParameterId (const std::string_view id)
{
    const auto descriptors = ParameterRegistry::descriptors();
    const auto found = std::find_if (descriptors.begin(), descriptors.end(), [&] (const auto& item) {
        return item.id == id;
    });
    if (found == descriptors.end())
        return std::nullopt;
    return static_cast<ParameterRegistry::Key> (std::distance (descriptors.begin(), found));
}

bool sameControlTrace (const std::vector<ControlTracePoint>& left,
                       const std::vector<ControlTracePoint>& right)
{
    return left.size() == right.size()
        && std::equal (left.begin(), left.end(), right.begin(), [] (const auto& a, const auto& b) {
               return a.sample == b.sample && a.key == b.key
                   && a.normalizedValue == b.normalizedValue
                   && a.physicalValue == b.physicalValue;
           });
}

std::string sha256Bytes (const void* data, const size_t size)
{
    return juce::SHA256 { data, size }.toHexString().toStdString();
}

struct BoundCandidate {
    std::vector<ControlTracePoint> controlTrace;
    std::vector<std::string> eventTrace;
    juce::File controlFile;
    std::string controlSha256;
};

LoadResult<BoundCandidate> loadBoundCandidate (const SmoothingEvidence& evidence)
{
    const auto& directory = evidence.candidateDirectory;
    const auto renderFile = directory.getChildFile ("render.json");
    const auto controlFile = directory.getChildFile ("control-trace.json");
    const auto eventFile = directory.getChildFile ("event-trace.json");
    const auto mainFile = directory.getChildFile ("main.wav");
    const auto metricsFile = directory.getChildFile ("metrics.json");
    const auto phonesFile = directory.getChildFile ("phones.wav");
    const std::array files { controlFile, eventFile, mainFile, metricsFile, phonesFile };
    if (! directory.isDirectory() || ! renderFile.existsAsFile()
        || std::any_of (files.begin(), files.end(), [] (const auto& file) {
               return ! file.existsAsFile();
           }))
        return failure<BoundCandidate> (
            "smoothing.artifact-hash", "candidate evidence files must all exist");

    juce::var parsedRender;
    if (juce::JSON::parse (renderFile.loadFileAsString(), parsedRender).failed())
        return failure<BoundCandidate> (
            "smoothing.artifact-hash", "candidate render manifest must be valid JSON");
    const auto* root = parsedRender.getDynamicObject();
    const auto* reproducibilityValue = root == nullptr ? nullptr : property (*root, "reproducibility");
    const auto* reproducibility = reproducibilityValue == nullptr
                                    ? nullptr : reproducibilityValue->getDynamicObject();
    const auto* outputHashesValue = reproducibility == nullptr
                                      ? nullptr : property (*reproducibility, "outputHashes");
    const auto* outputHashes = outputHashesValue == nullptr
                                 ? nullptr : outputHashesValue->getDynamicObject();
    if (root == nullptr || root->getProperties().size() != 12
        || reproducibility == nullptr || reproducibility->getProperties().size() != 14
        || outputHashes == nullptr
        || outputHashes->getProperties().size() != 5)
        return failure<BoundCandidate> (
            "smoothing.artifact-hash", "candidate render hashes are missing or malformed");

    const std::array<std::pair<const char*, juce::File>, 5> hashedFiles {
        std::pair { "control-trace.json", controlFile },
        std::pair { "event-trace.json", eventFile },
        std::pair { "main.wav", mainFile },
        std::pair { "metrics.json", metricsFile },
        std::pair { "phones.wav", phonesFile },
    };
    for (const auto& [name, file] : hashedFiles) {
        const auto* declared = property (*outputHashes, name);
        if (declared == nullptr || ! declared->isString()
            || ! lowercaseSha256 (declared->toString().toStdString())
            || declared->toString().toStdString() != sha256File (file))
            return failure<BoundCandidate> (
                "smoothing.artifact-hash",
                "candidate render hashes must bind every output artifact byte-for-byte");
    }

    std::string schema;
    std::string fixtureId;
    std::string fixturePurpose;
    std::string fixtureReviewStatus;
    double sampleRate = 0.0;
    int mainChannels = 0;
    int phonesChannels = 0;
    std::uint64_t totalSamples = 0;
    if (! readString (*root, "schema", schema) || schema != "model-d.render-result.v1"
        || ! readString (*root, "fixtureId", fixtureId)
        || fixtureId != evidence.fixture.id
        || ! readString (*root, "purpose", fixturePurpose)
        || fixturePurpose != evidence.fixture.purpose
        || ! readString (*root, "reviewStatus", fixtureReviewStatus)
        || fixtureReviewStatus != fixtureReviewStatusName (evidence.fixture.reviewStatus)
        || ! readNumber (*root, "sampleRate", sampleRate)
        || sampleRate != evidence.fixture.config.sampleRate
        || sampleRate != evidence.render.sampleRate
        || ! readInteger (*root, "mainChannels", mainChannels)
        || mainChannels != evidence.render.mainChannels
        || ! readInteger (*root, "phonesChannels", phonesChannels)
        || phonesChannels != evidence.render.phonesChannels
        || ! readUnsignedInteger (*root, "totalSamples", totalSamples)
        || totalSamples != evidence.fixture.config.totalSamples
        || property (*root, "analysisRequests") == nullptr
        || canonicalJson (*property (*root, "analysisRequests"))
               != canonicalJson (analysisRequestsJson (evidence.fixture.analysisRequests))
        || ! exactStringArray (property (*root, "requirements"), evidence.fixture.requirements)
        || ! exactBlockPatterns (property (*root, "blockPatterns"),
                                 evidence.fixture.config.blockPatterns)
        || evidence.fixture.config.blockPatterns.empty()
        || evidence.render.blockPattern != evidence.fixture.config.blockPatterns.front())
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch",
            "candidate render identity and configuration must match the supplied fixture and render");

    std::string sourceCommit;
    std::string sourceTree;
    std::string sourceContent;
    bool sourceDirty = false;
    std::string juceCommit;
    std::string buildType;
    std::string platform;
    std::string architecture;
    std::string compilerId;
    std::string compilerVersion;
    std::string fixtureSha256;
    std::uint64_t seed = 0;
    std::map<std::string, std::string> expectedInputHashes {
        { "state", evidence.fixture.stateSha256 },
    };
    if (evidence.fixture.input.kind == InputKind::wav)
        expectedInputHashes.emplace ("audio", evidence.fixture.input.sha256);
    const auto& builtIdentity = builtSourceIdentity();
    if (! readString (*reproducibility, "sourceCommit", sourceCommit)
        || sourceCommit != evidence.render.reproducibility.sourceCommit
        || sourceCommit != builtIdentity.commit
        || ! readString (*reproducibility, "sourceTree", sourceTree)
        || sourceTree != evidence.render.reproducibility.sourceTree
        || sourceTree != builtIdentity.tree
        || ! readString (*reproducibility, "sourceContent", sourceContent)
        || sourceContent != evidence.render.reproducibility.sourceContent
        || sourceContent != builtIdentity.content
        || ! readBoolean (*reproducibility, "sourceDirty", sourceDirty)
        || sourceDirty != evidence.render.reproducibility.sourceDirty
        || sourceDirty != builtIdentity.dirty
        || ! readString (*reproducibility, "juceCommit", juceCommit)
        || juceCommit != evidence.render.reproducibility.juceCommit
        || ! readString (*reproducibility, "buildType", buildType)
        || buildType != evidence.render.reproducibility.buildType
        || ! readString (*reproducibility, "platform", platform)
        || platform != evidence.render.reproducibility.platform
        || ! readString (*reproducibility, "architecture", architecture)
        || architecture != evidence.render.reproducibility.architecture
        || ! readString (*reproducibility, "compilerId", compilerId)
        || compilerId != evidence.render.reproducibility.compilerId
        || compilerId != SYNTH_CONFIGURED_COMPILER_ID
        || ! readString (*reproducibility, "compilerVersion", compilerVersion)
        || compilerVersion != evidence.render.reproducibility.compilerVersion
        || compilerVersion != SYNTH_CONFIGURED_COMPILER_VERSION
        || ! readString (*reproducibility, "fixtureSha256", fixtureSha256)
        || fixtureSha256 != evidence.render.reproducibility.fixtureSha256
        || ! evidence.fixture.fixtureFile.existsAsFile()
        || fixtureSha256 != sha256File (evidence.fixture.fixtureFile)
        || ! evidence.fixture.stateFile.existsAsFile()
        || evidence.fixture.stateSha256 != sha256File (evidence.fixture.stateFile)
        || (evidence.fixture.input.kind == InputKind::wav
            && (! evidence.fixture.input.file.existsAsFile()
                || evidence.fixture.input.sha256 != sha256File (evidence.fixture.input.file)))
        || ! readUnsignedInteger (*reproducibility, "seed", seed)
        || seed != evidence.render.reproducibility.seed
        || seed != evidence.fixture.config.seed
        || evidence.render.reproducibility.inputHashes != expectedInputHashes
        || ! exactStringMap (property (*reproducibility, "inputHashes"),
                             expectedInputHashes))
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch",
            "candidate provenance must match the supplied fixture and render");

    juce::var parsedControl;
    if (juce::JSON::parse (controlFile.loadFileAsString(), parsedControl).failed())
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch", "candidate control trace must be valid JSON");
    const auto* controlRoot = parsedControl.getDynamicObject();
    std::string controlSchema;
    const auto* pointsValue = controlRoot == nullptr ? nullptr : property (*controlRoot, "points");
    const auto* points = pointsValue == nullptr ? nullptr : pointsValue->getArray();
    BoundCandidate bound;
    bound.controlFile = controlFile;
    bound.controlSha256 = sha256File (controlFile);
    if (controlRoot == nullptr || controlRoot->getProperties().size() != 2
        || ! readString (*controlRoot, "schema", controlSchema)
        || controlSchema != "model-d.control-trace.v1" || points == nullptr)
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch", "candidate control trace schema is unsupported");
    bound.controlTrace.reserve (static_cast<size_t> (points->size()));
    for (const auto& value : *points) {
        const auto* point = value.getDynamicObject();
        std::uint64_t sample = 0;
        std::string parameterId;
        double normalized = 0.0;
        double physical = 0.0;
        if (point == nullptr || point->getProperties().size() != 4
            || ! readUnsignedInteger (*point, "sample", sample)
            || ! readString (*point, "parameterId", parameterId)
            || ! readNumber (*point, "normalizedValue", normalized)
            || ! readNumber (*point, "physicalValue", physical))
            return failure<BoundCandidate> (
                "smoothing.trace-mismatch", "candidate control trace point is malformed");
        const auto key = keyForParameterId (parameterId);
        const auto normalizedFloat = static_cast<float> (normalized);
        const auto physicalFloat = static_cast<float> (physical);
        if (! key.has_value()
            || static_cast<double> (normalizedFloat) != normalized
            || static_cast<double> (physicalFloat) != physical)
            return failure<BoundCandidate> (
                "smoothing.trace-mismatch", "candidate control trace point is not renderer-exact");
        bound.controlTrace.push_back ({ sample, *key, normalizedFloat, physicalFloat });
    }
    if (! sameControlTrace (bound.controlTrace, evidence.render.controlTrace))
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch",
            "complete candidate control trace must match the supplied render result");

    juce::var parsedEvent;
    if (juce::JSON::parse (eventFile.loadFileAsString(), parsedEvent).failed())
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch", "candidate event trace must be valid JSON");
    const auto* eventRoot = parsedEvent.getDynamicObject();
    std::string eventSchema;
    if (eventRoot == nullptr || eventRoot->getProperties().size() != 2
        || ! readString (*eventRoot, "schema", eventSchema)
        || eventSchema != "model-d.event-trace.v1"
        || ! exactStringArray (property (*eventRoot, "events"), evidence.render.eventTrace))
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch",
            "complete candidate event trace must match the supplied render result");
    bound.eventTrace = evidence.render.eventTrace;

    const auto controlOutput = evidence.render.reproducibility.outputHashes.find ("control");
    const auto eventOutput = evidence.render.reproducibility.outputHashes.find ("event");
    const auto mainOutput = evidence.render.reproducibility.outputHashes.find ("main");
    const auto phonesOutput = evidence.render.reproducibility.outputHashes.find ("phones");
    if (evidence.render.reproducibility.outputHashes.size() != 4
        || controlOutput == evidence.render.reproducibility.outputHashes.end()
        || eventOutput == evidence.render.reproducibility.outputHashes.end()
        || mainOutput == evidence.render.reproducibility.outputHashes.end()
        || phonesOutput == evidence.render.reproducibility.outputHashes.end()
        || controlOutput->second != bound.controlSha256
        || eventOutput->second != sha256File (eventFile)
        || mainOutput->second
               != sha256Bytes (evidence.render.main.data(),
                               evidence.render.main.size() * sizeof (float))
        || phonesOutput->second
               != sha256Bytes (evidence.render.phones.data(),
                               evidence.render.phones.size() * sizeof (float)))
        return failure<BoundCandidate> (
            "smoothing.trace-mismatch",
            "supplied render trace hashes must match the Task 2 candidate trace bytes");
    return { std::move (bound), {} };
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
    if (analyzer == "audio.pitch.v1")
        return metric == "frequency-hz" || metric == "midi-semitones"
            || metric == "confidence";
    return false;
}

bool knownUnit (const std::string_view unit)
{
    constexpr std::array units {
        "count", "amplitude", "amplitude/sample", "samples", "boolean",
        "normalized", "normalized/sample", "dB", "seconds", "Hz", "semitones",
        "ratio",
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

    const auto hasFixtureBinding = property (object, "fixtureId") != nullptr;
    const auto hasRequestBinding = property (object, "requestId") != nullptr;
    if (hasFixtureBinding != hasRequestBinding)
        return failure<GateDefinition> (
            "acceptance.metric-binding",
            "render metric binding requires both fixture and request IDs");
    if (hasFixtureBinding) {
        RenderMetricBinding binding;
        if (! readString (object, "fixtureId", binding.fixtureId)
            || ! readString (object, "requestId", binding.requestId)
            || ! isPortableIdentifierComponent (binding.fixtureId)
            || ! isPortableIdentifierComponent (binding.requestId))
            return failure<GateDefinition> (
                "acceptance.metric-binding",
                "render metric binding IDs must be nonempty portable components");
        gate.renderMetric = std::move (binding);
    }

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
            || ! std::isfinite (static_cast<double> (*allowance))
            || static_cast<double> (*allowance) < 0.0)
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

bool validIsoDate (const std::string& value)
{
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
        return false;
    for (size_t index = 0; index < value.size(); ++index)
        if (index != 4 && index != 7
            && std::isdigit (static_cast<unsigned char> (value[index])) == 0)
            return false;
    const auto year = std::stoi (value.substr (0, 4));
    const auto month = static_cast<unsigned> (std::stoi (value.substr (5, 2)));
    const auto day = static_cast<unsigned> (std::stoi (value.substr (8, 2)));
    return std::chrono::year_month_day { std::chrono::year { year },
                                         std::chrono::month { month },
                                         std::chrono::day { day } }.ok();
}

bool readPublishedProvenance (const juce::DynamicObject& object, GateDefinition& gate)
{
    PublishedProvenance provenance;
    if (! readString (object, "source", provenance.source)
        || ! readString (object, "sourceVersion", provenance.sourceVersion)
        || ! readString (object, "page", provenance.page))
        return false;
    gate.published = std::move (provenance);
    return true;
}

bool readMeasuredHardwareProvenance (const juce::DynamicObject& object,
                                     GateDefinition& gate)
{
    MeasuredHardwareProvenance provenance;
    const auto* rawHashesValue = property (object, "rawSha256");
    const auto* rawHashes = rawHashesValue == nullptr ? nullptr : rawHashesValue->getArray();
    const auto* repetitionCount = property (object, "repetitionCount");
    if (! readString (object, "referenceStatus", provenance.referenceStatus)
        || provenance.referenceStatus != "approved"
        || ! readString (object, "referenceSet", provenance.referenceSet)
        || ! readString (object, "bandArtifact", provenance.bandArtifact)
        || rawHashes == nullptr || rawHashes->isEmpty()
        || ! readString (object, "instrument", provenance.instrument)
        || ! readString (object, "environment", provenance.environment)
        || ! readString (object, "captureChain", provenance.captureChain)
        || repetitionCount == nullptr || ! repetitionCount->isInt()
        || static_cast<int> (*repetitionCount) <= 0
        || ! readString (object, "repetitionStatistic", provenance.repetitionStatistic)
        || ! readString (object, "uncertaintyMethod", provenance.uncertaintyMethod)
        || ! readNumber (object, "uncertaintyValue", provenance.uncertaintyValue)
        || provenance.uncertaintyValue < 0.0
        || ! readString (object, "approver", provenance.approver)
        || ! readString (object, "approvalDate", provenance.approvalDate)
        || ! validIsoDate (provenance.approvalDate))
        return false;
    provenance.repetitionCount = static_cast<int> (*repetitionCount);
    for (const auto& hash : *rawHashes) {
        if (! hash.isString() || ! lowercaseSha256 (hash.toString().toStdString()))
            return false;
        provenance.rawSha256.push_back (hash.toString().toStdString());
    }
    gate.measuredHardware = std::move (provenance);
    return true;
}

bool readPerformanceProvenance (const juce::DynamicObject& object, GateDefinition& gate)
{
    PerformanceProvenance provenance;
    if (! readString (object, "targetSystem", provenance.targetSystem)
        || ! readString (object, "budgetBasis", provenance.budgetBasis)
        || ! readString (object, "rationale", provenance.rationale)
        || ! readString (object, "reviewStatus", provenance.reviewStatus)
        || provenance.reviewStatus != "approved"
        || ! readString (object, "reviewer", provenance.reviewer)
        || ! readString (object, "reviewDate", provenance.reviewDate)
        || ! validIsoDate (provenance.reviewDate))
        return false;
    gate.performance = std::move (provenance);
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
                if (! readPublishedProvenance (*object, *gate.value))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.source", "published gates require source, source version, and page provenance");
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
                if (! readMeasuredHardwareProvenance (*object, *gate.value))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.reference", "measured gates require approved typed capture provenance");
                break;
            case GateClassification::performance:
                if (! readPerformanceProvenance (*object, *gate.value))
                    return failure<std::vector<GateDefinition>> (
                        "acceptance.performance", "performance gates require approved typed review provenance");
                break;
        }
        gates.push_back (std::move (*gate.value));
    }
    return { std::move (gates), {} };
}

bool exactDerivedPolicy (const GateDefinition& gate)
{
    if (gate.id.starts_with ("par006.")) {
        if ((gate.status != Status::notRun && gate.status != Status::pass)
            || gate.requirements != std::vector<std::string> { "PAR-006", "TST-006" }
            || gate.analyzer.id != "control.step.v1" || gate.analyzer.version != 1
            || gate.allowance != 0.0
            || gate.renderMetric.has_value()
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

    struct PitchPolicy {
        std::string_view id;
        std::string_view requirement;
        double value;
        std::string_view fixtureId;
        std::string_view requestId;
        std::string_view derivation;
    };
    constexpr std::array pitchPolicies {
        PitchPolicy { "pit001.osc2.minus8", "PIT-001", 61.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc2-offset-m08",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit001.osc2.center", "PIT-001", 69.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc2-offset-z00",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit001.osc2.plus8", "PIT-001", 77.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc2-offset-p08",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit001.osc3.minus8", "PIT-001", 61.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc3-offset-m08",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit001.osc3.center", "PIT-001", 69.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc3-offset-z00",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit001.osc3.plus8", "PIT-001", 77.0,
                      "pit-001-oscillator-offset-sweep-v1", "osc3-offset-p08",
                      "selector index minus eight in the shared semitone domain" },
        PitchPolicy { "pit002.osc1.composed", "PIT-002", 72.5,
                      "pit-002-composed-pitch-v1", "osc1-composed",
                      "analytic sum of note, musical range, tune, oscillator offset, preserved bend contribution, zero calibration, and zero modulation" },
        PitchPolicy { "pit002.osc2.composed", "PIT-002", 51.863137138648348,
                      "pit-002-composed-pitch-v1", "osc2-composed",
                      "analytic sum of note, musical range, tune, oscillator offset, preserved bend contribution, zero calibration, and zero modulation" },
        PitchPolicy { "pit002.osc3.composed", "PIT-002", 87.343587129994475,
                      "pit-002-composed-pitch-v1", "osc3-composed",
                      "analytic sum of note, musical range, tune, oscillator offset, preserved bend contribution, zero calibration, and zero modulation" },
    };
    const auto policy = std::find_if (
        pitchPolicies.begin(), pitchPolicies.end(),
        [&] (const auto& expected) { return gate.id == expected.id; });
    return policy != pitchPolicies.end()
        && gate.status == Status::notRun
        && gate.requirements == std::vector<std::string> { std::string { policy->requirement } }
        && gate.analyzer.id == "audio.pitch.v1" && gate.analyzer.version == 1
        && gate.metric == "midi-semitones" && gate.unit == "semitones"
        && gate.value == policy->value && gate.allowance == 0.01
        && gate.renderMetric.has_value()
        && gate.renderMetric->fixtureId == policy->fixtureId
        && gate.renderMetric->requestId == policy->requestId
        && gate.provenance.at ("derivation") == policy->derivation
        && gate.provenance.at ("reviewStatus") == "approved"
        && gate.provenance.at ("reviewBasis")
               == "2026-07-23 approved PIT-001/PIT-002 design"
        && gate.provenance.at ("reviewDate") == "2026-07-23"
        && gate.provenance.at ("claimScope")
               == "derived equal-tempered software pitch; not a hardware calibration";
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

    constexpr std::array<std::string_view, 13> exactDerivedOrder {
        "par006.gain-control.duration",
        "par006.control.duration",
        "par006.settling.allowance",
        "par006.none.intermediate",
        "pit001.osc2.minus8",
        "pit001.osc2.center",
        "pit001.osc2.plus8",
        "pit001.osc3.minus8",
        "pit001.osc3.center",
        "pit001.osc3.plus8",
        "pit002.osc1.composed",
        "pit002.osc2.composed",
        "pit002.osc3.composed",
    };
    if (manifest.derivedSoftware.size() != exactDerivedOrder.size()
        || ! std::equal (
            manifest.derivedSoftware.begin(), manifest.derivedSoftware.end(),
            exactDerivedOrder.begin(), exactDerivedOrder.end(),
            [] (const auto& gate, const auto id) {
                return gate.id == id && exactDerivedPolicy (gate);
            }))
        return failure<AcceptanceManifest> (
            "acceptance.derived-policy",
            "only the exact ordered thirteen approved derived software policies are permitted");

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
    const AnalyzerRegistry& analyzers,
    const std::span<const GateMetricEvidence> metricEvidence)
{
    std::vector<GateResult> results;
    const auto appendManifestSection = [&] (const std::vector<GateDefinition>& gates) {
        for (const auto& gate : gates) {
            const auto awaiting = gate.status == Status::awaitingApprovedReference;
            results.push_back ({ gate.id,
                                 awaiting ? Status::awaitingApprovedReference : Status::notRun,
                                 awaiting ? "acceptance.reference-awaiting"
                                          : "acceptance.evidence-missing",
                                 gate.requirements, std::nullopt, {}, {} });
        }
    };
    appendManifestSection (manifest.hardSoftware);
    appendManifestSection (manifest.published);
    appendManifestSection (manifest.derivedSoftware);
    appendManifestSection (manifest.measuredHardware);
    appendManifestSection (manifest.performance);

    std::map<std::string, const GateDefinition*> manifestGates;
    const auto indexManifestSection = [&] (const std::vector<GateDefinition>& gates) {
        for (const auto& gate : gates)
            manifestGates.emplace (gate.id, &gate);
    };
    indexManifestSection (manifest.hardSoftware);
    indexManifestSection (manifest.published);
    indexManifestSection (manifest.derivedSoftware);
    indexManifestSection (manifest.measuredHardware);
    indexManifestSection (manifest.performance);
    std::set<std::string> metricEvidenceIds;
    for (const auto& evidenceItem : metricEvidence) {
        const auto definition = manifestGates.find (evidenceItem.gateId);
        if (definition == manifestGates.end()
            || ! metricEvidenceIds.insert (evidenceItem.gateId).second)
            return failure<std::vector<GateResult>> (
                "acceptance.metric-evidence-identity",
                "metric evidence gate IDs must be unique declared manifest gates");
        const auto result = std::find_if (results.begin(), results.end(), [&] (const auto& gate) {
            return gate.id == evidenceItem.gateId;
        });
        const auto& gate = *definition->second;
        const auto& metric = evidenceItem.record.metric;
        const auto registered = analyzers.find (metric.analyzer.id);
        auto validProvenance = false;
        auto validBinding = false;
        auto authoritativeMetric = true;
        auto expectedArtifactPayload = metricEvidenceJson (
            std::span<const MetricEvidenceRecord> { &evidenceItem.record, 1 });
        auto validArtifactPlacement = ! evidenceItem.artifactPath.empty();
        const auto& provenance = evidenceItem.record.provenance;
        if (provenance.kind == MetricSubjectKind::liveRegistry) {
            validBinding = ! gate.renderMetric.has_value();
            const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
            const auto registryFile = resolveBoundedRegularFile (
                sourceRoot, provenance.registryPath);
            const auto& builtIdentity = builtSourceIdentity();
            validProvenance = provenance.registryPath
                                    == "Tests/fixtures/parameters/parameter-registry-v2.json"
                           && provenance.registryCount == ParameterRegistry::descriptors().size()
                           && provenance.registryCount == 48
                           && provenance.reproducibility.sourceCommit == builtIdentity.commit
                           && provenance.reproducibility.sourceTree == builtIdentity.tree
                           && provenance.reproducibility.sourceContent == builtIdentity.content
                           && provenance.reproducibility.sourceDirty == builtIdentity.dirty
                           && provenance.reproducibility.compilerId
                                  == SYNTH_CONFIGURED_COMPILER_ID
                           && provenance.reproducibility.compilerVersion
                                  == SYNTH_CONFIGURED_COMPILER_VERSION
                           && registryFile.ok()
                           && provenance.registrySha256 == sha256File (*registryFile.value);
            std::vector<double> immutableRegistry;
            immutableRegistry.reserve (ParameterRegistry::descriptors().size());
            for (size_t index = 0; index < ParameterRegistry::descriptors().size(); ++index)
                immutableRegistry.push_back (static_cast<double> (index));
            const auto rerun = analyzers.analyze (
                gate.analyzer.id, AnalysisRequest {
                    .metric = gate.metric, .control = immutableRegistry,
                });
            authoritativeMetric = rerun.ok() && rerun.value.has_value()
                && rerun.value->size() == 1
                && metricResultsJson (*rerun.value)
                       == metricResultsJson (
                           std::span<const MetricResult> { &metric, 1 });
        } else {
            validProvenance = false;
            authoritativeMetric = false;
            validBinding = gate.renderMetric.has_value()
                        && gate.renderMetric->fixtureId == provenance.fixtureId
                        && gate.renderMetric->requestId == provenance.requestId;
            const auto sourceRoot = juce::File { SYNTH_SOURCE_ROOT };
            const auto index = loadFixtureIndex (
                sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
            if (index.ok()) {
                const auto indexedFixture = std::find_if (
                    index.value->renderFixtures.begin(), index.value->renderFixtures.end(),
                    [&] (const auto& item) {
                        return item.id == provenance.fixtureId
                            && item.relativePath == provenance.fixturePath
                            && item.sha256 == provenance.fixtureSha256;
                    });
                if (indexedFixture != index.value->renderFixtures.end()) {
                    const auto fixture = loadIndexedRenderFixture (sourceRoot, *indexedFixture);
                    if (fixture.ok()) {
                        const auto request = std::find_if (
                            fixture.value->analysisRequests.begin(),
                            fixture.value->analysisRequests.end(), [&] (const auto& item) {
                                return item.id == provenance.requestId
                                    && item.version == provenance.requestVersion;
                            });
                        if (request != fixture.value->analysisRequests.end()) {
                            const auto renders = renderFixture (*fixture.value);
                            const auto records = renders.ok()
                                ? analyzeFixtureMetrics (*fixture.value, *renders.value)
                                : LoadResult<std::vector<MetricEvidenceRecord>> {};
                            if (records.ok()) {
                                const auto authoritativeRecord = std::find_if (
                                    records.value->begin(), records.value->end(),
                                    [&] (const auto& item) {
                                        return item.provenance.requestId == request->id
                                            && item.provenance.requestVersion == request->version;
                                    });
                                if (authoritativeRecord != records.value->end()) {
                                    authoritativeMetric = metricEvidenceJson (
                                        std::span<const MetricEvidenceRecord> {
                                            &*authoritativeRecord, 1 })
                                        == metricEvidenceJson (
                                            std::span<const MetricEvidenceRecord> {
                                                &evidenceItem.record, 1 });
                                    validProvenance = authoritativeMetric;
                                    expectedArtifactPayload = metricEvidenceJson (*records.value);
                                    const auto expectedPath = "renders/" + fixture.value->id
                                                            + "/metrics.json";
                                    validArtifactPlacement = evidenceItem.artifactPath == expectedPath
                                        && evidenceItem.artifactFile.getFileName() == "metrics.json"
                                        && evidenceItem.artifactFile.getParentDirectory().getFileName()
                                               == juce::String { fixture.value->id }
                                        && evidenceItem.artifactFile.getParentDirectory()
                                               .getParentDirectory().getFileName() == "renders";
                                }
                            }
                        }
                    }
                }
            }
        }
        const auto exactArtifact = evidenceItem.artifactFile.existsAsFile()
            && lowercaseSha256 (evidenceItem.artifactSha256)
            && sha256File (evidenceItem.artifactFile) == evidenceItem.artifactSha256
            && evidenceItem.artifactFile.loadFileAsString() == expectedArtifactPayload;
        const auto validMetric = registered.ok()
            && metric.analyzer.id == gate.analyzer.id
            && metric.analyzer.version == gate.analyzer.version
            && registered.value->version == metric.analyzer.version
            && metric.metric == gate.metric && metric.unit == gate.unit
            && metric.finite && std::isfinite (metric.value)
            && std::isfinite (metric.allowance) && metric.allowance >= 0.0
            && metric.allowance == gate.allowance && authoritativeMetric;
        if (! exactArtifact || ! validArtifactPlacement || ! validBinding
            || ! validProvenance || ! validMetric) {
            result->status = Status::fail;
            result->reasonCode = "acceptance.metric-evidence-invalid";
            continue;
        }
        if (std::abs (metric.value - gate.value) > gate.allowance) {
            result->status = Status::fail;
            result->reasonCode = "acceptance.metric-out-of-bound";
            continue;
        }
        result->status = Status::pass;
        result->reasonCode = "acceptance.metric-pass";
        result->metric = metric;
        result->artifactPath = evidenceItem.artifactPath;
        result->artifactSha256 = evidenceItem.artifactSha256;
    }

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
            { "PAR-006", "TST-006" },
            std::nullopt, {}, {},
        };

        const auto supplied = evidenceByParameter.find (smoothingCase.parameterId);
        if (smoothingCase.smoothingClass == ParameterRegistry::SmoothingClass::none
            && supplied != evidenceByParameter.end()) {
            const auto& item = *supplied->second;
            const auto bound = loadBoundCandidate (item);
            if (! bound.ok()) {
                control.status = Status::fail;
                control.reasonCode = bound.diagnostics.empty()
                                       ? "smoothing.trace-mismatch"
                                       : bound.diagnostics.front().code;
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
                for (const auto& point : bound.value->controlTrace)
                    if (point.key == smoothingCase.key)
                        points.push_back (&point);
                std::sort (points.begin(), points.end(), [] (const auto* left, const auto* right) {
                    return left->sample < right->sample;
                });
                const auto eventPrefix = "automation:"
                                       + std::to_string (smoothingCase.eventSample) + ":";
                const auto parameterToken = ":" + smoothingCase.parameterId + ":";
                const auto hasEvent = std::any_of (
                    bound.value->eventTrace.begin(), bound.value->eventTrace.end(),
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
                        control.artifactPath = bound.value->controlFile
                                                   .getFullPathName().toStdString();
                        control.artifactSha256 = bound.value->controlSha256;
                    }
                }
            }
        }
        results.push_back (std::move (control));
        results.push_back ({ "par006." + smoothingCase.parameterId + ".reference",
                             smoothingCase.referenceStatus,
                             smoothingCase.referenceReasonCode,
                             { "PAR-006", "TST-006" }, std::nullopt, {}, {} });
    }
    return { std::move (results), {} };
}

} // namespace ReferenceHarness
