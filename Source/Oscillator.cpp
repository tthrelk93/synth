//
//  Oscillator.cpp
//  NewProject - Shared Code
//
//  Created by Thomas Threlkeld on 11/9/23.
//

#include <stdio.h>
#include "Oscillator.h"
#include <cmath>

void Oscillator::setWaveform(Waveform newWaveform) {
    waveform = newWaveform;
}

void Oscillator::setFrequency(float newFrequency) {
    frequency = newFrequency;
    updateIncrement();
}

void Oscillator::setSampleRate(float newSampleRate) {
    sampleRate = newSampleRate;
    updateIncrement();
}

void Oscillator::setRange(Range newRange) {
    range = newRange;
    updateIncrement();
}

void Oscillator::setVolume(Volume newVol) {
    vol = newVol;
}

void Oscillator::setTune(Tune newTune) {
    tune = newTune;
    updateIncrement();
}

void Oscillator::start(float newFrequency) {
    setFrequency(newFrequency);
    active = true;
}

void Oscillator::stop() {
    active = false;
    phase = 0.0f; // Reset phase
}

bool Oscillator::isActive() const {
    return active;
}

void Oscillator::setControlMode(bool midiControlled) {
    isMidiControlled = midiControlled;
    updateIncrement(); // Update the increment as the mode changes
}

float Oscillator::generateModulationSignal() {
    if (!isMidiControlled) {
        // Generate a signal based on the oscillator's current settings
        // This signal is independent of MIDI note data
        return generateWaveform(); // This method should generate the waveform based on the oscillator's current settings (waveform type, frequency, etc.)
    } else {
        // If the oscillator is MIDI controlled, return zero or some default value
        return 0.0f;
    }
}

juce::String Oscillator::getRangeLabel(int value) {
    switch (value) {
        case 0: return "LO";
        case 1: return "32";
        case 2: return "16";
        case 3: return "8";
        case 4: return "4";
        case 5: return "2";
        default: return "";
    }
}


float Oscillator::generateWaveform() {
    float sample = 0.0f;
    switch (waveform) {
        case Sin:
            // Sine wave calculation
            sample = std::sin(2.0 * M_PI * phase);
            break;
        case Triangle:
            // Triangle wave calculation
            sample = 2.0f * (2.0f * std::abs(phase - 0.5f) - 1.0f);
            break;
        case Sharktooth:
            // Sharktooth wave calculation
            if (phase < 0.2f) {
                sample = 5.0f * phase;
            } else {
                sample = 1.25f - 1.25f * phase;
            }
            break;
        case ReverseSaw:
            // Reverse Sawtooth wave calculation
            sample = 1.0f - 2.0f * phase;
            break;
        case Sawtooth:
            // Sawtooth wave calculation
            sample = 2.0f * (phase - 0.5f);
            break;
        case Square:
            // Square wave calculation
            sample = phase < 0.5f ? 1.0f : -1.0f;
            break;
        case WideRectangle:
            // Wide rectangle wave calculation
            sample = phase < 0.7f ? 1.0f : -1.0f;
            break;
        case NarrowRectangle:
            // Narrow rectangle wave calculation
            sample = phase < 0.3f ? 1.0f : -1.0f;
            break;
        // Add additional cases for other waveforms if necessary
    }

    // Increment phase
    phase += phaseIncrement;
    if (phase >= 1.0f) phase -= 1.0f;

    return sample;
}

