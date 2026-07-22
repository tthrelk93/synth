#include "StateContract.h"

#include "ParameterRegistry.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace StateContract
{
namespace
{
using namespace std::literals;

constexpr std::array reservedNames {
    "modelDState"sv, "parameters"sv, "PARAM"sv, "compatibility"sv,
    "ui"sv, "recreation"sv, "extensions"sv, "migrationLog"sv, "ENTRY"sv
};

struct ParameterRecord
{
    std::string id;
    float value = 0.0f;
    bool known = false;
    juce::String sourceValueToken;
};

struct ExtensionBudget
{
    int nodes = 0;
    int totalValueBytes = 0;
};

juce::String string (std::string_view text)
{
    return juce::String { text.data(), text.size() };
}

bool hasExactTagName (const juce::XmlElement& element, std::string_view expected)
{
    return std::string_view { element.getTagName().toRawUTF8() } == expected;
}

PreparedRestore failure (RestoreCode code)
{
    return { RestoreResult { code }, {}, {} };
}

bool isSafeAsciiName (const juce::String& value)
{
    const auto bytes = std::string_view { value.toRawUTF8() };
    if (bytes.empty() || bytes.size() > static_cast<std::size_t> (maxExtensionNameBytes))
        return false;

    const auto first = static_cast<unsigned char> (bytes.front());
    if (! ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')
           || first == '_'))
        return false;

    return std::all_of (bytes.begin() + 1, bytes.end(), [] (char character)
    {
        const auto byte = static_cast<unsigned char> (character);
        return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
            || (byte >= '0' && byte <= '9') || byte == '_' || byte == '-'
            || byte == '.';
    });
}

bool isSafeAsciiToken (const juce::String& value, int maximumBytes = 128)
{
    const auto bytes = std::string_view { value.toRawUTF8() };
    return ! bytes.empty() && bytes.size() <= static_cast<std::size_t> (maximumBytes)
        && std::all_of (bytes.begin(), bytes.end(), [] (char character)
        {
            const auto byte = static_cast<unsigned char> (character);
            return byte >= 0x21 && byte <= 0x7e;
        });
}

bool containsReservedName (const juce::String& name)
{
    return std::any_of (reservedNames.begin(), reservedNames.end(), [&name] (auto reserved)
    {
        return std::string_view { name.toRawUTF8() } == reserved;
    });
}

template <std::size_t Size>
bool hasOnlyAttributes (const juce::XmlElement& element,
                        const std::array<std::string_view, Size>& allowed)
{
    for (int index = 0; index < element.getNumAttributes(); ++index)
    {
        const auto name = element.getAttributeName (index);
        if (std::none_of (allowed.begin(), allowed.end(), [&name] (auto candidate)
            {
                return std::string_view { name.toRawUTF8() } == candidate;
            }))
            return false;
    }
    return true;
}

bool hasElementChildren (const juce::XmlElement& element)
{
    for (auto* child = element.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
        if (! child->isTextElement())
            return true;
    return false;
}

bool hasNonWhitespaceText (const juce::XmlElement& element)
{
    for (auto* child = element.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
        if (child->isTextElement() && child->getText().trim().isNotEmpty())
            return true;
    return false;
}

const ParameterRegistry::Descriptor* findDescriptor (std::string_view id)
{
    const auto descriptors = ParameterRegistry::descriptors();
    const auto found = std::find_if (descriptors.begin(), descriptors.end(), [id] (const auto& descriptor)
    {
        return descriptor.id == id;
    });
    return found == descriptors.end() ? nullptr : &*found;
}

enum class NumberStatus
{
    success,
    invalid,
    nonFinite
};

NumberStatus parseFloatStrict (const juce::String& text, float& result)
{
    const auto bytes = std::string_view { text.toRawUTF8() };
    if (bytes.empty())
        return NumberStatus::invalid;
    auto special = text.toLowerCase();
    if (special.startsWithChar ('+') || special.startsWithChar ('-'))
        special = special.substring (1);
    if (special == "nan" || special == "inf" || special == "infinity")
        return NumberStatus::nonFinite;

    std::size_t index = 0;
    if (bytes[index] == '+' || bytes[index] == '-')
        ++index;
    const auto integerStart = index;
    while (index < bytes.size() && bytes[index] >= '0' && bytes[index] <= '9')
        ++index;
    const auto hasIntegerDigits = index != integerStart;
    bool hasFractionDigits = false;
    if (index < bytes.size() && bytes[index] == '.')
    {
        const auto fractionStart = ++index;
        while (index < bytes.size() && bytes[index] >= '0' && bytes[index] <= '9')
            ++index;
        hasFractionDigits = index != fractionStart;
    }
    if (! hasIntegerDigits && ! hasFractionDigits)
        return NumberStatus::invalid;
    if (index < bytes.size() && (bytes[index] == 'e' || bytes[index] == 'E'))
    {
        ++index;
        if (index < bytes.size() && (bytes[index] == '+' || bytes[index] == '-'))
            ++index;
        const auto exponentStart = index;
        while (index < bytes.size() && bytes[index] >= '0' && bytes[index] <= '9')
            ++index;
        if (index == exponentStart)
            return NumberStatus::invalid;
    }
    if (index != bytes.size())
        return NumberStatus::invalid;

    auto cursor = text.getCharPointer();
    const auto parsed = juce::CharacterFunctions::readDoubleValue (cursor);
    if (*cursor != 0)
        return NumberStatus::invalid;
    if (! std::isfinite (parsed)
        || std::abs (parsed) > static_cast<double> (std::numeric_limits<float>::max()))
        return NumberStatus::nonFinite;
    result = parsed == 0.0 ? 0.0f : static_cast<float> (parsed);
    return std::isfinite (result) ? NumberStatus::success : NumberStatus::nonFinite;
}

enum class IntegerStatus
{
    success,
    invalid,
    nonInteger,
    negative,
    overflow
};

IntegerStatus parseNonNegativeIntegerStrict (const juce::String& text, int& result)
{
    const auto bytes = std::string_view { text.toRawUTF8() };
    if (bytes.empty())
        return IntegerStatus::invalid;

    const auto digitStart = bytes.front() == '-' ? std::size_t { 1 } : std::size_t { 0 };
    const auto isIntegerToken = digitStart < bytes.size()
        && std::all_of (bytes.begin() + static_cast<std::ptrdiff_t> (digitStart), bytes.end(),
                        [] (char character) { return character >= '0' && character <= '9'; });
    if (isIntegerToken && digitStart == 1)
        return IntegerStatus::negative;

    long long parsedInteger = 0;
    const auto integerResult = std::from_chars (bytes.data(), bytes.data() + bytes.size(), parsedInteger);
    if (integerResult.ec == std::errc {} && integerResult.ptr == bytes.data() + bytes.size())
    {
        if (parsedInteger < 0)
            return IntegerStatus::negative;
        if (parsedInteger > std::numeric_limits<int>::max())
            return IntegerStatus::overflow;
        result = static_cast<int> (parsedInteger);
        return IntegerStatus::success;
    }
    if (isIntegerToken && integerResult.ec == std::errc::result_out_of_range)
        return IntegerStatus::overflow;

    float parsedNumber = 0.0f;
    if (parseFloatStrict (text, parsedNumber) == NumberStatus::success)
        return IntegerStatus::nonInteger;
    return IntegerStatus::invalid;
}

RestoreCode validatePhysicalValue (const ParameterRegistry::Descriptor& descriptor,
                                   const juce::String& text,
                                   float& value)
{
    const auto numberStatus = parseFloatStrict (text, value);
    if (numberStatus == NumberStatus::nonFinite)
        return RestoreCode::nonFiniteValue;
    if (numberStatus != NumberStatus::success)
        return RestoreCode::invalidNumericValue;
    if (value < descriptor.rangeStart || value > descriptor.rangeEnd)
        return RestoreCode::outOfRangeValue;
    if ((descriptor.kind == ParameterRegistry::Kind::choice
         || descriptor.kind == ParameterRegistry::Kind::boolean)
        && std::trunc (value) != value)
        return RestoreCode::invalidDiscreteValue;
    if (descriptor.kind == ParameterRegistry::Kind::boolean && value != 0.0f && value != 1.0f)
        return RestoreCode::invalidDiscreteValue;
    return RestoreCode::success;
}

juce::ValueTree makeParameterTree (const std::map<std::string, float>& values)
{
    juce::ValueTree parameters { "parameters" };
    for (const auto& descriptor : ParameterRegistry::descriptors())
    {
        const auto found = values.find (std::string { descriptor.id });
        const auto value = found == values.end() ? descriptor.physicalDefault : found->second;
        juce::ValueTree parameter { "PARAM" };
        parameter.setProperty ("id", string (descriptor.id), nullptr);
        parameter.setProperty ("value", value, nullptr);
        parameters.addChild (parameter, -1, nullptr);
    }
    return parameters;
}

std::map<std::string, float> valuesFromApvts (const juce::ValueTree& apvtsState)
{
    std::map<std::string, float> values;
    for (int index = 0; index < apvtsState.getNumChildren(); ++index)
    {
        const auto child = apvtsState.getChild (index);
        if (child.hasType ("PARAM"))
            values.emplace (child.getProperty ("id").toString().toStdString(),
                            static_cast<float> (child.getProperty ("value")));
    }
    return values;
}

juce::ValueTree makeApvtsTree (const std::map<std::string, float>& values)
{
    auto parameters = makeParameterTree (values);
    juce::ValueTree result { "Parameters" };
    for (int index = 0; index < parameters.getNumChildren(); ++index)
        result.addChild (parameters.getChild (index).createCopy(), -1, nullptr);
    return result;
}

juce::ValueTree makeCompatibility (ContourContract contour,
                                   int sourceVersion,
                                   int migratedAtVersion,
                                   const juce::String& sourceHash)
{
    juce::ValueTree compatibility { "compatibility" };
    compatibility.setProperty ("contourContract",
                               contour == ContourContract::canonicalContours
                                   ? "canonicalContours" : "legacyCrossedContours",
                               nullptr);
    compatibility.setProperty ("sourceVersion", sourceVersion, nullptr);
    compatibility.setProperty ("migratedAtVersion", migratedAtVersion, nullptr);
    compatibility.setProperty ("sourceHash", sourceHash, nullptr);
    return compatibility;
}

juce::ValueTree makeUi (const juce::String& viewMode)
{
    juce::ValueTree ui { "ui" };
    ui.setProperty ("viewMode", viewMode, nullptr);
    return ui;
}

juce::ValueTree makeRecreation (bool enabled, const juce::String& linkedPresetId)
{
    juce::ValueTree recreation { "recreation" };
    recreation.setProperty ("enabled", enabled ? 1 : 0, nullptr);
    recreation.setProperty ("linkedPresetId", linkedPresetId, nullptr);
    return recreation;
}

juce::ValueTree makeEntry (const juce::String& action, const juce::String& warningCode)
{
    juce::ValueTree entry { "ENTRY" };
    entry.setProperty ("from", 0, nullptr);
    entry.setProperty ("to", currentVersion, nullptr);
    entry.setProperty ("action", action, nullptr);
    entry.setProperty ("warningCode", warningCode, nullptr);
    return entry;
}

juce::ValueTree makeEntry (int from, int to, const juce::String& action,
                           const juce::String& warningCode)
{
    auto entry = makeEntry (action, warningCode);
    entry.setProperty ("from", from, nullptr);
    entry.setProperty ("to", to, nullptr);
    return entry;
}

juce::ValueTree assembleState (const std::map<std::string, float>& values,
                               const juce::ValueTree& compatibility,
                               const juce::ValueTree& ui,
                               const juce::ValueTree& recreation,
                               const juce::ValueTree& extensions,
                               const juce::ValueTree& migrationLog,
                               const juce::String& engineVersion,
                               const juce::String& calibrationProfileId)
{
    juce::ValueTree root { "modelDState" };
    root.setProperty ("stateVersion", currentVersion, nullptr);
    root.setProperty ("engineVersion", engineVersion, nullptr);
    root.setProperty ("calibrationProfileId", calibrationProfileId, nullptr);
    root.addChild (makeParameterTree (values), -1, nullptr);
    root.addChild (compatibility.createCopy(), -1, nullptr);
    root.addChild (ui.createCopy(), -1, nullptr);
    root.addChild (recreation.createCopy(), -1, nullptr);
    root.addChild (extensions.createCopy(), -1, nullptr);
    root.addChild (migrationLog.createCopy(), -1, nullptr);
    return root;
}

std::string floatBitsHex (float value)
{
    const auto bits = std::bit_cast<std::uint32_t> (value);
    std::array<char, 8> padded {};
    std::array<char, 8> digits {};
    const auto converted = std::to_chars (digits.data(), digits.data() + digits.size(), bits, 16);
    const auto count = static_cast<std::size_t> (converted.ptr - digits.data());
    std::fill (padded.begin(), padded.begin() + static_cast<std::ptrdiff_t> (8 - count), '0');
    std::copy (digits.begin(), digits.begin() + static_cast<std::ptrdiff_t> (count),
               padded.begin() + static_cast<std::ptrdiff_t> (8 - count));
    return { padded.data(), padded.size() };
}

juce::String hashLegacyRecords (std::vector<ParameterRecord> records,
                                bool hasPresetName,
                                const juce::String& presetName)
{
    std::sort (records.begin(), records.end(), [] (const auto& left, const auto& right)
    {
        return left.id < right.id;
    });

    std::string canonical { "model-d-v0-parameters\n" };
    const auto presetNameBytes = std::string_view { presetName.toRawUTF8() };
    canonical += hasPresetName ? "presetName-present:" : "presetName-absent:";
    canonical += std::to_string (presetNameBytes.size()) + ":";
    canonical.append (presetNameBytes);
    canonical += '\n';
    for (const auto& record : records)
    {
        canonical += record.known ? "known:" : "unknown:";
        canonical += std::to_string (record.id.size());
        canonical += ':';
        canonical += record.id;
        canonical += ':';
        canonical += floatBitsHex (record.value);
        canonical += '\n';
    }
    return juce::SHA256 { canonical.data(), canonical.size() }.toHexString();
}

bool isLowercaseSha256 (const juce::String& value)
{
    const auto bytes = std::string_view { value.toRawUTF8() };
    return bytes.size() == 64 && std::all_of (bytes.begin(), bytes.end(), [] (char character)
    {
        return (character >= '0' && character <= '9')
            || (character >= 'a' && character <= 'f');
    });
}

bool validateExtensionElement (const juce::XmlElement& element,
                               int depth,
                               ExtensionBudget& budget)
{
    if (depth > maxExtensionDepth || ++budget.nodes > maxExtensionNodes
        || ! isSafeAsciiName (element.getTagName()) || containsReservedName (element.getTagName())
        || hasNonWhitespaceText (element))
        return false;

    for (int index = 0; index < element.getNumAttributes(); ++index)
    {
        const auto name = element.getAttributeName (index);
        const auto value = element.getAttributeValue (index);
        const auto valueBytes = value.getNumBytesAsUTF8();
        budget.totalValueBytes += valueBytes;
        if (! isSafeAsciiName (name) || valueBytes > maxExtensionValueBytes
            || budget.totalValueBytes > maxExtensionTotalValueBytes)
            return false;
    }

    for (auto* child = element.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->isTextElement())
        {
            if (child->getText().trim().isNotEmpty())
                return false;
            continue;
        }
        if (! validateExtensionElement (*child, depth + 1, budget))
            return false;
    }
    return true;
}

RestoreCode validateExtensions (const juce::XmlElement& extensions)
{
    if (! hasOnlyAttributes (extensions, std::array<std::string_view, 0> {}))
        return RestoreCode::unexpectedProperty;
    if (hasNonWhitespaceText (extensions))
        return RestoreCode::unsafeExtensionStructure;

    ExtensionBudget budget;
    for (auto* child = extensions.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->isTextElement())
            continue;
        if (! validateExtensionElement (*child, 1, budget))
            return RestoreCode::unsafeExtensionStructure;
    }
    return RestoreCode::success;
}

float legacyDefault (const ParameterRegistry::Descriptor& descriptor)
{
    return descriptor.id == "tune" ? 0.0f : descriptor.physicalDefault;
}

PreparedRestore parseLegacy (const juce::XmlElement& root)
{
    constexpr std::array legacyRootAttributes { "presetName"sv };
    if (! hasOnlyAttributes (root, legacyRootAttributes) || hasNonWhitespaceText (root))
        return failure (RestoreCode::unexpectedProperty);
    const auto presetName = root.getStringAttribute ("presetName");
    if (presetName.getNumBytesAsUTF8() > maxExtensionValueBytes)
        return failure (RestoreCode::unsafeExtensionStructure);

    std::map<std::string, float> values;
    std::vector<ParameterRecord> records;
    std::vector<ParameterRecord> unknownRecords;
    std::set<std::string> observedIds;

    for (auto* child = root.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->isTextElement())
            continue;
        if (! hasExactTagName (*child, "PARAM"))
            return failure (RestoreCode::unexpectedChild);
        constexpr std::array attributes { "id"sv, "value"sv };
        if (! hasOnlyAttributes (*child, attributes))
            return failure (RestoreCode::unexpectedProperty);
        if (! child->hasAttribute ("id") || ! child->hasAttribute ("value"))
            return failure (RestoreCode::missingRequiredProperty);
        if (hasElementChildren (*child) || hasNonWhitespaceText (*child))
            return failure (RestoreCode::unsafeExtensionStructure);

        const auto id = child->getStringAttribute ("id");
        const auto idString = id.toStdString();
        if (! observedIds.emplace (idString).second)
            return failure (RestoreCode::duplicateParameterId);

        float value = 0.0f;
        const auto* descriptor = findDescriptor (idString);
        if (descriptor != nullptr)
        {
            const auto validation = validatePhysicalValue (*descriptor,
                                                           child->getStringAttribute ("value"), value);
            if (validation != RestoreCode::success)
                return failure (validation);
            values.emplace (idString, value);
            records.push_back ({ idString, value, true, {} });
        }
        else
        {
            const auto sourceValueToken = child->getStringAttribute ("value");
            if (sourceValueToken.getNumBytesAsUTF8() > maxExtensionValueBytes)
                return failure (RestoreCode::unsafeExtensionStructure);
            const auto numberStatus = parseFloatStrict (sourceValueToken, value);
            if (numberStatus == NumberStatus::nonFinite)
                return failure (RestoreCode::nonFiniteValue);
            if (numberStatus != NumberStatus::success || ! isSafeAsciiName (id))
                return failure (RestoreCode::unsafeExtensionStructure);
            records.push_back ({ idString, value, false, sourceValueToken });
            unknownRecords.push_back ({ idString, value, false, sourceValueToken });
        }
    }

    juce::ValueTree migrationLog { "migrationLog" };
    for (const auto& descriptor : ParameterRegistry::descriptors())
    {
        const auto id = std::string { descriptor.id };
        if (values.contains (id))
            continue;
        const auto value = descriptor.versionHint == 0 ? legacyDefault (descriptor)
                                                       : descriptor.physicalDefault;
        values.emplace (id, value);
        migrationLog.addChild (
            makeEntry ("defaultParameter", "defaulted.parameter." + string (descriptor.id)),
            -1, nullptr);
    }

    migrationLog.addChild (makeEntry ("defaultMetadata", "defaulted.ui.viewMode"), -1, nullptr);
    migrationLog.addChild (makeEntry ("defaultMetadata", "defaulted.recreation.enabled"), -1, nullptr);
    migrationLog.addChild (makeEntry ("defaultMetadata", "defaulted.recreation.linkedPresetId"), -1, nullptr);
    migrationLog.addChild (makeEntry ("defaultMetadata", "defaulted.calibrationProfileId"), -1, nullptr);
    migrationLog.addChild (makeEntry ("preserveLegacyCrossedContours",
                                      "legacy.contourRoutingPreserved"), -1, nullptr);

    juce::ValueTree extensions { "extensions" };
    if (root.hasAttribute ("presetName"))
    {
        juce::ValueTree legacyPreset { "legacyPreset" };
        legacyPreset.setProperty ("presetName", presetName, nullptr);
        extensions.addChild (legacyPreset, -1, nullptr);
    }
    if (! unknownRecords.empty())
    {
        std::sort (unknownRecords.begin(), unknownRecords.end(), [] (const auto& left, const auto& right)
        {
            return left.id < right.id;
        });
        juce::ValueTree legacyParameters { "legacyParameters" };
        for (const auto& record : unknownRecords)
        {
            juce::ValueTree parameter { "legacyParameter" };
            parameter.setProperty ("id", juce::String { record.id }, nullptr);
            parameter.setProperty ("value", record.sourceValueToken, nullptr);
            legacyParameters.addChild (parameter, -1, nullptr);
        }
        extensions.addChild (legacyParameters, -1, nullptr);
    }

    const auto extensionsXml = extensions.createXml();
    if (extensionsXml == nullptr
        || validateExtensions (*extensionsXml) != RestoreCode::success)
        return failure (RestoreCode::unsafeExtensionStructure);

    const auto canonical = assembleState (
        values,
        makeCompatibility (ContourContract::legacyCrossedContours, 0, currentVersion,
                           hashLegacyRecords (records, root.hasAttribute ("presetName"), presetName)),
        makeUi ("authentic"), makeRecreation (false, {}), extensions, migrationLog,
        JucePlugin_VersionString, "baseline");
    return { RestoreResult { RestoreCode::successMigratedV0 }, canonical,
             makeApvtsTree (values) };
}

