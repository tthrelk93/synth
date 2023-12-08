//
//  BlackBackgroundComponent.h
//  MiniMoog
//
//  Created by Thomas Threlkeld on 12/5/23.
//

#ifndef BlackBackgroundComponent_h
#define BlackBackgroundComponent_h
#include <JuceHeader.h>

class BlackBackgroundComponent : public juce::Component
{
public:
    BlackBackgroundComponent();

    void paint(juce::Graphics& g) override;
};

#endif /* BlackBackgroundComponent_h */