float Oscillator::processNextSample(float modulationEffect, bool osc3CtrlMode) {
    juce::ignoreUnused(osc3CtrlMode);

    // Apply modulation to frequency
    float baseFrequency = calculateFrequencyForRange();
    float modulatedFrequency = baseFrequency * (1.0f + modulationEffect); // Adjust frequency based on modulation effect
    float finalFrequency = calculateDetunedFrequency(modulatedFrequency); // Calculate final frequency considering detune
    phaseIncrement = finalFrequency / sampleRate; // Recalculate phase increment
    float curvature = 0.8f; // A value between 0 and 1, where 1 is a straight line.
    // Phase points
    float nonlinearRiseEnd = 0.3f; // End of the nonlinear rise
    float toothEnd = 0.5f; // End of the sharp tooth, start of the nonlinear fall
    // Reset the phase to zero at the end of each cycle
//      if (phase >= 1.0f) {
//        phase = 0.0f;
//      }
    juce::String isGap;
    float triangularSample = 0.0f;
    float sawtoothSample;
    float blendWeight;
    float blend;
    // Generate waveform based on the current waveform setting
    float sample = 0.0f;
    switch (waveform) {
        case Sin:
            sample = std::sin(2.0 * M_PI * phase);
           
                break;
            break;
        case Triangle:
            // Adjust this parameter to control the curvature of the entire wave.

               if (phase < 0.5f) // Rising edge
               {
                   // A smooth curve that starts slow and accelerates towards the midpoint.
                   sample = std::pow(phase * 2.0f, curvature);
               }
               else // Falling edge
               {
                   // A smooth curve that starts fast and decelerates towards the endpoint.
                   sample = 1.0f - std::pow((phase - 0.5f) * 2.0f, curvature);
               }

               // Map the sample to the correct amplitude range.
               sample = 2.0f * (sample - 0.5f);


            break;
        case Sharktooth:
            if (phase < 0.5f) {
                // A smooth curve that starts slow and accelerates towards the midpoint.
                triangularSample = std::pow(phase * 2.0f, curvature);
            } else {
                // A smooth curve that starts fast and decelerates towards the endpoint.
                triangularSample = 1.0f - std::pow((phase - 0.5f) * 2.0f, curvature);
            }
              // Calculate the sawtooth portion of the wave
              sawtoothSample = 2.0f * (phase - 0.5f);
              // Blend the triangular and sawtooth portions
              sample = (1.0f - 0.35) * triangularSample +  0.35 * sawtoothSample;
              // Map the sample to the correct amplitude range
              sample = 2.0f * (sample - 0.5f);

            break;
        case ReverseSaw:
            sample = 1.0f - 2.0f * phase;
            break;
        case Sawtooth:
            sample = 2.0f * (phase - 0.5f);
            break;
        case Square:
            sample = phase < 0.5f ? 1.0f : -1.0f;
            break;
        case WideRectangle:
            sample = phase < 0.7f ? 1.0f : -1.0f;
            break;
        case NarrowRectangle:
            sample = phase < 0.3f ? 1.0f : -1.0f;
            break;
        // Add additional cases for other waveforms if necessary
    }
    // Increment phase
    phase += phaseIncrement;
    if (phase >= 1.0f) phase -= 1.0f;

    return sample;
}





void Oscillator::updateIncrement() {
    float baseFrequency = calculateFrequencyForRange(); // Get the base frequency adjusted for range
    float finalFrequency = calculateDetunedFrequency(baseFrequency); // Calculate final frequency considering detune
    phaseIncrement = finalFrequency / sampleRate;
}

float Oscillator::calculateFrequencyForRange() {
    // First, adjust frequency based on range
    float adjustedFrequency = frequency; // Start with the base frequency

    switch (range) {
        case LO:
            adjustedFrequency /= 256.0f; // Sub-audio range for LFO use
            break;
        case ThirtyTwo:
            adjustedFrequency *= 0.25f; // 32' (two octaves below 8')
            break;
        case Sixteen:
            adjustedFrequency *= 0.5f; // 16' (one octave below 8')
            break;
        case Eight:
            break;  // 8' (base frequency)
        case Four:
            adjustedFrequency *= 2.0f;  // 4' (one octave above 8')
            break;
        case Two:
            adjustedFrequency *= 4.0f;  // 2' (two octaves above 8')
            break;
        default:
            break; // Default case, no change
    }


    return adjustedFrequency;
}

float Oscillator::calculateTuneForOsc1(float adjustedFrequency){
    // Now apply tune adjustment
    const float semitoneRatio = std::pow(2.0f, 1.0f / 12.0f); // Ratio for one semitone change

    switch (tune) {
        case NegTwoHalf: adjustedFrequency *= std::pow(semitoneRatio, -2.5f); break;
        case NegTwo:     adjustedFrequency *= std::pow(semitoneRatio, -2.0f); break;
        case NegOneHalf: adjustedFrequency *= std::pow(semitoneRatio, -1.5f); break;
        case NegOne:     adjustedFrequency *= std::pow(semitoneRatio, -1.0f); break;
        case NegHalf:    adjustedFrequency *= std::pow(semitoneRatio, -0.5f); break;
        case Zero:       break; // No change
        case PosHalf:    adjustedFrequency *= std::pow(semitoneRatio, 0.5f); break;
        case PosOne:     adjustedFrequency *= std::pow(semitoneRatio, 1.0f); break;
        case PosOneHalf: adjustedFrequency *= std::pow(semitoneRatio, 1.5f); break;
        case PosTwo:     adjustedFrequency *= std::pow(semitoneRatio, 2.0f); break;
        case PosTwoHalf: adjustedFrequency *= std::pow(semitoneRatio, 2.5f); break;
        default:         break;
    }

    return adjustedFrequency;
}

void Oscillator::setDetuneAmount(Frequency newDetuneAmount) {
    detuneAmount = newDetuneAmount;
    updateIncrement();
}
float Oscillator::calculateDetunedFrequency(float modulatedFrequency) {
    float adjustedFrequency = calculateTuneForOsc1(modulatedFrequency);

    if (shouldApplyDetune) {
        const float detuneStep = 1.0f; // Step size for each detune position
        adjustedFrequency *= std::pow(2.0f, static_cast<float>(detuneAmount) * detuneStep / 12.0f);
    }

    return adjustedFrequency;
}

float Oscillator::getFrequency(){
    return frequency;
}
