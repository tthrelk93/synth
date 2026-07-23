#include "AnalyzerRegistry.h"

#include "ReferenceData.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace ReferenceHarness {
namespace {

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

using Metrics = std::vector<MetricResult>;

MetricResult metric (const AnalyzerIdentity& analyzer,
                     std::string name,
                     std::string unit,
                     const double value,
                     const double allowance,
                     const bool finite,
                     const std::map<std::string, std::string>& settings)
{
    return { analyzer, std::move (name), std::move (unit), value, allowance, finite, settings };
}

bool safeDifference (const double left, const double right, double& result)
{
    const auto oppositeSigns = (left >= 0.0 && right < 0.0)
                            || (left < 0.0 && right >= 0.0);
    if (! oppositeSigns) {
        result = left - right;
        return std::isfinite (result);
    }

    const auto leftMagnitude = std::abs (left);
    const auto rightMagnitude = std::abs (right);
    if (leftMagnitude > std::numeric_limits<double>::max() - rightMagnitude)
        return false;
    const auto magnitude = leftMagnitude + rightMagnitude;
    result = left >= 0.0 ? magnitude : -magnitude;
    return true;
}

bool sanitizeMetrics (Metrics& metrics)
{
    const auto invalid = std::any_of (metrics.begin(), metrics.end(), [] (const auto& result) {
        return ! std::isfinite (result.value) || ! std::isfinite (result.allowance);
    });
    if (! invalid)
        return false;
    for (auto& result : metrics) {
        if (! std::isfinite (result.value))
            result.value = 0.0;
        if (! std::isfinite (result.allowance))
            result.allowance = 0.0;
        result.finite = false;
    }
    return true;
}

LoadResult<Metrics> selectMetric (Metrics metrics,
                                  const std::string_view requested,
                                  std::vector<Diagnostic> diagnostics = {})
{
    if (sanitizeMetrics (metrics) && diagnostics.empty())
        diagnostics.push_back (
            { "analyzer.overflow", "analyzer arithmetic exceeded the finite metric domain" });
    if (! requested.empty()) {
        const auto selected = std::find_if (metrics.begin(), metrics.end(), [&] (const auto& result) {
            return result.metric == requested;
        });
        if (selected == metrics.end())
            return failure<Metrics> ("analyzer.unknown-metric",
                                     "analyzer metric is not registered for this analyzer version");
        MetricResult result = *selected;
        metrics = { std::move (result) };
    }
    return { std::move (metrics), std::move (diagnostics) };
}

LoadResult<Metrics> analyzeSignalStats (const AnalysisRequest& request)
{
    const AnalyzerIdentity identity { "signal.stats.v1", 1 };
    const auto usesAudio = ! request.audio.empty();
    std::vector<double> samples;
    if (usesAudio) {
        samples.reserve (request.audio.size());
        for (const auto sample : request.audio)
            samples.push_back (static_cast<double> (sample));
    } else {
        samples.assign (request.control.begin(), request.control.end());
    }

    const std::map<std::string, std::string> settings {
        { "finite-policy", "reject-non-finite" },
        { "input", usesAudio ? "audio" : "control" },
        { "sample-domain", "contiguous" },
    };
    const auto sampleCount = static_cast<double> (samples.size());
    const auto finiteCount = static_cast<double> (std::count_if (
        samples.begin(), samples.end(), [] (const auto sample) { return std::isfinite (sample); }));
    const auto valid = ! samples.empty() && finiteCount == sampleCount;

    double minimum = 0.0;
    double maximum = 0.0;
    double peak = 0.0;
    double mean = 0.0;
    double rms = 0.0;
    double maximumDifference = 0.0;
    auto arithmeticOverflow = false;
    if (valid) {
        const auto bounds = std::minmax_element (samples.begin(), samples.end());
        minimum = *bounds.first;
        maximum = *bounds.second;
        for (const auto sample : samples)
            peak = std::max (peak, std::abs (sample));
        double normalizedSum = 0.0;
        double normalizedSquares = 0.0;
        for (size_t index = 0; index < samples.size(); ++index) {
            const auto normalized = peak == 0.0 ? 0.0 : samples[index] / peak;
            normalizedSum += normalized;
            normalizedSquares += normalized * normalized;
            if (index != 0) {
                double difference = 0.0;
                if (! safeDifference (samples[index], samples[index - 1], difference)) {
                    arithmeticOverflow = true;
                } else {
                    maximumDifference = std::max (maximumDifference, std::abs (difference));
                }
            }
        }
        const auto normalizedMean = std::clamp (normalizedSum / sampleCount, -1.0, 1.0);
        const auto normalizedRms = std::sqrt (
            std::clamp (normalizedSquares / sampleCount, 0.0, 1.0));
        mean = peak * normalizedMean;
        rms = peak * normalizedRms;
    }

    const auto metricsValid = valid && ! arithmeticOverflow;

    Metrics metrics {
        metric (identity, "sample-count", "count", sampleCount, 0.0, true, settings),
        metric (identity, "finite-count", "count", finiteCount, 0.0, true, settings),
        metric (identity, "minimum", "amplitude", minimum, 0.0, metricsValid, settings),
        metric (identity, "maximum", "amplitude", maximum, 0.0, metricsValid, settings),
        metric (identity, "peak-absolute", "amplitude", peak, 0.0, metricsValid, settings),
        metric (identity, "mean", "amplitude", mean, 0.0, metricsValid, settings),
        metric (identity, "rms", "amplitude", rms, 0.0, metricsValid, settings),
        metric (identity, "maximum-first-difference", "amplitude/sample", maximumDifference,
                0.0, metricsValid, settings),
    };
    if (samples.empty())
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.empty-input", "signal input must not be empty" } });
    if (! valid)
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.non-finite", "signal input must contain only finite samples" } });
    if (arithmeticOverflow) {
        for (auto& result : metrics)
            result.finite = false;
        return selectMetric (
            std::move (metrics), request.metric,
            { { "analyzer.overflow", "signal differences exceed the finite metric domain" } });
    }
    return selectMetric (std::move (metrics), request.metric);
}

LoadResult<Metrics> analyzeControlStep (const AnalysisRequest& request)
{
    const AnalyzerIdentity identity { "control.step.v1", 1 };
    const auto finiteEndpoints = std::isfinite (request.start) && std::isfinite (request.target);
    double travel = 0.0;
    auto finiteTravel = false;
    auto arithmeticOverflow = false;
    if (finiteEndpoints) {
        double signedTravel = 0.0;
        finiteTravel = safeDifference (request.target, request.start, signedTravel);
        if (finiteTravel)
            travel = std::abs (signedTravel);
        else
            arithmeticOverflow = true;
    }
    const auto epsilonAllowance = 8.0 * std::numeric_limits<double>::epsilon()
                                * std::max (1.0, travel);
    const auto idealIncrement = request.durationSamples == 0
                              ? travel
                              : travel / static_cast<double> (request.durationSamples);
    const std::map<std::string, std::string> settings {
        { "allowance", "8*epsilon*max(1,travel)" },
        { "first-change", "first sample at/after event outside start allowance" },
        { "ideal-increment", "travel/duration-or-travel-for-zero-duration" },
        { "ramp-origin", "first ideal increment occurs at event sample" },
        { "settled", "first sample whose suffix remains within target allowance" },
    };

    const auto inputValid = ! request.control.empty()
                         && request.eventSample < request.control.size()
                         && finiteEndpoints
                         && std::all_of (request.control.begin(), request.control.end(), [] (const auto value) {
                                return std::isfinite (value);
                            });
    std::uint64_t firstChange = request.control.size();
    std::uint64_t settled = request.control.size();
    double monotonic = 0.0;
    double overshoot = 0.0;
    double maximumMovement = 0.0;
    if (inputValid && finiteTravel) {
        for (std::uint64_t sample = request.eventSample; sample < request.control.size(); ++sample) {
            double difference = 0.0;
            if (! safeDifference (request.control[static_cast<size_t> (sample)],
                                  request.start, difference)) {
                arithmeticOverflow = true;
                break;
            }
            if (std::abs (difference) > epsilonAllowance) {
                firstChange = sample;
                break;
            }
        }
        if (! arithmeticOverflow) {
            for (std::uint64_t sample = request.eventSample;
                 sample < request.control.size(); ++sample) {
                auto suffixSettled = true;
                for (auto suffix = sample; suffix < request.control.size(); ++suffix) {
                    double difference = 0.0;
                    if (! safeDifference (request.control[static_cast<size_t> (suffix)],
                                          request.target, difference)) {
                        arithmeticOverflow = true;
                        suffixSettled = false;
                        break;
                    }
                    if (std::abs (difference) > epsilonAllowance) {
                        suffixSettled = false;
                        break;
                    }
                }
                if (arithmeticOverflow)
                    break;
                if (suffixSettled) {
                    settled = sample;
                    break;
                }
            }
        }

        const auto direction = request.target >= request.start ? 1.0 : -1.0;
        monotonic = 1.0;
        if (! arithmeticOverflow) {
            const auto firstMovement = std::max<size_t> (
                1, static_cast<size_t> (request.eventSample));
            for (size_t sample = firstMovement; sample < request.control.size(); ++sample) {
                double movement = 0.0;
                if (! safeDifference (request.control[sample], request.control[sample - 1],
                                      movement)) {
                    arithmeticOverflow = true;
                    break;
                }
                maximumMovement = std::max (maximumMovement, std::abs (movement));
                if (sample >= request.eventSample && movement * direction < -epsilonAllowance)
                    monotonic = 0.0;
            }
        }
        if (! arithmeticOverflow) {
            for (size_t sample = static_cast<size_t> (request.eventSample);
                 sample < request.control.size(); ++sample) {
                double difference = 0.0;
                if (! safeDifference (request.control[sample], request.target, difference)) {
                    arithmeticOverflow = true;
                    break;
                }
                overshoot = std::max (overshoot, difference * direction);
            }
            overshoot = std::max (0.0, overshoot);
        }
    }

    const auto valid = inputValid && finiteTravel && ! arithmeticOverflow;

    Metrics metrics {
        metric (identity, "first-change-sample", "samples", static_cast<double> (firstChange),
                1.0, valid, settings),
        metric (identity, "settled-sample", "samples", static_cast<double> (settled),
                1.0, valid, settings),
        metric (identity, "monotonic", "boolean", monotonic, epsilonAllowance, valid, settings),
        metric (identity, "overshoot", "normalized", overshoot, epsilonAllowance, valid, settings),
        metric (identity, "maximum-per-sample-movement", "normalized/sample", maximumMovement,
                idealIncrement + epsilonAllowance, valid, settings),
        metric (identity, "floating-allowance", "normalized", epsilonAllowance,
                epsilonAllowance, valid, settings),
    };
    if (request.control.empty())
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.empty-input", "control input must not be empty" } });
    if (! inputValid)
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.non-finite", "control request and samples must be finite and in range" } });
    if (arithmeticOverflow) {
        for (auto& result : metrics)
            result.finite = false;
        return selectMetric (
            std::move (metrics), request.metric,
            { { "analyzer.overflow", "control differences exceed the finite metric domain" } });
    }
    return selectMetric (std::move (metrics), request.metric);
}

double rmsOf (const std::span<const float> audio, const size_t begin, const size_t end)
{
    if (begin >= end)
        return 0.0;
    double scale = 0.0;
    for (size_t index = begin; index < end; ++index)
        scale = std::max (scale, std::abs (static_cast<double> (audio[index])));
    if (scale == 0.0)
        return 0.0;
    double normalizedSquares = 0.0;
    for (size_t index = begin; index < end; ++index) {
        const auto normalized = static_cast<double> (audio[index]) / scale;
        normalizedSquares += normalized * normalized;
    }
    const auto normalizedRms = std::sqrt (std::clamp (
        normalizedSquares / static_cast<double> (end - begin), 0.0, 1.0));
    return scale * normalizedRms;
}

LoadResult<Metrics> analyzeAudioClick (const AnalysisRequest& request)
{
    const AnalyzerIdentity identity { "audio.click.v1", 1 };
    const std::map<std::string, std::string> settings {
        { "event-window", "inclusive transition indices event..event+duration" },
        { "finite-policy", "reject-non-finite" },
        { "steady-state", "mean of pre/post RMS" },
    };
    const auto finiteCount = static_cast<double> (std::count_if (
        request.audio.begin(), request.audio.end(), [] (const auto sample) {
            return std::isfinite (sample);
        }));
    const auto valid = ! request.audio.empty()
                    && request.eventSample < request.audio.size()
                    && finiteCount == static_cast<double> (request.audio.size());
    double maximumDifference = 0.0;
    double preRms = 0.0;
    double postRms = 0.0;
    double peakOverSteady = 0.0;
    if (valid) {
        const auto first = std::max<std::uint64_t> (1, request.eventSample);
        const auto lastLimit = static_cast<std::uint64_t> (request.audio.size() - 1);
        const auto last = request.eventSample
                        + std::min (request.durationSamples,
                                    lastLimit - request.eventSample);
        for (auto sample = first; sample <= last; ++sample)
            maximumDifference = std::max (
                maximumDifference,
                std::abs (static_cast<double> (request.audio[static_cast<size_t> (sample)])
                          - static_cast<double> (request.audio[static_cast<size_t> (sample - 1)])));
        preRms = rmsOf (request.audio, 0, static_cast<size_t> (request.eventSample));
        const auto postLimit = static_cast<std::uint64_t> (request.audio.size());
        const auto postStart = request.eventSample
                             + std::min (request.durationSamples,
                                         postLimit - request.eventSample);
        postRms = rmsOf (request.audio, static_cast<size_t> (postStart), request.audio.size());
        const auto steady = preRms * 0.5 + postRms * 0.5;
        if (maximumDifference > 0.0 && steady > 0.0)
            peakOverSteady = 20.0 * std::log10 (maximumDifference / steady);
    }

    Metrics metrics {
        metric (identity, "maximum-first-difference", "amplitude/sample", maximumDifference,
                0.0, valid, settings),
        metric (identity, "pre-rms", "amplitude", preRms, 0.0, valid, settings),
        metric (identity, "post-rms", "amplitude", postRms, 0.0, valid, settings),
        metric (identity, "peak-over-steady-state", "dB", peakOverSteady, 0.0, valid, settings),
        metric (identity, "finite-count", "count", finiteCount, 0.0, true, settings),
    };
    if (request.audio.empty())
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.empty-input", "audio input must not be empty" } });
    if (! valid)
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.non-finite", "audio samples must be finite and event window valid" } });
    return selectMetric (std::move (metrics), request.metric);
}

} // namespace

