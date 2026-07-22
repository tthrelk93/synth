#include "ParameterBinding.h"

ParameterBinding::ParameterBinding (ParameterRegistry::Key parameterKey,
                                    juce::RangedAudioParameter& preparedParameter,
                                    juce::Slider& targetSlider,
                                    DisplayMapping displayMapping,
                                    ValueChangedCallback callback)
    : key (parameterKey),
      parameter (preparedParameter),
      slider (&targetSlider),
      mapping (displayMapping),
      sideEffect (std::move (callback)),
      attachment (parameter, [this] (float value) { parameterValueChanged (value); })
{
    slider->addListener (this);
    attachment.sendInitialUpdate();
}

ParameterBinding::ParameterBinding (ParameterRegistry::Key parameterKey,
                                    juce::RangedAudioParameter& preparedParameter,
                                    juce::Button& targetButton,
                                    ValueChangedCallback callback)
    : key (parameterKey),
      parameter (preparedParameter),
      button (&targetButton),
      sideEffect (std::move (callback)),
      attachment (parameter, [this] (float value) { parameterValueChanged (value); })
{
    button->addListener (this);
    attachment.sendInitialUpdate();
}

ParameterBinding::ParameterBinding (ParameterRegistry::Key parameterKey,
                                    juce::RangedAudioParameter& preparedParameter,
                                    ValueChangedCallback targetCallback)
    : key (parameterKey),
      parameter (preparedParameter),
      target (std::move (targetCallback)),
      attachment (parameter, [this] (float value) { parameterValueChanged (value); })
{
    attachment.sendInitialUpdate();
}

ParameterBinding::~ParameterBinding()
{
    endGesture();
    if (slider != nullptr)
        slider->removeListener (this);
    if (button != nullptr)
        button->removeListener (this);
}

void ParameterBinding::beginGesture()
{
    if (gestureInProgress)
        return;
    gestureInProgress = true;
    attachment.beginGesture();
}

void ParameterBinding::endGesture()
{
    if (! gestureInProgress)
        return;
    attachment.endGesture();
    gestureInProgress = false;
}

void ParameterBinding::setValueAsCompleteGesture (float physicalValue)
{
    attachment.setValueAsCompleteGesture (physicalValue);
}

void ParameterBinding::setNormalisedValueAsCompleteGesture (float normalisedValue)
{
    setValueAsCompleteGesture (
        parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalisedValue)));
}

void ParameterBinding::sliderValueChanged (juce::Slider* changedSlider)
{
    if (ignoreComponentCallbacks || changedSlider != slider)
        return;

    const auto physicalValue = componentToPhysical (slider->getValue());
    if (gestureInProgress)
        attachment.setValueAsPartOfGesture (physicalValue);
    else
        attachment.setValueAsCompleteGesture (physicalValue);
}

void ParameterBinding::sliderDragStarted (juce::Slider* changedSlider)
{
    if (changedSlider == slider)
        beginGesture();
}

void ParameterBinding::sliderDragEnded (juce::Slider* changedSlider)
{
    if (changedSlider == slider)
        endGesture();
}

void ParameterBinding::buttonClicked (juce::Button* changedButton)
{
    if (ignoreComponentCallbacks || changedButton != button)
        return;
    attachment.setValueAsCompleteGesture (button->getToggleState() ? 1.0f : 0.0f);
}

float ParameterBinding::componentToPhysical (double componentValue) const
{
    if (mapping == DisplayMapping::contourTimeMilliseconds)
        return static_cast<float> (componentValue / 10000.0);

    jassert (slider != nullptr);
    const auto componentNormalised = slider->getNormalisableRange().convertTo0to1 (
        componentValue);
    return parameter.convertFrom0to1 (static_cast<float> (componentNormalised));
}

double ParameterBinding::physicalToComponent (float physicalValue) const
{
    jassert (slider != nullptr);
    if (mapping == DisplayMapping::contourTimeMilliseconds)
        return juce::jlimit (slider->getMinimum(), slider->getMaximum(),
                            static_cast<double> (physicalValue) * 10000.0);

    const auto parameterNormalised = parameter.convertTo0to1 (physicalValue);
    return slider->getNormalisableRange().convertFrom0to1 (parameterNormalised);
}

void ParameterBinding::parameterValueChanged (float physicalValue)
{
    const juce::ScopedValueSetter<bool> scopedIgnore (ignoreComponentCallbacks, true);
    if (slider != nullptr)
        slider->setValue (physicalToComponent (physicalValue), juce::dontSendNotification);
    if (button != nullptr)
        button->setToggleState (physicalValue >= 0.5f, juce::dontSendNotification);
    if (target)
        target (physicalValue);
    if (sideEffect)
        sideEffect (physicalValue);
}
