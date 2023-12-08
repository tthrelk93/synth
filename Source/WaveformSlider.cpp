//
//  WaveformSlider.cpp
//  NewProject - Shared Code
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#include <stdio.h>
#include "WaveformSlider.h"
#include "CustomSliderLookAndFeel.h"


WaveformSlider::WaveformSlider(int numDiscretePos, bool useCustomRange, double stepLength, double first, double last, bool isTime, bool isOscWaveform, juce::String sliderKey){
    numDiscretePositions = numDiscretePos;
    customRange = useCustomRange;
    step = stepLength;
    firstPos = first;
    lastPos = last;
    isTimeKnob = isTime;
    customLookAndFeel.isOscWaveformSlider = isOscWaveform;
    customLookAndFeel.sliderKey = sliderKey;
    
    setLookAndFeel(&customLookAndFeel);
   
}
WaveformSlider::WaveformSlider(int numDiscretePos, bool useCustomRange, bool isOscWaveform, juce::String sliderKey){
    numDiscretePositions = numDiscretePos;
    customRange = useCustomRange;
    customLookAndFeel.isOscWaveformSlider = isOscWaveform;
    customLookAndFeel.sliderKey = sliderKey;
    setLookAndFeel(&customLookAndFeel);
}
WaveformSlider::WaveformSlider() {}

WaveformSlider::~WaveformSlider() {
    setLookAndFeel(nullptr);  // Reset the look and feel
    // Destructor code here (if needed)
}

double WaveformSlider::loudnessAttackValueToSliderPosition(double value) {
    double position;
    
    if (value <= 1000.0) {  // If value is in milliseconds (up to 1 second)
        // Map from 10 ms to 1000 ms (1 s)
        position = (value - 10.0) / (1000.0 - 10.0) * 0.5;
    } else {  // If value is in seconds (above 1 second)
        // Convert to seconds for easier mapping
        double valueInSeconds = value / 1000.0;
        // Map from 1 s to 10 s
        position = 0.5 + (valueInSeconds - 1.0) / (10.0 - 1.0) * 0.5;
    }

    return position;
}


double WaveformSlider::sliderPositionToLoudnessAttackValue(double position) {
    double value;

    if (position <= 0.5) {  // First half of the slider
        // Map back to milliseconds (10 ms to 1000 ms)
        value = position * 2 * (1000.0 - 10.0) + 10.0;
    } else {  // Second half of the slider
        // Map back to seconds (1 s to 10 s) and convert to milliseconds
        double valueInSeconds = (position - 0.5) * 2 * (10.0 - 1.0) + 1.0;
        value = valueInSeconds * 1000.0;
    }

    return value;
}

double WaveformSlider::snapValue(double attemptedValue, juce::Slider::DragMode dragMode) {
    if (isTimeKnob && dragMode != juce::Slider::DragMode::notDragging) {
        // Convert the slider position to loudness attack value
        double loudnessAttackValue = sliderPositionToLoudnessAttackValue(attemptedValue);

        // Normalize the loudness attack value back to a slider position
        double sliderPosition;
        if (loudnessAttackValue <= 1000.0) {  // Milliseconds range (10 ms to 1000 ms)
            sliderPosition = (loudnessAttackValue - 10.0) / (1000.0 - 10.0) * 0.5;
        } else {  // Seconds range (1 s to 10 s)
            double loudnessAttackInSeconds = loudnessAttackValue / 1000.0;
            sliderPosition = 0.5 + (loudnessAttackInSeconds - 1.0) / (10.0 - 1.0) * 0.5;
        }
        return sliderPosition;
    } else if (customRange && dragMode != juce::Slider::DragMode::notDragging) {
        // Custom range handling for -2.5 to 2.5 in 0.5 increments
        double snappedValue = round(attemptedValue / step) * step;
        return juce::jlimit(firstPos, lastPos, snappedValue);
    } else if (dragMode != juce::Slider::DragMode::notDragging) {
        // Default handling
        const double defaultStep = 1.0;
        double snappedValue = round(attemptedValue / defaultStep) * defaultStep;
        return juce::jlimit(0.0, double(numDiscretePositions - 1), snappedValue);
    } else {
        return attemptedValue;
    }
}

