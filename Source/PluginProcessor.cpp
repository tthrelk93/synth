/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fstream>
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
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                  ),
apvts(*this, nullptr, "Parameters", createParameterLayout()),
logFile(juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("my_plugin_log.txt")),
circularBuffer(1024)
#endif
{
    
    osc1.shouldApplyDetune = false;
    osc2.shouldApplyDetune = true;
    osc3.shouldApplyDetune = true;
    
    a440Oscillator = juce::dsp::Oscillator<float>([](float x) { return std::sin(x); });
    
    osc1.setTune(Oscillator::Tune::Zero);
    
    currentNoteNumber = -1; // Initialize current note number
    
    fileLogger = std::make_unique<juce::FileLogger>(logFile, "My Plugin Log");
    juce::Logger::setCurrentLogger(fileLogger.get());
    
    // Now you can use juce::Logger::writeToLog anywhere in your code
    juce::Logger::writeToLog("Plugin constructed");
}

MoogMiniAudioProcessor::~MoogMiniAudioProcessor()
{
    
    
    juce::Logger::setCurrentLogger(nullptr);
    
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MoogMiniAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Waveform selection parameters for each oscillator
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc1Waveform", "Oscillator 1 Waveform",
                                                                  juce::StringArray{"Triangle", "Sharktooth", "Sawtooth", "Square", "WideRectangle", "NarrowRectangle"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc2Waveform", "Oscillator 2 Waveform",
                                                                  juce::StringArray{"Triangle", "Sharktooth", "Sawtooth", "Square", "WideRectangle", "NarrowRectangle"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc3Waveform", "Oscillator 3 Waveform",
                                                                  juce::StringArray{"Triangle", "ReverseSaw", "Sawtooth", "Square", "WideRectangle", "NarrowRectangle"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc1Range", "Oscillator 1 Range",
                                                                  juce::StringArray{"LO", "ThirtyTwo", "Sixteen", "Eight", "Four", "Two"}, 2));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc2Range", "Oscillator 2 Range",
                                                                  juce::StringArray{"LO", "ThirtyTwo", "Sixteen", "Eight", "Four", "Two"}, 2));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc3Range", "Oscillator 3 Range",
                                                                  juce::StringArray{"LO", "ThirtyTwo", "Sixteen", "Eight", "Four", "Two"}, 2));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc1Vol", "Oscillator 1 Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc2Vol", "Oscillator 2 Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc3Vol", "Oscillator 3 Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("tune", "Tune",
                                                                  juce::StringArray{"NegTwoHalf", "NegTwo", "NegOneHalf", "NegOne", "NegHalf", "Zero", "PosHalf", "PosOne", "PosOneHalf", "PosTwo", "PosTwoHalf"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc2Freq", "Oscillator 2 Frequency",
                                                                  juce::StringArray{"FreqNegEight", "FreqNegSeven", "FreqNegSix", "FreqNegFive", "FreqNegFour", "FreqNegThree", "FreqNegTwo", "FreqNegOne", "FreqZero", "FreqPosOne", "FreqPosTwo", "FreqPosThree", "FreqPosFour", "FreqPosFive", "FreqPosSix", "FreqPosSeven", "FreqPosEight",}, 8));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("osc3Freq", "Oscillator 3 Frequency",
                                                                  juce::StringArray{"FreqNegEight", "FreqNegSeven", "FreqNegSix", "FreqNegFive", "FreqNegFour", "FreqNegThree", "FreqNegTwo", "FreqNegOne", "FreqZero", "FreqPosOne", "FreqPosTwo", "FreqPosThree", "FreqPosFour", "FreqPosFive", "FreqPosSix", "FreqPosSeven", "FreqPosEight",}, 8));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterCutoff", "Filter Cutoff Frequency", 0.0f, 1.0f, 0.5f));
    
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("filterEmphasis", "Filter Emphasis",
                                                                  juce::StringArray{"EmphZero", "EmphOne", "EmphTwo", "EmphThree", "EmphFour", "EmphFive", "EmphSix", "EmphSeven", "EmphEight", "EmphNine", "EmphTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("filterContour", "Filter Contour",
                                                                  juce::StringArray{"ContourZero", "ContourOne", "ContourTwo", "ContourThree", "ContourFour", "ContourFive", "ContourSix", "ContourSeven", "ContourEight", "ContourNine", "ContourTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("outputVolKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("extInputVolKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ctrlGlideKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ctrlModMixKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterAttackTimeKnob", "Filter Attack Time", 0.0f, 1.0f, 0.0f));
    
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterDecayTimeKnob", "Filter Decay Time", 0.0f, 1.0f, 0.0f));
    
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("loudnessAttackTimeKnob", "Loudness Attack Time", 0.0f, 1.0f, 0.5f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("loudnessDecayTimeKnob", "Loudness Decay Time", 0.0f, 1.0f, 0.5f));
    
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("filterSustainKnob", "Filter Sustain",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("noiseVolKnob", "Noise Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("loudnessSustainLevelKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>("outputPhonesVolKnob", "Output Volume",
                                                                  juce::StringArray{"VolZero", "VolOne", "VolTwo", "VolThree", "VolFour", "VolFive", "VolSix", "VolSeven", "VolEight", "VolNine", "VolTen"}, 0));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedbackKnob", "Feedback Knob", 0.0, 1.0, 0.0));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("modWheelValue", "Mod Wheel Value", 0.0, 1.0, 0.0));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pitchWheelValue", "Pitch Wheel Value", 0.0, 1.0, 0.5));
    
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("osc1OnOff", "Oscillator 1 On/Off", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("osc2OnOff", "Oscillator 2 On/Off", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("osc3OnOff", "Oscillator 3 On/Off", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("a440HzOnOff", "A440Hz On/Off", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("osc3CtrlMode", "Oscillator 3 Control Mode", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("oscModSwitch", "Oscillator Mod Switch", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("noiseOnOffSwitch", "Noise On/Off", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("extInputVolSwitch", "External Input On/Off", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("whitePinkSwitch", "White / Pink", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("filterModSwitch", "Filter Mod Switch", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("keyboardCtrlSwitch1", "Keyboard Control Switch 1", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("keyboardCtrlSwitch2", "Keyboard Control Switch 1", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("decaySwitch", "Decay Switch", false));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("glideSwitch", "Glide Switch", false));
    
    
    
    return { params.begin(), params.end() };
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

void MoogMiniAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MoogMiniAudioProcessor::getProgramName (int index)
{
    return {};
}

void MoogMiniAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MoogMiniAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    osc1.setSampleRate(getSampleRate());
    osc2.setSampleRate(getSampleRate());
    osc3.setSampleRate(getSampleRate());
    a440Oscillator.setFrequency(440.0f);
    a440Oscillator.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumInputChannels()) });
    
}

void MoogMiniAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MoogMiniAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    
    return true;
#endif
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
    float milliseconds;
    if(normalizedValue == 0){
        return 0.01 * 10000;
    } else {
        return normalizedValue * 10000;
    }
    
}
void MoogMiniAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Merge incoming MIDI messages at the beginning of the processing block
    {
        const juce::ScopedLock lock(midiCriticalSection);
        midiMessages.addEvents(incomingMidi, 0, buffer.getNumSamples(), 0);
        incomingMidi.clear();
    }
    
    // Calculate elapsed time in seconds for this block
    float elapsedTime = buffer.getNumSamples() / getSampleRate();
    logTimer += elapsedTime;
    
    // Retrieve parameter values
    int waveformSelectionOsc1 = *apvts.getRawParameterValue("osc1Waveform");
    int waveformSelectionOsc2 = *apvts.getRawParameterValue("osc2Waveform");
    int waveformSelectionOsc3 = *apvts.getRawParameterValue("osc3Waveform");
    int rangeSelectionOsc1 = *apvts.getRawParameterValue("osc1Range");
    int rangeSelectionOsc2 = *apvts.getRawParameterValue("osc2Range");
    int rangeSelectionOsc3 = *apvts.getRawParameterValue("osc3Range");
    int freqSelectionOsc2 = *apvts.getRawParameterValue("osc2Freq");
    int freqSelectionOsc3 = *apvts.getRawParameterValue("osc3Freq");
    int volSelectionOsc1 = *apvts.getRawParameterValue("osc1Vol");
    int volSelectionOsc2 = *apvts.getRawParameterValue("osc2Vol");
    int volSelectionOsc3 = *apvts.getRawParameterValue("osc3Vol");
    int tuneSelectionOsc1 = *apvts.getRawParameterValue("tune");
    
    bool osc1OnOff = *apvts.getRawParameterValue("osc1OnOff");
    bool osc2OnOff = *apvts.getRawParameterValue("osc2OnOff");
    bool osc3OnOff = *apvts.getRawParameterValue("osc3OnOff");
    bool a440HzOnOff = *apvts.getRawParameterValue("a440HzOnOff");
    
    bool whitePink = *apvts.getRawParameterValue("whitePinkSwitch");
    bool noiseOnOff = *apvts.getRawParameterValue("noiseOnOffSwitch");
    int noiseVolume = *apvts.getRawParameterValue("noiseVolKnob");
    bool extInputOn = *apvts.getRawParameterValue("extInputVolSwitch");
    int extInputVolLevel = *apvts.getRawParameterValue("extInputVolKnob");
    
    float filterCutoffValue = mapFilterCutoffValueToFrequency(*apvts.getRawParameterValue("filterCutoff"));
    int filterEmphasisValue = *apvts.getRawParameterValue("filterEmphasis");
    int filterContourValue = *apvts.getRawParameterValue("filterContour");
    float filterAttackTimeValue = *apvts.getRawParameterValue("filterAttackTimeKnob");
    float filterAttackTimeMilliseconds = normalizedToMilliseconds(filterAttackTimeValue);
    float filterDecayTimeValue = *apvts.getRawParameterValue("filterDecayTimeKnob");
    float filterDecayTimeMilliseconds = normalizedToMilliseconds(filterDecayTimeValue);
    float filterSustainTimeValue = *apvts.getRawParameterValue("filterSustainKnob");
    
    float loudnessAttackTimeValue = *apvts.getRawParameterValue("loudnessAttackTimeKnob");
    float loudnessAttackTimeMilliseconds = normalizedToMilliseconds(loudnessAttackTimeValue);
    float loudnessDecayTimeValue = *apvts.getRawParameterValue("loudnessDecayTimeKnob");
    float loudnessDecayTimeMilliseconds = normalizedToMilliseconds(loudnessDecayTimeValue);
    float loudnessSustainTimeValue = *apvts.getRawParameterValue("loudnessSustainLevelKnob");
    bool decaySwitchValue = *apvts.getRawParameterValue("decaySwitch");
    
    bool keyboardCtrlSwitch1Value = *apvts.getRawParameterValue("keyboardCtrlSwitch1");
    bool keyboardCtrlSwitch2Value = *apvts.getRawParameterValue("keyboardCtrlSwitch2");
    bool filterModSwitchValue =*apvts.getRawParameterValue("filterModSwitch");
    
    bool osc3CtrlMode = *apvts.getRawParameterValue("osc3CtrlMode");
    float osc3ModulationValue = *apvts.getRawParameterValue("modWheelValue");
    bool oscModSwitchValue = *apvts.getRawParameterValue("oscModSwitch");
    
    int modulationMixLevel = *apvts.getRawParameterValue("ctrlModMixKnob");
    int outputVolLevel = *apvts.getRawParameterValue("outputVolKnob");
    
    bool glideSwitchValue = *apvts.getRawParameterValue("glideSwitch");
    int glideValue = *apvts.getRawParameterValue("ctrlGlideKnob");
    
    float pitchWheelValue = *apvts.getRawParameterValue("pitchWheelValue");
    
    float feedbackValue = *apvts.getRawParameterValue("feedbackKnob");
    
    // Set the ladder filter parameters based on your UI control values
    // juce::Logger::writeToLog("filterCutoff: " + juce::String(filterCutoffValue));
    ladderFilter.setCutoffFrequency(filterCutoffValue);
    ladderFilter.setResonance(static_cast<float>(filterEmphasisValue) / 10.0f); // Assuming range 0-10
    ladderFilter.setEnvelopeAmount(static_cast<float>(filterContourValue) / 10.0f); // Assuming range 0-10
    ladderFilter.setSampleRate(getSampleRate());
    ladderFilter.setFeedback(feedbackValue);
    const float contourSustainLevel = decaySwitchValue ? (loudnessSustainTimeValue / 10.0f) : 1.0f;
    const float loudnessSustainLevel = decaySwitchValue ? (filterSustainTimeValue / 10.0f) : 1.0f;
    // C Attack/Decay/Sustain (contour) drive the filter envelope; Attack/Decay/Sustain drive loudness.
    ladderFilter.setEnvelopeSettings(loudnessAttackTimeMilliseconds, loudnessDecayTimeMilliseconds, contourSustainLevel);
    ladderFilter.setContourEnvelopeSettings(filterAttackTimeMilliseconds, filterDecayTimeMilliseconds, loudnessSustainLevel);
    
    
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
    const float referenceFrequency = juce::MidiMessage::getMidiNoteInHertz(referenceNote);
    
    
    for (const auto midiMessage : midiMessages)
    {
        auto message = midiMessage.getMessage();
        
        if (message.isNoteOn())
        {
            // Handle note on
            currentNoteNumber = message.getNoteNumber(); // Update current note number
            
            float frequency = juce::MidiMessage::getMidiNoteInHertz(currentNoteNumber);
            
            float newFrequency = juce::MidiMessage::getMidiNoteInHertz(message.getNoteNumber());
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
    const float* inputLeft = totalNumInputChannels > 0 ? buffer.getReadPointer(0) : nullptr;
    const float* inputRight = totalNumInputChannels > 1 ? buffer.getReadPointer(1) : inputLeft;

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
                float glideStep = (targetFrequency - currentGlideFrequency) / (glideRate * getSampleRate());
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
        
        // Write the processed samples to each channel in the buffer
        buffer.getWritePointer(0)[sampleIndex] = sampleLeft;
        if (totalNumOutputChannels > 1) {
            buffer.getWritePointer(1)[sampleIndex] = sampleRight;
        }
        
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
    // Here we create an XML element with the state information
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MoogMiniAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Here we retrieve the XML from the binary data
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        // If the XML is valid, we use it to restore the state
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            restoredStateFromHost = true;
        }
    }
}

bool MoogMiniAudioProcessor::shouldAutoLoadLastPreset() const {
    return !restoredStateFromHost;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoogMiniAudioProcessor();
}
