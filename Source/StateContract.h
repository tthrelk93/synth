#pragma once

#include <JuceHeader.h>

#include <string_view>

namespace StateContract
{
inline constexpr int currentVersion = 2;
inline constexpr int maxExtensionDepth = 8;
inline constexpr int maxExtensionNodes = 128;
inline constexpr int maxExtensionNameBytes = 64;
inline constexpr int maxExtensionValueBytes = 1024;
inline constexpr int maxExtensionTotalValueBytes = 16384;

enum class ContourContract
{
    canonicalContours,
    legacyCrossedContours
};

enum class RestoreCode
{
    success,
    successMigratedV0,
    malformedData,
    wrongRoot,
    missingStateVersion,
    invalidStateVersion,
    nonIntegerStateVersion,
    negativeStateVersion,
    futureStateVersion,
    unsupportedStateVersion,
    missingRequiredProperty,
    missingRequiredChild,
    duplicateRequiredChild,
    duplicateParameterId,
    missingParameterId,
    unknownParameterId,
    invalidNumericValue,
    nonFiniteValue,
    outOfRangeValue,
    invalidDiscreteValue,
    unsafeExtensionStructure,
    unexpectedProperty,
    unexpectedChild,
    invalidCompatibility,
    invalidUi,
    invalidRecreation,
    invalidMigrationLog
};

std::string_view stableCode (RestoreCode) noexcept;

struct RestoreResult
{
    RestoreCode code = RestoreCode::malformedData;

    bool succeeded() const noexcept
    {
        return code == RestoreCode::success || code == RestoreCode::successMigratedV0;
    }

    bool migratedLegacyState() const noexcept
    {
        return code == RestoreCode::successMigratedV0;
    }

    std::string_view codeString() const noexcept { return stableCode (code); }
};

struct PreparedRestore
{
    RestoreResult result;
    juce::ValueTree canonicalState;
    juce::ValueTree apvtsState;
};

juce::ValueTree makeNativeState (const juce::ValueTree& apvtsState);
juce::ValueTree withCurrentParameters (const juce::ValueTree& canonicalState,
                                       const juce::ValueTree& apvtsState);
void serialiseBinary (const juce::ValueTree& canonicalState,
                      juce::MemoryBlock& destination);
PreparedRestore parseAndPrepare (const void* data, int sizeInBytes);
PreparedRestore prepareLegacyContourConversion (const juce::ValueTree& canonicalState);
ContourContract contourContract (const juce::ValueTree& canonicalState) noexcept;
}
