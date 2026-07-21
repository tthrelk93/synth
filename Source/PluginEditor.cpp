#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <unordered_map>
#include "VintageLookAndFeel.h"
#include "WaveformSlider.h"
#include "PianoKey.h"
#include <cmath>
#include <limits>

static_assert(SignalFlowOverlay::waveformSize == MoogMiniAudioProcessor::stageBufferSize,
              "Signal flow overlay buffer size must match processor stage buffer size.");

namespace {
constexpr int kDesignWidth = 1782;
constexpr int kDesignHeight = 1000;
constexpr float kMinScale = 0.5f;
constexpr float kMaxScale = 1.5f;
}

//==============================================================================
MoogMiniAudioProcessorEditor::MoogMiniAudioProcessorEditor (MoogMiniAudioProcessor& p)
: AudioProcessorEditor (&p)
, audioProcessor (p)
, presetManager(p.apvts)
, osc1WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc1Raw)
, osc2WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc2Raw)
, osc3WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc3Raw)
, combinedWaveformDisplay (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::PostFilter)

{
    
    setSize (kDesignWidth, kDesignHeight);
    setResizable(true, true);
    const int minWidth = juce::roundToInt(kDesignWidth * kMinScale);
    const int minHeight = juce::roundToInt(kDesignHeight * kMinScale);
    const int maxWidth = juce::roundToInt(kDesignWidth * kMaxScale);
    const int maxHeight = juce::roundToInt(kDesignHeight * kMaxScale);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    
    customPanel.setBounds(0, 0, kDesignWidth, kDesignHeight);
    addAndMakeVisible(customPanel);
    
    const int gridWidth = 26;
    const int gridHeight = 7;
    const int cellWidth = kDesignWidth / gridWidth;
    const int cellHeight = 700 / gridHeight;
    
    // Set bounds for pitch and mod wheels
    int wheelSize = 200;
   
    customPanel.addAndMakeVisible(pitchWheelSlider);
    customPanel.addAndMakeVisible(modWheelSlider);
    

    
  


    // Make sure the editor can intercept mouse clicks and receive mouse drag events
    setInterceptsMouseClicks(true, true);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    presetLabel.setColour(juce::Label::textColourId,
                          juce::Colour::fromRGB(200, 210, 230).withAlpha(0.9f));
    presetLabel.setBounds(24, 18, 70, 24);
    addAndMakeVisible(presetLabel);

    presetComboBox.setTextWhenNothingSelected("Select Preset");
    presetComboBox.setJustificationType(juce::Justification::centredLeft);
    presetComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(12, 16, 24));
    presetComboBox.setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(210, 220, 245));
    presetComboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(60, 80, 120).withAlpha(0.6f));
    presetComboBox.setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(150, 180, 230));
    presetComboBox.setBounds(96, 16, 200, 28);
    addAndMakeVisible(presetComboBox);

    savePresetButton.setButtonText("Save");
    savePresetButton.setBounds(304, 16, 60, 28);
    savePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 28, 44));
    savePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(210, 220, 245));
    addAndMakeVisible(savePresetButton);

    loadPresetButton.setButtonText("Load");
    loadPresetButton.setBounds(370, 16, 60, 28);
    loadPresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 28, 44));
    loadPresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(210, 220, 245));
    addAndMakeVisible(loadPresetButton);
    
    
    
    labelMap["ctrlTuneKnob"] = &ctrlTuneKnobLabel;
    labelMap["osc1RangeKnob"] = &osc1RangeKnobLabel;
    labelMap["osc1WaveFormKnob"] = &osc1WaveFormKnobLabel;
    labelMap["osc1VolKnob"] = &osc1VolKnobLabel;
    labelMap["filterCutoffFreqKnob"] = &filterCutoffFreqKnobLabel;
    labelMap["filterEmphasisKnob"] = &filterEmphasisKnobLabel;
    labelMap["filterAmtContourKnob"] = &filterAmtContourKnobLabel;
    labelMap["outputVolKnob"] = &outputVolKnobLabel;
    labelMap["extInputVolKnob"] = &extInputVolKnobLabel;
    labelMap["ctrlGlideKnob"] = &ctrlGlideKnobLabel;
    labelMap["ctrlModMixKnob"] = &ctrlModMixKnobLabel;
    labelMap["osc2RangeKnob"] = &osc2RangeKnobLabel;
    labelMap["osc2FreqKnob"] = &osc2FreqKnobLabel;
    labelMap["osc2WaveFormKnob"] = &osc2WaveFormKnobLabel;
    labelMap["osc2VolKnob"] = &osc2VolKnobLabel;
    labelMap["filterAttackTimeKnob"] = &filterAttackTimeKnobLabel;
    labelMap["filterDecayTimeKnob"] = &filterDecayTimeKnobLabel;
    labelMap["filterSustainKnob"] = &filterSustainKnobLabel;
    labelMap["noiseVolKnob"] = &noiseVolKnobLabel;
    labelMap["osc3RangeKnob"] = &osc3RangeKnobLabel;
    labelMap["osc3FreqKnob"] = &osc3FreqKnobLabel;
    labelMap["osc3WaveFormKnob"] = &osc3WaveFormKnobLabel;
    labelMap["osc3VolKnob"] = &osc3VolKnobLabel;
    labelMap["loudnessAttackTimeKnob"] = &loudnessAttackTimeKnobLabel;
    labelMap["loudnessDecayTimeKnob"] = &loudnessDecayTimeKnobLabel;
    labelMap["loudnessSustainLevelKnob"] = &loudnessSustainLevelKnobLabel;
    labelMap["outputPhonesVolKnob"] = &outputPhonesVolKnobLabel;
    labelMap["osc1OnOffSwitch"] = &osc1OnOffSwitchLabel;
    labelMap["filterModSwitch"] = &filterModSwitchLabel;
    labelMap["ouputMainOutputSwitch"] = &ouputMainOutputSwitchLabel;
    labelMap["oscModSwitch"] = &oscModSwitchLabel;
    labelMap["extInputVolSwitch"] = &extInputVolSwitchLabel;
    labelMap["keyboardCtrlSwitch1"] = &keyboardCtrlSwitch1Label;
    labelMap["osc2OnOffSwitch"] = &osc2OnOffSwitchLabel;
    labelMap["keyboardCtrlSwitch2"] = &keyboardCtrlSwitch2Label;
    labelMap["a440hzSwitch"] = &a440hzSwitchLabel;
    labelMap["noiseOnOffSwitch"] = &noiseOnOffSwitchLabel;
    labelMap["whitePinkSwitch"] = &whitePinkSwitchLabel;
    labelMap["osc3CtrlSwitch"] = &osc3CtrlSwitchLabel;
    labelMap["osc3OnOffSwitch"] = &osc3OnOffSwitchLabel;
    labelMap["overloadButton"] = &overloadButtonLabel;
    labelMap["decaySwitch"] = &decaySwitchLabel;
    labelMap["glideSwitch"] = &glideSwitchLabel;
    labelMap["feedbackKnob"] = &feedbackKnobLabel;
    
    
    labelStringMap["ctrlTuneKnob"] = "Tune";
    labelStringMap["osc1RangeKnob"] = "Range";
    labelStringMap["osc1WaveFormKnob"] = "Waveform";
    labelStringMap["osc1VolKnob"] = "Volume";
    labelStringMap["filterCutoffFreqKnob"] = "Cutoff";
    labelStringMap["filterEmphasisKnob"] = "Emphasis";
    labelStringMap["filterAmtContourKnob"] = "Contour";
    labelStringMap["outputVolKnob"] = "Output Vol";
    labelStringMap["extInputVolKnob"] = "Input Vol";
    labelStringMap["ctrlGlideKnob"] = "Glide";
    labelStringMap["ctrlModMixKnob"] = "Mod Mix";
    labelStringMap["osc2RangeKnob"] = "Range";
    labelStringMap["osc2FreqKnob"] = "Freq";
    labelStringMap["osc2WaveFormKnob"] = "Waveform";
    labelStringMap["osc2VolKnob"] = "Volume";
    labelStringMap["filterAttackTimeKnob"] = "Attack";
    labelStringMap["filterDecayTimeKnob"] = "Decay";
    labelStringMap["filterSustainKnob"] = "Sustain";
    labelStringMap["noiseVolKnob"] = "Noise Vol";
    labelStringMap["osc3RangeKnob"] = "Range";
    labelStringMap["osc3FreqKnob"] = "Freq";
    labelStringMap["osc3WaveFormKnob"] = "Waveform";
    labelStringMap["osc3VolKnob"] = "Volume";
    labelStringMap["loudnessAttackTimeKnob"] = "C Attack";
    labelStringMap["loudnessDecayTimeKnob"] = "C Decay";
    labelStringMap["loudnessSustainLevelKnob"] = "C Sustain";
    labelStringMap["outputPhonesVolKnob"] = "Phones Vol";
    labelStringMap["osc1OnOffSwitch"] = "OFF / ON";
    labelStringMap["filterModSwitch"] = "Filter Mod";
    labelStringMap["ouputMainOutputSwitch"] = "Output Main";
    labelStringMap["oscModSwitch"] = "Osc Mod";
    labelStringMap["extInputVolSwitch"] = "External Input";
    labelStringMap["keyboardCtrlSwitch1"] = "Keys Ctrl 1";
    labelStringMap["osc2OnOffSwitch"] = "OFF / ON";
    labelStringMap["keyboardCtrlSwitch2"] = "Keys Ctrl 2";
    labelStringMap["a440hzSwitch"] = "A 440Hz";
    labelStringMap["noiseOnOffSwitch"] = "OFF / ON";
    labelStringMap["whitePinkSwitch"] = "White/Pink";
    labelStringMap["osc3CtrlSwitch"] = "Osc 3 Ctrl";
    labelStringMap["osc3OnOffSwitch"] = "OFF / ON";
    labelStringMap["overloadButton"] = "Overload";
    labelStringMap["decaySwitch"] = "Allow Decay";
    labelStringMap["glideSwitch"] = "Glide";
    labelStringMap["feedbackKnob"] = "Feedback";
    
    
    
    
    nestedMap[2][3] = "osc1RangeKnob";
    nestedMap[2][2] = "ctrlTuneKnob";
    nestedMap[2][1] = "osc1WaveFormKnob";
    nestedMap[2][4] = "osc1VolKnob";
    nestedMap[2][5] = "osc1OnOffSwitch";
    nestedMap[1][13] = "filterModSwitch";
    nestedMap[1][14] = "keyboardCtrlSwitch1";
    nestedMap[1][15] = "keyboardCtrlSwitch2";
    nestedMap[1][17] = "outputVolKnob";
    nestedMap[1][16] = "feedbackKnob";
    //nestedMap[1][19] = "ouputMainOutputSwitch";
    
    nestedMap[6][1] = "oscModSwitch";
    nestedMap[1][5] = "extInputVolSwitch";
    nestedMap[1][4] = "extInputVolKnob";
    nestedMap[2][13] = "filterCutoffFreqKnob";
    nestedMap[2][14] = "filterEmphasisKnob";
    nestedMap[2][15] = "filterAmtContourKnob";
    nestedMap[2][12] = "overloadButton";
    
    
    nestedMap[5][1] = "ctrlGlideKnob";
    nestedMap[5][2] = "ctrlModMixKnob";
    nestedMap[3][3] = "osc2RangeKnob";
    nestedMap[3][2] = "osc2FreqKnob";
    nestedMap[3][1] = "osc2WaveFormKnob";
    nestedMap[3][4] = "osc2VolKnob";
    nestedMap[3][5] = "osc2OnOffSwitch";
    
    nestedMap[3][13] = "filterAttackTimeKnob";
    nestedMap[3][14] = "filterDecayTimeKnob";
    nestedMap[3][15] = "filterSustainKnob";
    
    
    nestedMap[6][2] = "glideSwitch";
    nestedMap[5][14] = "decaySwitch";
    nestedMap[5][5] = "noiseOnOffSwitch";
    nestedMap[5][4] = "noiseVolKnob";
    nestedMap[5][3] = "whitePinkSwitch";
    nestedMap[4][13] = "loudnessAttackTimeKnob";
    nestedMap[4][14] = "loudnessDecayTimeKnob";
    nestedMap[4][15] = "loudnessSustainLevelKnob";
    
    nestedMap[5][1] = "osc3CtrlSwitch";
    nestedMap[4][3] = "osc3RangeKnob";
    nestedMap[4][2] = "osc3FreqKnob";
    nestedMap[4][1] = "osc3WaveFormKnob";
    nestedMap[4][4] = "osc3VolKnob";
    nestedMap[4][5] = "osc3OnOffSwitch";
    nestedMap[5][15] = "a440hzSwitch";
    //nestedMap[5][17] = "outputPhonesVolKnob";
    
    
    // Your UI grid represented as a 2D array
    int uiGrid[gridHeight][gridWidth] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 4, 2, 2, 2, 1, 1, 0, 0},
        {0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0},
        {0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    
    
    int osc1WaveformPos[2];
    int osc2WaveformPos[2];
    int osc3WaveformPos[2];
    int osc1RangePos[2];
    int osc2RangePos[2];
    int osc3RangePos[2];
    int osc2FreqPos[2];
    int osc3FreqPos[2];
    int osc1VolPos[2];
    int osc2VolPos[2];
    int osc3VolPos[2];
    int osc1OnOffPos[2];
    int osc2OnOffPos[2];
    int osc3OnOffPos[2];
    int ctrlTunePos[2];
    int a440OnOffPos[2];
    int filterCutoffPos[2];
    int filterEmphasisPos[2];
    int filterContourPos[2];
    int osc3CtrlPos[2];
    int oscModSwitchPos[2];
    int outputVolKnobPos[2];
    int extInputVolKnobPos[2];
    int ctrlGlideKnobPos[2];
    int ctrlModMixKnobPos[2];
    int filterAttackTimeKnobPos[2];
    int filterDecayTimeKnobPos[2];
    int filterSustainKnobPos[2];
    int noiseVolKnobPos[2];
    int loudnessAttackTimeKnobPos[2];
    int loudnessDecayTimeKnobPos[2];
    int loudnessSustainLevelKnobPos[2];
    int decaySwitchPos[2];
    int glideSwitchPos[2];
    int extInputVolSwitchPos[2];
    int keyboardCtrlSwitch1Pos[2];
    int keyboardCtrlSwitch2Pos[2];
    int noiseOnOffSwitchPos[2];
    int whitePinkSwitchPos[2];
    int overloadButtonPos[2] = {};
    int filterModSwitchPos[2];
    
    
    
    // Create and position components based on the diagram
    for (int row = 0; row < gridHeight; ++row) {
        for (int col = 0; col < gridWidth; ++col) {
            switch (uiGrid[row][col]) {
                case 1: // Rotary Knob
                {
                    std::string sliderKey = nestedMap[row][col];
                    
                    
                    if(sliderKey == "osc1WaveFormKnob"){
                        osc1WaveformPos[0] = row;
                        osc1WaveformPos[1] = col;
                    } else if(sliderKey == "osc2WaveFormKnob"){
                        osc2WaveformPos[0] = row;
                        osc2WaveformPos[1] = col;
                    } else if(sliderKey == "osc3WaveFormKnob"){
                        osc3WaveformPos[0] = row;
                        osc3WaveformPos[1] = col;
                    } else if(sliderKey == "osc1RangeKnob") {
                        osc1RangePos[0] = row;
                        osc1RangePos[1] = col;
                    }  else if(sliderKey == "osc2RangeKnob") {
                        osc2RangePos[0] = row;
                        osc2RangePos[1] = col;
                    }  else if(sliderKey == "osc3RangeKnob") {
                        osc3RangePos[0] = row;
                        osc3RangePos[1] = col;
                    }  else if(sliderKey == "osc2FreqKnob") {
                        osc2FreqPos[0] = row;
                        osc2FreqPos[1] = col;
                    }  else if(sliderKey == "osc3FreqKnob") {
                        osc3FreqPos[0] = row;
                        osc3FreqPos[1] = col;
                    } else if(sliderKey == "osc1VolKnob") {
                        osc1VolPos[0] = row;
                        osc1VolPos[1] = col;
                    }  else if(sliderKey == "osc2VolKnob") {
                        osc2VolPos[0] = row;
                        osc2VolPos[1] = col;
                    }  else if(sliderKey == "osc3VolKnob") {
                        osc3VolPos[0] = row;
                        osc3VolPos[1] = col;
                    } else if(sliderKey == "ctrlTuneKnob") {
                        ctrlTunePos[0] = row;
                        ctrlTunePos[1] = col;
                    } else if(sliderKey == "filterCutoffFreqKnob") {
                        filterCutoffPos[0] = row;
                        filterCutoffPos[1] = col;
                    } else if(sliderKey == "filterAmtContourKnob") {
                        filterContourPos[0] = row;
                        filterContourPos[1] = col;
                    } else if(sliderKey == "filterEmphasisKnob") {
                        filterEmphasisPos[0] = row;
                        filterEmphasisPos[1] = col;
                    } else if(sliderKey == "outputVolKnob") {
                        outputVolKnobPos[0] = row;
                        outputVolKnobPos[1] = col;
                    } else if(sliderKey == "extInputVolKnob") {
                        extInputVolKnobPos[0] = row;
                        extInputVolKnobPos[1] = col;
                    } else if(sliderKey == "ctrlGlideKnob") {
                        ctrlGlideKnobPos[0] = row;
                        ctrlGlideKnobPos[1] = col;
                    } else if(sliderKey == "ctrlModMixKnob") {
                        ctrlModMixKnobPos[0] = row;
                        ctrlModMixKnobPos[1] = col;
                    } else if(sliderKey == "filterAttackTimeKnob") {
                        filterAttackTimeKnobPos[0] = row;
                        filterAttackTimeKnobPos[1] = col;
                    } else if(sliderKey == "filterDecayTimeKnob") {
                        filterDecayTimeKnobPos[0] = row;
                        filterDecayTimeKnobPos[1] = col;
                    } else if(sliderKey == "filterSustainKnob") {
                        filterSustainKnobPos[0] = row;
                        filterSustainKnobPos[1] = col;
                    } else if(sliderKey == "noiseVolKnob") {
                        noiseVolKnobPos[0] = row;
                        noiseVolKnobPos[1] = col;
                    } else if(sliderKey == "loudnessAttackTimeKnob") {
                        loudnessAttackTimeKnobPos[0] = row;
                        loudnessAttackTimeKnobPos[1] = col;
                    } else if(sliderKey == "loudnessDecayTimeKnob") {
                        loudnessDecayTimeKnobPos[0] = row;
                        loudnessDecayTimeKnobPos[1] = col;
                    } else if(sliderKey == "loudnessSustainLevelKnob") {
                        loudnessSustainLevelKnobPos[0] = row;
                        loudnessSustainLevelKnobPos[1] = col;
                    }
                    
                    
                    
                    
                }
                    break;
                case 2: // Horizontal Toggle
                {
                    std::string toggleKey = nestedMap[row][col];
                    
                    if(toggleKey == "osc1OnOffSwitch"){
                        osc1OnOffPos[0] = row;
                        osc1OnOffPos[1] = col;
                    } else if(toggleKey == "osc2OnOffSwitch"){
                        osc2OnOffPos[0] = row;
                        osc2OnOffPos[1] = col;
                    } else if(toggleKey == "osc3OnOffSwitch"){
                        osc3OnOffPos[0] = row;
                        osc3OnOffPos[1] = col;
                    } else if(toggleKey == "a440hzSwitch"){
                        a440OnOffPos[0] = row;
                        a440OnOffPos[1] = col;
                    } else if(toggleKey == "oscModSwitch"){
                        oscModSwitchPos[0] = row;
                        oscModSwitchPos[1] = col;
                    } else if(toggleKey == "keyboardCtrlSwitch1") {
                        keyboardCtrlSwitch1Pos[0] = row;
                        keyboardCtrlSwitch1Pos[1] = col;
                    } else if(toggleKey == "keyboardCtrlSwitch2") {
                        keyboardCtrlSwitch2Pos[0] = row;
                        keyboardCtrlSwitch2Pos[1] = col;
                    } else if(toggleKey == "noiseOnOffSwitch") {
                        noiseOnOffSwitchPos[0] = row;
                        noiseOnOffSwitchPos[1] = col;
                    } else if(toggleKey == "filterModSwitch") {
                        filterModSwitchPos[0] = row;
                        filterModSwitchPos[1] = col;
                    } else if(toggleKey == "decaySwitch") {
                        decaySwitchPos[0] = row;
                        decaySwitchPos[1] = col;
                    } else if(toggleKey == "glideSwitch") {
                        glideSwitchPos[0] = row;
                        glideSwitchPos[1] = col;
                    } else if(toggleKey == "extInputVolSwitch") {
                        extInputVolSwitchPos[0] = row;
                        extInputVolSwitchPos[1] = col;
                    }
                    
                }
                    break;
                case 3: // Vertical Toggle
                {
                    std::string toggleKey = nestedMap[row][col];
                    
                    if(toggleKey == "osc3CtrlSwitch"){
                        osc3CtrlPos[0] = row;
                        osc3CtrlPos[1] = col;
                    } else if(toggleKey == "whitePinkSwitch") {
                        
                        whitePinkSwitchPos[0] = row;
                        whitePinkSwitchPos[1] = col;
                    }
                }
                    break;
                case 4: // Push Button
                {
                    std::string buttonKey = nestedMap[row][col];
                    juce::Logger::writeToLog("in case 4: " + buttonKey);
                    
                    if(buttonKey == "overloadButton") {
                        overloadButtonPos[0] = row;
                        overloadButtonPos[1] = col;
                    }
                    
                }
                    break;
                default:
                    // No component for this cell
                    break;
            }
        }
    }
    
    addAndMakeVisible(lowerPanel);
    lowerPanel.setBounds(static_cast<int>((static_cast<double>(getWidth()) / 2.5) + 18.0),
                         static_cast<int>(static_cast<double>(getHeight()) / 6.3),
                         300,
                         320);
    
    addAndMakeVisible(osc1WaveformDisplayRaw);
    addAndMakeVisible(osc2WaveformDisplayRaw);
    addAndMakeVisible(osc3WaveformDisplayRaw);
    addAndMakeVisible(combinedWaveformDisplay);
    
   
    
    
    createSliderKnob(osc1WaveFormKnob, "osc1WaveFormKnob", 6, 0, 5, 1, "osc1Waveform", false, osc1WaveformPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc2WaveFormKnob, "osc2WaveFormKnob", 6, 0, 5, 1, "osc2Waveform", false, osc2WaveformPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc3WaveFormKnob, "osc3WaveFormKnob", 6, 0, 5, 1, "osc3Waveform", false, osc3WaveformPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc1RangeKnob, "osc1RangeKnob", 6, 0, 5, 1, "osc1Range", false, osc1RangePos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc2RangeKnob, "osc2RangeKnob", 6, 0, 5, 1, "osc2Range", false, osc2RangePos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc3RangeKnob, "osc3RangeKnob", 6, 0, 5, 1, "osc3Range", false, osc3RangePos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc2FreqKnob, "osc2FreqKnob", 17, -8, 8, 1.0, "osc2Freq", true, osc2FreqPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc3FreqKnob, "osc3FreqKnob", 17, -8, 8, 1.0, "osc3Freq", true, osc3FreqPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc1VolKnob, "osc1VolKnob", 11, 0, 10, 1.0, "osc1Vol", false, osc1VolPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc2VolKnob, "osc2VolKnob", 11, 0, 10, 1.0, "osc2Vol", false, osc2VolPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc3VolKnob, "osc3VolKnob", 11, 0, 10, 1.0, "osc3Vol", false, osc3VolPos, cellWidth, cellHeight, false);
    
    createSliderKnob(ctrlTuneKnob, "ctrlTuneKnob", 11, -2.5, 2.5, 0.5, "tune", true, ctrlTunePos, cellWidth, cellHeight, false);
    
    createSliderKnob(filterCutoffFreqKnob, "filterCutoffFreqKnob", 11, -5, 5, 1.0, "filterCutoff", true, filterCutoffPos, cellWidth, cellHeight, false);
    
    createSliderKnob(filterEmphasisKnob, "filterEmphasisKnob", 11, 0, 10, 1.0, "filterEmphasis", true, filterEmphasisPos, cellWidth, cellHeight, false);
    
    createSliderKnob(filterAmtContourKnob, "filterAmtContourKnob", 11, 0, 10, 1.0, "filterContour", true, filterContourPos, cellWidth, cellHeight, false);
    
    createSliderKnob(outputVolKnob, "outputVolKnob", 11, 0, 10, 1.0, "outputVolKnob", true, outputVolKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(extInputVolKnob, "extInputVolKnob", 11, 0, 10, 1.0, "extInputVolKnob", true, extInputVolKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(ctrlModMixKnob, "ctrlModMixKnob", 11, 0, 10, 1.0, "ctrlModMixKnob", true, ctrlModMixKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(ctrlGlideKnob, "ctrlGlideKnob", 11, 0, 10, 1.0, "ctrlGlideKnob", true, ctrlGlideKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(filterAttackTimeKnob, "filterAttackTimeKnob", 1, 10, 10000, 0.01f, "filterAttackTimeKnob", true, filterAttackTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(filterDecayTimeKnob, "filterDecayTimeKnob", 1, 10, 10000, 0.01f, "filterDecayTimeKnob", true, filterDecayTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(filterSustainKnob, "filterSustainKnob", 11, 0, 10, 1.0, "filterSustainKnob", true, filterSustainKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(noiseVolKnob, "noiseVolKnob", 11, 0, 10, 1.0, "noiseVolKnob", true, noiseVolKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(loudnessAttackTimeKnob, "loudnessAttackTimeKnob", 1, 10, 10000, 0.01f, "loudnessAttackTimeKnob", true, loudnessAttackTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(loudnessDecayTimeKnob, "loudnessDecayTimeKnob", 1, 10, 10000, 0.01f, "loudnessDecayTimeKnob", true, loudnessDecayTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(loudnessSustainLevelKnob, "loudnessSustainLevelKnob", 11, 0, 10, 1.0, "loudnessSustainLevelKnob", true, loudnessSustainLevelKnobPos, cellWidth, cellHeight, false);
    
    // The current panel intentionally omits the Phones Vol control (its grid
    // entry is disabled above); keep the pointer null rather than positioning
    // it from an uninitialised stack coordinate.

    //createSliderKnob(feedbackKnob, "feedbackKnob", 11, 0, 10, 1.0, "feedbackKnob", true, feedbackKnobPos, cellWidth, cellHeight, false);

    auto configureToggleSlider = [&](WaveformSlider* slider, const juce::String& paramId, bool drivesSmoke) {
        if (slider == nullptr) {
            return;
        }
        slider->setToggleEnabled(true);
        const bool isOn = audioProcessor.apvts.getRawParameterValue(paramId)->load() > 0.5f;
        slider->setToggleActive(isOn);
        if (drivesSmoke) {
            smokeComponent.setVisible(isOn);
        }
        slider->setToggleCallback([this, paramId, slider, drivesSmoke](bool nextState) {
            audioProcessor.apvts.getParameter(paramId)->setValueNotifyingHost(nextState ? 1.0f : 0.0f);
            slider->setToggleActive(nextState);
            if (drivesSmoke) {
                smokeComponent.setVisible(nextState);
            }
        });
    };

    configureToggleSlider(osc1WaveFormKnob, "osc1OnOff", false);
    configureToggleSlider(osc2WaveFormKnob, "osc2OnOff", false);
    configureToggleSlider(osc3WaveFormKnob, "osc3OnOff", false);
    configureToggleSlider(noiseVolKnob, "noiseOnOffSwitch", true);
    configureToggleSlider(extInputVolKnob, "extInputVolSwitch", false);
    
    createToggleSwitch(osc1OnOffSwitch, "osc1OnOffSwitch", "osc1OnOff", osc1OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc2OnOffSwitch, "osc2OnOffSwitch", "osc2OnOff", osc2OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc3OnOffSwitch, "osc3OnOffSwitch", "osc3OnOff", osc3OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(a440hzSwitch, "a440hzSwitch", "a440HzOnOff", a440OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc3CtrlSwitch, "osc3CtrlSwitch", "osc3CtrlMode", osc3CtrlPos, false, cellWidth, cellHeight);
    
    createToggleSwitch(oscModSwitch, "oscModSwitch", "oscModSwitch", oscModSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(noiseOnOffSwitch, "noiseOnOffSwitch", "noiseOnOffSwitch", noiseOnOffSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(extInputVolSwitch, "extInputVolSwitch", "extInputVolSwitch", extInputVolSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(whitePinkSwitch, "whitePinkSwitch", "whitePinkSwitch", whitePinkSwitchPos, false, cellWidth, cellHeight);
    
    createToggleSwitch(filterModSwitch, "filterModSwitch", "filterModSwitch", filterModSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(keyboardCtrlSwitch1, "keyboardCtrlSwitch1", "keyboardCtrlSwitch1", keyboardCtrlSwitch1Pos, true, cellWidth, cellHeight);
    
    createToggleSwitch(keyboardCtrlSwitch2, "keyboardCtrlSwitch2", "keyboardCtrlSwitch2", keyboardCtrlSwitch2Pos, true, cellWidth, cellHeight);
    
    createToggleSwitch(decaySwitch, "decaySwitch", "decaySwitch", decaySwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(glideSwitch, "glideSwitch", "glideSwitch", glideSwitchPos, true, cellWidth, cellHeight);

    auto hideToggle = [&](juce::ToggleButton& toggle, juce::Label& label) {
        toggle.setVisible(false);
        toggle.setEnabled(false);
        label.setVisible(false);
    };

    auto hideSlider = [&](WaveformSlider* slider, juce::Label& label) {
        if (slider == nullptr) {
            return;
        }
        slider->setVisible(false);
        slider->setEnabled(false);
        label.setVisible(false);
    };

    hideToggle(osc1OnOffSwitch, osc1OnOffSwitchLabel);
    hideToggle(osc2OnOffSwitch, osc2OnOffSwitchLabel);
    hideToggle(osc3OnOffSwitch, osc3OnOffSwitchLabel);
    hideToggle(noiseOnOffSwitch, noiseOnOffSwitchLabel);
    hideToggle(extInputVolSwitch, extInputVolSwitchLabel);

    hideSlider(filterCutoffFreqKnob, filterCutoffFreqKnobLabel);
    hideSlider(filterEmphasisKnob, filterEmphasisKnobLabel);
    hideSlider(filterAmtContourKnob, filterAmtContourKnobLabel);
    hideSlider(filterAttackTimeKnob, filterAttackTimeKnobLabel);
    hideSlider(filterDecayTimeKnob, filterDecayTimeKnobLabel);
    hideSlider(filterSustainKnob, filterSustainKnobLabel);
    hideSlider(loudnessAttackTimeKnob, loudnessAttackTimeKnobLabel);
    hideSlider(loudnessDecayTimeKnob, loudnessDecayTimeKnobLabel);
    hideSlider(loudnessSustainLevelKnob, loudnessSustainLevelKnobLabel);

    const float rightPanelAnchorX = static_cast<float>(lowerPanel.getRight());
    const float rightPanelScale = 2.0f / 3.0f;
    auto compressRightComponent = [&](juce::Component* component) {
        if (component == nullptr) {
            return 0;
        }
        auto bounds = component->getBounds();
        if (bounds.getX() <= rightPanelAnchorX) {
            return 0;
        }
        const float offset = static_cast<float>(bounds.getX()) - rightPanelAnchorX;
        const int newX = juce::roundToInt(rightPanelAnchorX + offset * rightPanelScale);
        const int deltaX = newX - bounds.getX();
        component->setBounds(newX, bounds.getY(), bounds.getWidth(), bounds.getHeight());
        return deltaX;
    };
    auto compressRightWithLabel = [&](juce::Component* component, juce::Label& label) {
        const int deltaX = compressRightComponent(component);
        if (deltaX != 0 && label.isVisible()) {
            label.setBounds(label.getX() + deltaX, label.getY(), label.getWidth(), label.getHeight());
        }
    };

    compressRightWithLabel(outputVolKnob, outputVolKnobLabel);
    compressRightWithLabel(outputPhonesVolKnob, outputPhonesVolKnobLabel);

    auto shiftComponentX = [](juce::Component* component, int deltaX) {
        if (component == nullptr || deltaX == 0) {
            return;
        }
        component->setBounds(component->getX() + deltaX,
                             component->getY(),
                             component->getWidth(),
                             component->getHeight());
    };
    auto shiftLabelX = [](juce::Label& label, int deltaX) {
        if (!label.isVisible() || deltaX == 0) {
            return;
        }
        label.setBounds(label.getX() + deltaX,
                        label.getY(),
                        label.getWidth(),
                        label.getHeight());
    };

    juce::Rectangle<int> rightPanelBounds;
    bool hasRightPanelBounds = false;
    auto addRightBounds = [&](juce::Component* component) {
        if (component == nullptr) {
            return;
        }
        auto bounds = component->getBounds();
        if (!hasRightPanelBounds) {
            rightPanelBounds = bounds;
            hasRightPanelBounds = true;
        } else {
            rightPanelBounds = rightPanelBounds.getUnion(bounds);
        }
    };

    addRightBounds(&lowerPanel);
    addRightBounds(filterCutoffFreqKnob);
    addRightBounds(filterEmphasisKnob);
    addRightBounds(filterAmtContourKnob);
    addRightBounds(filterAttackTimeKnob);
    addRightBounds(filterDecayTimeKnob);
    addRightBounds(filterSustainKnob);
    addRightBounds(loudnessAttackTimeKnob);
    addRightBounds(loudnessDecayTimeKnob);
    addRightBounds(loudnessSustainLevelKnob);
    addRightBounds(outputVolKnob);
    addRightBounds(outputPhonesVolKnob);
    addRightBounds(&filterModSwitch);
    addRightBounds(&keyboardCtrlSwitch1);
    addRightBounds(&keyboardCtrlSwitch2);
    addRightBounds(&decaySwitch);
    addRightBounds(&a440hzSwitch);

    if (hasRightPanelBounds) {
        const int rightMargin = 30;
        const int targetRight = getWidth() - rightMargin;
        const int deltaX = targetRight - rightPanelBounds.getRight();
        if (deltaX > 0) {
            shiftComponentX(&lowerPanel, deltaX);
            shiftComponentX(filterCutoffFreqKnob, deltaX);
            shiftComponentX(filterEmphasisKnob, deltaX);
            shiftComponentX(filterAmtContourKnob, deltaX);
            shiftComponentX(filterAttackTimeKnob, deltaX);
            shiftComponentX(filterDecayTimeKnob, deltaX);
            shiftComponentX(filterSustainKnob, deltaX);
            shiftComponentX(loudnessAttackTimeKnob, deltaX);
            shiftComponentX(loudnessDecayTimeKnob, deltaX);
            shiftComponentX(loudnessSustainLevelKnob, deltaX);
            shiftComponentX(outputVolKnob, deltaX);
            shiftComponentX(outputPhonesVolKnob, deltaX);
            shiftComponentX(&filterModSwitch, deltaX);
            shiftComponentX(&keyboardCtrlSwitch1, deltaX);
            shiftComponentX(&keyboardCtrlSwitch2, deltaX);
            shiftComponentX(&decaySwitch, deltaX);
            shiftComponentX(&a440hzSwitch, deltaX);

            shiftLabelX(filterCutoffFreqKnobLabel, deltaX);
            shiftLabelX(filterEmphasisKnobLabel, deltaX);
            shiftLabelX(filterAmtContourKnobLabel, deltaX);
            shiftLabelX(filterAttackTimeKnobLabel, deltaX);
            shiftLabelX(filterDecayTimeKnobLabel, deltaX);
            shiftLabelX(filterSustainKnobLabel, deltaX);
            shiftLabelX(loudnessAttackTimeKnobLabel, deltaX);
            shiftLabelX(loudnessDecayTimeKnobLabel, deltaX);
            shiftLabelX(loudnessSustainLevelKnobLabel, deltaX);
            shiftLabelX(outputVolKnobLabel, deltaX);
            shiftLabelX(outputPhonesVolKnobLabel, deltaX);
        }
    }

    if (outputVolKnob != nullptr) {
        auto panelBounds = lowerPanel.getBounds().toFloat();
        if (!panelBounds.isEmpty()) {
            auto knobBounds = outputVolKnob->getBounds();
            const float filterOutX = panelBounds.getRight()
                + SignalFlowOverlay::filterScopeGap
                + SignalFlowOverlay::filterScopeWidth * 0.5f;
            const float knobGap = 12.0f;
            const float targetLeft = filterOutX + SignalFlowOverlay::filterScopeWidth * 0.5f + knobGap;
            const float targetCenterY = panelBounds.getCentreY();
            const int newX = juce::roundToInt(targetLeft);
            const int newY = juce::roundToInt(targetCenterY - knobBounds.getHeight() * 0.5f);
            const int deltaX = newX - knobBounds.getX();
            const int deltaY = newY - knobBounds.getY();
            outputVolKnob->setBounds(newX, newY, knobBounds.getWidth(), knobBounds.getHeight());
            if ((deltaX != 0 || deltaY != 0) && outputVolKnobLabel.isVisible()) {
                outputVolKnobLabel.setBounds(outputVolKnobLabel.getX() + deltaX,
                                             outputVolKnobLabel.getY() + deltaY,
                                             outputVolKnobLabel.getWidth(),
                                             outputVolKnobLabel.getHeight());
            }
        }
    }

    
    auto* button = buttonMap["overloadButton"];
    auto* label = labelMap["overloadButton"]; // Get the reference to the label from the map
    if (button) { // Always check if the pointer is not null
        
        button->setBounds(overloadButtonPos[1] * cellWidth, overloadButtonPos[0] * cellHeight, 50, 50);
        addAndMakeVisible(button);
        buttons.add(button);
        if(label){
            // Set up the label for the knob
            label->setText(labelStringMap["overloadButton"], juce::dontSendNotification);
            label->attachToComponent(button, false);
            label->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(label);
        }
    }
    
   
    osc1WaveformDisplayRaw.setBounds(osc1OnOffSwitch.getX() + osc1OnOffSwitch.getWidth(), osc1OnOffSwitch.getY() - 30, filterCutoffFreqKnob->getX() - osc1OnOffSwitch.getX() - 100, 120);
    osc2WaveformDisplayRaw.setBounds(osc2OnOffSwitch.getX() + osc2OnOffSwitch.getWidth(), osc2OnOffSwitch.getY() - 30, filterCutoffFreqKnob->getX() - osc2OnOffSwitch.getX() - 100, 120);
    osc3WaveformDisplayRaw.setBounds(osc3OnOffSwitch.getX() + osc3OnOffSwitch.getWidth(), osc3OnOffSwitch.getY() - 30, filterCutoffFreqKnob->getX() - osc3OnOffSwitch.getX() - 100, 120);
    combinedWaveformDisplay.setBounds(filterSustainKnob->getX() + filterSustainKnob->getWidth() + 31, filterSustainKnob->getY() - 20, 435, 120);
    osc1WaveformDisplayRaw.setVisible(false);
    osc2WaveformDisplayRaw.setVisible(false);
    osc3WaveformDisplayRaw.setVisible(false);
    combinedWaveformDisplay.setVisible(false);

    pitchWheelSlider.setBounds(ctrlGlideKnob->getX() + ctrlGlideKnob->getWidth() - 7 , ctrlGlideKnob->getY() - 55, wheelSize/2, wheelSize);
    pitchWheelSlider.addListener(this);

    modWheelSlider.setBounds(pitchWheelSlider.getRight() - 15,ctrlGlideKnob->getY() - 55, wheelSize/2, wheelSize);
    modWheelSlider.addListener(this);

    const float whiteKeyWidth = 43.6f;
    const float blackKeyWidth = whiteKeyWidth / 1.7f;
    const int blackKeyHeight = static_cast<int>((300 * 0.6f) - 10.0f); // 60% of the panel height
    const int startX = modWheelSlider.getRight() + 20; // Start right next to the mod wheel sliders
    
    int midiNoteNumber = 53; // MIDI note number for F3
    int whiteKeyIndex = 0; // Index for positioning white keys
    int blackKeyIndex = 0; // Index for positioning black keys
    int xPosOfLastWhiteKey = 0;
   
    int blackKeyPosArr[18];
    // First loop: Add all white keys
    for (int i = 0; i < 44; ++i) {
        bool isBlackKey = midiNoteNumber % 12 == 1 || midiNoteNumber % 12 == 3 || midiNoteNumber % 12 == 6 || midiNoteNumber % 12 == 8 || midiNoteNumber % 12 == 10;
        if (!isBlackKey) {
            // Create a white key
            PianoKey* whiteKey = new PianoKey(false, midiNoteNumber, this);
            pianoKeys.add(whiteKey);
            whiteKey->setBounds(static_cast<int>(static_cast<float>(startX) + (static_cast<float>(whiteKeyIndex) * whiteKeyWidth)),
                                700 + 5,
                                static_cast<int>(whiteKeyWidth),
                                300 - 15);
            xPosOfLastWhiteKey = whiteKey->getX();
            customPanel.addAndMakeVisible(whiteKey);
            whiteKeyIndex++;
        } else {
            blackKeyPosArr[blackKeyIndex] = xPosOfLastWhiteKey;
            blackKeyIndex++;
        }
        midiNoteNumber++;
    }
    blackKeyIndex = 0;

    // Reset MIDI note number for black keys
    midiNoteNumber = 53;

    // Second loop: Add all black keys
    for (int i = 0; i < 44; ++i) {
        bool isBlackKey = midiNoteNumber % 12 == 1 || midiNoteNumber % 12 == 3 || midiNoteNumber % 12 == 6 || midiNoteNumber % 12 == 8 || midiNoteNumber % 12 == 10;
        if (isBlackKey) {
            // Create a black key
            PianoKey* blackKey = new PianoKey(true, midiNoteNumber, this);
            pianoKeys.add(blackKey);
            if(blackKeyIndex == 0 || blackKeyIndex == 3 || blackKeyIndex == 5 || blackKeyIndex == 8 || blackKeyIndex == 10 || blackKeyIndex == 13 || blackKeyIndex == 15) {
                blackKey->setBounds(static_cast<int>(static_cast<float>(blackKeyPosArr[blackKeyIndex]) + (whiteKeyWidth - blackKeyWidth / 4.0f) - blackKeyWidth / 2.0f),
                                    700 + 5,
                                    static_cast<int>(blackKeyWidth),
                                    blackKeyHeight);
            } else if(blackKeyIndex == 2 || blackKeyIndex == 4 || blackKeyIndex == 7 || blackKeyIndex == 9 || blackKeyIndex == 12 || blackKeyIndex == 14 || blackKeyIndex == 17) {
                blackKey->setBounds(static_cast<int>(static_cast<float>(blackKeyPosArr[blackKeyIndex]) + (whiteKeyWidth + blackKeyWidth / 4.0f) - blackKeyWidth / 2.0f),
                                    700 + 5,
                                    static_cast<int>(blackKeyWidth),
                                    blackKeyHeight);
            } else {
                blackKey->setBounds(static_cast<int>(static_cast<float>(blackKeyPosArr[blackKeyIndex]) + whiteKeyWidth - blackKeyWidth / 2.0f),
                                    700 + 5,
                                    static_cast<int>(blackKeyWidth),
                                    blackKeyHeight);
            }
            
            customPanel.addAndMakeVisible(blackKey);
            blackKeyIndex++;
        }
        midiNoteNumber++;
    }
    addAndMakeVisible(signalFlowOverlay);
    signalFlowOverlay.setBounds(getLocalBounds());
    signalFlowOverlay.setInterceptsMouseClicks(true, true);
    {
        auto setParam = [this](const juce::String& paramId, float value) {
            if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
                param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, value));
            }
        };
        auto setChoiceParam = [setParam](const juce::String& paramId, float value) {
            const float stepped = std::round(juce::jlimit(0.0f, 1.0f, value) * 10.0f) / 10.0f;
            setParam(paramId, stepped);
        };

        SignalFlowOverlay::FilterVizCallbacks callbacks;
        callbacks.setCutoff = [setParam](float value) { setParam("filterCutoff", value); };
        callbacks.setResonance = [setChoiceParam](float value) { setChoiceParam("filterEmphasis", value); };
        callbacks.setContour = [setChoiceParam](float value) { setChoiceParam("filterContour", value); };
        callbacks.setContourAttack = [setParam](float value) { setParam("loudnessAttackTimeKnob", value); };
        callbacks.setContourDecay = [setParam](float value) { setParam("loudnessDecayTimeKnob", value); };
        callbacks.setContourSustain = [setChoiceParam](float value) { setChoiceParam("loudnessSustainLevelKnob", value); };
        callbacks.setLoudnessAttack = [setParam](float value) { setParam("filterAttackTimeKnob", value); };
        callbacks.setLoudnessDecay = [setParam](float value) { setParam("filterDecayTimeKnob", value); };
        callbacks.setLoudnessSustain = [setChoiceParam](float value) { setChoiceParam("filterSustainKnob", value); };
    signalFlowOverlay.setFilterVizCallbacks(callbacks);
    }
    signalFlowOverlay.toFront(false);
    updateSignalFlowOverlayLayout();

    savePresetButton.onClick = [this] { promptSavePreset(); };
    loadPresetButton.onClick = [this] { loadSelectedPreset(); };
    refreshPresetList();
    if (audioProcessor.shouldAutoLoadLastPreset()) {
        const auto lastPreset = presetManager.getLastPresetName();
        if (lastPreset.isNotEmpty() && presetManager.loadPreset(lastPreset)) {
            const int presetIndex = findPresetIndex(lastPreset);
            if (presetIndex >= 0) {
                presetComboBox.setSelectedItemIndex(presetIndex, juce::dontSendNotification);
            }
        }
    }
    
    startTimerHz(60); // repaint at 60 Hz
    resized();
}

void MoogMiniAudioProcessorEditor::createSliderKnob(WaveformSlider*& sliderKnob, std::string sliderKey, int numPositions, float minPosVal, float maxPosVal, float increment, std::string paramName, bool useCustomRange, int posArray[], int cellWidth, int cellHeight, bool isTimeKnob){
    const double adjustedWidth = static_cast<double>(cellWidth) * 0.94;
    const auto toPixel = [](double coordinate) { return static_cast<int>(coordinate); };
    const int oscColumnSpacing = 72;
    bool isOscWaveform = (sliderKey == "osc1WaveFormKnob") || (sliderKey == "osc2WaveFormKnob") || (sliderKey == "osc3WaveFormKnob");
   
    if(useCustomRange){
        sliderKnob = new WaveformSlider(numPositions, useCustomRange, increment, minPosVal, maxPosVal, isTimeKnob, isOscWaveform, sliderKey);
    } else {
        sliderKnob = new WaveformSlider(numPositions, useCustomRange, isOscWaveform, sliderKey);
    }
    waveformSliders.add(sliderKnob); // Take ownership
    sliderKnob->setRange(minPosVal, maxPosVal, increment);
    if (auto* parameter = audioProcessor.apvts.getParameter(paramName)) {
        sliderKnob->setValue(getSliderValueFromNormalized(sliderKnob, parameter->getValue()),
                             juce::dontSendNotification);
    } else {
        jassertfalse;
    }
    sliderKnob->addListener(this);
    sliderKnob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);

    sliderKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 100, 20);
    if(sliderKey == "osc1VolKnob" || sliderKey == "osc2VolKnob" || sliderKey == "osc3VolKnob" || sliderKey == "extInputVolKnob" || sliderKey == "noiseVolKnob"){
        int volumeGap = 12;
        if (sliderKey == "osc1VolKnob" || sliderKey == "osc2VolKnob" || sliderKey == "osc3VolKnob" || sliderKey == "noiseVolKnob" || sliderKey == "extInputVolKnob") {
            volumeGap = 108;
        }
        sliderKnob->setBounds(osc1RangeKnob->getX() + osc1RangeKnob->getWidth() + volumeGap, (posArray[0] * cellHeight), 70, 70); // Set the bounds of the slider
    } else if( sliderKey == "osc1FreqKnob" || sliderKey == "osc2FreqKnob" || sliderKey == "osc3FreqKnob" || sliderKey == "ctrlTuneKnob" ){
        const int oscOffset = posArray[1] * oscColumnSpacing;
        sliderKnob->setBounds(toPixel(((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth) - 10.0 + oscOffset), posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "osc1WaveFormKnob" || sliderKey == "osc2WaveFormKnob" || sliderKey == "osc3WaveFormKnob"){
        const int oscOffset = posArray[1] * oscColumnSpacing;
        sliderKnob->setBounds(toPixel(((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth) - 12.0 + oscOffset), (posArray[0] * cellHeight), 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "osc1RangeKnob" || sliderKey == "osc2RangeKnob"  || sliderKey == "osc3RangeKnob"){
        const int oscOffset = posArray[1] * oscColumnSpacing;
        sliderKnob->setBounds(toPixel(((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth) - 13.0 + oscOffset), posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "extInputVolKnob" || sliderKey == "feedbackKnob" ){
        sliderKnob->setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) + 30.0) - adjustedWidth), (posArray[0] * cellHeight) + 15 , 40, 40); // Set the bounds of the slider
    } else if(sliderKey == "ctrlModMixKnob"){
        sliderKnob->setBounds(osc3WaveFormKnob->getX(), toPixel(osc3WaveFormKnob->getY() + (osc3WaveFormKnob->getHeight() * 4.5) + 110.0), 60, 60); // Set the bounds of the slider
    } else if(sliderKey == "ctrlGlideKnob"){
        sliderKnob->setBounds(ctrlModMixKnob->getX() + ctrlModMixKnob->getWidth() + 15, toPixel(osc3WaveFormKnob->getY() + (osc3WaveFormKnob->getHeight() * 4.5) + 110.0), 60, 60); // Set the bounds of the slider
    } else if(sliderKey == "filterCutoffFreqKnob" || sliderKey == "filterEmphasisKnob" || sliderKey == "filterAmtContourKnob" || sliderKey == "filterAttackTimeKnob" || sliderKey == "filterDecayTimeKnob" || sliderKey == "filterSustainKnob" || sliderKey == "loudnessAttackTimeKnob" || sliderKey == "loudnessDecayTimeKnob" || sliderKey == "loudnessSustainLevelKnob"){
        sliderKnob->setBounds(toPixel(((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth) - 190.0), posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "outputVolKnob"){
        sliderKnob->setBounds(toPixel((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth + 100.0), posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else {
        sliderKnob->setBounds(toPixel((posArray[1] * (adjustedWidth + 15.0)) - adjustedWidth), posArray[0] * cellHeight, 60, 60); // Set the bounds of the slider
    }
    if(sliderKey == "noiseVolKnob"){
        
        addAndMakeVisible(smokeComponent);
        smokeComponent.setIsPink(false);

        auto sliderBounds = noiseVolKnob->getBounds();
        // Adjust bounds as necessary to fit the smoke within the slider's area
        smokeComponent.setBounds(sliderBounds.reduced(10)); // Example reduction
        smokeComponent.setVisible(false);
    }
    addAndMakeVisible(sliderKnob); // Make the slider visible on the component
    auto* label = labelMap[sliderKey]; // Get the reference to the label from the map
    if(label){
        // Set up the label for the knob
        label->setText(labelStringMap[sliderKey], juce::dontSendNotification);
        const auto labelColour = juce::Colour::fromRGB(200, 210, 230).withAlpha(0.9f);
        label->setColour(juce::Label::textColourId, labelColour);
        
        // Get the current bounds of the slider knob
        auto sliderBounds = sliderKnob->getBounds();

        // Calculate new bounds for the label. Here, we're doubling the width.
        // The height remains the same, and the position is adjusted to center the label.
        int labelWidth = sliderBounds.getWidth() * 2;
        int labelHeight = sliderBounds.getHeight();
        int labelX = sliderBounds.getX() - ((labelWidth - sliderBounds.getWidth()) / 2);
        int labelY = sliderBounds.getY() - (sliderBounds.getHeight()/2) - 10; // Position below the slider

        label->setBounds(labelX, labelY, labelWidth, labelHeight);
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }
   
}

void MoogMiniAudioProcessorEditor::createToggleSwitch(juce::ToggleButton& toggleSwitch, std::string toggleKey, std::string paramName, int posArray[], bool isHorizontal, int cellWidth, int cellHeight){
    const double adjustedWidth = static_cast<double>(cellWidth) * 0.94;
    const auto toPixel = [](double coordinate) { return static_cast<int>(coordinate); };
    auto* label = labelMap[toggleKey];
    VintageLookAndFeel* vintageLook = new VintageLookAndFeel(isHorizontal);
    if (toggleKey == "whitePinkSwitch") {
        vintageLook->setHandleColours(juce::Colours::hotpink.withAlpha(0.9f),
                                      juce::Colours::whitesmoke.withAlpha(0.9f));
    }
    vintageLookAndFeels.add(vintageLook); // Take ownership
    toggleSwitch.setLookAndFeel(vintageLook);
    toggleSwitch.addListener(this);
    toggleSwitch.setToggleState(audioProcessor.apvts.getParameterAsValue(paramName).getValue(), juce::dontSendNotification);
    toggleSwitch.setEnabled(true);
    if(isHorizontal){
        if (toggleKey == "extInputVolSwitch" && extInputVolKnob != nullptr) {
            auto knobBounds = extInputVolKnob->getBounds();
            toggleSwitch.setBounds(knobBounds.getRight() + 12, knobBounds.getCentreY() - 15, 60, 30);
        } else
        if(toggleKey == "osc1OnOffSwitch" || toggleKey == "osc2OnOffSwitch" || toggleKey == "osc3OnOffSwitch" || toggleKey == "noiseOnOffSwitch" || toggleKey == "extInputVolSwitch"){
            toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - 60.0) - adjustedWidth), (posArray[0] * cellHeight) + 15, 60, 30);
        } else if(toggleKey == "oscModSwitch"){
            if (ctrlModMixKnob != nullptr) {
                auto knobBounds = ctrlModMixKnob->getBounds();
                const int toggleWidth = 60;
                const int toggleHeight = 30;
                const int toggleX = knobBounds.getCentreX() - toggleWidth / 2;
                const int toggleY = knobBounds.getBottom() + 22;
                toggleSwitch.setBounds(toggleX, toggleY, toggleWidth, toggleHeight);
            } else {
                toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth) - 20.0),
                                       (ctrlGlideKnob->getY() + ctrlGlideKnob->getHeight()) + 30,
                                       60,
                                       30);
            }
        } else if(toggleKey == "glideSwitch" ){
            if (ctrlGlideKnob != nullptr) {
                auto knobBounds = ctrlGlideKnob->getBounds();
                const int toggleWidth = 60;
                const int toggleHeight = 30;
                const int toggleX = knobBounds.getCentreX() - toggleWidth / 2;
                const int toggleY = knobBounds.getBottom() + 22;
                toggleSwitch.setBounds(toggleX, toggleY, toggleWidth, toggleHeight);
            } else {
                toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 20.0)) - adjustedWidth) - 20.0),
                                       (ctrlGlideKnob->getY() + ctrlGlideKnob->getHeight()) + 30,
                                       60,
                                       30);
            }
        } else if(toggleKey == "filterModSwitch" || toggleKey == "keyboardCtrlSwitch1" || toggleKey == "keyboardCtrlSwitch2"){
            toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth) - 400.0), (posArray[0] * cellHeight) , 60, 30);
        } else if(toggleKey == "decaySwitch" || toggleKey == "a440hzSwitch"){
            toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth) - 435.0), (posArray[0] * cellHeight) - 10, 60, 30);
        } else {
            toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth) + 10.0), (posArray[0] * cellHeight) + 30, 60, 30);
        }
    } else {
         if (toggleKey == "osc3CtrlSwitch"){
            toggleSwitch.setBounds(toPixel((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth - 5.0), (posArray[0] * cellHeight) - 15, 30, 60);
        } else if (toggleKey == "whitePinkSwitch" && noiseVolKnob != nullptr){
            auto knobBounds = noiseVolKnob->getBounds();
            toggleSwitch.setBounds(knobBounds.getX() - 40,
                                   knobBounds.getCentreY() - 30,
                                   30,
                                   60);
        } else if (toggleKey == "whitePinkSwitch"){
            toggleSwitch.setBounds(toPixel(((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth) - 45.0), (posArray[0] * cellHeight) - 15, 30, 60);
        } else {
            toggleSwitch.setBounds(toPixel((posArray[1] * (adjustedWidth + 30.0)) - adjustedWidth), (posArray[0] * cellHeight) + 10, 30, 60);
        }
    }
    
    addAndMakeVisible(toggleSwitch);
    if(label){
        // Set up the label for the knob
        // Set up the label for the knob
        label->setText(labelStringMap[toggleKey], juce::dontSendNotification);
        const auto labelColour = juce::Colour::fromRGB(200, 210, 230).withAlpha(0.9f);
        label->setColour(juce::Label::textColourId, labelColour);
        if (isHorizontal) {
            label->attachToComponent(&toggleSwitch, false);
            // Manually position the label for vertical toggle switch
            int labelWidth = 200; // Adjust as needed
            int labelHeight = 30; // Adjust as needed
            int labelX = toggleSwitch.getX() - (labelWidth - toggleSwitch.getWidth()) / 2;
            int labelY = toggleSwitch.getY() + toggleSwitch.getHeight();
            label->setBounds(labelX, labelY, labelWidth, labelHeight);
        } else {
            // Manually position the label for vertical toggle switch
            int labelWidth = 200; // Adjust as needed
            int labelHeight = 30; // Adjust as needed
            int labelX = toggleSwitch.getX() - (labelWidth - toggleSwitch.getWidth()) / 2;
            int labelY = toggleSwitch.getY() + toggleSwitch.getHeight();
            label->setBounds(labelX, labelY, labelWidth, labelHeight);
        }
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }
    
}


MoogMiniAudioProcessorEditor::~MoogMiniAudioProcessorEditor() {
    // Clean up
    //sliders.clear(true);
    toggles.clear(true);
    buttons.clear(true);
    osc1WaveFormKnob->removeListener (this);
    osc2WaveFormKnob->removeListener (this);
    osc3WaveFormKnob->removeListener (this);
    osc1RangeKnob->removeListener (this);
    osc2RangeKnob->removeListener (this);
    osc3RangeKnob->removeListener (this);
    osc2FreqKnob->removeListener (this);
    osc3FreqKnob->removeListener (this);
    osc1VolKnob->removeListener (this);
    osc2VolKnob->removeListener (this);
    osc3VolKnob->removeListener (this);
    osc1OnOffSwitch.removeListener (this);
    osc2OnOffSwitch.removeListener (this);
    osc3OnOffSwitch.removeListener (this);
    ctrlTuneKnob->removeListener(this);
    filterEmphasisKnob->removeListener(this);
    filterCutoffFreqKnob->removeListener(this);
    filterEmphasisKnob->removeListener(this);
    
}

double MoogMiniAudioProcessorEditor::sliderPositionToLoudnessAttackValue(double position) {
    juce::Logger::writeToLog("Slider Position: " + juce::String(position));
    double value;
    
    if (position <= 0.5) {  // First half of the slider
        // Map back to milliseconds
        value = position * 2 * (1000.0 - 10.0) + 10.0;
    } else {  // Second half of the slider
        // Map back to seconds and convert to milliseconds
        double valueInSeconds = (position - 0.5) * 2 * (10.0 - 1.0) + 1.0;
        value = valueInSeconds * 1000.0;
    }
    juce::Logger::writeToLog("Loudness Attack Time: " + juce::String(value));
    return value;
}


bool MoogMiniAudioProcessorEditor::sliderHasChanged(juce::Slider* slider) {
    // Retrieve the current value of the parameter linked to the slider
    float currentValue = audioProcessor.apvts.getParameter(getParameterID(slider))->getValue();
    
    // Compare with the slider's new value
    float newValue = getNormalizedValue(slider, getEnumSizeLessOne(slider));
    
    return currentValue != newValue; // Return true if the values are different
}

float MoogMiniAudioProcessorEditor::getNormalizedValue(juce::Slider* slider, float) {
    const float index = static_cast<float>(slider->getValue());
    //juce::Logger::writeToLog("Slider Raw Value: " + juce::String(index));
    if (slider == osc1RangeKnob || slider == osc2RangeKnob || slider == osc3RangeKnob ) {
        return index / 5.0f;
    } else if (slider == osc1WaveFormKnob || slider == osc2WaveFormKnob || slider == osc3WaveFormKnob) {
        return index / 5.0f;
    }
    
    else if (slider == osc2FreqKnob || slider == osc3FreqKnob) {
        return (index + 8) / 16.0f; // Maps -8 to 0.0 and +8 to 1.0
    }
    else if (slider == ctrlTuneKnob) {
        // Assuming the range is -2.5 to +2.5
        return (index + 2.5f) / 5.0f; // Maps -2.5 to 0.0 and +2.5 to 1.0
    }
    else if(slider == filterCutoffFreqKnob || slider == &pitchWheelSlider){
        
        return (index + 5) / 10.0f;
    }
    else if (slider == osc1VolKnob || slider == osc2VolKnob || slider == osc3VolKnob || slider == ctrlTuneKnob || slider == filterEmphasisKnob || slider == filterAmtContourKnob     || slider == outputVolKnob  || slider == extInputVolKnob || slider == ctrlGlideKnob || slider == ctrlModMixKnob || slider == filterSustainKnob || slider == loudnessSustainLevelKnob || slider == outputPhonesVolKnob || slider == noiseVolKnob|| slider == feedbackKnob) {
        return index / 10.0f;
    }
    else if(slider == filterAttackTimeKnob || slider == filterDecayTimeKnob || slider == loudnessAttackTimeKnob || slider == loudnessDecayTimeKnob){
       
        const float normalizedValue = index / 10000.0f;
        
       
        return normalizedValue;
    }
   
    else {
        return index;
    }
    // Add other conditions for different sliders...
}

float MoogMiniAudioProcessorEditor::getSliderValueFromNormalized(juce::Slider* slider, float normalizedValue) {
    if (slider == nullptr) {
        return normalizedValue;
    }
    float value = normalizedValue;
    if (slider == osc1RangeKnob || slider == osc2RangeKnob || slider == osc3RangeKnob) {
        value = std::round(normalizedValue * 5.0f);
    } else if (slider == osc1WaveFormKnob || slider == osc2WaveFormKnob || slider == osc3WaveFormKnob) {
        value = std::round(normalizedValue * 5.0f);
    } else if (slider == osc2FreqKnob || slider == osc3FreqKnob) {
        value = std::round(normalizedValue * 16.0f) - 8.0f;
    } else if (slider == ctrlTuneKnob) {
        value = normalizedValue * 5.0f - 2.5f;
        value = std::round(value * 2.0f) / 2.0f;
    } else if (slider == filterCutoffFreqKnob || slider == &pitchWheelSlider) {
        value = normalizedValue * 10.0f - 5.0f;
    } else if (slider == osc1VolKnob || slider == osc2VolKnob || slider == osc3VolKnob || slider == ctrlTuneKnob
               || slider == filterEmphasisKnob || slider == filterAmtContourKnob || slider == outputVolKnob
               || slider == extInputVolKnob || slider == ctrlGlideKnob || slider == ctrlModMixKnob
               || slider == filterSustainKnob || slider == loudnessSustainLevelKnob || slider == outputPhonesVolKnob
               || slider == noiseVolKnob || slider == feedbackKnob) {
        value = std::round(normalizedValue * 10.0f);
    } else if (slider == filterAttackTimeKnob || slider == filterDecayTimeKnob
               || slider == loudnessAttackTimeKnob || slider == loudnessDecayTimeKnob) {
        value = normalizedValue * 10000.0f;
    }

    const float minValue = static_cast<float>(slider->getMinimum());
    const float maxValue = static_cast<float>(slider->getMaximum());
    return juce::jlimit(minValue, maxValue, value);
}

juce::String MoogMiniAudioProcessorEditor::getParameterID(juce::Slider* slider) {
    if (slider == osc1WaveFormKnob) {
        
        return "osc1Waveform";
    }
    else if (slider == osc2WaveFormKnob) {
        
        return "osc2Waveform";
    }
    else if (slider == osc3WaveFormKnob) {
        
        return "osc3Waveform";
    }
    else if (slider == osc1RangeKnob) {
        
        return "osc1Range";
        
    }
    else if (slider == osc2RangeKnob) {
        
        return "osc2Range";
        
    }
    else if (slider == osc3RangeKnob) {
        
        return "osc3Range";
        
    }
    else if (slider == osc2FreqKnob) {
        
        return "osc2Freq";
    }
    else if (slider == osc3FreqKnob) {
        
        return "osc3Freq";
    }
    else if (slider == osc1VolKnob) {
        
        return "osc1Vol";
    }
    else if (slider == osc2VolKnob) {
        
        return "osc2Vol";
    }
    else if (slider == osc3VolKnob) {
        
        return "osc3Vol";
    }
    else if (slider == ctrlTuneKnob) {
        
        return "tune";
    }
    else if (slider == filterCutoffFreqKnob) {
        
        return "filterCutoff";
    }
    else if (slider == filterEmphasisKnob) {
        
        return "filterEmphasis";
    }
    else if (slider == filterAmtContourKnob) {
        
        return "filterContour";
    } else if(slider == &modWheelSlider){
        return "modWheelValue";
    } else if(slider == &pitchWheelSlider){
        return "pitchWheelValue";
    } else if(slider == outputVolKnob){
        return "outputVolKnob";
    } else if(slider == extInputVolKnob){
        return "extInputVolKnob";
    } else if(slider == ctrlGlideKnob){
        return "ctrlGlideKnob";
    } else if(slider == ctrlModMixKnob){
        return "ctrlModMixKnob";
    } else if(slider == filterAttackTimeKnob){
        return "filterAttackTimeKnob";
    } else if(slider == filterDecayTimeKnob){
        return "filterDecayTimeKnob";
    } else if(slider == filterSustainKnob){
        return "filterSustainKnob";
    } else if(slider == noiseVolKnob){
        return "noiseVolKnob";
    } else if(slider == loudnessAttackTimeKnob){
        return "loudnessAttackTimeKnob";
    } else if(slider == loudnessDecayTimeKnob){
        return "loudnessDecayTimeKnob";
    } else if(slider == loudnessSustainLevelKnob){
        return "loudnessSustainLevelKnob";
    } else if(slider == outputPhonesVolKnob){
        return "outputPhonesVolKnob";
    } else if(slider == feedbackKnob){
        return "feedbackKnob";
    } else {
        return "";
    }
    // Add other conditions for different sliders...
}

float MoogMiniAudioProcessorEditor::getEnumSizeLessOne(juce::Slider* slider) {
    
    if (slider == osc1RangeKnob || slider == osc2RangeKnob || slider == osc3RangeKnob) {
        return 5.0;
    } else if (slider == osc1WaveFormKnob || slider == osc2WaveFormKnob || slider == osc3WaveFormKnob) {
        return 5.0;
    }
    
    else if (slider == osc2FreqKnob || slider == osc3FreqKnob) {
        return 16.0;
    }
    
    else if (slider == osc1VolKnob || slider == osc2VolKnob || slider == osc3VolKnob || slider == ctrlTuneKnob || slider == filterCutoffFreqKnob || slider == filterEmphasisKnob || slider == filterAmtContourKnob     || slider == outputVolKnob  || slider == extInputVolKnob || slider == ctrlGlideKnob || slider == ctrlModMixKnob || slider == filterSustainKnob || slider == loudnessSustainLevelKnob || slider == outputPhonesVolKnob|| slider == feedbackKnob) {
        return 10.0;
    }
    else if(slider == filterAttackTimeKnob || slider == filterDecayTimeKnob || slider == loudnessAttackTimeKnob || slider == loudnessDecayTimeKnob){
        return 1.0;
    }
    else {
        return 1.0;
    }
    // Add other conditions for different sliders...
}

void MoogMiniAudioProcessorEditor::sliderValueChanged(juce::Slider* slider) {
    if (!sliderHasChanged(slider)) {
        return; // Exit if the slider value hasn't actually changed
    }
    auto parameterID = getParameterID(slider);
    auto normalizedValue = getNormalizedValue(slider, getEnumSizeLessOne(slider));
    if(slider == noiseVolKnob){
        
        smokeComponent.setAlpha(normalizedValue);
    }
    
    if(slider == &pitchWheelSlider){
        
        juce::Logger::writeToLog(parameterID + " = " + juce::String(normalizedValue));
    }
    audioProcessor.apvts.getParameter(parameterID)->setValueNotifyingHost(normalizedValue);
    
    int index = static_cast<int>(slider->getValue());
    
    if (slider == osc1RangeKnob || slider == osc2RangeKnob || slider == osc3RangeKnob) {
        
        juce::String textLabel = Oscillator::getRangeLabel(index);
        slider->setTextValueSuffix(" " + textLabel);
    }
    
    
}

void MoogMiniAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &osc1OnOffSwitch)
    {
        bool toggleState = osc1OnOffSwitch.getToggleState();
        
        audioProcessor.apvts.getParameter("osc1OnOff")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &osc2OnOffSwitch) {
        bool toggleState = osc2OnOffSwitch.getToggleState();
        audioProcessor.apvts.getParameter("osc2OnOff")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &osc3OnOffSwitch) {
        bool toggleState = osc3OnOffSwitch.getToggleState();
        audioProcessor.apvts.getParameter("osc3OnOff")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &a440hzSwitch) {
        bool toggleState = a440hzSwitch.getToggleState();
        audioProcessor.apvts.getParameter("a440HzOnOff")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &osc3CtrlSwitch) {
        bool toggleState = osc3CtrlSwitch.getToggleState();
        audioProcessor.apvts.getParameter("osc3CtrlMode")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &oscModSwitch) {
        bool toggleState = oscModSwitch.getToggleState();
        audioProcessor.apvts.getParameter("oscModSwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &noiseOnOffSwitch) {
        bool toggleState = noiseOnOffSwitch.getToggleState();
        smokeComponent.setVisible(toggleState);
        audioProcessor.apvts.getParameter("noiseOnOffSwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &whitePinkSwitch) {
        bool toggleState = whitePinkSwitch.getToggleState();
        smokeComponent.setIsPink(toggleState);
        audioProcessor.apvts.getParameter("whitePinkSwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &filterModSwitch) {
        bool toggleState = filterModSwitch.getToggleState();
        audioProcessor.apvts.getParameter("filterModSwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &keyboardCtrlSwitch1) {
        bool toggleState = keyboardCtrlSwitch1.getToggleState();
        audioProcessor.apvts.getParameter("keyboardCtrlSwitch1")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &keyboardCtrlSwitch2) {
        bool toggleState = keyboardCtrlSwitch2.getToggleState();
        audioProcessor.apvts.getParameter("keyboardCtrlSwitch2")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &decaySwitch) {
        bool toggleState = decaySwitch.getToggleState();
        audioProcessor.apvts.getParameter("decaySwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    } else if (button == &glideSwitch) {
        bool toggleState = glideSwitch.getToggleState();
        audioProcessor.apvts.getParameter("glideSwitch")->setValueNotifyingHost(toggleState ? 1.0f : 0.0f);
    }
}

void MoogMiniAudioProcessorEditor::pianoKeyPressed(int noteNumber, PianoKey* key) {
    pressedKey = key;
  audioProcessor.handleNoteOn(1, noteNumber, 1.0f); // Assuming channel 1 and full velocity
}


void MoogMiniAudioProcessorEditor::pianoKeyReleased(int noteNumber) {
  audioProcessor.handleNoteOff(1, noteNumber, 1.0f); // Assuming channel 1 and full velocity
}


void MoogMiniAudioProcessorEditor::pianoKeyDragged(int, const juce::MouseEvent& event) {
  PianoKey* currentKey = nullptr;

    // Adjust the mouse position by the offset
    juce::Point<int> adjustedMousePosition = event.getPosition() + pressedKey->getPosition();

    // Check keys in reverse order to prioritize black keys
    for (int i = pianoKeys.size() - 1; i >= 0; --i) {
      auto* key = pianoKeys[i];

      // Check if the adjusted mouse position is within the key's bounds
      if (key->getBoundsInParent().contains(adjustedMousePosition)) {
        currentKey = key;
        break;
      }
    }

  // Handle the note on/off logic
  if (currentKey != lastDraggedKey) {
    if (lastDraggedKey) {
      lastDraggedKey->triggerNoteOff();
    }
    lastDraggedKey = currentKey;
    if (currentKey) {
        currentKey->setKeyPressed(true);
        audioProcessor.handleNoteOn(1, currentKey->getMidiNoteNumber(), 1.0f); // Assuming channel 1 and full velocity
    }
  }
}






void MoogMiniAudioProcessorEditor::releaseAllKeys() {
    for (auto* key : pianoKeys) {
        if (key->isCurrentlyPressed()) {
            key->triggerNoteOff();
        }
    }
    lastDraggedKey = nullptr; // Reset last dragged key
}


void MoogMiniAudioProcessorEditor::pianoKeyMouseUp() {
    releaseAllKeys();
}

void MoogMiniAudioProcessorEditor::refreshPresetList() {
    juce::String selectedName;
    if (presetComboBox.getSelectedId() > 0) {
        selectedName = presetComboBox.getItemText(presetComboBox.getSelectedItemIndex());
    }

    presetComboBox.clear();
    const auto presetNames = presetManager.getPresetNames();
    int presetId = 1;
    for (const auto& presetName : presetNames) {
        presetComboBox.addItem(presetName, presetId++);
    }

    if (selectedName.isNotEmpty()) {
        const int presetIndex = findPresetIndex(selectedName);
        if (presetIndex >= 0) {
            presetComboBox.setSelectedItemIndex(presetIndex, juce::dontSendNotification);
        }
    }
}

int MoogMiniAudioProcessorEditor::findPresetIndex(const juce::String& name) const {
    for (int i = 0; i < presetComboBox.getNumItems(); ++i) {
        if (presetComboBox.getItemText(i) == name) {
            return i;
        }
    }
    return -1;
}

void MoogMiniAudioProcessorEditor::loadSelectedPreset() {
    if (presetComboBox.getSelectedId() == 0) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Load Preset",
                                               "Select a preset to load.");
        return;
    }
    const auto presetName = presetComboBox.getItemText(presetComboBox.getSelectedItemIndex());
    if (!presetManager.loadPreset(presetName)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Load Preset",
                                               "Unable to load the selected preset.");
        return;
    }
    const int presetIndex = findPresetIndex(presetName);
    if (presetIndex >= 0) {
        presetComboBox.setSelectedItemIndex(presetIndex, juce::dontSendNotification);
    }
}

void MoogMiniAudioProcessorEditor::promptSavePreset() {
    auto* window = new juce::AlertWindow("Save Preset", "Enter a preset name.", juce::AlertWindow::NoIcon);
    if (presetComboBox.getSelectedId() > 0) {
        window->addTextEditor("presetName", presetComboBox.getItemText(presetComboBox.getSelectedItemIndex()), "Name");
    } else {
        window->addTextEditor("presetName", "", "Name");
    }
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MoogMiniAudioProcessorEditor> safeThis(this);
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([safeThis, window](int result) {
                                std::unique_ptr<juce::AlertWindow> cleanup(window);
                                if (!safeThis || result != 1) {
                                    return;
                                }
                                const auto presetName = cleanup->getTextEditorContents("presetName");
                                safeThis->confirmAndSavePreset(presetName);
                            }),
                            false);
}

