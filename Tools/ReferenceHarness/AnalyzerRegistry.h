#pragma once

#include "ReferenceTypes.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ReferenceHarness {

// V2 bounds the total normalized-correlation sample pairs: the exact coarse
// lag search plus a worst-case upper bound for original-rate refinement.
inline constexpr std::uint64_t pitchAnalysisPairBudget = 64'000'000;

struct PitchAnalysisWorkPlan {
    bool valid = false;
    std::size_t inputSamples = 0;
    std::size_t analysisSamples = 0;
    std::size_t decimation = 1;
    std::size_t decimatedSamples = 0;
    std::size_t minimumLag = 0;
    std::size_t maximumLag = 0;
    std::uint64_t coarsePairIterations = 0;
    std::uint64_t refinementPairUpperBound = 0;
    std::uint64_t totalPairUpperBound = 0;
};

PitchAnalysisWorkPlan planPitchV2AnalysisWork (
    std::size_t inputSamples,
    double sampleRate) noexcept;

class AnalyzerRegistry {
public:
    static AnalyzerRegistry withFoundationAnalyzers();

    LoadResult<AnalyzerIdentity> find (std::string_view analyzerId) const;
    LoadResult<std::vector<MetricResult>> analyze (
        std::string_view analyzerId, const AnalysisRequest&) const;

private:
    std::vector<AnalyzerIdentity> identities;
};

juce::String metricResultsJson (std::span<const MetricResult> metrics);
juce::String metricEvidenceJson (std::span<const MetricEvidenceRecord> records);

} // namespace ReferenceHarness
