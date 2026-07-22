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

RoutedContours mapStoredControls (StateContract::ContourContract contract,
                                  const ContourControls& controls) noexcept
{
    const ContourSettings filterIds {
        controls.filterAttack, controls.filterDecay, controls.filterSustain
    };
    const ContourSettings loudnessIds {
        controls.loudnessAttack, controls.loudnessDecay, controls.loudnessSustain
    };
    return contract == StateContract::ContourContract::canonicalContours
             ? RoutedContours { filterIds, loudnessIds }
             : RoutedContours { loudnessIds, filterIds };
}

RoutedContours route (StateContract::ContourContract contract,
                      const ContourControls& controls) noexcept
{
    auto routed = mapStoredControls (contract, controls);
    if (! controls.decayEnabled)
    {
        routed.filter.sustain = 1.0f;
        routed.loudness.sustain = 1.0f;
    }
    return routed;
}

std::size_t index (Parameter parameter) noexcept
{
    return static_cast<std::size_t> (parameter);
}
}
