//
//  PitchWheelSlider.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#include <stdio.h>
#include "PitchWheelSlider.h"

PitchWheelSlider::PitchWheelSlider() {
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setColour(juce::Slider::trackColourId, juce::Colours::papayawhip.withAlpha(0.6f));
    setColour(juce::Slider::thumbColourId, juce::Colours::linen);
    setRange(-5.0, 5.0, 0.01);
    setValue(0.0);

    // Set the custom look and feel
    setLookAndFeel(&lookAndFeel);
}


void PitchWheelSlider::mouseUp(const juce::MouseEvent& event) {
    Slider::mouseUp(event);
    // Use sendNotificationSync to immediately trigger the listener
    setValue(0.0, juce::sendNotificationSync);
}
