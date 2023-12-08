//
//  ModWheel.h
//  MoogMini
//
//  Created by Thomas Threlkeld on 11/14/23.
//

#ifndef MODWHEEL_H
#define MODWHEEL_H

class ModWheel {
public:
    ModWheel();
    void processModulation(float modulationSignal, float modWheelValue);
    void setModulationMix(float mix);
    void setOscillatorModulationOn(bool on);
    void setFilterModulationOn(bool on);

    float getModulatedSignal() const;
    float modulatedSignal;

private:
    float modulationMix;
    bool oscillatorModulationOn;
    bool filterModulationOn;
    
};

#endif // MODWHEEL_H
