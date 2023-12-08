//
//  VintageLookAndFeel.h
//  NewProject
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#ifndef VintageLookAndFeel_h
#define VintageLookAndFeel_h
#include <JuceHeader.h>

class VintageLookAndFeel : public juce::LookAndFeel_V4 {
public:
    VintageLookAndFeel(bool isHorizontal = true);
    ~VintageLookAndFeel();

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& toggleButton,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    bool isHorizontalSwitch;
};

#endif /* VintageLookAndFeel_h */
