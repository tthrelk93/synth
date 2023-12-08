//
//  PianoKey.cpp
//  MoogMini - Shared Code
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#include <stdio.h>
#include "PianoKey.h"

PianoKey::PianoKey(bool isBlack, int midiNoteNumber, Listener* listener)
    : isBlackKey(isBlack), midiNoteNumber(midiNoteNumber), listener(listener) {}

void PianoKey::mouseDown(const juce::MouseEvent& event) {
   
    triggerNoteOn();
}


void PianoKey::mouseUp(const juce::MouseEvent&) {
    triggerNoteOff();
    if (listener) {
        listener->pianoKeyMouseUp();
    }
}


void PianoKey::setKeyPressed(bool isPressed) {
    if (isKeyPressed != isPressed) {
        isKeyPressed = isPressed;
        repaint();
    }
}

void PianoKey::triggerNoteOn() {
    
    setKeyPressed(true);
    if (listener) {
        
        listener->pianoKeyPressed(midiNoteNumber, this);
    }
}

void PianoKey::triggerNoteOff() {
    setKeyPressed(false);
    if (listener) {
        listener->pianoKeyReleased(midiNoteNumber);
    }
}

bool PianoKey::isMouseOverKey(const juce::Point<int>& position) const {
    return getBounds().contains(position);
}

void PianoKey::mouseDrag(const juce::MouseEvent& event) {
    if (listener) {
        
        listener->pianoKeyDragged(midiNoteNumber, event);
    }
}

int PianoKey::getMidiNoteNumber(){
    return midiNoteNumber;
}



void PianoKey::paint(juce::Graphics& g) {
    if (isKeyPressed) {
        g.setColour(isBlackKey ? juce::Colours::darkgrey : juce::Colours::lightgrey);
    } else {
        g.setColour(isBlackKey ? juce::Colours::black.withAlpha(0.90f) : juce::Colours::white.withAlpha(0.75f));
    }
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1); // border
}

