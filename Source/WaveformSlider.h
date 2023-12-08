//
//  WaveformSlider.h
//  NewProject
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#ifndef WaveformSlider_h
#define WaveformSlider_h

#include <JuceHeader.h>
#include "CustomSliderLookAndFeel.h"



class WaveformSlider : public juce::Slider {
public:
    WaveformSlider();
    WaveformSlider(int numDiscretePos, bool useCustomRange, bool isOscWaveform, juce::String sliderKey);
    WaveformSlider(int numDiscretePos, bool useCustomRange, double stepLength, double first, double last, bool isTime, bool isOscWaveform, juce::String sliderKey);
    double loudnessAttackValueToSliderPosition(double value);
    double sliderPositionToLoudnessAttackValue(double position);
    
    virtual ~WaveformSlider();

protected:
    // Override the snapValue method from the juce::Slider class
    double snapValue(double attemptedValue, DragMode dragMode) override;
    int numDiscretePositions;
    bool customRange;
    double step;
    double firstPos;
    double lastPos;
    bool isTimeKnob;
    CustomSliderLookAndFeel customLookAndFeel;  // Member variable
};

#endif /* WaveformSlider_h */
