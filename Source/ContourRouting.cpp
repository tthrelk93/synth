#include "ContourRouting.h"

namespace ContourRouting
{
namespace
{
Parameter canonicalParameter (SemanticContour contour, Stage stage) noexcept
{
    if (contour == SemanticContour::filter)
    {
        if (stage == Stage::attack) return Parameter::filterAttack;
        if (stage == Stage::decay) return Parameter::filterDecay;
        return Parameter::filterSustain;
    }
    if (stage == Stage::attack) return Parameter::loudnessAttack;
    if (stage == Stage::decay) return Parameter::loudnessDecay;
    return Parameter::loudnessSustain;
}
}

Parameter parameterFor (StateContract::ContourContract contract,
                        SemanticContour semanticContour,
                        Stage stage) noexcept
{
    if (contract == StateContract::ContourContract::legacyCrossedContours)
        semanticContour = semanticContour == SemanticContour::filter
                            ? SemanticContour::loudness : SemanticContour::filter;
    return canonicalParameter (semanticContour, stage);
}

RoutedContours route (StateContract::ContourContract contract,
                      const ContourControls& controls) noexcept
{
    const auto sustain = [&] (float value) noexcept
    {
        return controls.decayEnabled ? value : 1.0f;
    };
    const ContourSettings filterIds {
        controls.filterAttack, controls.filterDecay, sustain (controls.filterSustain)
    };
    const ContourSettings loudnessIds {
        controls.loudnessAttack, controls.loudnessDecay, sustain (controls.loudnessSustain)
    };
    return contract == StateContract::ContourContract::canonicalContours
             ? RoutedContours { filterIds, loudnessIds }
             : RoutedContours { loudnessIds, filterIds };
}

std::size_t index (Parameter parameter) noexcept
{
    return static_cast<std::size_t> (parameter);
}
}
