#pragma once

#include "Acceptance.h"

#include <span>
#include <string>
#include <vector>

namespace ReferenceHarness {

struct RequirementDefinition {
    std::string id;
    std::string owner;
    std::vector<std::string> verification;
    std::vector<std::string> gateIds;
    std::vector<std::string> artifactPaths;
    Status status = Status::notRun;
    std::vector<std::string> artifactSha256;
};

struct RequirementResult {
    RequirementDefinition definition;
    Status status = Status::notRun;
    std::vector<std::string> reasons;
    std::vector<std::string> artifactHashes;
};

struct RequirementReport {
    int manifestVersion = 1;
    std::vector<RequirementResult> requirements;
    bool releaseReady = false;
    int fixtureVersion = 1;
    int analyzerVersion = 1;
    std::string sourceCommit;
    std::string buildType;
    std::string platform;
    std::string architecture;
    std::vector<GateResult> gateResults;
};

LoadResult<std::vector<GateResult>> buildF0GateResults (
    const juce::File& sourceRoot,
    const FixtureIndex& index,
    const AcceptanceManifest& acceptance);

LoadResult<std::vector<RequirementDefinition>> loadRequirementMap (
    const juce::File& sourceRoot,
    const juce::File& mapFile,
    const juce::File& matrixFile,
    const AcceptanceManifest& acceptance);

LoadResult<bool> validateRequirementSet (
    std::span<const RequirementDefinition> requirements,
    const juce::File& matrixFile);

LoadResult<RequirementReport> buildRequirementReport (
    const juce::File& sourceRoot,
    const juce::File& candidateRoot,
    std::span<const RequirementDefinition> requirements,
    std::span<const GateResult> gateResults,
    const AcceptanceManifest& acceptance);

LoadResult<juce::File> writeRequirementReport (
    const RequirementReport& report,
    const juce::File& candidateDirectory);

LoadResult<bool> verifyReleaseReady (const juce::File& reportFile);

} // namespace ReferenceHarness
