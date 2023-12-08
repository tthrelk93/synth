//
//  LadderFilter.h
//  MoogMini
//
//  Created by Thomas Threlkeld on 11/12/23.
//

#ifndef LADDERFILTER_H
#define LADDERFILTER_H

#include <JuceHeader.h>
#include "EnvelopeGenerator.h"

class LadderFilter {
public:
    enum FilterCutoff {
        FilterNegFive = 0,
        FilterNegFour,
        FilterNegThree,
        FilterNegTwo,
        FilterNegOne,
        FilterZero,
        FilterPosOne,
        FilterPosTwo,
        FilterPosThree,
        FilterPosFour,
        FilterPosFive
    };
    
    enum FilterEmphasis {
        EmphZero = 0,
        EmphOne,
        EmphTwo,
        EmphThree,
        EmphFour,
        EmphFive,
        EmphSix,
        EmphSeven,
        EmphEight,
        EmphNine,
        EmphTen
    };
    
    enum FilterContour {
        ContourZero = 0,
        ContourOne,
        ContourTwo,
        ContourThree,
        ContourFour,
        ContourFive,
        ContourSix,
        ContourSeven,
        ContourEight,
        ContourNine,
        ContourTen
    };
    LadderFilter();

    void setCutoffFrequency(float frequency);
        void setResonance(float resonance);
        void setEnvelopeAmount(float amount);
        void process(float* input, float* output, int numSamples);
        void noteOn();
        void noteOff();
   
       void setFilterModulationSwitch(bool isOn);
       void setEmphasis(float value);
    void setSampleRate(float newSampleRate); // New method to set the sample rate
    void setEnvelopeSettings(float attack, float decay, float sustain);
    void setContourEnvelopeSettings(float attack, float decay, float sustain);
    float getContourEnvelopeValue();
    float getFeedback();
    void setFeedback(float f);
private:
    float feedback;
    float cutoffFrequency;
       float resonance;
       float envelopeAmount;
       EnvelopeGenerator envelopeGenerator;
       float feedbackAmount; // Additional variable for feedback amount
    
    EnvelopeGenerator contourEnvelopeGenerator;
    float contourEnvelopeAmount;

       // New variables for the stages of the filter
       float stage[4]; // Array to hold the state of each filter stage

       // Internal methods for filter calculations
       float computeFilter(float inputSample, float modulatedCutoff);
       
       float sampleRate; // Variable to store the current sample rate
    int modulationScaleFactor = 50000;
};

#endif // LADDERFILTER_H

