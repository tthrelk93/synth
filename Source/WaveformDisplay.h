// WaveformDisplay.h
#ifndef WAVEFORMDISPLAY_H
#define WAVEFORMDISPLAY_H

#include <JuceHeader.h>
#include "CircularBuffer.h"
#include "PluginProcessor.h"


class WaveformDisplay : public juce::Component, public juce::Timer {
public:
    enum WaveformDisplayKey {
        Osc1Raw = 0,
        Osc2Raw,
        Osc3Raw,
        PostFilter,
        Stereo
    };
    explicit WaveformDisplay(CircularBuffer& buffer, MoogMiniAudioProcessor& p, WaveformDisplayKey waveKey);
    ~WaveformDisplay() override;

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void update();
    void drawWaveform(juce::Graphics& g, const float* buffer, juce::Colour colour, int section, int bufferSize);
    void drawStereoScope(juce::Graphics& g, float* leftWaveform, float* rightWaveform, float width, float height);

private:
    CircularBuffer& circularBuffer;
    static constexpr int bufferSize = 512; // or whatever size you need
    MoogMiniAudioProcessor& audioProcessor;
    WaveformDisplayKey waveformDisplayKey;
};

#endif // WAVEFORMDISPLAY_H
