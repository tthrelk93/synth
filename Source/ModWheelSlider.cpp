//
//  ModWheelSlider.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#include <stdio.h>
#include "ModWheelSlider.h"

ModWheelSlider::ModWheelSlider() {
    const auto accentColour = juce::Colour::fromRGB(60, 160, 255);
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setColour(juce::Slider::trackColourId, accentColour.withAlpha(0.55f));
    setColour(juce::Slider::thumbColourId, accentColour.withAlpha(0.9f));
    setRange(0.0, 1.0, 0.01); // Example range
    setValue(0.0); // Start at zero position
}
