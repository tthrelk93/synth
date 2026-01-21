#include "SignalFlowOverlay.h"
#include <algorithm>
#include <cmath>

SignalFlowOverlay::SignalFlowOverlay()
{
    setInterceptsMouseClicks(false, false);
}

void SignalFlowOverlay::setNodePosition(Node node, juce::Point<float> position)
{
    auto index = static_cast<int>(node);
    nodes[static_cast<size_t>(index)] = position;
    nodeValid[static_cast<size_t>(index)] = true;
}

void SignalFlowOverlay::updatePaths()
{
    auto isValid = [this](Node node) {
        return nodeValid[static_cast<size_t>(node)];
    };
    auto nodePoint = [this](Node node) {
        return nodes[static_cast<size_t>(node)];
    };

    auto setPath = [&](int pathIndex, Node startNode, Node endNode, float bend, juce::Colour colour) {
        if (!isValid(startNode) || !isValid(endNode)) {
            paths[static_cast<size_t>(pathIndex)].path.clear();
            return;
        }
        paths[static_cast<size_t>(pathIndex)] = makePath(nodePoint(startNode), nodePoint(endNode), bend, colour);
    };

    const juce::Colour osc1Colour = juce::Colour::fromRGB(214, 185, 130);
    const juce::Colour osc2Colour = juce::Colour::fromRGB(166, 200, 210);
    const juce::Colour osc3Colour = juce::Colour::fromRGB(216, 170, 156);
    const juce::Colour noiseColour = juce::Colour::fromRGB(190, 190, 190);
    const juce::Colour mixColour = juce::Colour::fromRGB(210, 190, 160);
    const juce::Colour filterColour = juce::Colour::fromRGB(210, 180, 140);

    setPath(Osc1Control, Node::Osc1Wave, Node::Osc1Vol, -25.0f, osc1Colour);
    setPath(Osc2Control, Node::Osc2Wave, Node::Osc2Vol, -25.0f, osc2Colour);
    setPath(Osc3Control, Node::Osc3Wave, Node::Osc3Vol, -25.0f, osc3Colour);

    setPath(Osc1ToMixer, Node::Osc1Vol, Node::Mixer, 40.0f, osc1Colour);
    setPath(Osc2ToMixer, Node::Osc2Vol, Node::Mixer, 40.0f, osc2Colour);
    setPath(Osc3ToMixer, Node::Osc3Vol, Node::Mixer, 40.0f, osc3Colour);
    setPath(NoiseToMixer, Node::NoiseVol, Node::Mixer, 30.0f, noiseColour);

    setPath(MixerToFilter, Node::Mixer, Node::Filter, -35.0f, mixColour);
    setPath(FilterToOutput, Node::Filter, Node::Output, -25.0f, filterColour);
}

void SignalFlowOverlay::setLevels(const Levels& newLevels)
{
    updatePathLevel(Osc1Control, newLevels.osc1);
    updatePathLevel(Osc2Control, newLevels.osc2);
    updatePathLevel(Osc3Control, newLevels.osc3);
    updatePathLevel(Osc1ToMixer, newLevels.osc1);
    updatePathLevel(Osc2ToMixer, newLevels.osc2);
    updatePathLevel(Osc3ToMixer, newLevels.osc3);
    updatePathLevel(NoiseToMixer, newLevels.noise);
    updatePathLevel(MixerToFilter, newLevels.mix);
    updatePathLevel(FilterToOutput, newLevels.output);
}

void SignalFlowOverlay::advance()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    if (lastUpdateMs == 0.0) {
        lastUpdateMs = nowMs;
        return;
    }

    const double deltaSeconds = (nowMs - lastUpdateMs) / 1000.0;
    lastUpdateMs = nowMs;
    if (deltaSeconds <= 0.0) {
        return;
    }

    constexpr float spawnRate = 6.0f;
    constexpr float minSpeed = 0.35f;
    constexpr float maxSpeed = 1.1f;

    for (size_t i = 0; i < paths.size(); ++i) {
        if (paths[i].path.isEmpty()) {
            continue;
        }

        const float level = pathLevels[i];
        spawnAccumulators[i] += static_cast<float>(deltaSeconds) * level * spawnRate;
        while (spawnAccumulators[i] >= 1.0f) {
            spawnAccumulators[i] -= 1.0f;
            Pulse pulse;
            pulse.pathIndex = static_cast<int>(i);
            pulse.position = 0.0f;
            pulse.speed = juce::jmap(level, 0.0f, 1.0f, minSpeed, maxSpeed);
            pulse.intensity = juce::jlimit(0.0f, 1.0f, level);
            pulses.push_back(pulse);
        }
    }

    for (auto& pulse : pulses) {
        pulse.position += pulse.speed * static_cast<float>(deltaSeconds);
    }

    pulses.erase(std::remove_if(pulses.begin(), pulses.end(),
                                [](const Pulse& pulse) { return pulse.position >= 1.0f; }),
                 pulses.end());
}

void SignalFlowOverlay::paint(juce::Graphics& g)
{
    for (size_t i = 0; i < paths.size(); ++i) {
        const auto& path = paths[i];
        if (path.path.isEmpty()) {
            continue;
        }

        const float level = pathLevels[i];
        g.setColour(path.colour.withAlpha(0.12f));
        g.strokePath(path.path, juce::PathStrokeType(path.baseThickness));

        if (level > 0.001f) {
            g.setColour(path.colour.withAlpha(0.15f + 0.5f * level));
            g.strokePath(path.path, juce::PathStrokeType(path.baseThickness + level * 1.2f));
        }
    }

    for (const auto& pulse : pulses) {
        if (pulse.pathIndex < 0 || pulse.pathIndex >= static_cast<int>(paths.size())) {
            continue;
        }

        const auto& path = paths[static_cast<size_t>(pulse.pathIndex)];
        if (path.path.isEmpty()) {
            continue;
        }

        auto point = pointOnPath(path, pulse.position);
        const float radius = 1.6f + 2.4f * pulse.intensity;
        g.setColour(path.colour.withAlpha(0.25f + 0.55f * pulse.intensity));
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(path.colour.withAlpha(0.12f + 0.35f * pulse.intensity));
        g.fillEllipse(point.x - radius * 2.4f, point.y - radius * 2.4f, radius * 4.8f, radius * 4.8f);
    }
}

juce::Point<float> SignalFlowOverlay::pointOnPath(const FlowPath& path, float t)
{
    const float oneMinusT = 1.0f - t;
    const float oneMinusTSquared = oneMinusT * oneMinusT;
    const float tSquared = t * t;
    return path.start * oneMinusTSquared
        + path.control * (2.0f * oneMinusT * t)
        + path.end * tSquared;
}

SignalFlowOverlay::FlowPath SignalFlowOverlay::makePath(juce::Point<float> start, juce::Point<float> end, float bend, juce::Colour colour)
{
    FlowPath path;
    path.start = start;
    path.end = end;
    path.colour = colour;
    path.baseThickness = 1.0f;

    auto mid = (start + end) * 0.5f;
    auto delta = end - start;
    const float length = std::hypot(delta.x, delta.y);
    if (length > 0.0001f) {
        juce::Point<float> normal(-delta.y / length, delta.x / length);
        mid += normal * bend;
    }
    path.control = mid;

    path.path.clear();
    path.path.startNewSubPath(start);
    path.path.quadraticTo(path.control, end);

    return path;
}

void SignalFlowOverlay::updatePathLevel(int pathIndex, float level)
{
    pathLevels[static_cast<size_t>(pathIndex)] = juce::jlimit(0.0f, 1.0f, level);
}
