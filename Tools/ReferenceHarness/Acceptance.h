#pragma once

#include "AnalyzerRegistry.h"

namespace ReferenceHarness {

LoadResult<AcceptanceManifest> loadAcceptanceManifest (
    const juce::File& sourceRoot,
    const juce::File& manifestFile,
    const AnalyzerRegistry& analyzers);

LoadResult<std::vector<SmoothingCase>> expandSmoothingFixtures (
    const juce::File& sourceRoot,
    const FixtureIndex& index);

LoadResult<std::vector<GateResult>> evaluateAcceptance (
    const AcceptanceManifest& manifest,
    std::span<const SmoothingCase> smoothingCases);

} // namespace ReferenceHarness
