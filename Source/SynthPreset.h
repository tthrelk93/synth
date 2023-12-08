//
//  SynthPreset.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 12/8/23.
//

#ifndef SynthPreset_h
#define SynthPreset_h
#pragma once
#include <JuceHeader.h>

class SynthPreset {
    
public:
    
    juce::String getPresetName();
    void setPresetName(juce::String name);
    std::map<juce::String, float> getParamValueMap();
    
private:
    juce::String presetName;
    std::map<juce::String, float> paramValueMap;
};

#endif /* SynthPreset_h */
