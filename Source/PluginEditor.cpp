#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <unordered_map>
#include "VintageLookAndFeel.h"
#include "WaveformSlider.h"
#include "PianoKey.h"

//==============================================================================
MoogMiniAudioProcessorEditor::MoogMiniAudioProcessorEditor (MoogMiniAudioProcessor& p)
: AudioProcessorEditor (&p)
, audioProcessor (p)
, osc1WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc1Raw)
, osc2WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc2Raw)
, osc3WaveformDisplayRaw (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::Osc3Raw)
, combinedWaveformDisplay (audioProcessor.getCircularBuffer(), p, WaveformDisplay::WaveformDisplayKey::PostFilter)

{
    
    setSize (1782, 1000);
    
    customPanel.setBounds(0, 0, getWidth(), getHeight());
    addAndMakeVisible(customPanel);
    
    const int gridWidth = 26;
    const int gridHeight = 7;
    const int cellWidth = 1782 / gridWidth;
    const int cellHeight = 700 / gridHeight;
    
    // Set bounds for pitch and mod wheels
    int wheelSize = 200;
    int padding = 30;
   
    customPanel.addAndMakeVisible(pitchWheelSlider);
    customPanel.addAndMakeVisible(modWheelSlider);
    

    
  


    // Make sure the editor can intercept mouse clicks and receive mouse drag events
    setInterceptsMouseClicks(true, true);
    
    
    
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
    int outputPhonesVolKnobPos[2];
    int feedbackKnobPos[2];
    int ouputMainOutputSwitchPos[2];
    int extInputVolSwitchPos[2];
    int keyboardCtrlSwitch1Pos[2];
    int keyboardCtrlSwitch2Pos[2];
    int noiseOnOffSwitchPos[2];
    int whitePinkSwitchPos[2];
    int overloadButtonPos[2];
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
                    } else if(sliderKey == "outputPhonesVolKnob") {
                        outputPhonesVolKnobPos[0] = row;
                        outputPhonesVolKnobPos[1] = col;
                    } else if(sliderKey == "feedbackKnob") {
                        feedbackKnobPos[0] = row;
                        feedbackKnobPos[1] = col;
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
                    } else if(toggleKey == "ouputMainOutputSwitch") {
                        ouputMainOutputSwitchPos[0] = row;
                        ouputMainOutputSwitchPos[1] = col;
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
    lowerPanel.setBounds((getWidth()/2.5 )+ 18, getHeight()/6.3, 300, 320);
    
    addAndMakeVisible(osc1WaveformDisplayRaw);
    addAndMakeVisible(osc2WaveformDisplayRaw);
    addAndMakeVisible(osc3WaveformDisplayRaw);
    addAndMakeVisible(combinedWaveformDisplay);
    
   
    
    
    createSliderKnob(osc1WaveFormKnob, "osc1WaveFormKnob", 7, 0, 6, 1, "osc1Waveform", false, osc1WaveformPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc2WaveFormKnob, "osc2WaveFormKnob", 7, 0, 6, 1, "osc2Waveform", false, osc2WaveformPos, cellWidth, cellHeight, false);
    
    createSliderKnob(osc3WaveFormKnob, "osc3WaveFormKnob", 7, 0, 6, 1, "osc3Waveform", false, osc3WaveformPos, cellWidth, cellHeight, false);
    
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
    
    createSliderKnob(filterAttackTimeKnob, "filterAttackTimeKnob", 1, 10, 10000, 0.01, "filterAttackTimeKnob", true, filterAttackTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(filterDecayTimeKnob, "filterDecayTimeKnob", 1, 10, 10000, 0.01, "filterDecayTimeKnob", true, filterDecayTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(filterSustainKnob, "filterSustainKnob", 11, 0, 10, 1.0, "filterSustainKnob", true, filterSustainKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(noiseVolKnob, "noiseVolKnob", 11, 0, 10, 1.0, "noiseVolKnob", true, noiseVolKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(loudnessAttackTimeKnob, "loudnessAttackTimeKnob", 1, 10, 10000, 0.01, "loudnessAttackTimeKnob", true, loudnessAttackTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(loudnessDecayTimeKnob, "loudnessDecayTimeKnob", 1, 10, 10000, 0.01, "loudnessDecayTimeKnob", true, loudnessDecayTimeKnobPos, cellWidth, cellHeight, true);
    
    createSliderKnob(loudnessSustainLevelKnob, "loudnessSustainLevelKnob", 11, 0, 10, 1.0, "loudnessSustainLevelKnob", true, loudnessSustainLevelKnobPos, cellWidth, cellHeight, false);
    
    createSliderKnob(outputPhonesVolKnob, "outputPhonesVolKnob", 11, 0, 10, 1.0, "outputPhonesVolKnob", true, outputPhonesVolKnobPos, cellWidth, cellHeight, false);
    
    //createSliderKnob(feedbackKnob, "feedbackKnob", 11, 0, 10, 1.0, "feedbackKnob", true, feedbackKnobPos, cellWidth, cellHeight, false);
    
    createToggleSwitch(osc1OnOffSwitch, "osc1OnOffSwitch", "osc1OnOff", osc1OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc2OnOffSwitch, "osc2OnOffSwitch", "osc2OnOff", osc2OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc3OnOffSwitch, "osc3OnOffSwitch", "osc3OnOff", osc3OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(a440hzSwitch, "a440hzSwitch", "a440HzOnOff", a440OnOffPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(osc3CtrlSwitch, "osc3CtrlSwitch", "a440HzOnOff", osc3CtrlPos, false, cellWidth, cellHeight);
    
    createToggleSwitch(oscModSwitch, "oscModSwitch", "oscModSwitch", oscModSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(noiseOnOffSwitch, "noiseOnOffSwitch", "noiseOnOffSwitch", noiseOnOffSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(extInputVolSwitch, "extInputVolSwitch", "extInputVolSwitch", extInputVolSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(whitePinkSwitch, "whitePinkSwitch", "whitePinkSwitch", whitePinkSwitchPos, false, cellWidth, cellHeight);
    
    createToggleSwitch(filterModSwitch, "filterModSwitch", "filterModSwitch", filterModSwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(keyboardCtrlSwitch1, "keyboardCtrlSwitch1", "keyboardCtrlSwitch1", keyboardCtrlSwitch1Pos, true, cellWidth, cellHeight);
    
    createToggleSwitch(keyboardCtrlSwitch2, "keyboardCtrlSwitch2", "keyboardCtrlSwitch2", keyboardCtrlSwitch2Pos, true, cellWidth, cellHeight);
    
    createToggleSwitch(decaySwitch, "decaySwitch", "decaySwitch", decaySwitchPos, true, cellWidth, cellHeight);
    
    createToggleSwitch(glideSwitch, "glideSwitch", "glideSwitch", glideSwitchPos, true, cellWidth, cellHeight);

    
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
    
    float whiteKeyWidth = 43.6;
    float blackKeyWidth = whiteKeyWidth / 1.7;
    int blackKeyHeight = (300 * 0.6)-10; // 60% of the panel height
    
    int startX = osc3VolKnob->getX() + osc3VolKnob->getWidth() + 20; // Starting X position for the keys
    
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
            whiteKey->setBounds(startX + (whiteKeyIndex * whiteKeyWidth), 700 + 5, whiteKeyWidth, 300-15);
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
                blackKey->setBounds(blackKeyPosArr[blackKeyIndex] + (whiteKeyWidth - blackKeyWidth/4) - blackKeyWidth/2, 700 + 5, blackKeyWidth, blackKeyHeight);
            } else if(blackKeyIndex == 2 || blackKeyIndex == 4 || blackKeyIndex == 7 || blackKeyIndex == 9 || blackKeyIndex == 12 || blackKeyIndex == 14 || blackKeyIndex == 17) {
                blackKey->setBounds(blackKeyPosArr[blackKeyIndex] + (whiteKeyWidth + blackKeyWidth/4) - blackKeyWidth/2,  700 +5, blackKeyWidth, blackKeyHeight);
            } else {
                blackKey->setBounds(blackKeyPosArr[blackKeyIndex] + whiteKeyWidth  - blackKeyWidth/2, 700 + 5, blackKeyWidth, blackKeyHeight);
            }
            
            customPanel.addAndMakeVisible(blackKey);
            blackKeyIndex++;
        }
        midiNoteNumber++;
    }
    pitchWheelSlider.setBounds(ctrlGlideKnob->getX() + ctrlGlideKnob->getWidth() - 7 , ctrlGlideKnob->getY() - 55, wheelSize/2, wheelSize);
    pitchWheelSlider.addListener(this);
    
    modWheelSlider.setBounds(pitchWheelSlider.getRight() - 15,ctrlGlideKnob->getY() - 55, wheelSize/2, wheelSize);
    modWheelSlider.addListener(this);
    
    
    startTimerHz(60); // repaint at 60 Hz
    resized();
}

void MoogMiniAudioProcessorEditor::createSliderKnob(WaveformSlider*& sliderKnob, std::string sliderKey, int numPositions, float minPosVal, float maxPosVal, float increment, std::string paramName, bool useCustomRange, int posArray[], int cellWidth, int cellHeight, bool isTimeKnob){
    auto adjustedWidth = cellWidth*0.94;
    bool isOscWaveform = (sliderKey == "osc1WaveFormKnob") || (sliderKey == "osc2WaveFormKnob") || (sliderKey == "osc3WaveFormKnob");
   
    if(useCustomRange){
        sliderKnob = new WaveformSlider(numPositions, useCustomRange, increment, minPosVal, maxPosVal, isTimeKnob, isOscWaveform, sliderKey);
    } else {
        sliderKnob = new WaveformSlider(numPositions, useCustomRange, isOscWaveform, sliderKey);
    }
    waveformSliders.add(sliderKnob); // Take ownership
    sliderKnob->addListener(this);
    sliderKnob->setRange(minPosVal, maxPosVal, increment);
    sliderKnob->setValue(audioProcessor.apvts.getParameterAsValue(paramName).getValue());
    sliderKnob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);

    sliderKnob->setTextBoxStyle(juce::Slider::NoTextBox, false, 100, 20);
    if(sliderKey == "osc1VolKnob" || sliderKey == "osc2VolKnob" || sliderKey == "osc3VolKnob" || sliderKey == "extInputVolKnob" || sliderKey == "noiseVolKnob"){
        sliderKnob->setBounds(osc1RangeKnob->getX() + osc1RangeKnob->getWidth() + 5, (posArray[0] * cellHeight), 70, 70); // Set the bounds of the slider
    } else if( sliderKey == "osc1FreqKnob" || sliderKey == "osc2FreqKnob" || sliderKey == "osc3FreqKnob" || sliderKey == "ctrlTuneKnob" ){
        sliderKnob->setBounds(((posArray[1] * (adjustedWidth + 15)) - adjustedWidth) - 10, posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "osc1WaveFormKnob" || sliderKey == "osc2WaveFormKnob" || sliderKey == "osc3WaveFormKnob"){
        sliderKnob->setBounds(((posArray[1] * (adjustedWidth + 15)) - adjustedWidth) - 12, (posArray[0] * cellHeight), 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "osc1RangeKnob" || sliderKey == "osc2RangeKnob"  || sliderKey == "osc3RangeKnob"){
        sliderKnob->setBounds(((posArray[1] * (adjustedWidth + 15)) - adjustedWidth) - 13, posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "extInputVolKnob" || sliderKey == "feedbackKnob" ){
        sliderKnob->setBounds(((posArray[1] * (adjustedWidth + 30)) + 30) - adjustedWidth, (posArray[0] * cellHeight) + 15 , 40, 40); // Set the bounds of the slider
    } else if(sliderKey == "ctrlModMixKnob"){
        sliderKnob->setBounds(osc3WaveFormKnob->getX(), (osc3WaveFormKnob->getY() + (osc3WaveFormKnob->getHeight() * 4.5) + 110) , 60, 60); // Set the bounds of the slider
    } else if(sliderKey == "ctrlGlideKnob"){
        sliderKnob->setBounds(ctrlModMixKnob->getX() + ctrlModMixKnob->getWidth() + 15, (osc3WaveFormKnob->getY() + (osc3WaveFormKnob->getHeight() * 4.5) + 110), 60, 60); // Set the bounds of the slider
    } else if(sliderKey == "filterCutoffFreqKnob" || sliderKey == "filterEmphasisKnob" || sliderKey == "filterAmtContourKnob" || sliderKey == "filterAttackTimeKnob" || sliderKey == "filterDecayTimeKnob" || sliderKey == "filterSustainKnob" || sliderKey == "loudnessAttackTimeKnob" || sliderKey == "loudnessDecayTimeKnob" || sliderKey == "loudnessSustainLevelKnob"){
        sliderKnob->setBounds(((posArray[1] * (adjustedWidth + 15)) - adjustedWidth)-190, posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else if(sliderKey == "outputVolKnob"){
        sliderKnob->setBounds((posArray[1] * (adjustedWidth + 15)) - adjustedWidth + 100, posArray[0] * cellHeight, 70, 70); // Set the bounds of the slider
    } else {
        sliderKnob->setBounds((posArray[1] * (adjustedWidth + 15)) - adjustedWidth, posArray[0] * cellHeight, 60, 60); // Set the bounds of the slider
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
        label->setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.85f));
        
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
    auto adjustedWidth = cellWidth*0.94;
    auto* label = labelMap[toggleKey];
    VintageLookAndFeel* vintageLook = new VintageLookAndFeel(isHorizontal);
    vintageLookAndFeels.add(vintageLook); // Take ownership
    toggleSwitch.setLookAndFeel(vintageLook);
    toggleSwitch.addListener(this);
    toggleSwitch.setToggleState(audioProcessor.apvts.getParameterAsValue(paramName).getValue(), juce::dontSendNotification);
    toggleSwitch.setEnabled(true);
    if(isHorizontal){
        if(toggleKey == "osc1OnOffSwitch" || toggleKey == "osc2OnOffSwitch" || toggleKey == "osc3OnOffSwitch" || toggleKey == "noiseOnOffSwitch" || toggleKey == "extInputVolSwitch"){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - 60) - adjustedWidth, (posArray[0] * cellHeight) + 15, 60, 30);
        } else if(toggleKey == "oscModSwitch"){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - adjustedWidth) - 20, (ctrlGlideKnob->getY() + ctrlGlideKnob->getHeight()) + 30, 60, 30);
        } else if(toggleKey == "glideSwitch" ){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 20)) - adjustedWidth) - 20, (ctrlGlideKnob->getY() + ctrlGlideKnob->getHeight()) + 30, 60, 30);
        } else if(toggleKey == "filterModSwitch" || toggleKey == "keyboardCtrlSwitch1" || toggleKey == "keyboardCtrlSwitch2"){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - adjustedWidth) - 400, (posArray[0] * cellHeight) , 60, 30);
        } else if(toggleKey == "decaySwitch" || toggleKey == "a440hzSwitch"){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - adjustedWidth) - 435, (posArray[0] * cellHeight) - 10, 60, 30);
        } else {
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - adjustedWidth) + 10, (posArray[0] * cellHeight) + 30, 60, 30);
        }
    } else {
         if (toggleKey == "osc3CtrlSwitch"){
            toggleSwitch.setBounds((posArray[1] * (adjustedWidth + 30)) - adjustedWidth - 5, (posArray[0] * cellHeight) - 15, 30, 60);
        } else if (toggleKey == "whitePinkSwitch"){
            toggleSwitch.setBounds(((posArray[1] * (adjustedWidth + 30)) - adjustedWidth) - 45, (posArray[0] * cellHeight) - 15, 30, 60);
        } else {
            toggleSwitch.setBounds((posArray[1] * (adjustedWidth + 30)) - adjustedWidth, (posArray[0] * cellHeight) + 10, 30, 60);
        }
    }
    
    addAndMakeVisible(toggleSwitch);
    if(label){
        // Set up the label for the knob
        // Set up the label for the knob
        label->setText(labelStringMap[toggleKey], juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.85f));
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
    
    delete osc1WaveFormKnob;
    delete osc2WaveFormKnob;
    delete osc3WaveFormKnob;
    delete osc1RangeKnob;
    delete osc2RangeKnob;
    delete osc3RangeKnob;
    delete osc2FreqKnob;
    delete osc3FreqKnob;
    delete osc1VolKnob;
    delete osc2VolKnob;
    delete osc3VolKnob;
    delete ctrlTuneKnob;
    delete filterEmphasisKnob;
    delete filterCutoffFreqKnob;
    delete filterEmphasisKnob;
    
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

