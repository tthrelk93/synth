//
//  EnvelopeGenerator.cpp
//  MiniMoog - Shared Code
//
//  Created by Thomas Threlkeld on 11/19/23.
//

#include <stdio.h>
#include "EnvelopeGenerator.h"
#include <cmath>
#include <JuceHeader.h>

EnvelopeGenerator::EnvelopeGenerator()
    : stage(OFF), attackTime(0.1f), decayTime(0.1f), sustainLevel(0.7f),
      currentLevel(0.0f), sampleRate(44100.0f) // Default sample rate
{
    updateRates();
}

void EnvelopeGenerator::setAttackTime(float time) {
    attackTime = time;
    updateRates();
}

void EnvelopeGenerator::setDecayTime(float time) {
    decayTime = time;
    updateRates();
}

void EnvelopeGenerator::setSustainLevel(float level) {
    sustainLevel = level;
}

void EnvelopeGenerator::noteOn() {
    stage = ATTACK;
    currentLevel = 0.0f;
   
}

void EnvelopeGenerator::noteOff() {
    stage = DECAY;
    
}

void EnvelopeGenerator::updateRates() {
    // Convert time from milliseconds to seconds
    float attackTimeInSeconds = attackTime / 1000.0f;
    float decayTimeInSeconds = decayTime / 1000.0f;

    // Calculate the rate
    attackRate = 1.0f / (attackTimeInSeconds * sampleRate);
    decayRate = 1.0f / (decayTimeInSeconds * sampleRate);

    // Ensure the rates are above a minimum threshold to avoid division by zero or extremely high values
    attackRate = std::fmax(attackRate, 1.0f / sampleRate);
    decayRate = std::fmax(decayRate, 1.0f / sampleRate);

//    juce::Logger::writeToLog("attackTimeInSeconds: " + juce::String(attackTimeInSeconds));
//    juce::Logger::writeToLog("decayTimeInSeconds: " + juce::String(decayTimeInSeconds));
//    juce::Logger::writeToLog("attackRate: " + juce::String(attackRate));
//    juce::Logger::writeToLog("decayRate: " + juce::String(decayRate));
   // juce::Logger::writeToLog("currentLevel: " + juce::String(currentLevel));
}


float EnvelopeGenerator::getNextSample() {
    juce::String stageString = "";
    switch (stage) {
        case ATTACK:
            stageString = "ATTACK";
            // Increase the current level at the rate calculated based on the attack time
            currentLevel += attackRate;
            // Transition to the DECAY stage once the level reaches 1.0
            if (currentLevel >= 1.0f) {
                currentLevel = 1.0f;
                stage = DECAY;
            }
            break;
        case DECAY:
            stageString = "DECAY";
            // Decrease the current level at the rate calculated based on the decay time
            currentLevel -= decayRate;
            // Transition to the SUSTAIN stage once the level reaches the sustain level
            if (currentLevel <= sustainLevel) {
                currentLevel = sustainLevel;
                stage = SUSTAIN;
            }
            break;
        case SUSTAIN:
            stageString = "SUSTAIN";
            // The level remains constant at the sustain level
            break;
        case OFF:
            stageString = "stageString";
            // The envelope is off, and the current level is set to 0
            currentLevel = 0.0f;
            break;
    }
    //juce::Logger::writeToLog("Envelope Stage: " + juce::String(stageString) + ", Current Level: " + juce::String(currentLevel));
    return currentLevel;
}

void EnvelopeGenerator::setSampleRate(float newSampleRate) {
    if (newSampleRate > 0) {
        sampleRate = newSampleRate;
        updateRates();
    }
}
