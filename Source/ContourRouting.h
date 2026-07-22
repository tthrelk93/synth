#pragma once

#include "StateContract.h"

#include <cstddef>

namespace ContourRouting
{
enum class SemanticContour { filter, loudness };
enum class Stage { attack, decay, sustain };
enum class Parameter
{
    filterAttack,
    filterDecay,
    filterSustain,
    loudnessAttack,
    loudnessDecay,
    loudnessSustain,
    count
};

struct ContourControls
{
    float filterAttack = 0.0f;
    float filterDecay = 0.0f;
    float filterSustain = 0.0f;
    float loudnessAttack = 0.0f;
    float loudnessDecay = 0.0f;
    float loudnessSustain = 0.0f;
    bool decayEnabled = false;
};

struct ContourSettings
{
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 1.0f;
};

struct RoutedContours
{
    ContourSettings filter;
    ContourSettings loudness;
};

Parameter parameterFor (StateContract::ContourContract contract,
                        SemanticContour semanticContour,
                        Stage stage) noexcept;
RoutedContours route (StateContract::ContourContract contract,
                      const ContourControls& controls) noexcept;
std::size_t index (Parameter parameter) noexcept;
}
