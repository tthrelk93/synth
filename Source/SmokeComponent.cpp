//
//  SmokeComponent.cpp
//  MiniMoog - VST3
//
//  Created by Thomas Threlkeld on 12/3/23.
//

#include <stdio.h>
#include "SmokeComponent.h"

SmokeComponent::SmokeComponent() : smokePosition(100.0f, 100.0f), smokeSize(4.0f) {
    startTimerHz(60); // Update 60 times per second
}

SmokeComponent::~SmokeComponent() {
    stopTimer();
}

void SmokeComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::transparentBlack);
    
    auto bounds = getLocalBounds().reduced(10).toFloat(); // Slightly reduce bounds to contain smoke
    
    for(int i = 0; i < 20; i++) {
        // Generate a random starting point within the adjusted bounds
        juce::Point<float> randomStartPoint(
                   juce::Random::getSystemRandom().nextFloat() * bounds.getWidth() + bounds.getX(),
                   juce::Random::getSystemRandom().nextFloat() * (bounds.getHeight() * 0.75f) + bounds.getY() + (bounds.getHeight() * 0.25f) // Start from lower within the bounds
               );
        
        smokePaths[i].clear();
        smokePaths[i].startNewSubPath(randomStartPoint);
        juce::Colour colour;
        if(isPink){
            // Generate a random color for each smoke trail
            colour = juce::Colour::fromHSV(
                                           juce::Random::getSystemRandom().nextFloat(), // Hue
                                           0.5f, // Saturation
                                           0.8f, // Brightness
                                           alpha  // Alpha
                                           );
        } else {
            colour = juce::Colours::whitesmoke.withAlpha(alpha);
        }
        
        g.setColour(colour);
        
        // Generate a series of points to simulate a smoke trail
        for (int j = 0; j < 50; ++j) {
            float x = randomStartPoint.x + (juce::Random::getSystemRandom().nextFloat() - 0.5f) * smokeSize;
            float y = randomStartPoint.y - j * (juce::Random::getSystemRandom().nextFloat() * 0.5f);
            smokePaths[i].lineTo(x, y);
        }
        
        g.strokePath(smokePaths[i], juce::PathStrokeType(1.0f));
    }
}


void SmokeComponent::timerCallback() {
    // Update smoke position and size
    smokePosition.x += juce::Random::getSystemRandom().nextFloat() * 4.0f - 2.0f;
    smokePosition.y -= juce::Random::getSystemRandom().nextFloat() * 2.0f;
    smokeSize += juce::Random::getSystemRandom().nextFloat() * 0.2f - 0.1f;
    
    // Reset smoke when it goes out of bounds
    if (smokePosition.y < 0 || smokePosition.x < 0 || smokePosition.x > getWidth()) {
        smokePosition = juce::Point<float>(getWidth() / 2.0f, getHeight());
        smokeSize = 1.0f;
    }
    
    repaint();
}

void SmokeComponent::setIsPink(bool isPinkNoise){
    isPink = isPinkNoise;
}
bool SmokeComponent::getIsPink(){
    return isPink;
}
void SmokeComponent::setAlpha(float a){
    alpha = a;
}
