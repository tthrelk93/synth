#ifndef Oscillator_h
#define Oscillator_h
#pragma once
#include <JuceHeader.h>

class Oscillator {
public:
    enum Waveform {
        Triangle = 0,
        Sharktooth,
        ReverseSaw,
        Sawtooth,
        Square,
        WideRectangle,
        NarrowRectangle,
        Sin
    };
    
    enum Range {
        LO = 0,
        ThirtyTwo,
        Sixteen,
        Eight,
        Four,
        Two
    };
    enum Volume {
        VolZero = 0,
        VolOne,
        VolTwo,
        VolThree,
        VolFour,
        VolFive,
        VolSix,
        VolSeven,
        VolEight,
        VolNine,
        VolTen
    };
    enum Tune {
        NegTwoHalf = 0,
        NegTwo,
        NegOneHalf,
        NegOne,
        NegHalf,
        Zero,
        PosHalf,
        PosOne,
        PosOneHalf,
        PosTwo,
        PosTwoHalf
    };
    enum Frequency {
        FreqNegEight = 0,
        FreqNegSeven,
        FreqNegSix,
        FreqNegFive,
        FreqNegFour,
        FreqNegThree,
        FreqNegTwo,
        FreqNegOne,
        FreqZero,
        FreqPosOne,
        FreqPosTwo,
        FreqPosThree,
        FreqPosFour,
        FreqPosFive,
        FreqPosSix,
        FreqPosSeven,
        FreqPosEight
    };
    
   
    void start(float frequency); // Start playing a note
    void stop(); // Stop playing a note

    bool isActive() const; // Check if the oscillator is active
    
    void setWaveform(Waveform newWaveform);
    void setFrequency(float newFrequency);
    void setSampleRate(float newSampleRate);
    void setRange(Range newRange);
    void setDetuneAmount(Frequency detuneAmount); // New method to set detune amount
    void setTune(Tune tune);
    void setVolume(Volume newVol);
    void setShouldApplyDetune(bool shouldApply);
    bool shouldApplyDetune = false;
    void setControlMode(bool midiControlled);
    float generateModulationSignal();
    float generateWaveform();
    float calculateTuneForOsc1(float adjustedFrequency);
    static juce::String getRangeLabel(int value);
    bool isMidiControlled = true;
  
    float processNextSample(float modulationEffect, bool osc3CtrlMode);
    float getFrequency();


  

private:
    Waveform waveform = Triangle;
    Range range = LO;
    Frequency detuneAmount = FreqZero;
    Volume vol = VolZero;
    Tune tune = Zero;
    float frequency;
    float sampleRate = 44100.0f;
    float phase = 0.0f; // Phase accumulator
    float phaseIncrement = 0.0f; // Increment to add to the phase
    float lfoFrequency = 2.0;
    
   
    bool active = false;
    void updateIncrement(); // Update this method to account for range
    float calculateFrequencyForRange(); // New method to calculate frequency based on range
    float calculateDetunedFrequency(float modulatedFrequency);
};

#endif /* Oscillator_h */