RestoreCode classifyVersion (const juce::XmlElement& root, int& version)
{
    if (! root.hasAttribute ("stateVersion"))
        return RestoreCode::missingStateVersion;
    const auto status = parseNonNegativeIntegerStrict (
        root.getStringAttribute ("stateVersion"), version);
    if (status == IntegerStatus::negative)
        return RestoreCode::negativeStateVersion;
    if (status == IntegerStatus::nonInteger)
        return RestoreCode::nonIntegerStateVersion;
    if (status == IntegerStatus::overflow)
        return RestoreCode::futureStateVersion;
    if (status != IntegerStatus::success)
        return RestoreCode::invalidStateVersion;
    if (version > currentVersion)
        return RestoreCode::futureStateVersion;
    if (version != currentVersion)
        return RestoreCode::unsupportedStateVersion;
    return RestoreCode::success;
}

PreparedRestore parseV2 (const juce::XmlElement& root)
{
    int version = 0;
    const auto versionResult = classifyVersion (root, version);
    if (versionResult != RestoreCode::success)
        return failure (versionResult);

    constexpr std::array rootAttributes {
        "stateVersion"sv, "engineVersion"sv, "calibrationProfileId"sv
    };
    if (! hasOnlyAttributes (root, rootAttributes))
        return failure (RestoreCode::unexpectedProperty);
    if (! root.hasAttribute ("engineVersion") || ! root.hasAttribute ("calibrationProfileId"))
        return failure (RestoreCode::missingRequiredProperty);
    const auto engineVersion = root.getStringAttribute ("engineVersion");
    const auto calibration = root.getStringAttribute ("calibrationProfileId");
    if (engineVersion.isEmpty() || engineVersion.getNumBytesAsUTF8() > 128
        || calibration != "baseline")
        return failure (RestoreCode::invalidCompatibility);
    if (hasNonWhitespaceText (root))
        return failure (RestoreCode::unexpectedChild);

    constexpr std::array childNames {
        "parameters"sv, "compatibility"sv, "ui"sv,
        "recreation"sv, "extensions"sv, "migrationLog"sv
    };
    std::array<const juce::XmlElement*, childNames.size()> children {};
    for (auto* child = root.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->isTextElement())
            continue;
        const auto found = std::find_if (childNames.begin(), childNames.end(), [&child] (auto name)
        {
            return std::string_view { child->getTagName().toRawUTF8() } == name;
        });
        if (found == childNames.end())
            return failure (RestoreCode::unexpectedChild);
        const auto index = static_cast<std::size_t> (found - childNames.begin());
        if (children[index] != nullptr)
            return failure (RestoreCode::duplicateRequiredChild);
        children[index] = child;
    }
    if (std::any_of (children.begin(), children.end(), [] (const auto* child) { return child == nullptr; }))
        return failure (RestoreCode::missingRequiredChild);

    const auto& parametersXml = *children[0];
    if (parametersXml.getNumAttributes() != 0 || hasNonWhitespaceText (parametersXml))
        return failure (RestoreCode::unexpectedProperty);
    std::map<std::string, float> values;
    for (auto* parameter = parametersXml.getFirstChildElement(); parameter != nullptr;
         parameter = parameter->getNextElement())
    {
        if (parameter->isTextElement())
            continue;
        if (! hasExactTagName (*parameter, "PARAM"))
            return failure (RestoreCode::unexpectedChild);
        constexpr std::array attributes { "id"sv, "value"sv };
        if (! hasOnlyAttributes (*parameter, attributes))
            return failure (RestoreCode::unexpectedProperty);
        if (! parameter->hasAttribute ("id") || ! parameter->hasAttribute ("value"))
            return failure (RestoreCode::missingRequiredProperty);
        if (hasElementChildren (*parameter) || hasNonWhitespaceText (*parameter))
            return failure (RestoreCode::unexpectedChild);

        const auto id = parameter->getStringAttribute ("id").toStdString();
        const auto* descriptor = findDescriptor (id);
        if (descriptor == nullptr)
            return failure (RestoreCode::unknownParameterId);
        if (values.contains (id))
            return failure (RestoreCode::duplicateParameterId);
        float value = 0.0f;
        const auto validation = validatePhysicalValue (
            *descriptor, parameter->getStringAttribute ("value"), value);
        if (validation != RestoreCode::success)
            return failure (validation);
        values.emplace (id, value);
    }
    for (const auto& descriptor : ParameterRegistry::descriptors())
        if (! values.contains (std::string { descriptor.id }))
            return failure (RestoreCode::missingParameterId);

    const auto& compatibilityXml = *children[1];
    constexpr std::array compatibilityAttributes {
        "contourContract"sv, "sourceVersion"sv, "migratedAtVersion"sv, "sourceHash"sv
    };
    if (! hasOnlyAttributes (compatibilityXml, compatibilityAttributes))
        return failure (RestoreCode::unexpectedProperty);
    for (const auto attribute : compatibilityAttributes)
        if (! compatibilityXml.hasAttribute (string (attribute)))
            return failure (RestoreCode::missingRequiredProperty);
    if (hasElementChildren (compatibilityXml) || hasNonWhitespaceText (compatibilityXml))
        return failure (RestoreCode::unexpectedChild);

    const auto contourText = compatibilityXml.getStringAttribute ("contourContract");
    const auto contour = contourText == "canonicalContours"
                           ? ContourContract::canonicalContours
                           : ContourContract::legacyCrossedContours;
    if (contourText != "canonicalContours" && contourText != "legacyCrossedContours")
        return failure (RestoreCode::invalidCompatibility);
    int sourceVersion = 0;
    int migratedAtVersion = 0;
    if (parseNonNegativeIntegerStrict (compatibilityXml.getStringAttribute ("sourceVersion"),
                                       sourceVersion) != IntegerStatus::success
        || parseNonNegativeIntegerStrict (
               compatibilityXml.getStringAttribute ("migratedAtVersion"),
               migratedAtVersion) != IntegerStatus::success)
        return failure (RestoreCode::invalidCompatibility);
    const auto sourceHash = compatibilityXml.getStringAttribute ("sourceHash");
    const auto nativeTuple = contour == ContourContract::canonicalContours
                          && sourceVersion == currentVersion && migratedAtVersion == 0
                          && sourceHash.isEmpty();
    const auto migratedTuple = contour == ContourContract::legacyCrossedContours
                            && sourceVersion == 0 && migratedAtVersion == currentVersion
                            && isLowercaseSha256 (sourceHash);
    const auto convertedTuple = contour == ContourContract::canonicalContours
                             && sourceVersion == 0 && migratedAtVersion == currentVersion
                             && isLowercaseSha256 (sourceHash);
    if (! nativeTuple && ! migratedTuple && ! convertedTuple)
        return failure (RestoreCode::invalidCompatibility);

    const auto& uiXml = *children[2];
    constexpr std::array uiAttributes { "viewMode"sv };
    if (! hasOnlyAttributes (uiXml, uiAttributes))
        return failure (RestoreCode::unexpectedProperty);
    const auto viewMode = uiXml.getStringAttribute ("viewMode");
    if (! uiXml.hasAttribute ("viewMode")
        || (viewMode != "authentic" && viewMode != "signalFlow")
        || hasElementChildren (uiXml) || hasNonWhitespaceText (uiXml))
        return failure (RestoreCode::invalidUi);

    const auto& recreationXml = *children[3];
    constexpr std::array recreationAttributes { "enabled"sv, "linkedPresetId"sv };
    if (! hasOnlyAttributes (recreationXml, recreationAttributes))
        return failure (RestoreCode::unexpectedProperty);
    if (! recreationXml.hasAttribute ("enabled")
        || ! recreationXml.hasAttribute ("linkedPresetId"))
        return failure (RestoreCode::missingRequiredProperty);
    int recreationEnabled = 0;
    if (parseNonNegativeIntegerStrict (recreationXml.getStringAttribute ("enabled"),
                                       recreationEnabled) != IntegerStatus::success
        || (recreationEnabled != 0 && recreationEnabled != 1)
        || recreationXml.getStringAttribute ("linkedPresetId").getNumBytesAsUTF8()
               > maxExtensionValueBytes
        || hasElementChildren (recreationXml) || hasNonWhitespaceText (recreationXml))
        return failure (RestoreCode::invalidRecreation);

    const auto extensionValidation = validateExtensions (*children[4]);
    if (extensionValidation != RestoreCode::success)
        return failure (extensionValidation);
    const auto extensions = juce::ValueTree::fromXml (*children[4]);
    if (! extensions.isValid())
        return failure (RestoreCode::unsafeExtensionStructure);

    const auto& migrationLogXml = *children[5];
    if (migrationLogXml.getNumAttributes() != 0 || hasNonWhitespaceText (migrationLogXml))
        return failure (RestoreCode::invalidMigrationLog);
    juce::ValueTree migrationLog { "migrationLog" };
    for (auto* entry = migrationLogXml.getFirstChildElement(); entry != nullptr;
         entry = entry->getNextElement())
    {
        if (entry->isTextElement())
            continue;
        constexpr std::array entryAttributes { "from"sv, "to"sv, "action"sv, "warningCode"sv };
        if (! hasExactTagName (*entry, "ENTRY") || ! hasOnlyAttributes (*entry, entryAttributes)
            || hasElementChildren (*entry) || hasNonWhitespaceText (*entry))
            return failure (RestoreCode::invalidMigrationLog);
        for (const auto attribute : entryAttributes)
            if (! entry->hasAttribute (string (attribute)))
                return failure (RestoreCode::missingRequiredProperty);
        int from = 0;
        int to = 0;
        const auto action = entry->getStringAttribute ("action");
        const auto warningCode = entry->getStringAttribute ("warningCode");
        if (parseNonNegativeIntegerStrict (entry->getStringAttribute ("from"), from)
                != IntegerStatus::success
            || parseNonNegativeIntegerStrict (entry->getStringAttribute ("to"), to)
                != IntegerStatus::success
            || from > currentVersion || to > currentVersion || from > to
            || ! isSafeAsciiToken (action) || ! isSafeAsciiToken (warningCode))
            return failure (RestoreCode::invalidMigrationLog);
        juce::ValueTree canonicalEntry { "ENTRY" };
        canonicalEntry.setProperty ("from", from, nullptr);
        canonicalEntry.setProperty ("to", to, nullptr);
        canonicalEntry.setProperty ("action", action, nullptr);
        canonicalEntry.setProperty ("warningCode", warningCode, nullptr);
        migrationLog.addChild (canonicalEntry, -1, nullptr);
    }
    int conversionEntries = 0;
    int conversionHistoryEntries = 0;
    for (int index = 0; index < migrationLog.getNumChildren(); ++index)
    {
        const auto entry = migrationLog.getChild (index);
        conversionHistoryEntries += entry.getProperty ("action").toString()
                                      == "convertLegacyContours" ? 1 : 0;
        const auto isConversion = static_cast<int> (entry.getProperty ("from")) == currentVersion
                               && static_cast<int> (entry.getProperty ("to")) == currentVersion
                               && entry.getProperty ("action").toString() == "convertLegacyContours"
                               && entry.getProperty ("warningCode").toString()
                                      == "legacy.hostAutomationNotRewritten";
        conversionEntries += isConversion ? 1 : 0;
    }
    const auto finalEntryIsConversion = migrationLog.getNumChildren() > 0
        && static_cast<int> (migrationLog.getChild (
               migrationLog.getNumChildren() - 1).getProperty ("from")) == currentVersion
        && static_cast<int> (migrationLog.getChild (
               migrationLog.getNumChildren() - 1).getProperty ("to")) == currentVersion
        && migrationLog.getChild (migrationLog.getNumChildren() - 1)
               .getProperty ("action").toString() == "convertLegacyContours"
        && migrationLog.getChild (migrationLog.getNumChildren() - 1)
               .getProperty ("warningCode").toString()
               == "legacy.hostAutomationNotRewritten";
    if ((nativeTuple && migrationLog.getNumChildren() != 0)
        || (migratedTuple && conversionHistoryEntries != 0)
        || (convertedTuple && (conversionEntries != 1 || conversionHistoryEntries != 1
                               || ! finalEntryIsConversion)))
        return failure (RestoreCode::invalidMigrationLog);

    const auto canonical = assembleState (
        values, makeCompatibility (contour, sourceVersion, migratedAtVersion, sourceHash),
        makeUi (viewMode),
        makeRecreation (recreationEnabled == 1,
                        recreationXml.getStringAttribute ("linkedPresetId")),
        extensions, migrationLog, engineVersion, calibration);
    return { RestoreResult { RestoreCode::success }, canonical, makeApvtsTree (values) };
}
}

