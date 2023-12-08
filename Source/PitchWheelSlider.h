//
//  PitchWheelSlider.h
//  MoogMini
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#ifndef PitchWheelSlider_h
#define PitchWheelSlider_h

#include <JuceHeader.h>
#include "PitchWheelLookAndFeel.h"

class PitchWheelSlider  : public juce::Slider {
public:
    PitchWheelSlider();
    void mouseUp(const juce::MouseEvent& event) override;
    
private:
    PitchWheelLookAndFeel lookAndFeel;
};

#endif /* PitchWheelSlider_h */
