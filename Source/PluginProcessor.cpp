/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterRegistry.h"
#include "ParameterSnapshotCapture.h"
#include "StateContract.h"
#include <algorithm>
#include <cmath>
#include "Oscillator.h"
#include "LadderFilter.h"

namespace {
Oscillator::Waveform mapOsc1Waveform(int selection) {
    switch (selection) {
        case 0: return Oscillator::Triangle;
        case 1: return Oscillator::Sharktooth; // Triangle/Saw hybrid for Osc 1/2
        case 2: return Oscillator::Sawtooth;
        case 3: return Oscillator::Square;
        case 4: return Oscillator::WideRectangle;
        case 5: return Oscillator::NarrowRectangle;
        default: return Oscillator::Triangle;
    }
}

Oscillator::Waveform mapOsc3Waveform(int selection) {
    switch (selection) {
        case 0: return Oscillator::Triangle;
        case 1: return Oscillator::ReverseSaw; // Reverse saw only on Osc 3
        case 2: return Oscillator::Sawtooth;
        case 3: return Oscillator::Square;
        case 4: return Oscillator::WideRectangle;
        case 5: return Oscillator::NarrowRectangle;
        default: return Oscillator::Triangle;
    }
}

} // namespace


//==============================================================================
MoogMiniAudioProcessor::MoogMiniAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
: AudioProcessor (BusesProperties()
                  .withInput  ("External Input", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Main Output", juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Phones/Cue", juce::AudioChannelSet::stereo(), false)
                  ),
apvts(*this, nullptr, "Parameters", createParameterLayout()),
circularBuffer(1024)
#endif
{
    
    osc1.shouldApplyDetune = false;
    osc2.shouldApplyDetune = true;
    osc3.shouldApplyDetune = true;
    
    a440Oscillator = juce::dsp::Oscillator<float>([](float x) { return std::sin(x); });
    
    osc1.setTune(Oscillator::Tune::Zero);
    
    currentNoteNumber = -1; // Initialize current note number
    canonicalState = StateContract::makeNativeState (apvts.copyState());
    std::array<float, ParameterRegistry::parameterCount> initialValues {};
    for (std::size_t index = 0; index < preparedParameterHandles.size(); ++index)
    {
        const auto& descriptor = ParameterRegistry::descriptor (
            static_cast<ParameterRegistry::Key> (index));
        const auto id = juce::String { descriptor.id.data(), descriptor.id.size() };
        preparedParameterHandles[index] = apvts.getRawParameterValue (id);
        preparedParameters[index] = apvts.getParameter (id);
        jassert (preparedParameterHandles[index] != nullptr);
        jassert (preparedParameters[index] != nullptr);
        initialValues[index] = preparedParameterHandles[index]->load (std::memory_order_relaxed);
    }
    initialCoherentParameterSnapshot = buildTypedSnapshot (
        initialValues, StateContract::ContourContract::canonicalContours, 0);
    lastCoherentParameterSnapshot = initialCoherentParameterSnapshot;
    
}

MoogMiniAudioProcessor::~MoogMiniAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MoogMiniAudioProcessor::createParameterLayout()
{
    return ParameterRegistry::createParameterLayout();
}

CircularBuffer& MoogMiniAudioProcessor::getCircularBuffer() {
    return circularBuffer;
}

MoogMiniAudioProcessor::SignalLevels MoogMiniAudioProcessor::getSignalLevels() const {
    SignalLevels levels;
    levels.osc1Raw = osc1RawLevel.load(std::memory_order_relaxed);
    levels.osc2Raw = osc2RawLevel.load(std::memory_order_relaxed);
    levels.osc3Raw = osc3RawLevel.load(std::memory_order_relaxed);
    levels.osc1 = osc1Level.load(std::memory_order_relaxed);
    levels.osc2 = osc2Level.load(std::memory_order_relaxed);
    levels.osc3 = osc3Level.load(std::memory_order_relaxed);
    levels.noise = noiseLevel.load(std::memory_order_relaxed);
    levels.mix = mixLevel.load(std::memory_order_relaxed);
    levels.filter = filterLevel.load(std::memory_order_relaxed);
    levels.output = outputLevel.load(std::memory_order_relaxed);
    levels.modOsc = modOscLevel.load(std::memory_order_relaxed);
    levels.modFilter = modFilterLevel.load(std::memory_order_relaxed);
    return levels;
}

float MoogMiniAudioProcessor::getFilterEnvelopeValue() const {
    return filterEnvelopeLevel.load(std::memory_order_relaxed);
}

float MoogMiniAudioProcessor::getContourEnvelopeValue() const {
    return contourEnvelopeLevel.load(std::memory_order_relaxed);
}

void MoogMiniAudioProcessor::copyStageBuffers(StageBuffers& dest) const {
    auto copyRing = [](const std::array<float, stageBufferSize>& source,
                       std::array<float, stageBufferSize>& target,
                       int writeIndex) {
        for (int i = 0; i < stageBufferSize; ++i) {
            int index = writeIndex + i;
            if (index >= stageBufferSize) {
                index -= stageBufferSize;
            }
            target[static_cast<size_t>(i)] = source[static_cast<size_t>(index)];
        }
    };

    const int writeIndex = stageBufferWriteIndex.load(std::memory_order_acquire);
    copyRing(stageOsc1RawBuffer, dest.osc1Raw, writeIndex);
    copyRing(stageOsc2RawBuffer, dest.osc2Raw, writeIndex);
    copyRing(stageOsc3RawBuffer, dest.osc3Raw, writeIndex);
    copyRing(stageOsc1Buffer, dest.osc1, writeIndex);
    copyRing(stageOsc2Buffer, dest.osc2, writeIndex);
    copyRing(stageOsc3Buffer, dest.osc3, writeIndex);
    copyRing(stageNoiseBuffer, dest.noise, writeIndex);
    copyRing(stageMixBuffer, dest.mix, writeIndex);
    copyRing(stageFilterBuffer, dest.filter, writeIndex);
    copyRing(stageOutputBuffer, dest.output, writeIndex);
    copyRing(stageModOscBuffer, dest.modOsc, writeIndex);
    copyRing(stageModFilterBuffer, dest.modFilter, writeIndex);
}

