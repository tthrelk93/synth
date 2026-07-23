#pragma once

#include "ReferenceTypes.h"

#include <juce_core/juce_core.h>

#include <string>
#include <string_view>

namespace ReferenceHarness {

std::string statusName (Status);
std::string sha256File (const juce::File&);
bool isPortableIdentifierComponent (std::string_view value) noexcept;
LoadResult<juce::File> resolveBoundedRegularFile (
    const juce::File& root, std::string_view relativePath);
juce::String canonicalJson (const juce::var&);
juce::var analysisRequestsJson (std::span<const FixtureAnalysisRequest> requests);
LoadResult<FixtureIndex> loadFixtureIndex (
    const juce::File& sourceRoot, const juce::File& indexFile);
LoadResult<RenderFixture> loadRenderFixture (
    const juce::File& sourceRoot, const juce::File& fixtureFile);
LoadResult<RenderFixture> loadIndexedRenderFixture (
    const juce::File& sourceRoot, const IndexedArtifact& indexedFixture);
LoadResult<SmoothingFixture> loadSmoothingFixture (const juce::File& fixtureFile);

} // namespace ReferenceHarness
