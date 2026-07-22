#pragma once

#include "ReferenceTypes.h"

#include <string_view>

namespace ReferenceHarness {

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