const juce::String MoogMiniAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MoogMiniAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool MoogMiniAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool MoogMiniAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

void MoogMiniAudioProcessor::handleNoteOn(int midiChannel, int midiNoteNumber, float velocity) {
    const juce::ScopedLock lock(midiCriticalSection);
    incomingMidi.addEvent(juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity), 0);
}

void MoogMiniAudioProcessor::handleNoteOff(int midiChannel, int midiNoteNumber, float velocity) {
    const juce::ScopedLock lock(midiCriticalSection);
    incomingMidi.addEvent(juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity), 0);
}



double MoogMiniAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MoogMiniAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int MoogMiniAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MoogMiniAudioProcessor::setCurrentProgram (int)
{
}

const juce::String MoogMiniAudioProcessor::getProgramName (int)
{
    return {};
}

void MoogMiniAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void MoogMiniAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto oscillatorSampleRate = static_cast<float>(sampleRate);
    osc1.setSampleRate(oscillatorSampleRate);
    osc2.setSampleRate(oscillatorSampleRate);
    osc3.setSampleRate(oscillatorSampleRate);
    a440Oscillator.setFrequency(440.0f);
    a440Oscillator.prepare({ sampleRate,
                             static_cast<juce::uint32>(samplesPerBlock),
                             static_cast<juce::uint32>(getChannelCountOfBus(false, 0)) });
    
}

void MoogMiniAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MoogMiniAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 2)
        return false;

    const std::array mainLayouts {
        juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()
    };
    const std::array optionalLayouts {
        juce::AudioChannelSet::disabled(),
        juce::AudioChannelSet::mono(),
        juce::AudioChannelSet::stereo()
    };
    const auto matches = [] (const juce::AudioChannelSet& layout, const auto& table)
    {
        return std::find (table.begin(), table.end(), layout) != table.end();
    };

    return matches (layouts.outputBuses[0], mainLayouts)
        && matches (layouts.inputBuses[0], optionalLayouts)
        && matches (layouts.outputBuses[1], optionalLayouts);
}
#endif

// Add these member functions to MoogMiniAudioProcessor class
float MoogMiniAudioProcessor::generateWhiteNoise() {
    return random.nextFloat() * 2.0f - 1.0f; // Generates a random value between -1.0 and 1.0
}

float MoogMiniAudioProcessor::generatePinkNoise() {
    // A simple algorithm for pink noise generation
    float white = generateWhiteNoise();
    
    // Update the state of the filter
    pinkNoiseState[0] = 0.99886f * pinkNoiseState[0] + white * 0.0555179f;
    pinkNoiseState[1] = 0.99332f * pinkNoiseState[1] + white * 0.0750759f;
    pinkNoiseState[2] = 0.96900f * pinkNoiseState[2] + white * 0.1538520f;
    pinkNoiseState[3] = 0.86650f * pinkNoiseState[3] + white * 0.3104856f;
    pinkNoiseState[4] = 0.55000f * pinkNoiseState[4] + white * 0.5329522f;
    pinkNoiseState[5] = -0.7616f * pinkNoiseState[5] - white * 0.0168980f;
    
    float pink = pinkNoiseState[0] + pinkNoiseState[1] + pinkNoiseState[2] + pinkNoiseState[3]
    + pinkNoiseState[4] + pinkNoiseState[5] + pinkNoiseState[6] + white * 0.5362f;
    pinkNoiseState[6] = white * 0.115926f;
    
    return pink * 0.11f; // Scaling factor for normalizing
}

float MoogMiniAudioProcessor::generateRedNoise() {
    const float white = generateWhiteNoise();
    // Simple leaky integrator for red (Brownian) noise.
    redNoiseState = juce::jlimit(-1.0f, 1.0f, redNoiseState + (white * 0.02f));
    return redNoiseState;
}

