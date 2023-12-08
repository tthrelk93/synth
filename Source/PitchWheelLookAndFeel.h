//
//  PitchWheelLookAndFeel.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 12/3/23.
//

#ifndef PitchWheelLookAndFeel_h
#define PitchWheelLookAndFeel_h
#include <JuceHeader.h>

class PitchWheelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

#endif /* PitchWheelLookAndFeel_h */
