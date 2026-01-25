//
//  CustomPanelComponent.cpp
//  NewProject
//
//  Created by Thomas Threlkeld on 11/10/23.
//

#include <stdio.h>
#include "CustomPanelComponent.h"

CustomPanelComponent::CustomPanelComponent() {
    auto baseDir = juce::File::getCurrentWorkingDirectory();
    juce::File woodgrainFile = baseDir.getChildFile("Source").getChildFile("woodgrain.jpeg");
    juce::File blackPanelFile = baseDir.getChildFile("Source").getChildFile("synthBrooding.png");

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
    auto bounds = getLocalBounds().toFloat();

    const auto topColour = juce::Colour::fromRGB(18, 22, 34);
    const auto bottomColour = juce::Colour::fromRGB(7, 10, 16);
    juce::ColourGradient baseGradient(topColour, 0.0f, 0.0f, bottomColour, 0.0f, bounds.getBottom(), false);
    g.setGradientFill(baseGradient);
    g.fillRect(bounds);

    auto glossArea = bounds.withHeight(bounds.getHeight() * 0.3f);
    glossArea.reduce(bounds.getWidth() * 0.03f, 0.0f);
    juce::ColourGradient glossGradient(juce::Colour::fromRGB(110, 170, 255).withAlpha(0.25f),
                                       glossArea.getX(), glossArea.getY(),
                                       juce::Colour::fromRGB(110, 170, 255).withAlpha(0.0f),
                                       glossArea.getX(), glossArea.getBottom(),
                                       false);
    g.setGradientFill(glossGradient);
    g.fillRoundedRectangle(glossArea, 30.0f);

    g.setColour(juce::Colour::fromRGB(40, 55, 80).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), 24.0f, 1.0f);
}
