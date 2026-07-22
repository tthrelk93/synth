#pragma once

#include "AnalyzerRegistry.h"

namespace ReferenceHarness {

LoadResult<AcceptanceManifest> loadAcceptanceManifest (
    const juce::File& sourceRoot,
    const juce::File& manifestFile,
    const AnalyzerRegistry& analyzers);

LoadResult<std::vector<SmoothingCase>> expandSmoothingFixtures (
    const juce::File& sourceRoot,
    const FixtureIndex& index,
    const AcceptanceManifest& manifest);

LoadResult<std::vector<GateResult>> evaluateAcceptance (
    const AcceptanceManifest& manifest,
    std::span<const SmoothingCase> smoothingCases,
    std::span<const SmoothingEvidence> evidence,
    const AnalyzerRegistry& analyzers);

} // namespace ReferenceHarness