float MoogMiniAudioProcessor::normalizedToMilliseconds(float normalizedValue) {
    if(normalizedValue == 0){
        return 0.01f * 10000.0f;
    } else {
        return normalizedValue * 10000.0f;
    }
    
}
void MoogMiniAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto externalInput = getBusBuffer (buffer, true, 0);
    auto mainOutput = getBusBuffer (buffer, false, 0);
    auto phonesOutput = getBusBuffer (buffer, false, 1);
    
    // Merge incoming MIDI messages at the beginning of the processing block
    {
        const juce::ScopedLock lock(midiCriticalSection);
        midiMessages.addEvents(incomingMidi, 0, buffer.getNumSamples(), 0);
        incomingMidi.clear();
    }
    
    // Calculate elapsed time in seconds for this block
    const float elapsedTime = static_cast<float>(
        static_cast<double>(buffer.getNumSamples()) / getSampleRate());
    logTimer += elapsedTime;
    
    // Retrieve one complete prepared parameter snapshot for this render block.
    const auto parameterSnapshot = captureParameterSnapshot (lastCoherentParameterSnapshot);
    if (! parameterSnapshot.usedFallback)
        lastCoherentParameterSnapshot = parameterSnapshot;
    const int waveformSelectionOsc1 = parameterSnapshot.oscillator1.waveform;
    const int waveformSelectionOsc2 = parameterSnapshot.oscillator2.waveform;
    const int waveformSelectionOsc3 = parameterSnapshot.oscillator3.waveform;
    const int rangeSelectionOsc1 = parameterSnapshot.oscillator1.range;
    const int rangeSelectionOsc2 = parameterSnapshot.oscillator2.range;
    const int rangeSelectionOsc3 = parameterSnapshot.oscillator3.range;
    const int freqSelectionOsc2 = parameterSnapshot.oscillator2.detune;
    const int freqSelectionOsc3 = parameterSnapshot.oscillator3.detune;
    const int volSelectionOsc1 = parameterSnapshot.oscillator1.level;
    const int volSelectionOsc2 = parameterSnapshot.oscillator2.level;
    const int volSelectionOsc3 = parameterSnapshot.oscillator3.level;
    const int tuneSelectionOsc1 = parameterSnapshot.masterTune;
    const bool osc1OnOff = parameterSnapshot.oscillator1.enabled;
    const bool osc2OnOff = parameterSnapshot.oscillator2.enabled;
    const bool osc3OnOff = parameterSnapshot.oscillator3.enabled;
    const bool a440HzOnOff = parameterSnapshot.a440Enabled;
    const bool whitePink = parameterSnapshot.pinkNoise;
    const bool noiseOnOff = parameterSnapshot.noiseEnabled;
    const int noiseVolume = parameterSnapshot.noiseLevel;
    const bool extInputOn = parameterSnapshot.externalInputEnabled;
    const int extInputVolLevel = parameterSnapshot.externalInputLevel;
    const float filterCutoffValue = mapFilterCutoffValueToFrequency (
        parameterSnapshot.filterCutoff);
    const int filterEmphasisValue = parameterSnapshot.filterEmphasis;
    const int filterContourValue = parameterSnapshot.filterContour;
    const auto routed = ContourRouting::route (parameterSnapshot.contourContract,
                                               parameterSnapshot.contours);
    const bool keyboardCtrlSwitch1Value = parameterSnapshot.keyboardControl1;
    const bool keyboardCtrlSwitch2Value = parameterSnapshot.keyboardControl2;
    const bool filterModSwitchValue = parameterSnapshot.filterModulationEnabled;
    const bool osc3CtrlMode = parameterSnapshot.oscillator3KeyboardControl;
    const float osc3ModulationValue = parameterSnapshot.modulationWheel;
    const bool oscModSwitchValue = parameterSnapshot.oscillatorModulationEnabled;
    const int modulationMixLevel = parameterSnapshot.modulationMix;
    const int outputVolLevel = parameterSnapshot.outputLevel;
    const bool glideSwitchValue = parameterSnapshot.glideEnabled;
    const int glideValue = parameterSnapshot.glide;
    const float pitchWheelValue = parameterSnapshot.pitchWheel;
    const float feedbackValue = parameterSnapshot.feedback;
    
    // Set the ladder filter parameters based on the prepared physical values.
    ladderFilter.setCutoffFrequency(filterCutoffValue);
    ladderFilter.setResonance(static_cast<float>(filterEmphasisValue) / 10.0f); // Assuming range 0-10
    ladderFilter.setEnvelopeAmount(static_cast<float>(filterContourValue) / 10.0f); // Assuming range 0-10
    ladderFilter.setSampleRate(static_cast<float>(getSampleRate()));
    ladderFilter.setFeedback(feedbackValue);
    // Legacy LadderFilter naming: setEnvelopeSettings is the physical Filter envelope;
    // setContourEnvelopeSettings is the contour used as the physical VCA/Loudness multiplier.
    ladderFilter.setEnvelopeSettings (normalizedToMilliseconds (routed.filter.attack),
                                      normalizedToMilliseconds (routed.filter.decay),
                                      routed.filter.sustain);
    ladderFilter.setContourEnvelopeSettings (normalizedToMilliseconds (routed.loudness.attack),
                                             normalizedToMilliseconds (routed.loudness.decay),
                                             routed.loudness.sustain);
    
    
    osc1.setWaveform(mapOsc1Waveform(waveformSelectionOsc1));
    osc1.setRange(static_cast<Oscillator::Range>(rangeSelectionOsc1));
    osc1.setVolume(static_cast<Oscillator::Volume>(volSelectionOsc1));
    const auto masterTune = static_cast<Oscillator::Tune>(tuneSelectionOsc1);
    osc1.setTune(masterTune);
    
    osc2.setWaveform(mapOsc1Waveform(waveformSelectionOsc2));
    osc2.setRange(static_cast<Oscillator::Range>(rangeSelectionOsc2));
    osc2.setVolume(static_cast<Oscillator::Volume>(volSelectionOsc2));
    osc2.setTune(masterTune);
    osc2.setDetuneAmount(static_cast<Oscillator::Frequency>(freqSelectionOsc2));
    
    osc3.setWaveform(mapOsc3Waveform(waveformSelectionOsc3));
    osc3.setRange(static_cast<Oscillator::Range>(rangeSelectionOsc3));
    osc3.setVolume(static_cast<Oscillator::Volume>(volSelectionOsc3));
    osc3.setTune(masterTune);
    osc3.setDetuneAmount(static_cast<Oscillator::Frequency>(freqSelectionOsc3));
    
    // Conditional logging
    if (logTimer >= logInterval) {
        // Your log statements here
        // Reset the timer
        logTimer = 0.0f;
    }
    
    const float keyTrackingAmount =
        (keyboardCtrlSwitch1Value ? (1.0f / 3.0f) : 0.0f)
        + (keyboardCtrlSwitch2Value ? (2.0f / 3.0f) : 0.0f);
    const float referenceFrequency = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(referenceNote));
    
    
    for (const auto midiMessage : midiMessages)
    {
        auto message = midiMessage.getMessage();
        
        if (message.isNoteOn())
        {
            // Handle note on
            currentNoteNumber = message.getNoteNumber(); // Update current note number
            
            const float frequency = static_cast<float>(
                juce::MidiMessage::getMidiNoteInHertz(currentNoteNumber));
            
            const float newFrequency = static_cast<float>(
                juce::MidiMessage::getMidiNoteInHertz(message.getNoteNumber()));
            if (glideSwitchValue && newFrequency != currentGlideFrequency) {
                targetFrequency = newFrequency;
                isGlideActive = true;
            } else {
                currentGlideFrequency = targetFrequency = newFrequency; // Directly set frequency if glide is off
            }
            
            // Start Oscillator 1 and 2 with the calculated frequency
            if(osc1OnOff){
                osc1.start(frequency);
            }
            if(osc2OnOff){
                osc2.start(frequency);
            }
            
            if (osc3OnOff && osc3CtrlMode) {
                osc3.start(frequency);
            }
            
            ladderFilter.noteOn();
            noteOffOccurred = false;
            timeSinceNoteOff = 0.0; // Reset the timer
        }
        else if (message.isNoteOff())
        {
            // Handle note off
            if (message.getNoteNumber() == currentNoteNumber)
            {
                osc1.stop();
                osc2.stop();
                
                if (osc3OnOff && osc3CtrlMode) {
                    osc3.stop();
                }
                currentNoteNumber = -1; // Reset current note number
            }
            ladderFilter.noteOff();
            noteOffOccurred = true; // Set the flag when the current note is released
        }
    }
    
    // Map the normalized value to the range of -5 to +5
    float mappedPitchWheelValue = (pitchWheelValue * 10.0f) - 5.0f; // Maps [0, 1] to [-5, 5]

    float pitchWheelAdjustment = 1.0f; // Default to no adjustment
    if (mappedPitchWheelValue > 0) {
        // Pitch wheel is up - increase frequency by up to a fifth
        pitchWheelAdjustment = 1.0f + (mappedPitchWheelValue / 5.0f * 0.5f); // Max 1.5
    } else if (mappedPitchWheelValue < 0) {
        // Pitch wheel is down - decrease frequency by up to a fifth
        pitchWheelAdjustment = 1.0f + (mappedPitchWheelValue / 5.0f * (1.0f / 3.0f)); // Min 2/3
    }


    const float noiseVolumeScalingFactor = 0.08f;
    // Get the modulation mix level (0.0 to 1.0)
    float modulationMix = static_cast<float>(modulationMixLevel) / 10.0f;
    float glideRate = calculateGlideRate(glideValue);

    float osc1Peak = 0.0f;
    float osc2Peak = 0.0f;
    float osc3Peak = 0.0f;
    float osc1RawPeak = 0.0f;
    float osc2RawPeak = 0.0f;
    float osc3RawPeak = 0.0f;
    float noisePeak = 0.0f;
    float mixPeak = 0.0f;
    float filterPeak = 0.0f;
    float outputPeak = 0.0f;
    float modOscPeak = 0.0f;
    float modFilterPeak = 0.0f;
    const float volumeScale = 0.1f;
    const float mixScale = 0.1f;
    const float noiseScale = 1.0f / (noiseVolumeScalingFactor * 10.0f);

    int stageWriteIndex = stageBufferWriteIndex.load(std::memory_order_relaxed);
    const float* inputLeft = externalInput.getNumChannels() > 0
                           ? externalInput.getReadPointer(0) : nullptr;
    const float* inputRight = externalInput.getNumChannels() > 1
                            ? externalInput.getReadPointer(1) : inputLeft;

    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        float sampleLeft = 0.0f;
        float sampleRight = 0.0f;
        float osc1Sample = 0.0f;
        float osc2Sample = 0.0f;
        float osc3Sample = 0.0f;
        float osc1Contribution = 0.0f;
        float osc2Contribution = 0.0f;
        float osc3Contribution = 0.0f;
        float modulationSignal = 0.0f;
        float modOscSample = 0.0f;
        float modFilterSample = 0.0f;
        // Generate noise samples
        const float noiseSource = whitePink ? generatePinkNoise() : generateWhiteNoise();
        const float modNoiseSource = whitePink ? generateRedNoise() : generatePinkNoise();
        float noiseSampleLeft = 0.0f;
        float noiseSampleRight = 0.0f;
        if (noiseOnOff) {
            noiseSampleLeft = noiseSource * noiseVolume;
            noiseSampleRight = noiseSampleLeft;
            noiseSampleLeft *= noiseVolumeScalingFactor;
            noiseSampleRight *= noiseVolumeScalingFactor;
            noisePeak = juce::jmax(noisePeak, std::abs(noiseSampleLeft) * noiseScale);
        }

        float extInputSample = 0.0f;
        if (extInputOn && inputLeft != nullptr) {
            float inL = inputLeft[sampleIndex];
            float inR = inputRight ? inputRight[sampleIndex] : inL;
            extInputSample = 0.5f * (inL + inR);
            extInputSample *= static_cast<float>(extInputVolLevel);
        }

        if (isGlideActive) {
            // Glide logic
            if (glideRate <= 0.0f) {
                currentGlideFrequency = targetFrequency;
                isGlideActive = false;
            } else {
                const float glideStep = (targetFrequency - currentGlideFrequency)
                    / (glideRate * static_cast<float>(getSampleRate()));
                currentGlideFrequency += glideStep;
                
                if ((glideStep > 0 && currentGlideFrequency >= targetFrequency) ||
                    (glideStep < 0 && currentGlideFrequency <= targetFrequency)) {
                    currentGlideFrequency = targetFrequency;
                    isGlideActive = false;
                }
            }
        }
        // Apply pitch wheel without accumulating drift.
        float effectiveFrequency = currentGlideFrequency * pitchWheelAdjustment;

        // Set frequencies for oscillators
        osc1.setFrequency(effectiveFrequency);
        osc2.setFrequency(effectiveFrequency);
        const float osc3BaseFrequency = osc3CtrlMode ? effectiveFrequency : referenceFrequency;
        osc3.setFrequency(osc3BaseFrequency);

        float osc3SampleRaw = 0.0f;
        if (osc3OnOff || filterModSwitchValue || oscModSwitchValue) {
            osc3SampleRaw = osc3.processNextSample(0.0f, osc3CtrlMode);
        }

        if (filterModSwitchValue || oscModSwitchValue) {
            modulationSignal = (1.0f - modulationMix) * osc3SampleRaw
                + modulationMix * modNoiseSource;
        }

        float pitchModulationEffect = 0.0f;
        if (oscModSwitchValue) {
            pitchModulationEffect = modulationSignal * osc3ModulationValue;
            pitchModulationEffect = juce::jlimit(-0.9f, 0.9f, pitchModulationEffect);
            modOscSample = pitchModulationEffect;
            modOscPeak = juce::jmax(modOscPeak, std::abs(modOscSample));
        }

        const float oscModAmount = oscModSwitchValue ? pitchModulationEffect : 0.0f;

        if (osc1OnOff && osc1.isActive()) {
            osc1Sample = osc1.processNextSample(oscModAmount, false);
            osc1RawPeak = juce::jmax(osc1RawPeak, std::abs(osc1Sample));
            osc1Contribution = osc1Sample * volSelectionOsc1;
            sampleLeft += osc1Contribution;
            sampleRight += osc1Contribution;
            osc1Peak = juce::jmax(osc1Peak, std::abs(osc1Sample) * (volSelectionOsc1 * volumeScale));
        }
        if (osc2OnOff && osc2.isActive()) {
            osc2Sample = osc2.processNextSample(oscModAmount, false);
            osc2RawPeak = juce::jmax(osc2RawPeak, std::abs(osc2Sample));
            osc2Contribution = osc2Sample * volSelectionOsc2;
            sampleLeft += osc2Contribution;
            sampleRight += osc2Contribution;
            osc2Peak = juce::jmax(osc2Peak, std::abs(osc2Sample) * (volSelectionOsc2 * volumeScale));
        }
        if (osc3OnOff) {
            osc3Sample = osc3SampleRaw;
            osc3RawPeak = juce::jmax(osc3RawPeak, std::abs(osc3Sample));
            osc3Contribution = osc3Sample * volSelectionOsc3;
            sampleLeft += osc3Contribution;
            sampleRight += osc3Contribution;
            osc3Peak = juce::jmax(osc3Peak, std::abs(osc3Sample) * (volSelectionOsc3 * volumeScale));
        }

        sampleLeft += noiseSampleLeft + extInputSample;
        sampleRight += noiseSampleRight + extInputSample;

        float baseCutoff = filterCutoffValue;
        if (keyTrackingAmount > 0.0f && referenceFrequency > 0.0f) {
            float ratio = effectiveFrequency / referenceFrequency;
            if (ratio > 0.0f) {
                baseCutoff *= std::pow(ratio, keyTrackingAmount);
            }
        }

        float cutoffForFilter = baseCutoff;
        if (filterModSwitchValue) {
            float filterModulationSignal = modulationSignal * osc3ModulationValue;
            float scaledFilterModulationSignal = filterModulationSignal * 5000.0f;
            modFilterSample = filterModulationSignal;
            modFilterPeak = juce::jmax(modFilterPeak, std::abs(modFilterSample));
            cutoffForFilter = baseCutoff + scaledFilterModulationSignal;
        }
        cutoffForFilter = juce::jlimit(0.0f, 20000.0f, cutoffForFilter);
        ladderFilter.setCutoffFrequency(cutoffForFilter);

        float monoSample = (sampleLeft + sampleRight) * 0.5f;
        mixPeak = juce::jmax(mixPeak, std::abs(monoSample) * mixScale);

        float filteredSample = 0.0f;
        ladderFilter.process(&monoSample, &filteredSample, 1); // Process as mono
        filterPeak = juce::jmax(filterPeak, std::abs(filteredSample) * mixScale);

        float loudnessEnvValue = ladderFilter.getContourEnvelopeValue();
        filteredSample *= loudnessEnvValue;

        sampleLeft = filteredSample;
        sampleRight = filteredSample;

        if (a440HzOnOff) {
            float a440Sample = a440Oscillator.processSample(0.0f);
            a440Sample *= noiseVolumeScalingFactor;
            sampleLeft += a440Sample;
            sampleRight += a440Sample;
        }
        
        // Apply the main output volume control to both channels
        sampleLeft *= outputVolLevel;
        sampleRight *= outputVolLevel;
        outputPeak = juce::jmax(outputPeak, std::abs(sampleLeft) * mixScale);
        
        // The engine is mono. Render it as mono or dual mono to the named output buses.
        for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
            mainOutput.getWritePointer(channel)[sampleIndex] = sampleLeft;

        for (int channel = 0; channel < phonesOutput.getNumChannels(); ++channel)
            phonesOutput.getWritePointer(channel)[sampleIndex] = sampleLeft;
        
        // Write to circular buffer for oscilloscope display
        circularBuffer.write(sampleLeft, sampleRight); // Assuming circularBuffer.write() is modified for stereo
        if (osc1OnOff && osc1.isActive()) {
            osc1Buffer[sampleIndex % waveformBufferSize] = osc1Sample;
        }
        if (osc2OnOff && osc2.isActive()) {
            osc2Buffer[sampleIndex % waveformBufferSize] = osc2Sample;
        }
        if (osc3OnOff && osc3.isActive()) {
            osc3Buffer[sampleIndex % waveformBufferSize] = osc3Sample;
        }

        stageOsc1Buffer[static_cast<size_t>(stageWriteIndex)] = osc1Contribution;
        stageOsc2Buffer[static_cast<size_t>(stageWriteIndex)] = osc2Contribution;
        stageOsc3Buffer[static_cast<size_t>(stageWriteIndex)] = osc3Contribution;
        stageOsc1RawBuffer[static_cast<size_t>(stageWriteIndex)] = osc1Sample;
        stageOsc2RawBuffer[static_cast<size_t>(stageWriteIndex)] = osc2Sample;
        stageOsc3RawBuffer[static_cast<size_t>(stageWriteIndex)] = osc3Sample;
        stageNoiseBuffer[static_cast<size_t>(stageWriteIndex)] = noiseSampleLeft;
        stageMixBuffer[static_cast<size_t>(stageWriteIndex)] = monoSample;
        stageFilterBuffer[static_cast<size_t>(stageWriteIndex)] = filteredSample;
        stageOutputBuffer[static_cast<size_t>(stageWriteIndex)] = sampleLeft;
        stageModOscBuffer[static_cast<size_t>(stageWriteIndex)] = modOscSample;
        stageModFilterBuffer[static_cast<size_t>(stageWriteIndex)] = modFilterSample;

        stageWriteIndex++;
        if (stageWriteIndex >= stageBufferSize) {
            stageWriteIndex = 0;
        }
    }

    auto updateLevel = [](std::atomic<float>& target, float peak) {
        float current = target.load(std::memory_order_relaxed);
        float decayed = current * 0.85f;
        float next = peak > decayed ? peak : decayed;
        target.store(juce::jlimit(0.0f, 1.0f, next), std::memory_order_relaxed);
    };

    updateLevel(osc1Level, osc1Peak);
    updateLevel(osc2Level, osc2Peak);
    updateLevel(osc3Level, osc3Peak);
    updateLevel(osc1RawLevel, osc1RawPeak);
    updateLevel(osc2RawLevel, osc2RawPeak);
    updateLevel(osc3RawLevel, osc3RawPeak);
    updateLevel(noiseLevel, noisePeak);
    updateLevel(mixLevel, mixPeak);
    updateLevel(filterLevel, filterPeak);
    updateLevel(outputLevel, outputPeak);
    updateLevel(modOscLevel, modOscPeak);
    updateLevel(modFilterLevel, modFilterPeak);

    filterEnvelopeLevel.store(ladderFilter.getEnvelopeValue(), std::memory_order_relaxed);
    contourEnvelopeLevel.store(ladderFilter.getLastContourEnvelopeValue(), std::memory_order_relaxed);

    stageBufferWriteIndex.store(stageWriteIndex, std::memory_order_release);
}

