//
//  PresetLibrary.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 12/8/23.
//

#ifndef PresetLibrary_h
#define PresetLibrary_h
#pragma once
#include <JuceHeader.h>
#include "SynthPreset.h"

class PresetLibrary {
    
public:
    
    SynthPreset default1;
    SynthPreset default2;
    SynthPreset default3;
    //SynthPreset getPresetLibraryArray();
    
private:
    SynthPreset presetLibraryArray[3];
};

#endif /* PresetLibrary_h */
