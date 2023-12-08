//
//  VintageLookAndFeel.cpp
//  NewProject - Shared Code
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#include <stdio.h>
#include "VintageLookAndFeel.h"

VintageLookAndFeel::VintageLookAndFeel(bool isHorizontal) : isHorizontalSwitch(isHorizontal) {
    // Constructor code here (if needed)
}

VintageLookAndFeel::~VintageLookAndFeel() {
    // Destructor code here (if needed)
}
void VintageLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& toggleButton,
                                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bounds = toggleButton.getLocalBounds().toFloat();
    float cornerRadius = 6.0f; // Rounded corners radius

    // Draw the background of the toggle with rounded corners
    g.setColour(toggleButton.getToggleState() ? juce::Colours::black.withAlpha(0.35f) : juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Determine handle color based on toggle state
    juce::Colour handleColour = toggleButton.getToggleState() ? juce::Colours::linen.withAlpha(0.90f) : juce::Colours::papayawhip.withAlpha(0.35f);

    // Calculate handle bounds
    auto handleBounds = bounds.reduced(4.0f); // Reduced for padding inside the switch

    if (isHorizontalSwitch) {
        float handleWidth = bounds.getWidth() / 2.0f - 4.0f; // Half width of the switch minus padding
        if (toggleButton.getToggleState()) {
            // Draw the handle on the right side for "on" state
            handleBounds.setX(bounds.getRight() - handleWidth - 4.0f); // 4.0f is the padding
        } else {
            // Draw the handle on the left side for "off" state
            handleBounds.setX(bounds.getX() + 4.0f); // 4.0f is the padding
        }
        handleBounds.setWidth(handleWidth);
    } else {
        float handleHeight = bounds.getHeight() / 2.0f - 4.0f; // Half height of the switch minus padding
        if (toggleButton.getToggleState()) {
            // Draw the handle on the bottom side for "on" state
            handleBounds.setY(bounds.getBottom() - handleHeight - 4.0f); // 4.0f is the padding
        } else {
            // Draw the handle on the top side for "off" state
            handleBounds.setY(bounds.getY() + 4.0f); // 4.0f is the padding
        }
        handleBounds.setHeight(handleHeight);
    }

    // Draw the handle
    g.setColour(handleColour);
    g.fillRoundedRectangle(handleBounds, cornerRadius);
}