float MoogMiniAudioProcessor::calculateGlideRate(int glideKnobValue) {
    // Glide knob range: 0 to 10
    // We map this range to a glide rate in seconds per semitone
    
    // Minimum and maximum glide times in seconds for a one-semitone interval
    const float minGlideTimePerSemitone = 0.0f; // Almost instant glide
    const float maxGlideTimePerSemitone = 0.5f; // 0.5 seconds per semitone at max setting
    
    // Calculate glide time per semitone based on knob position
    // Linear mapping: you may want to experiment with different curves (e.g., exponential)
    float glideTimePerSemitone = juce::jmap(static_cast<float>(glideKnobValue), 0.0f, 10.0f, minGlideTimePerSemitone, maxGlideTimePerSemitone);
    
    return glideTimePerSemitone;
}



float MoogMiniAudioProcessor::mapFilterCutoffValueToFrequency(float filterCutoffValue) {
    // Map the normalized range [0, 1] to frequency range [20 Hz, 20 kHz] using a logarithmic scale
    float minFreq = 20.0f;  // Minimum frequency
    float maxFreq = 20000.0f; // Maximum frequency
    float minFreqLog = log(minFreq);
    float maxFreqLog = log(maxFreq);
    
    // Calculate the logarithm of the mapped frequency
    float mappedFrequencyLog = minFreqLog + filterCutoffValue * (maxFreqLog - minFreqLog);
    
    // Exponentiate to get the frequency in Hz
    float mappedFrequency = exp(mappedFrequencyLog);
    
    // Logging for debugging
    //    juce::Logger::writeToLog("Normalized Value: " + juce::String(filterCutoffValue));
    //    juce::Logger::writeToLog("Mapped Frequency: " + juce::String(mappedFrequency));
    
    return mappedFrequency;
}

