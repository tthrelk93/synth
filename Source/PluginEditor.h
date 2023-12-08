#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformSlider.h"
#include "WaveformDisplay.h"
#include "CustomPanelComponent.h"
#include "VintageLookAndFeel.h"
#include "PitchWheelSlider.h"
#include "ModWheelSlider.h"
#include "PianoKey.h"
#include "SmokeComponent.h"
#include "BlackBackgroundComponent.h"

class MoogMiniAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer, public juce::Slider::Listener, public juce::Button::Listener, public PianoKey::Listener {
public:
    MoogMiniAudioProcessorEditor(MoogMiniAudioProcessor&);
    ~MoogMiniAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    float getEnumSizeLessOne(juce::Slider* slider);
    juce::String getParameterID(juce::Slider* slider);
    float getNormalizedValue(juce::Slider* slider, float enumSizeLessOne);
    bool sliderHasChanged(juce::Slider* slider);
    void createSliderKnob(WaveformSlider*& sliderKnob, std::string sliderKey, int numPositions, float minPosVal, float maxPosVal, float increment, std::string paramName, bool useCustomRange, int posArray[], int cellWidth, int cellHeight, bool isTimeKnob);
    void createToggleSwitch(juce::ToggleButton& toggleSwitch, std::string toggleKey, std::string paramName, int posArray[], bool isHorizontal, int cellWidth, int cellHeight);
    
    // PianoKey::Listener implementation
        void pianoKeyPressed(int noteNumber, PianoKey* key) override;
        void pianoKeyReleased(int noteNumber) override;
    
    void pianoKeyDragged(int noteNumber, const juce::MouseEvent& event) override;
    void releaseAllKeys();
    virtual void pianoKeyMouseUp() override;
    
    double sliderPositionToLoudnessAttackValue(double position);

    std::unordered_map<int, std::unordered_map<int, std::string>> nestedMap;
    std::map<std::string, juce::Slider*> sliderMap;
    std::map<std::string, juce::ToggleButton*>  toggleMap;
    std::map<std::string, juce::TextButton*>  buttonMap;
    std::map<std::string, juce::Label*>  labelMap;
    std::map<std::string, std::string>  labelStringMap;
    
    
   

private:
    MoogMiniAudioProcessor& audioProcessor;
    juce::Image image;
    juce::ImageComponent imageComponent;
    
    juce::OwnedArray<WaveformSlider> waveformSliders;
    juce::OwnedArray<VintageLookAndFeel> vintageLookAndFeels;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::TextButton> buttons;
    
    WaveformSlider* ctrlTuneKnob;
    WaveformSlider* ctrlGlideKnob;
    WaveformSlider* ctrlModMixKnob;
    juce::ToggleButton oscModSwitch;
    juce::ToggleButton decaySwitch;
    juce::ToggleButton glideSwitch;

    
    //Osc bank
    WaveformSlider* osc1RangeKnob;
    WaveformSlider* osc1WaveFormKnob;
    
    WaveformSlider* osc2RangeKnob;
    WaveformSlider* osc2FreqKnob;
    WaveformSlider* osc2WaveFormKnob;
    
    WaveformSlider* osc3RangeKnob;
    WaveformSlider* osc3FreqKnob;
    WaveformSlider* osc3WaveFormKnob;
    
    juce::ToggleButton osc3CtrlSwitch;
    
    //Mixer
    WaveformSlider* osc1VolKnob;
    WaveformSlider* osc2VolKnob;
    WaveformSlider* osc3VolKnob;
    
    WaveformSlider* feedbackKnob;
    
    juce::ToggleButton osc1OnOffSwitch;
    juce::ToggleButton extInputVolSwitch;
    juce::ToggleButton osc2OnOffSwitch;
    juce::ToggleButton noiseOnOffSwitch;
    juce::ToggleButton osc3OnOffSwitch;
    
    WaveformSlider* extInputVolKnob;
    WaveformSlider* noiseVolKnob;
    
    juce::TextButton overloadButton;
    juce::ToggleButton whitePinkSwitch;
    