std::string_view stableCode (RestoreCode code) noexcept
{
    switch (code)
    {
        case RestoreCode::success: return "success";
        case RestoreCode::successMigratedV0: return "success_migrated_v0";
        case RestoreCode::malformedData: return "malformed_data";
        case RestoreCode::wrongRoot: return "wrong_root";
        case RestoreCode::missingStateVersion: return "missing_state_version";
        case RestoreCode::invalidStateVersion: return "invalid_state_version";
        case RestoreCode::nonIntegerStateVersion: return "non_integer_state_version";
        case RestoreCode::negativeStateVersion: return "negative_state_version";
        case RestoreCode::futureStateVersion: return "future_state_version";
        case RestoreCode::unsupportedStateVersion: return "unsupported_state_version";
        case RestoreCode::missingRequiredProperty: return "missing_required_property";
        case RestoreCode::missingRequiredChild: return "missing_required_child";
        case RestoreCode::duplicateRequiredChild: return "duplicate_required_child";
        case RestoreCode::duplicateParameterId: return "duplicate_parameter_id";
        case RestoreCode::missingParameterId: return "missing_parameter_id";
        case RestoreCode::unknownParameterId: return "unknown_parameter_id";
        case RestoreCode::invalidNumericValue: return "invalid_numeric_value";
        case RestoreCode::nonFiniteValue: return "non_finite_value";
        case RestoreCode::outOfRangeValue: return "out_of_range_value";
        case RestoreCode::invalidDiscreteValue: return "invalid_discrete_value";
        case RestoreCode::unsafeExtensionStructure: return "unsafe_extension_structure";
        case RestoreCode::unexpectedProperty: return "unexpected_property";
        case RestoreCode::unexpectedChild: return "unexpected_child";
        case RestoreCode::invalidCompatibility: return "invalid_compatibility";
        case RestoreCode::invalidUi: return "invalid_ui";
        case RestoreCode::invalidRecreation: return "invalid_recreation";
        case RestoreCode::invalidMigrationLog: return "invalid_migration_log";
    }
    return "invalid_restore_code";
}