float MoogMiniAudioProcessor::calculateDecayEffect(double timeElapsed, float decayTimeMilliseconds) {
    // Normalize the time elapsed based on the decay time
    float normalizedTime = static_cast<float>(timeElapsed) / (decayTimeMilliseconds / 1000.0f);
    // Calculate the decay effect (this is a simple linear decay, adjust as needed)
    return normalizedTime; // This should be a value between 0 and 1
}





//==============================================================================
bool MoogMiniAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MoogMiniAudioProcessor::createEditor()
{
    return new MoogMiniAudioProcessorEditor (*this);
}

//==============================================================================
void MoogMiniAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock { statePublicationLock };
    StateContract::serialiseBinary (
        StateContract::withCurrentParameters (canonicalState, apvts.copyState()),
        destData);
}

void MoogMiniAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    static_cast<void> (restoreState (data, sizeInBytes));
}

StateContract::RestoreResult MoogMiniAudioProcessor::restoreState (const void* data,
                                                                   int sizeInBytes)
{
    auto prepared = StateContract::parseAndPrepare (data, sizeInBytes);
    if (! prepared.result.succeeded())
        return prepared.result;

    const auto preparedContour = StateContract::contourContract (prepared.canonicalState);
    const juce::ScopedLock lock { statePublicationLock };
    const auto generation = stateGeneration.load (std::memory_order_relaxed);
    stateGeneration.store (generation + 1, std::memory_order_release);
    apvts.replaceState (prepared.apvtsState);
    canonicalState = std::move (prepared.canonicalState);
    contourContractCache.store (preparedContour, std::memory_order_release);
    restoredStateFromHost.store (true, std::memory_order_release);
    contourConversionUndoState.reset();
    stateGeneration.store (generation + 2, std::memory_order_release);
    return prepared.result;
}

