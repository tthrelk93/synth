//
//  CustomPanelComponent.h
//  NewProject
//
//  Created by Thomas Threlkeld on 11/10/23.
//

#ifndef CustomPanelComponent_h
#define CustomPanelComponent_h
#pragma once

#include <JuceHeader.h>

class CustomPanelComponent : public juce::Component {
public:
    CustomPanelComponent();

    void paint(juce::Graphics& g) override;

private:
    juce::Image woodgrainImage;
    juce::Image blackPanelImage;
    int borderThickness = 20; // Adjust as needed for the border size
};


#endif /* CustomPanelComponent_h */
