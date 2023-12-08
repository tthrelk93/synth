//
//  CustomPanelComponent.cpp
//  NewProject
//
//  Created by Thomas Threlkeld on 11/10/23.
//

#include <stdio.h>
#include "CustomPanelComponent.h"

CustomPanelComponent::CustomPanelComponent() {
    juce::File woodgrainFile("/Users/thomasthrelkeld/MiniMoog/Source/woodgrain.jpeg");
        juce::File blackPanelFile("/Users/thomasthrelkeld/MiniMoog/Source/synthBrooding.png");

        if (woodgrainFile.existsAsFile()) {
            woodgrainImage = juce::ImageFileFormat::loadFrom(woodgrainFile);
        } else {
            juce::Logger::writeToLog("Woodgrain image file does not exist");
        }

        if (blackPanelFile.existsAsFile()) {
            blackPanelImage = juce::ImageFileFormat::loadFrom(blackPanelFile);
        } else {
            juce::Logger::writeToLog("Black panel image file does not exist");
        }

        // Check if images are loaded correctly
        if (!woodgrainImage.isValid()) {
            juce::Logger::writeToLog("Failed to load woodgrain image");
        }
        if (!blackPanelImage.isValid()) {
            juce::Logger::writeToLog("Failed to load black panel image");
        }

}

void CustomPanelComponent::paint(juce::Graphics& g) {
    int borderThickness = 15; // Adjust this value to match the desired border thickness

       // Draw the woodgrain border
       g.drawImageWithin(woodgrainImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);

       // Calculate the area for the black panel with consistent border thickness on all sides
       juce::Rectangle<int> panelArea = getLocalBounds();

       // Draw the black panel
       g.drawImageWithin(blackPanelImage, panelArea.getX(), panelArea.getY(), panelArea.getWidth(), panelArea.getHeight(), juce::RectanglePlacement::fillDestination);

       // Draw the opaque black layer over the black panel
       g.setColour(juce::Colours::black.withAlpha(0.7f)); // Set the alpha for opacity (change as needed)
       g.fillRect(panelArea); // Draw over the black panel area
}
