//
//  PianoKey.h
//  MoogMini
//
//  Created by Thomas Threlkeld on 11/17/23.
//

#ifndef PianoKey_h
#define PianoKey_h

#include <JuceHeader.h>

class PianoKey : public juce::Component {

public:
    
    void paint(juce::Graphics& g) override;
    struct Listener {
        virtual ~Listener() = default;
        virtual void pianoKeyPressed(int noteNumber, PianoKey* key) = 0;
        virtual void pianoKeyReleased(int noteNumber) = 0;
        virtual void pianoKeyDragged(int noteNumber, const juce::MouseEvent& event) = 0;
        virtual void pianoKeyMouseUp() = 0;
    };


        PianoKey(bool isBlack, int midiNoteNumber, Listener* listener);

    void mouseDrag(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
    void setKeyPressed(bool isPressed);
    void triggerNoteOn();
        void triggerNoteOff();
    bool isMouseOverKey(const juce::Point<int>& position) const;
    bool isCurrentlyPressed() const { return isKeyPressed; }
    bool getBlackKey() const { return isBlackKey; }
    int getMidiNoteNumber();
    
private:
    bool isBlackKey;
       int midiNoteNumber;
       Listener* listener;
    bool isKeyPressed;
};


#endif /* PianoKey_h */
