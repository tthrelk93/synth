#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ReferenceHarness {

enum class Status { pass, fail, notRun, awaitingApprovedReference };

struct Diagnostic {
    std::string code;
    std::string message;
};

template <typename T>
struct LoadResult {
    std::optional<T> value;
    std::vector<Diagnostic> diagnostics;
    bool ok() const noexcept { return value.has_value() && diagnostics.empty(); }
};

struct IndexedArtifact {
    std::string id;
    std::string relativePath;
    std::string sha256;
    std::vector<std::string> requirements;
};

struct FixtureIndex {
    int version = 0;
    std::string schema;
    std::vector<IndexedArtifact> frozenArtifacts;
    std::vector<IndexedArtifact> renderFixtures;
    std::vector<IndexedArtifact> smoothingFixtures;
};

} // namespace ReferenceHarness
