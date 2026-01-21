//
//  WaveformSlider.h
//  NewProject
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#ifndef WaveformSlider_h
#define WaveformSlider_h

#include <JuceHeader.h>
#include <functional>
#include "CustomSliderLookAndFeel.h"



class WaveformSlider : public juce::Slider {
public:
    WaveformSlider();
    WaveformSlider(int numDiscretePos, bool useCustomRange, bool isOscWaveform, juce::String sliderKey);
    WaveformSlider(int numDiscretePos, bool useCustomRange, double stepLength, double first, double last, bool isTime, bool isOscWaveform, juce::String sliderKey);
    double loudnessAttackValueToSliderPosition(double value);
    double sliderPositionToLoudnessAttackValue(double position);
    void setToggleEnabled(bool shouldToggle);
    void setToggleActive(bool isActive);
    bool isToggleActive() const;
    void setToggleCallback(std::function<void(bool)> callback);
    
    virtual ~WaveformSlider();

protected:
    // Override the snapValue method from the juce::Slider class
    double snapValue(double attemptedValue, DragMode dragMode) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    int numDiscretePositions;
    bool customRange;
    double step;
    double firstPos;
    double lastPos;
    bool isTimeKnob;
    CustomSliderLookAndFeel customLookAndFeel;  // Member variable

    bool toggleEnabled = false;
    bool toggleActive = false;
    bool wasDragged = false;
    std::function<void(bool)> toggleCallback;
};

#endif /* WaveformSlider_h */
