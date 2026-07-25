/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Oscillator.h"
#include "CircularBuffer.h"
#include "ContourRouting.h"
#include "LadderFilter.h"
#include "ModWheel.h"
#include "ParameterRegistry.h"
#include "PitchDomain.h"
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

    enum class PriorityMode : std::uint8_t { low, high, last };
    enum class TriggerMode : std::uint8_t { single, multi };

    struct OscillatorSnapshot {
        int waveform = 0;
        int range = 0;
        int level = 0;
        int detune = 8;
        bool enabled = false;
    };

    struct ParameterSnapshot {
        OscillatorSnapshot oscillator1, oscillator2, oscillator3;
        int masterTune = 5;
        float filterCutoff = 0.5f;
        int filterEmphasis = 0;
        int filterContour = 0;
        ContourRouting::ContourControls contours;
        int noiseLevel = 0;
        int externalInputLevel = 0;
        int outputLevel = 0;
        int phonesLevel = 0;
        int glide = 0;
        int modulationMix = 0;
        float feedback = 0.0f;
        float modulationWheel = 0.0f;
        float pitchWheel = 0.5f;
        bool a440Enabled = false;
        bool oscillator3KeyboardControl = false;
        bool oscillatorModulationEnabled = false;
        bool noiseEnabled = false;
        bool externalInputEnabled = false;
        bool pinkNoise = false;
        bool filterModulationEnabled = false;
        bool keyboardControl1 = false;
        bool keyboardControl2 = false;
        bool glideEnabled = false;
        PriorityMode priority = PriorityMode::low;
        TriggerMode trigger = TriggerMode::single;
        bool mainOutputEnabled = true;
        bool phonesOutputEnabled = true;
        StateContract::ContourContract contourContract =
            StateContract::ContourContract::canonicalContours;
        std::uint64_t generation = 0;
        bool usedFallback = false;
        bool coherent = false;
    };

    struct ContourSnapshot {
        ContourRouting::ContourControls controls;
        StateContract::ContourContract contract = StateContract::ContourContract::canonicalContours;
        std::uint64_t generation = 0;
        bool usedFallback = false;
        bool coherent = false;
    };
    struct ContourTrace {
        ContourRouting::RoutedContours routed;
        StateContract::ContourContract contract = StateContract::ContourContract::canonicalContours;
        std::uint64_t generation = 0;
        bool usedFallback = false;
    };
    enum class HostAutomationKnowledge { unknown, knownAbsent, knownPresent };
    struct ContourConversionRequest {
        bool confirmed = false;
        HostAutomationKnowledge automation = HostAutomationKnowledge::unknown;
    };
    enum class ContourConversionCode {
        cancelled, converted, alreadyCanonical, undoRestored, noUndoAvailable
    };
    enum class ContourConversionWarning { none, hostAutomationNotRewritten };
    struct ContourConversionResult {
        ContourConversionCode code = ContourConversionCode::cancelled;
        ContourConversionWarning warning = ContourConversionWarning::none;
        std::string_view codeString() const noexcept;
        std::string_view warningString() const noexcept;
    };

    ContourSnapshot captureContourSnapshot (const ContourSnapshot& fallback) const noexcept;
    ParameterSnapshot captureParameterSnapshot (const ParameterSnapshot& fallback) const noexcept;
    juce::RangedAudioParameter* getPreparedParameter (ParameterRegistry::Key key) const noexcept;
    std::uint64_t getInvalidParameterValueCount() const noexcept;
    ContourSnapshot getSignalFlowContourSnapshot (const ContourSnapshot& fallback) const noexcept;
    void setSignalFlowContourControl (ContourRouting::SemanticContour contour,
                                      ContourRouting::Stage stage,
                                      float normalisedValue);
    ContourTrace getContourTrace (const ContourSnapshot& fallback) const noexcept;
    std::uint64_t getStateGeneration() const noexcept;
    ContourConversionResult convertLegacyContours();
    ContourConversionResult convertLegacyContours (ContourConversionRequest request);
    ContourConversionResult undoContourConversion();
    
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
    std::atomic<std::uint64_t> stateGeneration { 0 };
    std::array<std::atomic<float>*, ParameterRegistry::parameterCount>
        preparedParameterHandles {};
    std::array<juce::RangedAudioParameter*, ParameterRegistry::parameterCount>
        preparedParameters {};
    mutable std::atomic<std::uint64_t> invalidParameterValueCount { 0 };
    ParameterSnapshot lastCoherentParameterSnapshot;
    ParameterSnapshot initialCoherentParameterSnapshot;
    juce::MemoryBlock contourConversionUndoState;
    bool contourConversionUndoLifecycle = false;
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
    std::array<float, 3> lastValidMusicalFrequency { 440.0f, 440.0f, 440.0f };

    float sanitizeParameterValue (ParameterRegistry::Key key, float value) const noexcept;
    float composeMusicalFrequency (
        PitchDomain::Checked<PitchDomain::Semitones> note,
        int rangeIndex,
        int masterTuneIndex,
        int oscillatorOffsetIndex,
        double pitchWheelNormalized,
        double modulationRatio,
        size_t oscillatorIndex,
        bool& invalidMusicalCompositionDiagnosed) noexcept;
    ParameterSnapshot buildTypedSnapshot (
        const std::array<float, ParameterRegistry::parameterCount>& values,
        StateContract::ContourContract contract,
        std::uint64_t generation) const noexcept;

    // ...
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogMiniAudioProcessor)
};
