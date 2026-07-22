#pragma once

#include "ReferenceTypes.h"

#include <span>
#include <string>

namespace ReferenceHarness {

LoadResult<std::vector<RenderResult>> renderFixture (const RenderFixture& fixture);
LoadResult<std::vector<juce::File>> writeCandidateArtifacts (
    const RenderFixture& fixture,
    std::span<const RenderResult> results,
    const juce::File& newDirectory);
LoadResult<GateMetricEvidence> writeRegistryMetricEvidence (
    const juce::File& sourceRoot,
    const juce::File& metricsFile,
    std::string artifactPath);

int runOfflineRendererCommand (std::span<const std::string> arguments);

} // namespace ReferenceHarness
