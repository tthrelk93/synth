#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <vector>

class SignalFlowOverlay : public juce::Component
{
public:
    static constexpr int waveformSize = 2048;
    static constexpr int fftOrder = 11;
    static constexpr int spectrumBins = 24;
    static constexpr float filterScopeWidth = 60.0f;
    static constexpr float filterScopeHeight = 140.0f;
    static constexpr float filterScopeGap = 18.0f;
    static constexpr float outputScopeWidth = 64.0f;
    static constexpr float outputScopeHeight = 40.0f;
    static constexpr float outputScopeGap = 14.0f;
    struct Levels {
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

    struct StageWaveforms {
        std::array<float, waveformSize> osc1Raw{};
        std::array<float, waveformSize> osc2Raw{};
        std::array<float, waveformSize> osc3Raw{};
        std::array<float, waveformSize> osc1{};
        std::array<float, waveformSize> osc2{};
        std::array<float, waveformSize> osc3{};
        std::array<float, waveformSize> noise{};
        std::array<float, waveformSize> mix{};
        std::array<float, waveformSize> filter{};
        std::array<float, waveformSize> output{};
        std::array<float, waveformSize> modOsc{};
        std::array<float, waveformSize> modFilter{};
    };

    struct FilterVizState {
        float cutoffHz = 1000.0f;
        float resonance = 0.0f;
        float contourAmount = 0.0f;
        float filterEnvelope = 0.0f;
        float contourEnvelope = 0.0f;
        float attackMs = 1000.0f;
        float decayMs = 1000.0f;
        float sustainLevel = 0.5f;
        float contourAttackNorm = 0.0f;
        float contourDecayNorm = 0.0f;
        float contourSustainLevel = 0.5f;
        float loudnessAttackNorm = 0.0f;
        float loudnessDecayNorm = 0.0f;
        float loudnessSustainLevel = 0.5f;
        float keyTracking = 0.0f;
    };

    enum class Node {
        Osc1Wave,
        Osc1Tune,
        Osc1Range,
        Osc2Wave,
        Osc2Freq,
        Osc2Range,
        Osc3Wave,
        Osc3Freq,
        Osc3Range,
        Osc1Vol,
        Osc2Vol,
        Osc3Vol,
        NoiseVol,
        Mixer,
        FilterIn,
        FilterCore,
        FilterOut,
        FilterCutoff,
        FilterEmphasis,
        FilterContour,
        FilterAttack,
        FilterDecay,
        FilterSustain,
        FilterKey1,
        FilterKey2,
        LoudnessAttack,
        LoudnessDecay,
        LoudnessSustain,
        LoudnessVCA,
        OutputPre,
        OutputPost,
        Output,
        ModMix,
        ModOscTarget,
        Count
    };

    SignalFlowOverlay();

    struct FilterVizCallbacks {
        std::function<void(float)> setCutoff;
        std::function<void(float)> setResonance;
        std::function<void(float)> setContour;
        std::function<void(float)> setContourAttack;
        std::function<void(float)> setContourDecay;
        std::function<void(float)> setContourSustain;
        std::function<void(float)> setLoudnessAttack;
        std::function<void(float)> setLoudnessDecay;
        std::function<void(float)> setLoudnessSustain;
    };

    void setNodePosition(Node node, juce::Point<float> position);
    void setNodeBounds(Node node, juce::Rectangle<float> bounds);
    void resetNodes();
    void updatePaths();
    void setLevels(const Levels& newLevels);
    void setStageWaveforms(const StageWaveforms& waveforms);
    void setFilterVizState(const FilterVizState& state);
    void setFilterVizBounds(juce::Rectangle<float> bounds);
    void setFilterVizCallbacks(const FilterVizCallbacks& callbacks);
    void setSampleRate(double newSampleRate);
    struct OscFrequencyScales {
        float osc1Tune = 1.0f;
        float osc1Range = 1.0f;
        float osc2Detune = 1.0f;
        float osc2Range = 1.0f;
        float osc3Detune = 1.0f;
        float osc3Range = 1.0f;
    };

    void setOscFrequencyScales(const OscFrequencyScales& scales);
    void advance();

