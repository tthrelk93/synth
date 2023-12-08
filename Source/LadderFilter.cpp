//
//  LadderFilter.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/12/23.
//

#include <stdio.h>
#include "LadderFilter.h"
#include <cmath>

LadderFilter::LadderFilter() : cutoffFrequency(440.0f), resonance(0.0f), envelopeAmount(0.0f) {
    // Initialize filter state variables
    for (int i = 0; i < 4; ++i) {
        stage[i] = 0.0f;
    }
}

void LadderFilter::setCutoffFrequency(float frequency) {
    cutoffFrequency = frequency;
}

void LadderFilter::setResonance(float resonance) {
    this->resonance = resonance;
   // juce::Logger::writeToLog(juce::String(feedback));
    // Adjust feedback amount based on resonance
    feedbackAmount = resonance * (1.0f - feedback * resonance);
}


void LadderFilter::setEnvelopeAmount(float amount) {
    //envelopeAmount = amount;
    envelopeAmount = amount * modulationScaleFactor;
}

void LadderFilter::process(float* input, float* output, int numSamples) {
    static int sampleCounter = 0;
    static const int logIntervalSamples = static_cast<int>(4.0f * sampleRate); // 4 seconds * sample rate

    for (int i = 0; i < numSamples; ++i) {
        float envNextSample = envelopeGenerator.getNextSample();
        float loudnessEnvSample = contourEnvelopeGenerator.getNextSample();
        if (sampleCounter++ >= logIntervalSamples) {
           // juce::Logger::writeToLog("in method LadderFilter::process | Cutoff Frequency (Before Modulation): " + juce::String(cutoffFrequency));
        }
        float modulatedCutoff = cutoffFrequency + envelopeAmount * envNextSample * loudnessEnvSample;

        if (sampleCounter++ >= logIntervalSamples) {
//            juce::Logger::writeToLog("in method LadderFilter::process | envelopeAmount: " + juce::String(envelopeAmount));
//            juce::Logger::writeToLog("in method LadderFilter::process | next sample: " + juce::String(envNextSample));
//            juce::Logger::writeToLog("in method LadderFilter::process | Modulated Cutoff: " + juce::String(modulatedCutoff));
//            juce::Logger::writeToLog("in method LadderFilter::process | resonance: " + juce::String(resonance));
           
            sampleCounter = 0; // Reset the counter
        }

        output[i] = computeFilter(input[i], modulatedCutoff);
        if (sampleCounter++ >= logIntervalSamples) {
          //  juce::Logger::writeToLog("in method LadderFilter::process | Filter Output: " + juce::String(output[i]));
        }
    }
}


float LadderFilter::computeFilter(float inputSample, float modulatedCutoff) {
    // Implementing a 4-stage ladder filter with non-linear feedback
    float cutoff = modulatedCutoff / sampleRate; // Normalize cutoff frequency

    // Drive control adjusts the input level to create non-linearity
    float drive = 0.7f; // Adjust the drive level as needed
    inputSample = tanh(inputSample * drive);

    // Four stages of the ladder filter
    for (int i = 0; i < 4; ++i) {
        float stageInput = inputSample - resonance * stage[3]; // Feedback from the last stage
        stageInput = tanh(stageInput); // Non-linearity

        // Calculate the filter stage (simplified one-pole lowpass)
        stage[i] = stage[i] + cutoff * (stageInput - stage[i]);
        inputSample = stage[i];
    }
   
    return inputSample;
}

void LadderFilter::noteOn() {
    envelopeGenerator.noteOn();
    contourEnvelopeGenerator.noteOn();
}

void LadderFilter::noteOff() {
    envelopeGenerator.noteOff();
    contourEnvelopeGenerator.noteOff();
}

void LadderFilter::setEnvelopeSettings(float attack, float decay, float sustain) {
    envelopeGenerator.setAttackTime(attack);
    envelopeGenerator.setDecayTime(decay);
    envelopeGenerator.setSustainLevel(sustain);
   
}

void LadderFilter::setContourEnvelopeSettings(float attack, float decay, float sustain) {
    contourEnvelopeGenerator.setAttackTime(attack);
    contourEnvelopeGenerator.setDecayTime(decay);
    contourEnvelopeGenerator.setSustainLevel(sustain);
}

float LadderFilter::getContourEnvelopeValue() {
    return contourEnvelopeGenerator.getNextSample();
}

void LadderFilter::setSampleRate(float newSampleRate) {
    if (newSampleRate > 0) {
        sampleRate = newSampleRate;
        envelopeGenerator.setSampleRate(newSampleRate);
        contourEnvelopeGenerator.setSampleRate(newSampleRate);
    }
}
float LadderFilter::getFeedback(){
    return feedback;
}
void LadderFilter::setFeedback(float f){
    feedback = f;
}