void MoogMiniAudioProcessorEditor::confirmAndSavePreset(const juce::String& presetName) {
    const auto trimmedName = presetName.trim();
    if (trimmedName.isEmpty()) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Save Preset",
                                               "Preset name cannot be empty.");
        return;
    }

    if (presetManager.presetExists(trimmedName)) {
        juce::Component::SafePointer<MoogMiniAudioProcessorEditor> safeThis(this);
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                           "Replace Preset",
                                           "A preset with this name already exists. Replace it?",
                                           "Replace",
                                           "Cancel",
                                           this,
                                           juce::ModalCallbackFunction::create([safeThis, trimmedName](int result) {
                                               if (!safeThis || result != 1) {
                                                   return;
                                               }
                                               safeThis->savePresetByName(trimmedName);
                                           }));
        return;
    }

    savePresetByName(trimmedName);
}

void MoogMiniAudioProcessorEditor::savePresetByName(const juce::String& presetName) {
    if (!presetManager.savePreset(presetName)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Save Preset",
                                               "Unable to save the preset.");
        return;
    }

    refreshPresetList();
    const int presetIndex = findPresetIndex(presetName);
    if (presetIndex >= 0) {
        presetComboBox.setSelectedItemIndex(presetIndex, juce::dontSendNotification);
    }
}