juce::ValueTree makeNativeState (const juce::ValueTree& apvtsState)
{
    const auto values = valuesFromApvts (apvtsState);
    return assembleState (values,
                          makeCompatibility (ContourContract::canonicalContours,
                                             currentVersion, 0, {}),
                          makeUi ("authentic"), makeRecreation (false, {}),
                          juce::ValueTree { "extensions" },
                          juce::ValueTree { "migrationLog" },
                          JucePlugin_VersionString, "baseline");
}

juce::ValueTree withCurrentParameters (const juce::ValueTree& canonicalState,
                                       const juce::ValueTree& apvtsState)
{
    auto result = canonicalState.createCopy();
    const auto parameters = result.getChildWithName ("parameters");
    if (parameters.isValid())
        result.removeChild (parameters, nullptr);
    result.addChild (makeParameterTree (valuesFromApvts (apvtsState)), 0, nullptr);
    return result;
}

void serialiseBinary (const juce::ValueTree& canonicalState,
                      juce::MemoryBlock& destination)
{
    const auto xml = canonicalState.createXml();
    juce::AudioProcessor::copyXmlToBinary (*xml, destination);
}

PreparedRestore parseAndPrepare (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return failure (RestoreCode::malformedData);
    const auto xml = std::unique_ptr<juce::XmlElement> (
        juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return failure (RestoreCode::malformedData);
    if (hasExactTagName (*xml, "Parameters"))
        return parseLegacy (*xml);
    if (! hasExactTagName (*xml, "modelDState"))
        return failure (RestoreCode::wrongRoot);
    return parseV2 (*xml);
}

PreparedRestore prepareLegacyContourConversion (const juce::ValueTree& canonicalState)
{
    if (! canonicalState.hasType ("modelDState")
        || contourContract (canonicalState) != ContourContract::legacyCrossedContours)
        return failure (RestoreCode::invalidCompatibility);
    auto converted = canonicalState.createCopy();
    auto parameters = converted.getChildWithName ("parameters");
    if (! parameters.isValid())
        return failure (RestoreCode::missingRequiredChild);
    const auto find = [&] (std::string_view id)
    {
        for (int child = 0; child < parameters.getNumChildren(); ++child)
        {
            auto parameter = parameters.getChild (child);
            if (std::string_view { parameter.getProperty ("id").toString().toRawUTF8() } == id)
                return parameter;
        }
        return juce::ValueTree {};
    };
    constexpr std::array pairs {
        std::pair { "filterAttackTimeKnob"sv, "loudnessAttackTimeKnob"sv },
        std::pair { "filterDecayTimeKnob"sv, "loudnessDecayTimeKnob"sv },
        std::pair { "filterSustainKnob"sv, "loudnessSustainLevelKnob"sv }
    };
    for (const auto& [filterId, loudnessId] : pairs)
    {
        auto filter = find (filterId);
        auto loudness = find (loudnessId);
        if (! filter.isValid() || ! loudness.isValid())
            return failure (RestoreCode::missingParameterId);
        const auto filterValue = filter.getProperty ("value");
        filter.setProperty ("value", loudness.getProperty ("value"), nullptr);
        loudness.setProperty ("value", filterValue, nullptr);
    }
    converted.getChildWithName ("compatibility").setProperty (
        "contourContract", "canonicalContours", nullptr);
    converted.getChildWithName ("migrationLog").addChild (
        makeEntry (currentVersion, currentVersion, "convertLegacyContours",
                   "legacy.hostAutomationNotRewritten"), -1, nullptr);
    const auto values = [&]
    {
        std::map<std::string, float> result;
        for (int child = 0; child < parameters.getNumChildren(); ++child)
        {
            const auto parameter = parameters.getChild (child);
            result.emplace (parameter.getProperty ("id").toString().toStdString(),
                            static_cast<float> (parameter.getProperty ("value")));
        }
        return result;
    }();
    return { RestoreResult { RestoreCode::success }, converted, makeApvtsTree (values) };
}

ContourContract contourContract (const juce::ValueTree& canonicalState) noexcept
{
    const auto compatibility = canonicalState.getChildWithName ("compatibility");
    return compatibility.getProperty ("contourContract").toString()
               == "legacyCrossedContours"
             ? ContourContract::legacyCrossedContours
             : ContourContract::canonicalContours;
}
}