    juce::ToggleButton filterModSwitch;
    juce::ToggleButton keyboardCtrlSwitch1;
    juce::ToggleButton keyboardCtrlSwitch2;
    
    //Modifiers
    //Filter
    WaveformSlider* filterCutoffFreqKnob;
    WaveformSlider* filterEmphasisKnob;
    WaveformSlider* filterAmtContourKnob;
    
    WaveformSlider* filterAttackTimeKnob;
    WaveformSlider* filterDecayTimeKnob;
    WaveformSlider* filterSustainKnob;
    
    WaveformSlider* loudnessAttackTimeKnob;
    WaveformSlider* loudnessDecayTimeKnob;
    WaveformSlider* loudnessSustainLevelKnob;
    
    //Output
    WaveformSlider* outputVolKnob;
    juce::ToggleButton ouputMainOutputSwitch;
    
    WaveformSlider* outputPhonesVolKnob;
    juce::ToggleButton a440hzSwitch;
    
    //juce::Slider modWheelSlider;
    
    
    
    juce::Label ctrlTuneKnobLabel;
    juce::Label ctrlGlideKnobLabel;
    juce::Label ctrlModMixKnobLabel;
    juce::Label oscModSwitchLabel;
    juce::Label feedbackKnobLabel;
    
    //Osc bank
    juce::Label osc1RangeKnobLabel;
    juce::Label osc1WaveFormKnobLabel;
    juce::Label osc2RangeKnobLabel;
    juce::Label osc2FreqKnobLabel;
    juce::Label osc2WaveFormKnobLabel;
    juce::Label osc3RangeKnobLabel;
    juce::Label osc3FreqKnobLabel;
    juce::Label osc3WaveFormKnobLabel;
    juce::Label osc3CtrlSwitchLabel;
    //Mixer
    juce::Label osc1VolKnobLabel;
    juce::Label osc2VolKnobLabel;
    juce::Label osc3VolKnobLabel;
    juce::Label osc1OnOffSwitchLabel;
    juce::Label extInputVolSwitchLabel;
    juce::Label osc2OnOffSwitchLabel;
    juce::Label noiseOnOffSwitchLabel;
    juce::Label osc3OnOffSwitchLabel;
    juce::Label extInputVolKnobLabel;
    juce::Label noiseVolKnobLabel;
    juce::Label overloadButtonLabel;
    juce::Label whitePinkSwitchLabel;
    juce::Label filterModSwitchLabel;
    juce::Label keyboardCtrlSwitch1Label;
    juce::Label keyboardCtrlSwitch2Label;
    //Modifiers
    //Filter
    juce::Label filterCutoffFreqKnobLabel;
    juce::Label filterEmphasisKnobLabel;
    juce::Label filterAmtContourKnobLabel;
    juce::Label filterAttackTimeKnobLabel;
    juce::Label filterDecayTimeKnobLabel;
    juce::Label filterSustainKnobLabel;
    juce::Label loudnessAttackTimeKnobLabel;
    juce::Label loudnessDecayTimeKnobLabel;
    juce::Label loudnessSustainLevelKnobLabel;
    juce::Label decaySwitchLabel;
    juce::Label glideSwitchLabel;
    //Output
    juce::Label outputVolKnobLabel;
    juce::Label ouputMainOutputSwitchLabel;
    juce::Label outputPhonesVolKnobLabel;
    juce::Label a440hzSwitchLabel;
    
    WaveformDisplay osc1WaveformDisplayRaw;
    WaveformDisplay osc2WaveformDisplayRaw;
    WaveformDisplay osc3WaveformDisplayRaw;
    WaveformDisplay combinedWaveformDisplay;
    
    CustomPanelComponent customPanel;
    
    BlackBackgroundComponent lowerPanel;
    PitchWheelSlider pitchWheelSlider;
    ModWheelSlider modWheelSlider;
    
    juce::OwnedArray<PianoKey> pianoKeys;
    PianoKey* lastDraggedKey = nullptr;
    PianoKey* pressedKey = nullptr;
    juce::Point<int> initialKeyPosition;
    int initialKeyIndex;
    SmokeComponent smokeComponent;

    void createUIComponents();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoogMiniAudioProcessorEditor)
};

