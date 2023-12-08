//
//  SynthPreset.cpp
//  MiniMoog - Shared Code
//
//  Created by Thomas Threlkeld on 12/8/23.
//

#include <stdio.h>
#include "SynthPreset.h"
#include <stdio.h>


juce::String SynthPreset::getPresetName(){
    return presetName;
}
void SynthPreset::setPresetName(juce::String name){
    presetName = name;
}
std::map<juce::String, float> SynthPreset::getParamValueMap(){
    return paramValueMap;
}
