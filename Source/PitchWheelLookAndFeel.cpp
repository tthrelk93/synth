//
//  PitchWheelLookAndFeel.cpp
//  MiniMoog - VST3
//
//  Created by Thomas Threlkeld on 12/3/23.
//

#include <stdio.h>
#include "PitchWheelLookAndFeel.h"

void PitchWheelLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider) {
    // Draw the track
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    if (slider.isHorizontal()) {
        g.fillRect(juce::Rectangle<float>(x, y + height * 0.5f - 2, width, 4));
    } else {
        g.fillRect(juce::Rectangle<float>(x + width * 0.5f - 2, y, 4, height));
    }

    // Calculate the diameter for the thumb
    float thumbDiameter = 15;

    // Create a rectangle for the thumb
    juce::Rectangle<float> thumbRect(0, 0, thumbDiameter, thumbDiameter);

    // Position the rectangle's centre
    thumbRect.setCentre(slider.isHorizontal() ? sliderPos : (x + width * 0.5f),
                        slider.isHorizontal() ? (y + height * 0.5f) : sliderPos);

    // Draw the thumb
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(thumbRect);
}