    void paint(juce::Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    enum PathId {
        Osc1WaveToTune = 0,
        Osc1TuneToRange,
        Osc1RangeToVol,
        Osc1VolToMixer,
        Osc2WaveToFreq,
        Osc2FreqToRange,
        Osc2RangeToVol,
        Osc2VolToMixer,
        Osc3WaveToFreq,
        Osc3FreqToRange,
        Osc3RangeToVol,
        Osc3VolToMixer,
        NoiseToMixer,
        MixerToFilterIn,
        FilterInToCore,
        FilterCoreToOut,
        FilterOutToVCA,
        VCAToOutputPre,
        OutputPreToKnob,
        KnobToOutputPost,
        ModToOsc,
        ModToFilter,
        CutoffToCore,
        EmphasisToCore,
        ContourToCore,
        FilterAttackToCutoff,
        FilterDecayToCutoff,
        FilterSustainToCutoff,
        Keys1ToCutoff,
        Keys2ToCutoff,
        LoudnessAttackToVCA,
        LoudnessDecayToVCA,
        LoudnessSustainToVCA,
        PathCount
    };

    enum StageIndex {
        StageOsc1Raw = 0,
        StageOsc2Raw,
        StageOsc3Raw,
        StageOsc1Post,
        StageOsc2Post,
        StageOsc3Post,
        StageNoise,
        StageMix,
        StageFilter,
        StageOutput,
        StageModOsc,
        StageModFilter,
        StageCount
    };

    struct StageStats {
        juce::Colour colour = juce::Colours::white;
        float centroidHz = 0.0f;
        float rms = 0.0f;
        float peak = 0.0f;
        float zeroCrossOffset = 0.0f;
        float periodSamples = static_cast<float>(waveformSize);
        std::array<float, spectrumBins> spectrum{};
    };

    struct FlowPath {
        juce::Point<float> start;
        juce::Point<float> control;
        juce::Point<float> end;
        juce::Path path;
        juce::Colour colour;
        float baseThickness = 1.0f;
        float length = 0.0f;
        bool usePathSampling = false;
    };

    struct Pulse {
        int pathIndex = 0;
        float position = 0.0f;
        float speed = 0.6f;
        float intensity = 0.0f;
    };

    enum class DragTarget {
        None,
        Cutoff,
        Resonance,
        ContourAmount,
        ContourAttack,
        ContourDecay,
        ContourSustain,
        LoudnessAttack,
        LoudnessDecay,
        LoudnessSustain
    };

    struct EnvelopeLayout {
        juce::Rectangle<float> rect;
        juce::Rectangle<float> innerRect;
        juce::Point<float> attackHandle;
        juce::Point<float> decayHandle;
        juce::Point<float> sustainHandle;
        float leftSegment = 0.0f;
        float rightSegment = 0.0f;
        float gap = 0.0f;
    };

    struct FilterVizLayout {
        bool valid = false;
        juce::Rectangle<float> panel;
        juce::Rectangle<float> headerRect;
        juce::Rectangle<float> topRect;
        juce::Rectangle<float> contourRect;
        juce::Rectangle<float> loudnessRect;
        juce::Point<float> cutoffHandle;
        juce::Point<float> resonanceHandle;
        juce::Point<float> contourHandle;
        EnvelopeLayout contourEnv;
        EnvelopeLayout loudnessEnv;
    };

    static juce::Point<float> pointOnPath(const FlowPath& path, float t);
    static juce::Point<float> tangentOnPath(const FlowPath& path, float t);
    FlowPath makePath(juce::Point<float> start, juce::Point<float> end, float bend, juce::Colour colour);
    FilterVizLayout buildFilterVizLayout() const;
    void handleFilterVizDrag(juce::Point<float> position);
    void updatePathLevel(int pathIndex, float level);
    void drawWaveformOnPath(juce::Graphics& g,
                            const FlowPath& path,
                            const std::array<float, waveformSize>& samples,
                            const std::array<float, waveformSize>* endSamples,
                            float blendStart,
                            float phaseOffset,
                            float timeScaleStart,
                            float timeScaleEnd,
                            float level,
                            juce::Colour colour,
                            float rms,
                            float displayScale,
                            float endDisplayScale) const;
    void drawMiniScope(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       const std::array<float, waveformSize>& samples,
                       float phaseOffset,
                       juce::Colour colour,
                       float rms,
                       float displayScale) const;
    void drawFilterVizPanels(juce::Graphics& g) const;
    void updateStageStats(StageIndex stage, const std::array<float, waveformSize>& samples);
    juce::Colour spectralColourForStage(StageIndex stage) const;
    float rmsForStage(StageIndex stage) const;
    float phaseOffsetForStage(StageIndex stage) const;
    float displayScaleForStage(StageIndex stage) const;
    float timeScaleForStage(StageIndex stage) const;
    int windowSamplesForDisplay() const;
    void setPathTimeScale(PathId path, float start, float end);

    std::array<juce::Point<float>, static_cast<int>(Node::Count)> nodes{};
    std::array<bool, static_cast<int>(Node::Count)> nodeValid{};
    std::array<juce::Rectangle<float>, static_cast<int>(Node::Count)> nodeBounds{};
    std::array<bool, static_cast<int>(Node::Count)> nodeBoundsValid{};
    std::array<FlowPath, PathCount> paths{};
    std::array<float, PathCount> pathLevels{};
    std::array<bool, PathCount> controlPaths{};
    std::array<float, PathCount> controlLevels{};
    std::array<float, PathCount> pathTimeScaleStart{};
    std::array<float, PathCount> pathTimeScaleEnd{};
    std::array<float, PathCount> spawnAccumulators{};
    std::vector<Pulse> pulses;
    double lastUpdateMs = 0.0;
    double sampleRate = 44100.0;
    StageWaveforms stageWaveforms{};
    std::array<StageStats, StageCount> stageStats{};
    std::array<float, StageCount> phaseOffsets{};
    FilterVizState filterVizState{};
    juce::Rectangle<float> filterVizBounds{};
    bool filterVizBoundsValid = false;
    FilterVizCallbacks filterVizCallbacks{};
    DragTarget activeDragTarget = DragTarget::None;
    DragTarget hoverTarget = DragTarget::None;
    juce::Point<float> hoverPoint{};
    float lastContourEnvelope = 0.0f;
    bool contourEnvelopeFalling = false;
    bool contourEnvelopeRising = false;
    bool contourEnvelopeSustain = false;
    float lastLoudnessEnvelope = 0.0f;
    bool loudnessEnvelopeFalling = false;
    bool loudnessEnvelopeRising = false;
    bool loudnessEnvelopeSustain = false;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { waveformSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, waveformSize * 2> fftScratch{};
};