StateContract::ContourContract MoogMiniAudioProcessor::getContourContract() const noexcept
{
    return contourContractCache.load (std::memory_order_acquire);
}

bool MoogMiniAudioProcessor::shouldAutoLoadLastPreset() const {
    return ! restoredStateFromHost.load (std::memory_order_acquire);
}

float MoogMiniAudioProcessor::sanitizeParameterValue (ParameterRegistry::Key key,
                                                       float value) const noexcept
{
    const auto& metadata = ParameterRegistry::descriptor (key);
    if (! std::isfinite (value))
    {
        value = metadata.physicalDefault;
        invalidParameterValueCount.fetch_add (1, std::memory_order_relaxed);
    }

    value = std::clamp (value, metadata.rangeStart, metadata.rangeEnd);
    if (metadata.kind == ParameterRegistry::Kind::choice)
        return std::round (value);
    if (metadata.kind == ParameterRegistry::Kind::boolean)
        return value >= 0.5f ? 1.0f : 0.0f;
    return value;
}

MoogMiniAudioProcessor::ParameterSnapshot MoogMiniAudioProcessor::buildTypedSnapshot (
    const std::array<float, ParameterRegistry::parameterCount>& values,
    StateContract::ContourContract contract,
    std::uint64_t generation) const noexcept
{
    std::array<float, ParameterRegistry::parameterCount> sanitized {};
    for (std::size_t index = 0; index < sanitized.size(); ++index)
        sanitized[index] = sanitizeParameterValue (
            static_cast<ParameterRegistry::Key> (index), values[index]);

    const auto value = [&sanitized] (ParameterRegistry::Key key) noexcept
    {
        return sanitized[ParameterRegistry::index (key)];
    };
    const auto integer = [&value] (ParameterRegistry::Key key) noexcept
    {
        return static_cast<int> (value (key));
    };
    const auto enabled = [&value] (ParameterRegistry::Key key) noexcept
    {
        return value (key) >= 0.5f;
    };

    ParameterSnapshot snapshot;
    snapshot.oscillator1 = { integer (ParameterRegistry::Key::osc1Waveform),
                             integer (ParameterRegistry::Key::osc1Range),
                             integer (ParameterRegistry::Key::osc1Vol), 8,
                             enabled (ParameterRegistry::Key::osc1OnOff) };
    snapshot.oscillator2 = { integer (ParameterRegistry::Key::osc2Waveform),
                             integer (ParameterRegistry::Key::osc2Range),
                             integer (ParameterRegistry::Key::osc2Vol),
                             integer (ParameterRegistry::Key::osc2Freq),
                             enabled (ParameterRegistry::Key::osc2OnOff) };
    snapshot.oscillator3 = { integer (ParameterRegistry::Key::osc3Waveform),
                             integer (ParameterRegistry::Key::osc3Range),
                             integer (ParameterRegistry::Key::osc3Vol),
                             integer (ParameterRegistry::Key::osc3Freq),
                             enabled (ParameterRegistry::Key::osc3OnOff) };
    snapshot.masterTune = integer (ParameterRegistry::Key::tune);
    snapshot.filterCutoff = value (ParameterRegistry::Key::filterCutoff);
    snapshot.filterEmphasis = integer (ParameterRegistry::Key::filterEmphasis);
    snapshot.filterContour = integer (ParameterRegistry::Key::filterContour);
    snapshot.contours = {
        value (ParameterRegistry::Key::filterAttackTimeKnob),
        value (ParameterRegistry::Key::filterDecayTimeKnob),
        value (ParameterRegistry::Key::filterSustainKnob) / 10.0f,
        value (ParameterRegistry::Key::loudnessAttackTimeKnob),
        value (ParameterRegistry::Key::loudnessDecayTimeKnob),
        value (ParameterRegistry::Key::loudnessSustainLevelKnob) / 10.0f,
        enabled (ParameterRegistry::Key::decaySwitch)
    };
    snapshot.noiseLevel = integer (ParameterRegistry::Key::noiseVolKnob);
    snapshot.externalInputLevel = integer (ParameterRegistry::Key::extInputVolKnob);
    snapshot.outputLevel = integer (ParameterRegistry::Key::outputVolKnob);
    snapshot.phonesLevel = integer (ParameterRegistry::Key::outputPhonesVolKnob);
    snapshot.glide = integer (ParameterRegistry::Key::ctrlGlideKnob);
    snapshot.modulationMix = integer (ParameterRegistry::Key::ctrlModMixKnob);
    snapshot.feedback = value (ParameterRegistry::Key::feedbackKnob);
    snapshot.modulationWheel = value (ParameterRegistry::Key::modWheelValue);
    snapshot.pitchWheel = value (ParameterRegistry::Key::pitchWheelValue);
    snapshot.a440Enabled = enabled (ParameterRegistry::Key::a440HzOnOff);
    snapshot.oscillator3KeyboardControl = enabled (ParameterRegistry::Key::osc3CtrlMode);
    snapshot.oscillatorModulationEnabled = enabled (ParameterRegistry::Key::oscModSwitch);
    snapshot.noiseEnabled = enabled (ParameterRegistry::Key::noiseOnOffSwitch);
    snapshot.externalInputEnabled = enabled (ParameterRegistry::Key::extInputVolSwitch);
    snapshot.pinkNoise = enabled (ParameterRegistry::Key::whitePinkSwitch);
    snapshot.filterModulationEnabled = enabled (ParameterRegistry::Key::filterModSwitch);
    snapshot.keyboardControl1 = enabled (ParameterRegistry::Key::keyboardCtrlSwitch1);
    snapshot.keyboardControl2 = enabled (ParameterRegistry::Key::keyboardCtrlSwitch2);
    snapshot.glideEnabled = enabled (ParameterRegistry::Key::glideSwitch);
    snapshot.priority = static_cast<PriorityMode> (
        integer (ParameterRegistry::Key::keyboardPriorityMode));
    snapshot.trigger = static_cast<TriggerMode> (
        integer (ParameterRegistry::Key::keyboardTriggerMode));
    snapshot.mainOutputEnabled = enabled (ParameterRegistry::Key::outputMainEnabled);
    snapshot.phonesOutputEnabled = enabled (ParameterRegistry::Key::outputPhonesEnabled);
    snapshot.contourContract = contract;
    snapshot.generation = generation;
    snapshot.coherent = true;
    return snapshot;
}

