#pragma once

#include <JuceHeader.h>

#include "ParameterRegistry.h"

#include <functional>

enum class DisplayMapping
{
    componentRange,
    contourTimeMilliseconds
};

class ParameterBinding final : private juce::Slider::Listener,
                               private juce::Button::Listener
{
public:
    using ValueChangedCallback = std::function<void(float)>;

    ParameterBinding (ParameterRegistry::Key key,
                      juce::RangedAudioParameter& parameter,
                      juce::Slider& slider,
                      DisplayMapping mapping,
                      ValueChangedCallback sideEffect = {});

    ParameterBinding (ParameterRegistry::Key key,
                      juce::RangedAudioParameter& parameter,
                      juce::Button& button,
                      ValueChangedCallback sideEffect = {});

    ParameterBinding (ParameterRegistry::Key key,
                      juce::RangedAudioParameter& parameter,
                      ValueChangedCallback target);

    ~ParameterBinding() override;

    ParameterRegistry::Key getKey() const noexcept { return key; }
    void beginGesture();
    void endGesture();
    void setValueAsCompleteGesture (float physicalValue);
    void setNormalisedValueAsCompleteGesture (float normalisedValue);

private:
    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted (juce::Slider*) override;
    void sliderDragEnded (juce::Slider*) override;
    void buttonClicked (juce::Button*) override;

    float componentToPhysical (double componentValue) const;
    double physicalToComponent (float physicalValue) const;
    void parameterValueChanged (float physicalValue);

    ParameterRegistry::Key key;
    juce::RangedAudioParameter& parameter;
    juce::Slider* slider = nullptr;
    juce::Button* button = nullptr;
    DisplayMapping mapping = DisplayMapping::componentRange;
    ValueChangedCallback target;
    ValueChangedCallback sideEffect;
    bool ignoreComponentCallbacks = false;
    bool gestureInProgress = false;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterBinding)
};
