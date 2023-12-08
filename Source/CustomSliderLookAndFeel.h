//
//  CustomSliderLookAndFeel.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 11/24/23.
//

#ifndef CustomSliderLookAndFeel_h
#define CustomSliderLookAndFeel_h

#include <JuceHeader.h>


class CustomSliderLookAndFeel : public juce::LookAndFeel_V4 {
public:
    bool isOscWaveformSlider = false;
    juce::String sliderKey = "";
    // Override the method for drawing linear sliders
    void drawRotarySlider (juce::Graphics&,
                               int x, int y, int width, int height,
                               float sliderPosProportional,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider&) override;
    void drawWaveform(juce::Graphics& g, int index, int x, int y, int width, int height);
    juce::String createTextLabelForSlider(float index);

};

#endif /* CustomSliderLookAndFeel_h */
