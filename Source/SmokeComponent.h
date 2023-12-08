//
//  SmokeComponent.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 12/3/23.
//

#ifndef SmokeComponent_h
#define SmokeComponent_h
#include <JuceHeader.h>

class SmokeComponent : public juce::Component, private juce::Timer {
public:
    SmokeComponent();
    ~SmokeComponent() override;
    void setIsPink(bool isPinkNoise);
    void setAlpha(float a);
    bool getIsPink();
private:
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

//    juce::Path smokePath1;
//    juce::Path smokePath2;
//    juce::Path smokePath3;
//    juce::Path smokePath4;
//    juce::Path smokePath5;
//    juce::Path smokePath6;
//    juce::Path smokePath7;
//    juce::Path smokePath8;
//    juce::Path smokePath9;
//    juce::Path smokePath10;
//    
//    juce::Path smokePath11;
//    juce::Path smokePath12;
//    juce::Path smokePath13;
//    juce::Path smokePath14;
//    juce::Path smokePath15;
//    juce::Path smokePath16;
//    juce::Path smokePath17;
//    juce::Path smokePath18;
//    juce::Path smokePath19;
//    juce::Path smokePath20;
    juce::Path smokePaths[20];
    juce::Point<float> smokePosition;
    float smokeSize;
    bool isPink = false;
    float alpha = 0.0;
};
#endif /* SmokeComponent_h */