float MoogMiniAudioProcessorEditor::getNormalizedValue(juce::Slider* slider, float enumSizeLessOne) {
    float index = slider->getValue();
    //juce::Logger::writeToLog("Slider Raw Value: " + juce::String(index));
    if (slider == osc1RangeKnob || slider == osc2RangeKnob || slider == osc3RangeKnob ) {
        return index / 5.0f;
    } else if (slider == osc1WaveFormKnob || slider == osc2WaveFormKnob || slider == osc3WaveFormKnob) {
        return index / 6.0f;
    }
    
    else if (slider == osc2FreqKnob || slider == osc3FreqKnob) {
        return (index + 8) / 16.0f; // Maps -8 to 0.0 and +8 to 1.0
    }
    else if (slider == ctrlTuneKnob) {
        // Assuming the range is -2.5 to +2.5
        return (index + 2.5) / 5.0f; // Maps -2.5 to 0.0 and +2.5 to 1.0
    }
    else if(slider == filterCutoffFreqKnob || slider == &pitchWheelSlider){
        
        return (index + 5) / 10.0f;
    }
    else if (slider == osc1VolKnob || slider == osc2VolKnob || slider == osc3VolKnob || slider == ctrlTuneKnob || slider == filterEmphasisKnob || slider == filterAmtContourKnob     || slider == outputVolKnob  || slider == extInputVolKnob || slider == ctrlGlideKnob || slider == ctrlModMixKnob || slider == filterSustainKnob || slider == loudnessSustainLevelKnob || slider == outputPhonesVolKnob || slider == noiseVolKnob|| slider == feedbackKnob) {
        return index / 10.0f;
    }
    else if(slider == filterAttackTimeKnob || slider == filterDecayTimeKnob || slider == loudnessAttackTimeKnob || slider == loudnessDecayTimeKnob){
       
        float normalizedValue = index / 10000;
        
       
        return normalizedValue;
    }
   
    else {
        return index;
    }
    // Add other conditions for different sliders...
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
        return 7.0;
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


void MoogMiniAudioProcessorEditor::pianoKeyDragged(int noteNumber, const juce::MouseEvent& event) {
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





//==============================================================================
void MoogMiniAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MoogMiniAudioProcessorEditor::resized() {
    // Assuming lowerPanel is an instance of CustomPanel or a subclass that has a paint method
     //  lowerPanel.setBounds(getWidth() / 2 - 200, getHeight() / 2 - 200, 400, 400); // Center the panel
      // addAndMakeVisible(lowerPanel);
    
}

void MoogMiniAudioProcessorEditor::timerCallback() {
    repaint();
}
