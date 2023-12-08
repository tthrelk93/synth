//
//  EnvelopeGenerator.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 11/19/23.
//

#ifndef EnvelopeGenerator_h
#define EnvelopeGenerator_h

class EnvelopeGenerator {
public:
    EnvelopeGenerator();
    void setAttackTime(float time);
    void setDecayTime(float time);
    void setSustainLevel(float level);
    void noteOn();
    void noteOff();
    float getNextSample();
    void setSampleRate(float newSampleRate);

private:
    enum EnvelopeStage { ATTACK, DECAY, SUSTAIN, OFF };
    EnvelopeStage stage;
    float attackTime, decayTime, sustainLevel;
    float currentLevel, attackRate, decayRate;
    float sampleRate; // Add sample rate to the envelope generator

       void updateRates(); // Helper function to update attack and decay rates
};
#endif /* EnvelopeGenerator_h */
