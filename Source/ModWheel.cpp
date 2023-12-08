//
//  ModWheel.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/14/23.
//

#include <stdio.h>
#include "ModWheel.h"

ModWheel::ModWheel()
    : modulationMix(0.0f), oscillatorModulationOn(false), filterModulationOn(false), modulatedSignal(0.0f) {
}

void ModWheel::processModulation(float modulationSignal, float modWheelValue) {
    // Assume modWheelValue is the value from the ModWheelSlider UI, ranging from 0 to 1
    modulatedSignal = modulationSignal * modWheelValue;

    // If modulation is to be applied to the oscillator pitch
    if (oscillatorModulationOn) {
        // Apply pitch modulation here
    }

    // If modulation is to be applied to the filter cutoff
    if (filterModulationOn) {
        // Apply filter modulation here
    }
}


void ModWheel::setModulationMix(float mix) {
    modulationMix = mix;
}

void ModWheel::setOscillatorModulationOn(bool on) {
    oscillatorModulationOn = on;
}

void ModWheel::setFilterModulationOn(bool on) {
    filterModulationOn = on;
}

float ModWheel::getModulatedSignal() const {
    return modulatedSignal;
}
