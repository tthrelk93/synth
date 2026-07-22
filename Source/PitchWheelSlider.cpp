//
//  PitchWheelSlider.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#include <stdio.h>
#include "PitchWheelSlider.h"

PitchWheelSlider::PitchWheelSlider() {
    const auto accentColour = juce::Colour::fromRGB(60, 160, 255);
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setColour(juce::Slider::trackColourId, accentColour.withAlpha(0.55f));
    setColour(juce::Slider::thumbColourId, accentColour.withAlpha(0.9f));
    setRange(-5.0, 5.0, 0.01);
    setValue(0.0);

    // Set the custom look and feel
    setLookAndFeel(&lookAndFeel);
}


void PitchWheelSlider::mouseUp(const juce::MouseEvent& event) {
    // Return to centre as the final value inside the active drag gesture.
    setValue(0.0, juce::sendNotificationSync);
    Slider::mouseUp(event);
}
