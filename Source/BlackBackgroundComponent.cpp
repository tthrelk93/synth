//
//  BlackBackgroundComponent.cpp
//  MiniMoog - Shared Code
//
//  Created by Thomas Threlkeld on 12/5/23.
//

#include <stdio.h>
#include "BlackBackgroundComponent.h"

BlackBackgroundComponent::BlackBackgroundComponent()
{
    // Constructor code here (if needed)
}

void BlackBackgroundComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat(); // Get the component's area as a Rectangle<float>
       float cornerSize = 30.0f; // This is the radius of the corners in pixels

       g.setColour(juce::Colours::black.withAlpha(0.8f)); // Set the colour with alpha
       g.fillRoundedRectangle(area, cornerSize); // Draw a rounded rectangle with the specified corner size
}