juce::Point<float> MoogMiniAudioProcessorEditor::getOverlayPointForComponent(juce::Component* component) const {
    if (component == nullptr) {
        return {};
    }

    auto centre = component->getLocalBounds().getCentre();
    auto localPoint = signalFlowOverlay.getLocalPoint(component, centre);
    return { static_cast<float>(localPoint.x), static_cast<float>(localPoint.y) };
}

juce::Rectangle<float> MoogMiniAudioProcessorEditor::getOverlayBoundsForComponent(juce::Component* component) const {
    if (component == nullptr) {
        return {};
    }
    auto localBounds = component->getLocalBounds().toFloat();
    return signalFlowOverlay.getLocalArea(component, localBounds);
}

void MoogMiniAudioProcessorEditor::updateSignalFlowOverlayLayout() {
    const auto designBounds = juce::Rectangle<int>(0, 0, kDesignWidth, kDesignHeight);
    signalFlowOverlay.setBounds(designBounds);
    signalFlowOverlay.resetNodes();

    auto setNodeForComponent = [&](SignalFlowOverlay::Node node, juce::Component* component) {
        if (component == nullptr) {
            return;
        }
        if (!component->isVisible()) {
            return;
        }
        signalFlowOverlay.setNodePosition(node, getOverlayPointForComponent(component));
        signalFlowOverlay.setNodeBounds(node, getOverlayBoundsForComponent(component));
    };

    setNodeForComponent(SignalFlowOverlay::Node::Osc1Wave, osc1WaveFormKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc1Tune, ctrlTuneKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc1Range, osc1RangeKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc2Wave, osc2WaveFormKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc2Freq, osc2FreqKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc2Range, osc2RangeKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc3Wave, osc3WaveFormKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc3Freq, osc3FreqKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc3Range, osc3RangeKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc1Vol, osc1VolKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc2Vol, osc2VolKnob);
    setNodeForComponent(SignalFlowOverlay::Node::Osc3Vol, osc3VolKnob);
    setNodeForComponent(SignalFlowOverlay::Node::NoiseVol, noiseVolKnob);
    setNodeForComponent(SignalFlowOverlay::Node::ModMix, ctrlModMixKnob);
    setNodeForComponent(SignalFlowOverlay::Node::FilterCutoff, filterCutoffFreqKnob);

    if (outputVolKnob) {
        auto outputPoint = getOverlayPointForComponent(outputVolKnob);
        signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::Output, outputPoint);
        signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::Output, getOverlayBoundsForComponent(outputVolKnob));
        auto outputBounds = outputVolKnob->getBounds().toFloat();
        const float scopeHalfWidth = SignalFlowOverlay::outputScopeWidth * 0.5f;
        const float knobHalfWidth = outputBounds.getWidth() * 0.5f;
        const float offset = knobHalfWidth + SignalFlowOverlay::outputScopeGap + scopeHalfWidth;
        juce::Point<float> outputPost { outputPoint.x + offset, outputPoint.y };
        signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::OutputPost, outputPost);
    }

    juce::Point<float> mixerSum {};
    int mixerCount = 0;
    float mixerRightEdge = 0.0f;
    float mixerTopEdge = std::numeric_limits<float>::max();
    float mixerBottomEdge = std::numeric_limits<float>::lowest();
    float mixerWidthSum = 0.0f;
    int mixerWidthCount = 0;
    juce::Point<float> filterInPoint {};
    juce::Point<float> filterOutPoint {};
    bool hasFilterPanel = false;
    auto addMixerPoint = [&](juce::Component* component) {
        if (component == nullptr) {
            return;
        }
        auto center = getOverlayPointForComponent(component);
        mixerSum += center;
        ++mixerCount;
        auto bounds = getOverlayBoundsForComponent(component);
        mixerRightEdge = std::max(mixerRightEdge, bounds.getRight());
        mixerTopEdge = std::min(mixerTopEdge, bounds.getY());
        mixerBottomEdge = std::max(mixerBottomEdge, bounds.getBottom());
        mixerWidthSum += bounds.getWidth();
        ++mixerWidthCount;
    };

    addMixerPoint(osc1VolKnob);
    addMixerPoint(osc2VolKnob);
    addMixerPoint(osc3VolKnob);
    addMixerPoint(noiseVolKnob);

    if (mixerCount > 0) {
        auto mixerPoint = mixerSum / static_cast<float>(mixerCount);
        const float averageWidth = mixerWidthCount > 0 ? mixerWidthSum / static_cast<float>(mixerWidthCount) : 0.0f;
        const float mixerOffset = juce::jlimit(32.0f, 90.0f, averageWidth * 0.75f);
        mixerPoint.x = mixerRightEdge + mixerOffset;
        if (mixerTopEdge > mixerBottomEdge) {
            mixerTopEdge = mixerPoint.y - 20.0f;
            mixerBottomEdge = mixerPoint.y + 20.0f;
        }

        juce::Point<float> oscTargetSum {};
        int oscTargetCount = 0;
        auto addOscTarget = [&](juce::Component* component) {
            if (component == nullptr) {
                return;
            }
            oscTargetSum += getOverlayPointForComponent(component);
            ++oscTargetCount;
        };

        addOscTarget(osc1WaveFormKnob);
        addOscTarget(osc2WaveFormKnob);
        addOscTarget(osc3WaveFormKnob);
        if (oscTargetCount > 0) {
            signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::ModOscTarget,
                                              oscTargetSum / static_cast<float>(oscTargetCount));
        }

        auto panelBounds = lowerPanel.getBounds().toFloat();
        if (!panelBounds.isEmpty()) {
            const float scopeHalfWidth = SignalFlowOverlay::filterScopeWidth * 0.5f;
            const float scopeGap = SignalFlowOverlay::filterScopeGap;
            const float centeredScopeX = static_cast<float>(designBounds.getCentreX());
            filterInPoint = juce::Point<float>(centeredScopeX,
                                               panelBounds.getCentreY());
            hasFilterPanel = true;
            filterOutPoint = juce::Point<float>(panelBounds.getRight() + scopeGap + scopeHalfWidth,
                                                panelBounds.getCentreY());
            auto filterCorePoint = panelBounds.getCentre();
            signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::FilterIn, filterInPoint);
            signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::FilterOut, filterOutPoint);
            signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::FilterCore, filterCorePoint);
            signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::FilterIn,
                                            { filterInPoint.x - scopeHalfWidth,
                                              filterInPoint.y - SignalFlowOverlay::filterScopeHeight * 0.5f,
                                              SignalFlowOverlay::filterScopeWidth,
                                              SignalFlowOverlay::filterScopeHeight });
            signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::FilterCore,
                                            { filterCorePoint.x - 6.0f, filterCorePoint.y - 6.0f, 12.0f, 12.0f });
            if (outputVolKnob) {
                auto outputPoint = getOverlayPointForComponent(outputVolKnob);
                auto vcaPoint = filterOutPoint + (outputPoint - filterOutPoint) * 0.35f;
                signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::LoudnessVCA, vcaPoint);
                signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::LoudnessVCA,
                                                { vcaPoint.x - 6.0f, vcaPoint.y - 6.0f, 12.0f, 12.0f });
            }
        }

        if (hasFilterPanel) {
            const float scopeHalfWidth = SignalFlowOverlay::filterScopeWidth * 0.5f;
            mixerPoint = { filterInPoint.x - scopeHalfWidth, filterInPoint.y };
            mixerTopEdge = mixerPoint.y - 0.25f;
            mixerBottomEdge = mixerPoint.y + 0.25f;
        }

        signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::Mixer, mixerPoint);
        const float mixerWidth = hasFilterPanel ? 0.5f : 8.0f;
        signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::Mixer,
                                        { mixerPoint.x - mixerWidth * 0.5f,
                                          mixerTopEdge,
                                          mixerWidth,
                                          mixerBottomEdge - mixerTopEdge });
    }

    juce::Rectangle<float> filterVizBounds;
    bool hasFilterVizBounds = false;
    {
        const float height = 150.0f;
        if (hasFilterPanel && outputVolKnob != nullptr) {
            const float scopeHalfWidth = SignalFlowOverlay::filterScopeWidth * 0.5f;
            const float left = filterInPoint.x + scopeHalfWidth + 8.0f;
            const float right = filterInPoint.x + (filterOutPoint.x - filterInPoint.x) - scopeHalfWidth - 8.0f;
            const float top = filterInPoint.y - height * 0.5f;
            if (right > left + 40.0f) {
                filterVizBounds = { left, top, right - left, height };
                hasFilterVizBounds = true;
            }
        }
    }
    signalFlowOverlay.setFilterVizBounds(hasFilterVizBounds ? filterVizBounds : juce::Rectangle<float>{});
    if (hasFilterVizBounds) {
        signalFlowOverlay.setNodePosition(SignalFlowOverlay::Node::FilterIn, filterVizBounds.getCentre());
        signalFlowOverlay.setNodeBounds(SignalFlowOverlay::Node::FilterIn, filterVizBounds);

        const int toggleWidth = 60;
        const int toggleHeight = 30;
        const int spacing = 12;
        auto placeToggleWithLabel = [&](juce::ToggleButton& toggle, juce::Label& label, int x, int y) {
            toggle.setBounds(x, y, toggleWidth, toggleHeight);
            if (label.isVisible()) {
                const int labelWidth = 200;
                const int labelHeight = 30;
                const int labelX = x - (labelWidth - toggleWidth) / 2;
                const int labelY = y - labelHeight + 4;
                label.setBounds(labelX, labelY, labelWidth, labelHeight);
            }
        };

        const int topRowX = static_cast<int>(filterVizBounds.getX()) + 12;
        const int topRowY = static_cast<int>(filterVizBounds.getY()) - toggleHeight - 14;
        placeToggleWithLabel(filterModSwitch, filterModSwitchLabel, topRowX, topRowY);
        placeToggleWithLabel(keyboardCtrlSwitch1,
                             keyboardCtrlSwitch1Label,
                             topRowX + toggleWidth + spacing,
                             topRowY);
        placeToggleWithLabel(keyboardCtrlSwitch2,
                             keyboardCtrlSwitch2Label,
                             topRowX + (toggleWidth + spacing) * 2,
                             topRowY);

        const int bottomRowX = static_cast<int>(filterVizBounds.getRight())
            - (toggleWidth * 2 + spacing) - 12;
        const int bottomRowY = static_cast<int>(filterVizBounds.getBottom()) + 22;
        placeToggleWithLabel(decaySwitch, decaySwitchLabel, bottomRowX, bottomRowY);
        placeToggleWithLabel(a440hzSwitch,
                             a440hzSwitchLabel,
                             bottomRowX + toggleWidth + spacing,
                             bottomRowY);
    }

    signalFlowOverlay.updatePaths();
}





