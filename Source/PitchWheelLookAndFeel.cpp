//
//  PitchWheelLookAndFeel.cpp
//  MiniMoog - VST3
//
//  Created by Thomas Threlkeld on 12/3/23.
//

#include <stdio.h>
#include "PitchWheelLookAndFeel.h"

void PitchWheelLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         const juce::Slider::SliderStyle, juce::Slider& slider) {
    // Draw the track
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    if (slider.isHorizontal()) {
        g.fillRect(juce::Rectangle<float>(static_cast<float>(x),
                                          static_cast<float>(y) + static_cast<float>(height) * 0.5f - 2.0f,
                                          static_cast<float>(width),
                                          4.0f));
    } else {
        g.fillRect(juce::Rectangle<float>(static_cast<float>(x) + static_cast<float>(width) * 0.5f - 2.0f,
                                          static_cast<float>(y),
                                          4.0f,
                                          static_cast<float>(height)));
    }

    // Calculate the diameter for the thumb
    const float thumbDiameter = 15.0f;

    // Create a rectangle for the thumb
    juce::Rectangle<float> thumbRect(0.0f, 0.0f, thumbDiameter, thumbDiameter);

    // Position the rectangle's centre
    thumbRect.setCentre(slider.isHorizontal() ? sliderPos : (x + width * 0.5f),
                        slider.isHorizontal() ? (y + height * 0.5f) : sliderPos);

    // Draw the thumb
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(thumbRect);
}