MoogMiniAudioProcessor::ParameterSnapshot MoogMiniAudioProcessor::captureParameterSnapshot (
    const ParameterSnapshot& fallback) const noexcept
{
    return ParameterSnapshotCapture::capture (
        fallback, initialCoherentParameterSnapshot,
        [this]() noexcept
        {
            return stateGeneration.load (std::memory_order_acquire);
        },
        [this]() noexcept
        {
            std::array<float, ParameterRegistry::parameterCount> values {};
            for (std::size_t index = 0; index < values.size(); ++index)
                values[index] = preparedParameterHandles[index]->load (
                    std::memory_order_relaxed);
            return values;
        },
        [this]() noexcept
        {
            return contourContractCache.load (std::memory_order_acquire);
        },
        [this](const auto& values, StateContract::ContourContract contract,
               std::uint64_t generation) noexcept
        {
            return buildTypedSnapshot (values, contract, generation);
        });
}

juce::RangedAudioParameter* MoogMiniAudioProcessor::getPreparedParameter (
    ParameterRegistry::Key key) const noexcept
{
    return preparedParameters[ParameterRegistry::index (key)];
}

std::uint64_t MoogMiniAudioProcessor::getInvalidParameterValueCount() const noexcept
{
    return invalidParameterValueCount.load (std::memory_order_relaxed);
}

MoogMiniAudioProcessor::ContourSnapshot MoogMiniAudioProcessor::captureContourSnapshot (
    const ContourSnapshot& fallback) const noexcept
{
    const auto complete = captureParameterSnapshot (initialCoherentParameterSnapshot);
    if (complete.usedFallback && fallback.coherent)
    {
        auto result = fallback;
        result.usedFallback = true;
        return result;
    }
    return { complete.contours, complete.contourContract, complete.generation,
             complete.usedFallback, complete.coherent };
}