//==============================================================================
void MoogMiniAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MoogMiniAudioProcessorEditor::resized() {
    const auto designBounds = juce::Rectangle<int>(0, 0, kDesignWidth, kDesignHeight);
    customPanel.setBounds(designBounds);
    updateSignalFlowOverlayLayout();
    const float scale = juce::jmin(getWidth() / static_cast<float>(kDesignWidth),
                                   getHeight() / static_cast<float>(kDesignHeight));
    const float scaledWidth = kDesignWidth * scale;
    const float scaledHeight = kDesignHeight * scale;
    const float offsetX = (getWidth() - scaledWidth) * 0.5f;
    const float offsetY = (getHeight() - scaledHeight) * 0.5f;
    const auto transform = juce::AffineTransform::scale(scale).translated(offsetX, offsetY);
    for (auto* child : getChildren()) {
        if (dynamic_cast<juce::ResizableCornerComponent*>(child) != nullptr) {
            continue;
        }
        child->setTransform(transform);
    }
}

void MoogMiniAudioProcessorEditor::timerCallback() {
    signalFlowOverlay.setSampleRate(audioProcessor.getSampleRate());
    auto syncSliderValue = [this](juce::Slider* slider) {
        if (slider == nullptr || slider->isMouseButtonDown()) {
            return;
        }
        const auto paramId = getParameterID(slider);
        if (paramId.isEmpty()) {
            return;
        }
        auto* param = audioProcessor.apvts.getParameter(paramId);
        if (param == nullptr) {
            return;
        }
        const float normalizedValue = param->getValue();
        const float newValue = getSliderValueFromNormalized(slider, normalizedValue);
        if (std::abs(static_cast<float>(slider->getValue()) - newValue) > 0.0001f) {
            slider->setValue(newValue, juce::dontSendNotification);
        }
    };

    auto syncToggleState = [this](juce::ToggleButton& toggle, const juce::String& paramId) {
        auto* paramValue = audioProcessor.apvts.getRawParameterValue(paramId);
        if (paramValue == nullptr || toggle.isMouseButtonDown()) {
            return;
        }
        const bool isOn = paramValue->load() > 0.5f;
        if (toggle.getToggleState() != isOn) {
            toggle.setToggleState(isOn, juce::dontSendNotification);
        }
    };

    syncSliderValue(&modWheelSlider);
    syncSliderValue(&pitchWheelSlider);
    for (auto* slider : waveformSliders) {
        syncSliderValue(slider);
    }

    syncToggleState(osc1OnOffSwitch, "osc1OnOff");
    syncToggleState(osc2OnOffSwitch, "osc2OnOff");
    syncToggleState(osc3OnOffSwitch, "osc3OnOff");
    syncToggleState(a440hzSwitch, "a440HzOnOff");
    syncToggleState(osc3CtrlSwitch, "osc3CtrlMode");
    syncToggleState(oscModSwitch, "oscModSwitch");
    syncToggleState(noiseOnOffSwitch, "noiseOnOffSwitch");
    syncToggleState(extInputVolSwitch, "extInputVolSwitch");
    syncToggleState(whitePinkSwitch, "whitePinkSwitch");
    syncToggleState(filterModSwitch, "filterModSwitch");
    syncToggleState(keyboardCtrlSwitch1, "keyboardCtrlSwitch1");
    syncToggleState(keyboardCtrlSwitch2, "keyboardCtrlSwitch2");
    syncToggleState(decaySwitch, "decaySwitch");
    syncToggleState(glideSwitch, "glideSwitch");

    const bool osc1On = audioProcessor.apvts.getRawParameterValue("osc1OnOff")->load() > 0.5f;
    const bool osc2On = audioProcessor.apvts.getRawParameterValue("osc2OnOff")->load() > 0.5f;
    const bool osc3On = audioProcessor.apvts.getRawParameterValue("osc3OnOff")->load() > 0.5f;
    if (osc1WaveFormKnob) {
        osc1WaveFormKnob->setToggleActive(osc1On);
    }
    if (osc2WaveFormKnob) {
        osc2WaveFormKnob->setToggleActive(osc2On);
    }
    if (osc3WaveFormKnob) {
        osc3WaveFormKnob->setToggleActive(osc3On);
    }
    const bool noiseOn = audioProcessor.apvts.getRawParameterValue("noiseOnOffSwitch")->load() > 0.5f;
    if (noiseVolKnob) {
        noiseVolKnob->setToggleActive(noiseOn);
    }
    const bool extInputOn = audioProcessor.apvts.getRawParameterValue("extInputVolSwitch")->load() > 0.5f;
    if (extInputVolKnob) {
        extInputVolKnob->setToggleActive(extInputOn);
    }
    const bool isPinkNoise = audioProcessor.apvts.getRawParameterValue("whitePinkSwitch")->load() > 0.5f;
    if (smokeComponent.getIsPink() != isPinkNoise) {
        smokeComponent.setIsPink(isPinkNoise);
    }
    if (smokeComponent.isVisible() != noiseOn) {
        smokeComponent.setVisible(noiseOn);
    }
    {
        const float noiseVol = audioProcessor.apvts.getRawParameterValue("noiseVolKnob")->load();
        const float noiseAlpha = juce::jlimit(0.0f, 1.0f, noiseVol / 10.0f);
        smokeComponent.setAlpha(noiseOn ? noiseAlpha : 0.0f);
    }

    MoogMiniAudioProcessor::StageBuffers stageBuffers;
    audioProcessor.copyStageBuffers(stageBuffers);
    SignalFlowOverlay::StageWaveforms waveforms;
    waveforms.osc1Raw = stageBuffers.osc1Raw;
    waveforms.osc2Raw = stageBuffers.osc2Raw;
    waveforms.osc3Raw = stageBuffers.osc3Raw;
    waveforms.osc1 = stageBuffers.osc1;
    waveforms.osc2 = stageBuffers.osc2;
    waveforms.osc3 = stageBuffers.osc3;
    waveforms.noise = stageBuffers.noise;
    waveforms.mix = stageBuffers.mix;
    waveforms.filter = stageBuffers.filter;
    waveforms.output = stageBuffers.output;
    waveforms.modOsc = stageBuffers.modOsc;
    waveforms.modFilter = stageBuffers.modFilter;
    signalFlowOverlay.setStageWaveforms(waveforms);

    auto levels = audioProcessor.getSignalLevels();
    SignalFlowOverlay::Levels overlayLevels;
    overlayLevels.osc1Raw = levels.osc1Raw;
    overlayLevels.osc2Raw = levels.osc2Raw;
    overlayLevels.osc3Raw = levels.osc3Raw;
    overlayLevels.osc1 = levels.osc1;
    overlayLevels.osc2 = levels.osc2;
    overlayLevels.osc3 = levels.osc3;
    overlayLevels.noise = levels.noise;
    overlayLevels.mix = levels.mix;
    overlayLevels.filter = levels.filter;
    overlayLevels.output = levels.output;
    overlayLevels.modOsc = levels.modOsc;
    overlayLevels.modFilter = levels.modFilter;
    signalFlowOverlay.setLevels(overlayLevels);

    SignalFlowOverlay::FilterVizState filterViz;
    filterViz.cutoffHz = audioProcessor.mapFilterCutoffValueToFrequency(
        audioProcessor.apvts.getRawParameterValue("filterCutoff")->load());
    filterViz.resonance = audioProcessor.apvts.getRawParameterValue("filterEmphasis")->load() / 10.0f;
    filterViz.contourAmount = audioProcessor.apvts.getRawParameterValue("filterContour")->load() / 10.0f;
    filterViz.filterEnvelope = audioProcessor.getFilterEnvelopeValue();
    filterViz.contourEnvelope = audioProcessor.getContourEnvelopeValue();
    filterViz.attackMs = audioProcessor.normalizedToMilliseconds(
        audioProcessor.apvts.getRawParameterValue("loudnessAttackTimeKnob")->load());
    filterViz.decayMs = audioProcessor.normalizedToMilliseconds(
        audioProcessor.apvts.getRawParameterValue("loudnessDecayTimeKnob")->load());
    filterViz.sustainLevel = audioProcessor.apvts.getRawParameterValue("loudnessSustainLevelKnob")->load() / 10.0f;
    filterViz.contourAttackNorm = audioProcessor.apvts.getRawParameterValue("loudnessAttackTimeKnob")->load();
    filterViz.contourDecayNorm = audioProcessor.apvts.getRawParameterValue("loudnessDecayTimeKnob")->load();
    filterViz.contourSustainLevel = audioProcessor.apvts.getRawParameterValue("loudnessSustainLevelKnob")->load() / 10.0f;
    filterViz.loudnessAttackNorm = audioProcessor.apvts.getRawParameterValue("filterAttackTimeKnob")->load();
    filterViz.loudnessDecayNorm = audioProcessor.apvts.getRawParameterValue("filterDecayTimeKnob")->load();
    filterViz.loudnessSustainLevel = audioProcessor.apvts.getRawParameterValue("filterSustainKnob")->load() / 10.0f;
    const bool keyCtrl1 = audioProcessor.apvts.getRawParameterValue("keyboardCtrlSwitch1")->load() > 0.5f;
    const bool keyCtrl2 = audioProcessor.apvts.getRawParameterValue("keyboardCtrlSwitch2")->load() > 0.5f;
    filterViz.keyTracking = (keyCtrl1 ? (1.0f / 3.0f) : 0.0f) + (keyCtrl2 ? (2.0f / 3.0f) : 0.0f);
    signalFlowOverlay.setFilterVizState(filterViz);

    auto rangeRatio = [](int rangeIndex) {
        switch (rangeIndex) {
            case 0: return 1.0f / 256.0f; // LO
            case 1: return 1.0f / 4.0f;   // 32'
            case 2: return 1.0f / 2.0f;   // 16'
            case 3: return 1.0f;          // 8'
            case 4: return 2.0f;          // 4'
            case 5: return 4.0f;          // 2'
            default: return 1.0f;
        }
    };
    auto tuneRatio = [](int tuneIndex) {
        const float semitoneRatio = std::pow(2.0f, 1.0f / 12.0f);
        const int offset = tuneIndex - 5; // Zero is index 5
        return std::pow(semitoneRatio, static_cast<float>(offset));
    };
    auto detuneRatio = [](int detuneIndex) {
        return std::pow(2.0f, static_cast<float>(detuneIndex) / 12.0f);
    };

    SignalFlowOverlay::OscFrequencyScales scales;
    scales.osc1Tune = tuneRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("tune")->load()));
    scales.osc1Range = rangeRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("osc1Range")->load()));
    scales.osc2Detune = detuneRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("osc2Freq")->load()));
    scales.osc2Range = rangeRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("osc2Range")->load()));
    scales.osc3Detune = detuneRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("osc3Freq")->load()));
    scales.osc3Range = rangeRatio(static_cast<int>(audioProcessor.apvts.getRawParameterValue("osc3Range")->load()));
    signalFlowOverlay.setOscFrequencyScales(scales);

    signalFlowOverlay.advance();
    repaint();
}
