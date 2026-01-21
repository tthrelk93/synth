#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class SignalFlowOverlay : public juce::Component
{
public:
    struct Levels {
        float osc1 = 0.0f;
        float osc2 = 0.0f;
        float osc3 = 0.0f;
        float noise = 0.0f;
        float mix = 0.0f;
        float filter = 0.0f;
        float output = 0.0f;
    };

    enum class Node {
        Osc1Wave,
        Osc2Wave,
        Osc3Wave,
        Osc1Vol,
        Osc2Vol,
        Osc3Vol,
        NoiseVol,
        Mixer,
        Filter,
        Output,
        Count
    };

    SignalFlowOverlay();

    void setNodePosition(Node node, juce::Point<float> position);
    void updatePaths();
    void setLevels(const Levels& newLevels);
    void advance();

    void paint(juce::Graphics& g) override;

private:
    enum PathId {
        Osc1Control = 0,
        Osc2Control,
        Osc3Control,
        Osc1ToMixer,
        Osc2ToMixer,
        Osc3ToMixer,
        NoiseToMixer,
        MixerToFilter,
        FilterToOutput,
        PathCount
    };

    struct FlowPath {
        juce::Point<float> start;
        juce::Point<float> control;
        juce::Point<float> end;
        juce::Path path;
        juce::Colour colour;
        float baseThickness = 1.0f;
    };

    struct Pulse {
        int pathIndex = 0;
        float position = 0.0f;
        float speed = 0.6f;
        float intensity = 0.0f;
    };

    static juce::Point<float> pointOnPath(const FlowPath& path, float t);
    FlowPath makePath(juce::Point<float> start, juce::Point<float> end, float bend, juce::Colour colour);
    void updatePathLevel(int pathIndex, float level);

    std::array<juce::Point<float>, static_cast<int>(Node::Count)> nodes{};
    std::array<bool, static_cast<int>(Node::Count)> nodeValid{};
    std::array<FlowPath, PathCount> paths{};
    std::array<float, PathCount> pathLevels{};
    std::array<float, PathCount> spawnAccumulators{};
    std::vector<Pulse> pulses;
    double lastUpdateMs = 0.0;
};