AnalyzerRegistry AnalyzerRegistry::withFoundationAnalyzers()
{
    AnalyzerRegistry registry;
    registry.identities = {
        { "signal.stats.v1", 1 },
        { "control.step.v1", 1 },
        { "audio.click.v1", 1 },
    };
    return registry;
}

LoadResult<AnalyzerIdentity> AnalyzerRegistry::find (const std::string_view analyzerId) const
{
    const auto found = std::find_if (identities.begin(), identities.end(), [&] (const auto& identity) {
        return identity.id == analyzerId;
    });
    if (found == identities.end())
        return failure<AnalyzerIdentity> (
            "analyzer.unknown", "analyzer identifier is not registered at this exact version");
    return { *found, {} };
}

LoadResult<std::vector<MetricResult>> AnalyzerRegistry::analyze (
    const std::string_view analyzerId, const AnalysisRequest& request) const
{
    const auto identity = find (analyzerId);
    if (! identity.ok())
        return { std::nullopt, identity.diagnostics };
    if (analyzerId == "signal.stats.v1")
        return analyzeSignalStats (request);
    if (analyzerId == "control.step.v1")
        return analyzeControlStep (request);
    return analyzeAudioClick (request);
}

juce::String metricResultsJson (const std::span<const MetricResult> metrics)
{
    juce::Array<juce::var> array;
    for (const auto& result : metrics) {
        const auto numericFinite = std::isfinite (result.value)
                                && std::isfinite (result.allowance);
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("allowance", numericFinite ? result.allowance : 0.0);
        object->setProperty ("analyzer", juce::String { result.analyzer.id });
        object->setProperty ("analyzerVersion", result.analyzer.version);
        object->setProperty ("finite", result.finite && numericFinite);
        object->setProperty ("metric", juce::String { result.metric });
        auto settings = std::make_unique<juce::DynamicObject>();
        for (const auto& [name, value] : result.settings)
            settings->setProperty (juce::Identifier { juce::String { name } },
                                   juce::String { value });
        object->setProperty ("settings", juce::var { settings.release() });
        object->setProperty ("unit", juce::String { result.unit });
        object->setProperty ("value", numericFinite ? result.value : 0.0);
        array.add (juce::var { object.release() });
    }
    return canonicalJson (juce::var { array });
}

