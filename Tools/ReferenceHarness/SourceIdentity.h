#pragma once

#include "ReferenceTypes.h"

#include <string>
#include <string_view>

namespace ReferenceHarness {

struct SourceIdentity {
    std::string commit;
    std::string tree;
    std::string content;
    bool dirty = false;
};

std::string_view buildGitExecutable() noexcept;
const SourceIdentity& builtSourceIdentity() noexcept;
LoadResult<SourceIdentity> inspectSourceIdentity (
    const juce::File& sourceRoot,
    std::string_view gitExecutable);
LoadResult<bool> validateAuthoritativeSourceIdentity (
    const SourceIdentity& built,
    const SourceIdentity& current);
LoadResult<bool> validateCurrentAuthoritativeSourceIdentity();

} // namespace ReferenceHarness
