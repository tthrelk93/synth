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
#include "StateContract.h"



//==============================================================================
/**
*/
class MoogMiniAudioProcessor  : public juce::AudioProcessor
                             , public juce::VST3ClientExtensions
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
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return this; }
    bool getPluginHasMainInput() const override { return false; }
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
    StateContract::RestoreResult restoreState (const void* data, int sizeInBytes);
    StateContract::ContourContract getContourContract() const noexcept;
    
    float mapFilterCutoffValueToFrequency(float filterCutoffValue);
    
    float generatePinkNoise();
    float generateWhiteNoise();
    float generateRedNoise();
    
    float normalizedToMilliseconds(float normalizedValue);

    static constexpr int stageBufferSize = 2048;
    struct StageBuffers {
        std::array<float, stageBufferSize> osc1Raw{};
        std::array<float, stageBufferSize> osc2Raw{};
        std::array<float, stageBufferSize> osc3Raw{};
        std::array<float, stageBufferSize> osc1{};
        std::array<float, stageBufferSize> osc2{};
        std::array<float, stageBufferSize> osc3{};
        std::array<float, stageBufferSize> noise{};
        std::array<float, stageBufferSize> mix{};
        std::array<float, stageBufferSize> filter{};
        std::array<float, stageBufferSize> output{};
        std::array<float, stageBufferSize> modOsc{};
        std::array<float, stageBufferSize> modFilter{};
    };

    void copyStageBuffers(StageBuffers& dest) const;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    const float* getWaveformData() const;
    int getWaveformSize() const;
    CircularBuffer& getCircularBuffer();

    struct SignalLevels {
        float osc1Raw = 0.0f;
        float osc2Raw = 0.0f;
        float osc3Raw = 0.0f;
        float osc1 = 0.0f;
        float osc2 = 0.0f;
        float osc3 = 0.0f;
        float noise = 0.0f;
        float mix = 0.0f;
        float filter = 0.0f;
        float output = 0.0f;
        float modOsc = 0.0f;
        float modFilter = 0.0f;
    };

    SignalLevels getSignalLevels() const;
    float getFilterEnvelopeValue() const;
    float getContourEnvelopeValue() const;
    bool shouldAutoLoadLastPreset() const;
    
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
    bool noteOffOccurred = false;
    double timeSinceNoteOff = 0.0; // Time in seconds
    
    
    juce::dsp::Oscillator<float> a440Oscillator;
    
    LadderFilter ladderFilter;
    
    ModWheel modWheel;
    
    juce::Random random;
    // Pink noise generator state
    float pinkNoiseState[7] = {0};
    float redNoiseState = 0.0f;
   
    
    const float defaultBaseFrequency = 440.0f;
    
    
    float waveformBuffer[waveformBufferSize];
    std::atomic<int> waveformBufferWritePosition{ 0 };
    
   

        std::array<float, waveformBufferSize> osc1Buffer{};
        std::array<float, waveformBufferSize> osc2Buffer{};
        std::array<float, waveformBufferSize> osc3Buffer{};
    
    CircularBuffer circularBuffer; // The CircularBuffer object
    juce::AudioBuffer<float> a440Buffer;

    std::atomic<float> osc1Level { 0.0f };
    std::atomic<float> osc2Level { 0.0f };
    std::atomic<float> osc3Level { 0.0f };
    std::atomic<float> osc1RawLevel { 0.0f };
    std::atomic<float> osc2RawLevel { 0.0f };
    std::atomic<float> osc3RawLevel { 0.0f };
    std::atomic<float> noiseLevel { 0.0f };
    std::atomic<float> mixLevel { 0.0f };
    std::atomic<float> filterLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> modOscLevel { 0.0f };
    std::atomic<float> modFilterLevel { 0.0f };
    std::atomic<float> filterEnvelopeLevel { 0.0f };
    std::atomic<float> contourEnvelopeLevel { 0.0f };

    std::array<float, stageBufferSize> stageOsc1RawBuffer{};
    std::array<float, stageBufferSize> stageOsc2RawBuffer{};
    std::array<float, stageBufferSize> stageOsc3RawBuffer{};
    std::array<float, stageBufferSize> stageOsc1Buffer{};
    std::array<float, stageBufferSize> stageOsc2Buffer{};
    std::array<float, stageBufferSize> stageOsc3Buffer{};
    std::array<float, stageBufferSize> stageNoiseBuffer{};
    std::array<float, stageBufferSize> stageMixBuffer{};
    std::array<float, stageBufferSize> stageFilterBuffer{};
    std::array<float, stageBufferSize> stageOutputBuffer{};
    std::array<float, stageBufferSize> stageModOscBuffer{};
    std::array<float, stageBufferSize> stageModFilterBuffer{};
    std::atomic<int> stageBufferWriteIndex { 0 };

    mutable juce::CriticalSection statePublicationLock;
    std::atomic<bool> restoredStateFromHost { false };
    std::atomic<StateContract::ContourContract> contourContractCache {
        StateContract::ContourContract::canonicalContours
    };
    juce::ValueTree canonicalState;
    
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
