//
//  ModWheelSlider.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#include <stdio.h>
#include "ModWheelSlider.h"

ModWheelSlider::ModWheelSlider() {
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setColour(juce::Slider::trackColourId, juce::Colours::papayawhip.withAlpha(0.6f));
    setColour(juce::Slider::thumbColourId, juce::Colours::linen);
    setRange(0.0, 1.0, 0.01); // Example range
    setValue(0.0); // Start at zero position
}
