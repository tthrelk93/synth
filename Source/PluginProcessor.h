/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Oscillator.h"
#include "CircularBuffer.h"
#include "LadderFilter.h"
#include "ModWheel.h"



//==============================================================================
/**
*/
class MoogMiniAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
   
    //==============================================================================
    MoogMiniAudioProcessor();
    ~MoogMiniAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    float mapFilterCutoffValueToFrequency(float filterCutoffValue);
    
    float generatePinkNoise();
    float generateWhiteNoise();
    
    float normalizedToMilliseconds(float normalizedValue);
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;
    juce::File logFile;
    std::unique_ptr<juce::FileLogger> fileLogger;
    
    const float* getWaveformData() const;
    int getWaveformSize() const;
    CircularBuffer& getCircularBuffer();
    
    void handleNoteOn(int midiChannel, int midiNoteNumber, float velocity);
       void handleNoteOff(int midiChannel, int midiNoteNumber, float velocity);
    Oscillator osc1;
    Oscillator osc2;
    Oscillator osc3;
    static constexpr int waveformBufferSize = 512; // Size of the buffer
    float calculateDecayEffect(double timeElapsed, float decayTimeMilliseconds);
    const std::array<float, waveformBufferSize>& getOsc1Buffer() const { return osc1Buffer; }
       const std::array<float, waveformBufferSize>& getOsc2Buffer() const { return osc2Buffer; }
       const std::array<float, waveformBufferSize>& getOsc3Buffer() const { return osc3Buffer; }
    float calculateGlideRate(int glideKnobValue);

private:
    //==============================================================================

    int currentNoteNumber;
    
    
    juce::dsp::Oscillator<float> a440Oscillator;
    
    LadderFilter ladderFilter;
    
    ModWheel modWheel;
    
    juce::Random random;
    // Pink noise generator state
    float pinkNoiseState[7] = {0};
   
    
    const float defaultBaseFrequency = 440.0f;
    
    
    float waveformBuffer[waveformBufferSize];
    std::atomic<int> waveformBufferWritePosition{ 0 };
    
   

        std::array<float, waveformBufferSize> osc1Buffer{};
        std::array<float, waveformBufferSize> osc2Buffer{};
        std::array<float, waveformBufferSize> osc3Buffer{};
    
    CircularBuffer circularBuffer; // The CircularBuffer object
    juce::AudioBuffer<float> a440Buffer;
    
    juce::MidiBuffer incomingMidi;
    juce::CriticalSection midiCriticalSection;
    
    // Assuming A4 (MIDI note 69) as the reference note for key tracking
    const int referenceNote = 69;
    
    // Timer for logging
        float logTimer = 0.0f; // in seconds
        const float logInterval = 5.0f; // log every 3 seconds
    
    float targetFrequency = 0.0f;
       float currentGlideFrequency = 440.0f;
       bool isGlideActive = false;

    // ...
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogMiniAudioProcessor)
};