juce::String metricEvidenceJson (const std::span<const MetricEvidenceRecord> records)
{
    const auto stringMap = [] (const std::map<std::string, std::string>& values) {
        auto object = std::make_unique<juce::DynamicObject>();
        for (const auto& [name, value] : values)
            object->setProperty (juce::Identifier { name }, juce::String { value });
        return juce::var { object.release() };
    };
    const auto blockPatterns = [] (const std::vector<std::vector<int>>& patterns) {
        juce::Array<juce::var> outer;
        for (const auto& pattern : patterns) {
            juce::Array<juce::var> inner;
            for (const auto size : pattern)
                inner.add (size);
            outer.add (inner);
        }
        return juce::var { outer };
    };
    juce::Array<juce::var> values;
    for (const auto& record : records) {
        const auto& metric = record.metric;
        auto analyzer = std::make_unique<juce::DynamicObject>();
        analyzer->setProperty ("id", juce::String { metric.analyzer.id });
        analyzer->setProperty ("version", metric.analyzer.version);
        auto metricObject = std::make_unique<juce::DynamicObject>();
        metricObject->setProperty ("allowance", metric.allowance);
        metricObject->setProperty ("analyzer", juce::var { analyzer.release() });
        metricObject->setProperty ("finite", metric.finite);
        metricObject->setProperty ("metric", juce::String { metric.metric });
        metricObject->setProperty ("settings", stringMap (metric.settings));
        metricObject->setProperty ("unit", juce::String { metric.unit });
        metricObject->setProperty ("value", metric.value);

        const auto& source = record.provenance;
        auto provenance = std::make_unique<juce::DynamicObject>();
        provenance->setProperty ("kind", source.kind == MetricSubjectKind::render
                                               ? "render" : "live-registry");
        if (source.kind == MetricSubjectKind::render) {
            provenance->setProperty ("architecture", juce::String { source.reproducibility.architecture });
            provenance->setProperty ("blockPatterns", blockPatterns (source.blockPatterns));
            provenance->setProperty ("buildType", juce::String { source.reproducibility.buildType });
            provenance->setProperty ("compilerId", juce::String { source.reproducibility.compilerId });
            provenance->setProperty ("compilerVersion", juce::String {
                source.reproducibility.compilerVersion });
            provenance->setProperty ("fixtureId", juce::String { source.fixtureId });
            provenance->setProperty ("fixturePath", juce::String { source.fixturePath });
            provenance->setProperty ("fixturePurpose", juce::String { source.fixturePurpose });
            provenance->setProperty ("fixtureReviewStatus", juce::String {
                fixtureReviewStatusName (source.fixtureReviewStatus).data() });
            provenance->setProperty ("fixtureSha256", juce::String { source.fixtureSha256 });
            provenance->setProperty ("inputHashes", stringMap (source.reproducibility.inputHashes));
            provenance->setProperty ("juceCommit", juce::String { source.reproducibility.juceCommit });
            provenance->setProperty ("outputHashes", stringMap (source.reproducibility.outputHashes));
            provenance->setProperty ("platform", juce::String { source.reproducibility.platform });
            provenance->setProperty ("requestId", juce::String { source.requestId });
            provenance->setProperty ("requestVersion", source.requestVersion);
            provenance->setProperty ("sampleRate", source.sampleRate);
            provenance->setProperty ("seed", static_cast<juce::int64> (source.seed));
            provenance->setProperty ("sourceCommit", juce::String { source.reproducibility.sourceCommit });
            provenance->setProperty ("sourceContent", juce::String {
                source.reproducibility.sourceContent });
            provenance->setProperty ("sourceDirty", source.reproducibility.sourceDirty);
            provenance->setProperty ("sourceTree", juce::String {
                source.reproducibility.sourceTree });
            provenance->setProperty ("totalSamples", static_cast<juce::int64> (source.totalSamples));
        } else {
            provenance->setProperty ("compilerId", juce::String { source.reproducibility.compilerId });
            provenance->setProperty ("compilerVersion", juce::String {
                source.reproducibility.compilerVersion });
            provenance->setProperty ("registryCount", static_cast<juce::int64> (source.registryCount));
            provenance->setProperty ("registryPath", juce::String { source.registryPath });
            provenance->setProperty ("registrySha256", juce::String { source.registrySha256 });
            provenance->setProperty ("sourceCommit", juce::String { source.reproducibility.sourceCommit });
            provenance->setProperty ("sourceContent", juce::String {
                source.reproducibility.sourceContent });
            provenance->setProperty ("sourceDirty", source.reproducibility.sourceDirty);
            provenance->setProperty ("sourceTree", juce::String {
                source.reproducibility.sourceTree });
        }
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("metric", juce::var { metricObject.release() });
        object->setProperty ("provenance", juce::var { provenance.release() });
        values.add (juce::var { object.release() });
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("records", values);
    root->setProperty ("schema", "model-d.metrics.v1");
    return canonicalJson (juce::var { root.release() });
}

} // namespace ReferenceHarness
