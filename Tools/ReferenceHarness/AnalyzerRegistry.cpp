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

LoadResult<Metrics> selectMetric (Metrics metrics,
                                  const std::string_view requested,
                                  std::vector<Diagnostic> diagnostics = {})
{
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
    if (valid) {
        const auto bounds = std::minmax_element (samples.begin(), samples.end());
        minimum = *bounds.first;
        maximum = *bounds.second;
        for (size_t index = 0; index < samples.size(); ++index) {
            peak = std::max (peak, std::abs (samples[index]));
            mean += samples[index];
            rms += samples[index] * samples[index];
            if (index != 0)
                maximumDifference = std::max (
                    maximumDifference, std::abs (samples[index] - samples[index - 1]));
        }
        mean /= sampleCount;
        rms = std::sqrt (rms / sampleCount);
    }

    Metrics metrics {
        metric (identity, "sample-count", "count", sampleCount, 0.0, true, settings),
        metric (identity, "finite-count", "count", finiteCount, 0.0, true, settings),
        metric (identity, "minimum", "amplitude", minimum, 0.0, valid, settings),
        metric (identity, "maximum", "amplitude", maximum, 0.0, valid, settings),
        metric (identity, "peak-absolute", "amplitude", peak, 0.0, valid, settings),
        metric (identity, "mean", "amplitude", mean, 0.0, valid, settings),
        metric (identity, "rms", "amplitude", rms, 0.0, valid, settings),
        metric (identity, "maximum-first-difference", "amplitude/sample", maximumDifference,
                0.0, valid, settings),
    };
    if (samples.empty())
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.empty-input", "signal input must not be empty" } });
    if (! valid)
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.non-finite", "signal input must contain only finite samples" } });
    return selectMetric (std::move (metrics), request.metric);
}

LoadResult<Metrics> analyzeControlStep (const AnalysisRequest& request)
{
    const AnalyzerIdentity identity { "control.step.v1", 1 };
    const auto travel = std::abs (request.target - request.start);
    const auto epsilonAllowance = 8.0 * std::numeric_limits<double>::epsilon()
                                * std::max (1.0, travel);
    const auto idealIncrement = request.durationSamples == 0
                              ? travel
                              : travel / static_cast<double> (request.durationSamples);
    const std::map<std::string, std::string> settings {
        { "allowance", "8*epsilon*max(1,travel)" },
        { "first-change", "first sample at/after event outside start allowance" },
        { "ideal-increment", "travel/duration-or-travel-for-zero-duration" },
        { "settled", "first sample whose suffix remains within target allowance" },
    };

    const auto valid = ! request.control.empty()
                    && request.eventSample < request.control.size()
                    && std::isfinite (request.start) && std::isfinite (request.target)
                    && std::all_of (request.control.begin(), request.control.end(), [] (const auto value) {
                           return std::isfinite (value);
                       });
    std::uint64_t firstChange = request.control.size();
    std::uint64_t settled = request.control.size();
    double monotonic = 0.0;
    double overshoot = 0.0;
    double maximumMovement = 0.0;
    if (valid) {
        for (std::uint64_t sample = request.eventSample; sample < request.control.size(); ++sample) {
            if (std::abs (request.control[static_cast<size_t> (sample)] - request.start)
                > epsilonAllowance) {
                firstChange = sample;
                break;
            }
        }
        for (std::uint64_t sample = request.eventSample; sample < request.control.size(); ++sample) {
            const auto suffixSettled = std::all_of (
                request.control.begin() + static_cast<std::ptrdiff_t> (sample),
                request.control.end(), [&] (const auto value) {
                    return std::abs (value - request.target) <= epsilonAllowance;
                });
            if (suffixSettled) {
                settled = sample;
                break;
            }
        }

        const auto direction = request.target >= request.start ? 1.0 : -1.0;
        monotonic = 1.0;
        for (size_t sample = 1; sample < request.control.size(); ++sample) {
            const auto movement = request.control[sample] - request.control[sample - 1];
            maximumMovement = std::max (maximumMovement, std::abs (movement));
            if (sample >= request.eventSample && movement * direction < -epsilonAllowance)
                monotonic = 0.0;
        }
        for (size_t sample = static_cast<size_t> (request.eventSample);
             sample < request.control.size(); ++sample)
            overshoot = std::max (overshoot,
                                  (request.control[sample] - request.target) * direction);
        overshoot = std::max (0.0, overshoot);
    }

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
    if (! valid)
        return selectMetric (std::move (metrics), request.metric,
                             { { "analyzer.non-finite", "control request and samples must be finite and in range" } });
    return selectMetric (std::move (metrics), request.metric);
}

double rmsOf (const std::span<const float> audio, const size_t begin, const size_t end)
{
    if (begin >= end)
        return 0.0;
    double sumSquares = 0.0;
    for (size_t index = begin; index < end; ++index)
        sumSquares += static_cast<double> (audio[index]) * static_cast<double> (audio[index]);
    return std::sqrt (sumSquares / static_cast<double> (end - begin));
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
        const auto last = std::min<std::uint64_t> (
            request.audio.size() - 1, request.eventSample + request.durationSamples);
        for (auto sample = first; sample <= last; ++sample)
            maximumDifference = std::max (
                maximumDifference,
                std::abs (static_cast<double> (request.audio[static_cast<size_t> (sample)])
                          - static_cast<double> (request.audio[static_cast<size_t> (sample - 1)])));
        preRms = rmsOf (request.audio, 0, static_cast<size_t> (request.eventSample));
        const auto postStart = std::min<std::uint64_t> (
            request.audio.size(), request.eventSample + request.durationSamples);
        postRms = rmsOf (request.audio, static_cast<size_t> (postStart), request.audio.size());
        const auto steady = (preRms + postRms) * 0.5;
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
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("allowance", result.allowance);
        object->setProperty ("analyzer", juce::String { result.analyzer.id });
        object->setProperty ("analyzerVersion", result.analyzer.version);
        object->setProperty ("finite", result.finite);
        object->setProperty ("metric", juce::String { result.metric });
        auto settings = std::make_unique<juce::DynamicObject>();
        for (const auto& [name, value] : result.settings)
            settings->setProperty (juce::Identifier { juce::String { name } },
                                   juce::String { value });
        object->setProperty ("settings", juce::var { settings.release() });
        object->setProperty ("unit", juce::String { result.unit });
        object->setProperty ("value", result.value);
        array.add (juce::var { object.release() });
    }
    return canonicalJson (juce::var { array });
}

} // namespace ReferenceHarness
