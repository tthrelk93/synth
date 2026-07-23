#include "SourceIdentity.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cctype>
#include <utility>

namespace ReferenceHarness {
namespace {

template <typename T>
LoadResult<T> failure (std::string code, std::string message)
{
    return { std::nullopt, { { std::move (code), std::move (message) } } };
}

bool isLowercaseHexDigest (const std::string_view value, const std::size_t expectedSize)
{
    return value.size() == expectedSize
        && std::all_of (value.begin(), value.end(), [] (const unsigned char character) {
               return std::isdigit (character) != 0
                   || (character >= 'a' && character <= 'f');
           });
}

bool isObjectId (const std::string_view value)
{
    return isLowercaseHexDigest (value, 40)
        || isLowercaseHexDigest (value, 64);
}

bool isSha256 (const std::string_view value)
{
    return isLowercaseHexDigest (value, 64);
}

LoadResult<std::string> runGit (const juce::File& sourceRoot,
                                const std::string_view gitExecutable,
                                std::initializer_list<const char*> arguments)
{
    juce::StringArray command;
    command.add (juce::String::fromUTF8 (gitExecutable.data(),
                                         static_cast<int> (gitExecutable.size())));
    command.add ("-C");
    command.add (sourceRoot.getFullPathName());
    for (const auto* argument : arguments)
        command.add (argument);

    juce::ChildProcess process;
    if (! process.start (command, juce::ChildProcess::wantStdOut
                                  | juce::ChildProcess::wantStdErr))
        return failure<std::string> (
            "source.identity-inspection", "Git source identity process could not start");
    const auto output = process.readAllProcessOutput();
    if (process.getExitCode() != 0)
        return failure<std::string> (
            "source.identity-inspection", "Git source identity command failed");
    return { output.toStdString(), {} };
}

std::string trimmed (const std::string& value)
{
    const auto first = std::find_if_not (value.begin(), value.end(), [] (const unsigned char character) {
        return std::isspace (character) != 0;
    });
    const auto last = std::find_if_not (value.rbegin(), value.rend(), [] (const unsigned char character) {
        return std::isspace (character) != 0;
    }).base();
    return first < last ? std::string { first, last } : std::string {};
}

} // namespace

std::string_view buildGitExecutable() noexcept
{
    return SYNTH_GIT_EXECUTABLE;
}

const SourceIdentity& builtSourceIdentity() noexcept
{
    static const SourceIdentity identity {
        SYNTH_SOURCE_COMMIT,
        SYNTH_SOURCE_TREE,
        SYNTH_SOURCE_CONTENT,
        SYNTH_SOURCE_DIRTY != 0,
    };
    return identity;
}

LoadResult<SourceIdentity> inspectSourceIdentity (
    const juce::File& sourceRoot,
    const std::string_view gitExecutable)
{
    if (! sourceRoot.isDirectory() || gitExecutable.empty())
        return failure<SourceIdentity> (
            "source.identity-inspection", "source root and Git executable are required");

    const auto commitOutput = runGit (sourceRoot, gitExecutable,
                                      { "rev-parse", "--verify", "HEAD" });
    if (! commitOutput.ok())
        return { std::nullopt, commitOutput.diagnostics };
    const auto treeOutput = runGit (sourceRoot, gitExecutable,
                                    { "rev-parse", "--verify", "HEAD^{tree}" });
    if (! treeOutput.ok())
        return { std::nullopt, treeOutput.diagnostics };
    const auto diffOutput = runGit (sourceRoot, gitExecutable,
                                    { "diff", "--no-ext-diff", "--binary", "HEAD", "--" });
    if (! diffOutput.ok())
        return { std::nullopt, diffOutput.diagnostics };
    const auto statusOutput = runGit (
        sourceRoot, gitExecutable,
        { "status", "--porcelain=v1", "--untracked-files=normal", "--ignore-submodules=none" });
    if (! statusOutput.ok())
        return { std::nullopt, statusOutput.diagnostics };

    SourceIdentity identity;
    identity.commit = trimmed (*commitOutput.value);
    identity.tree = trimmed (*treeOutput.value);
    if (! isObjectId (identity.commit) || ! isObjectId (identity.tree))
        return failure<SourceIdentity> (
            "source.identity-inspection", "Git source identity object IDs are malformed");
    const auto contentBytes = identity.tree + "\n" + *diffOutput.value;
    identity.content = juce::SHA256 {
        contentBytes.data(), contentBytes.size()
    }.toHexString().toStdString();
    identity.dirty = ! trimmed (*statusOutput.value).empty();
    return { std::move (identity), {} };
}

LoadResult<bool> validateAuthoritativeSourceIdentity (
    const SourceIdentity& built,
    const SourceIdentity& current)
{
    if (! isObjectId (built.commit) || ! isObjectId (built.tree)
        || ! isSha256 (built.content))
        return failure<bool> (
            "source.identity-built", "embedded build source identity is malformed");
    if (built.dirty)
        return failure<bool> (
            "source.identity-built-dirty", "authoritative evidence requires a clean build identity");
    if (! isObjectId (current.commit) || ! isObjectId (current.tree)
        || ! isSha256 (current.content))
        return failure<bool> (
            "source.identity-current", "current source identity is malformed");
    if (current.dirty)
        return failure<bool> (
            "source.identity-current-dirty", "authoritative evidence requires a clean current tree");
    if (built.commit != current.commit || built.tree != current.tree
        || built.content != current.content)
        return failure<bool> (
            "source.identity-stale", "binary build identity does not match the current source");
    return { true, {} };
}

LoadResult<bool> validateCurrentAuthoritativeSourceIdentity()
{
    const auto current = inspectSourceIdentity (
        juce::File { SYNTH_SOURCE_ROOT }, buildGitExecutable());
    if (! current.ok())
        return { std::nullopt, current.diagnostics };
    return validateAuthoritativeSourceIdentity (builtSourceIdentity(), *current.value);
}

} // namespace ReferenceHarness