MoogMiniAudioProcessor::ContourSnapshot MoogMiniAudioProcessor::getSignalFlowContourSnapshot (
    const ContourSnapshot& fallback) const noexcept
{
    return captureContourSnapshot (fallback);
}

void MoogMiniAudioProcessor::setSignalFlowContourControl (
    ContourRouting::SemanticContour contour, ContourRouting::Stage stage,
    float normalisedValue)
{
    const auto parameter = ContourRouting::parameterFor (getContourContract(), contour, stage);
    const auto key = [&]
    {
        switch (parameter)
        {
            case ContourRouting::Parameter::filterAttack: return ParameterRegistry::Key::filterAttackTimeKnob;
            case ContourRouting::Parameter::filterDecay: return ParameterRegistry::Key::filterDecayTimeKnob;
            case ContourRouting::Parameter::filterSustain: return ParameterRegistry::Key::filterSustainKnob;
            case ContourRouting::Parameter::loudnessAttack: return ParameterRegistry::Key::loudnessAttackTimeKnob;
            case ContourRouting::Parameter::loudnessDecay: return ParameterRegistry::Key::loudnessDecayTimeKnob;
            case ContourRouting::Parameter::loudnessSustain: return ParameterRegistry::Key::loudnessSustainLevelKnob;
            case ContourRouting::Parameter::count: break;
        }
        return ParameterRegistry::Key::filterAttackTimeKnob;
    }();
    if (auto* ranged = getPreparedParameter (key))
    {
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalisedValue));
        ranged->endChangeGesture();
    }
}

MoogMiniAudioProcessor::ContourTrace MoogMiniAudioProcessor::getContourTrace (
    const ContourSnapshot& fallback) const noexcept
{
    const auto snapshot = captureContourSnapshot (fallback);
    return { ContourRouting::route (snapshot.contract, snapshot.controls), snapshot.contract,
             snapshot.generation, snapshot.usedFallback };
}

std::uint64_t MoogMiniAudioProcessor::getStateGeneration() const noexcept
{
    return stateGeneration.load (std::memory_order_acquire);
}

std::string_view MoogMiniAudioProcessor::ContourConversionResult::codeString() const noexcept
{
    switch (code)
    {
        case ContourConversionCode::cancelled: return "cancelled";
        case ContourConversionCode::converted: return "converted";
        case ContourConversionCode::alreadyCanonical: return "already_canonical";
        case ContourConversionCode::undoRestored: return "undo_restored";
        case ContourConversionCode::noUndoAvailable: return "no_undo_available";
    }
    return "invalid_conversion_result";
}

std::string_view MoogMiniAudioProcessor::ContourConversionResult::warningString() const noexcept
{
    return warning == ContourConversionWarning::hostAutomationNotRewritten
             ? "host_automation_not_rewritten" : "none";
}

MoogMiniAudioProcessor::ContourConversionResult MoogMiniAudioProcessor::convertLegacyContours (
    ContourConversionRequest request)
{
    const auto warning = request.automation == HostAutomationKnowledge::knownAbsent
                           ? ContourConversionWarning::none
                           : ContourConversionWarning::hostAutomationNotRewritten;
    if (getContourContract() == StateContract::ContourContract::canonicalContours)
        return { ContourConversionCode::alreadyCanonical, ContourConversionWarning::none };
    if (! request.confirmed)
        return { ContourConversionCode::cancelled, warning };

    const juce::ScopedLock lock { statePublicationLock };
    if (contourContractCache.load (std::memory_order_acquire)
          == StateContract::ContourContract::canonicalContours)
        return { ContourConversionCode::alreadyCanonical, ContourConversionWarning::none };
    auto before = StateContract::withCurrentParameters (canonicalState, apvts.copyState());
    auto prepared = StateContract::prepareLegacyContourConversion (before);
    if (! prepared.result.succeeded())
        return { ContourConversionCode::cancelled, warning };
    juce::MemoryBlock undoBytes;
    StateContract::serialiseBinary (before, undoBytes);
    const auto generation = stateGeneration.load (std::memory_order_relaxed);
    stateGeneration.store (generation + 1, std::memory_order_release);
    apvts.replaceState (prepared.apvtsState);
    canonicalState = std::move (prepared.canonicalState);
    contourContractCache.store (StateContract::ContourContract::canonicalContours,
                                std::memory_order_release);
    contourConversionUndoLifecycle = restoredStateFromHost.load (std::memory_order_acquire);
    contourConversionUndoState = std::move (undoBytes);
    stateGeneration.store (generation + 2, std::memory_order_release);
    return { ContourConversionCode::converted, warning };
}

MoogMiniAudioProcessor::ContourConversionResult MoogMiniAudioProcessor::convertLegacyContours()
{
    return convertLegacyContours (ContourConversionRequest {});
}

MoogMiniAudioProcessor::ContourConversionResult MoogMiniAudioProcessor::undoContourConversion()
{
    const juce::ScopedLock lock { statePublicationLock };
    if (contourConversionUndoState.isEmpty())
        return { ContourConversionCode::noUndoAvailable, ContourConversionWarning::none };
    auto prepared = StateContract::parseAndPrepare (
        contourConversionUndoState.getData(), static_cast<int> (contourConversionUndoState.getSize()));
    if (! prepared.result.succeeded())
        return { ContourConversionCode::noUndoAvailable, ContourConversionWarning::none };
    const auto generation = stateGeneration.load (std::memory_order_relaxed);
    stateGeneration.store (generation + 1, std::memory_order_release);
    apvts.replaceState (prepared.apvtsState);
    canonicalState = std::move (prepared.canonicalState);
    contourContractCache.store (StateContract::contourContract (canonicalState),
                                std::memory_order_release);
    restoredStateFromHost.store (contourConversionUndoLifecycle, std::memory_order_release);
    contourConversionUndoState.reset();
    stateGeneration.store (generation + 2, std::memory_order_release);
    return { ContourConversionCode::undoRestored, ContourConversionWarning::none };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoogMiniAudioProcessor();
}
