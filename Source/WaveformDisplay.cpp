// WaveformDisplay.cpp
#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(CircularBuffer& buffer, MoogMiniAudioProcessor& p, WaveformDisplayKey waveKey)
    : circularBuffer(buffer), audioProcessor (p), waveformDisplayKey (waveKey) {
    startTimerHz(30); // Start the timer with a frequency of 30 Hz
}

WaveformDisplay::~WaveformDisplay() {
    stopTimer(); // Stop the timer when the component is destroyed
}

void WaveformDisplay::timerCallback() {
    // This will be called at the interval specified in startTimerHz
    repaint(); // Call repaint to update the waveform display
}


void WaveformDisplay::paint(juce::Graphics& g) {
    float leftWaveform[bufferSize];
    float rightWaveform[bufferSize];

    if (circularBuffer.read(leftWaveform, rightWaveform, bufferSize)) {
        //g.fillAll(juce::Colours::black); // Background color
        
        auto width = static_cast<float>(getWidth());
        auto height = static_cast<float>(getHeight());
        
        //drawStereoScope(g, leftWaveform, rightWaveform, width, height);

        
        if (waveformDisplayKey == WaveformDisplayKey::Osc1Raw && audioProcessor.osc1.isActive()) {
            const auto& osc1Buffer = audioProcessor.getOsc1Buffer();
            drawWaveform(g, osc1Buffer.data(), juce::Colours::darkorange, 1, audioProcessor.waveformBufferSize);
            }
            if (waveformDisplayKey == WaveformDisplayKey::Osc2Raw && audioProcessor.osc2.isActive()) {
                const auto& osc2Buffer = audioProcessor.getOsc2Buffer();
                drawWaveform(g, osc2Buffer.data(), juce::Colours::darkorange, 1, audioProcessor.waveformBufferSize);
            }
            if (waveformDisplayKey == WaveformDisplayKey::Osc3Raw && audioProcessor.osc3.isActive()) {
                const auto& osc3Buffer = audioProcessor.getOsc3Buffer();
                drawWaveform(g, osc3Buffer.data(), juce::Colours::darkorange, 1, audioProcessor.waveformBufferSize);
            }
        if (waveformDisplayKey == WaveformDisplayKey::PostFilter) {
            float monoWaveform[bufferSize];
            for (int i = 0; i < bufferSize; ++i) {
                monoWaveform[i] = (leftWaveform[i] + rightWaveform[i]) / 2.0f;
            }
            drawWaveform(g, monoWaveform, juce::Colours::cyan, 1, bufferSize);
        }
        
        // Draw the mono mix waveform in the fourth section

        // ... [Code to create the mono mix] ...
       // drawWaveform(g, monoWaveform, juce::Colours::cyan, 3, bufferSize);

    }
}

void WaveformDisplay::drawWaveform(juce::Graphics& g, const float* buffer, juce::Colour colour, int section, int bufferSize) {
    g.setColour(colour);

    float sectionHeight = getHeight() / 4.0f; // Adjusted section height
    float yOffset = sectionHeight * section;
    float sectionMiddle = yOffset + sectionHeight / 2.0f; // Middle of the section

    for (int i = 1; i < bufferSize; ++i) {
        // Directly map buffer values to display coordinates
        float previousSample = sectionMiddle + buffer[i - 1] * sectionHeight / 2.0f;
        float currentSample = sectionMiddle + buffer[i] * sectionHeight / 2.0f;

        float x1 = juce::jmap(static_cast<float>(i - 1), 0.0f, static_cast<float>(bufferSize), 0.0f, static_cast<float>(getWidth()));
        float x2 = juce::jmap(static_cast<float>(i), 0.0f, static_cast<float>(bufferSize), 0.0f, static_cast<float>(getWidth()));

        g.drawLine(x1, previousSample, x2, currentSample);
    }
}

void WaveformDisplay::drawStereoScope(juce::Graphics& g, float* leftWaveform, float* rightWaveform, float width, float height){
            // Loop through the buffer
            for (int i = 1; i < bufferSize; ++i) {
                // Map left and right waveforms to X and Y axes for the first line
                float x1 = juce::jmap(leftWaveform[i - 1], -1.0f, 1.0f, 0.0f, width);
                float y1 = juce::jmap(rightWaveform[i - 1], -1.0f, 1.0f, height, 0.0f);
                float x2 = juce::jmap(leftWaveform[i], -1.0f, 1.0f, 0.0f, width);
                float y2 = juce::jmap(-rightWaveform[i], -1.0f, 1.0f, height, 0.0f);
    
                // Map left and right waveforms to X and Y axes for the mirrored line
                float mx1 = width - x1; // Mirrored X1
                float my1 = height - y1; // Mirrored Y1
                float mx2 = width - x2; // Mirrored X2
                float my2 = height - y2; // Mirrored Y2
    
                // Use a gradient to simulate depth
                juce::Colour gradientColour = juce::Colour::fromHSV(
                    static_cast<float>(i) / bufferSize, 1.0f, 1.0f, 0.4f);
                g.setColour(gradientColour);
    
                if(audioProcessor.osc1.isActive()){
                    // Draw the original line
                    g.drawLine(x1, y1, x2, y2);
                   // g.drawLine(x1+10, y1+10, x2, y2);
                    // Draw the mirrored line
                    //g.drawLine(mx1, my1, mx2, my2);
                    g.drawLine(mx1+10, my1+10, mx2+10, my2+10);
                }
                if(audioProcessor.osc2.isActive()){
                    g.drawLine(x1/1, y1/1, x2/1, y2/1);
                   // g.drawLine((x1+10)/1, (y1+10)/1, x2/1, y2/1);
                    // Draw the mirrored line
                    //g.drawLine(-mx1/1, -my1/1, -mx2/1, -my2/1);
                    g.drawLine((mx1+10)/1, (my1+10)/1, (mx2+10)/1, (my2+10)/1);
                }
                if(audioProcessor.osc3.isActive()){
                    g.drawLine(x1/-1, y1/-1, x2/-1, y2/-1);
                   // g.drawLine((x1+10)/-1, (y1+10)/-1, x2/-1, y2/-1);
                    // Draw the mirrored line
                    //g.drawLine(-mx1/-1, -my1/-1, -mx2/-1, -my2/-1);
                    g.drawLine((mx1+10)/-1, (my1+10)/-1, (mx2+10)/-1, (my2+10)/-1);
                }
            }
    
}









void WaveformDisplay::update() {
    repaint();
}
