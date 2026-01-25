#include "SignalFlowOverlay.h"
#include <algorithm>
#include <cmath>
#include <limits>

static_assert((1 << SignalFlowOverlay::fftOrder) == SignalFlowOverlay::waveformSize,
              "SignalFlowOverlay FFT order must match waveform size.");

SignalFlowOverlay::SignalFlowOverlay()
{
    setInterceptsMouseClicks(true, true);
    pathTimeScaleStart.fill(1.0f);
    pathTimeScaleEnd.fill(1.0f);
    controlPaths.fill(false);
    controlLevels.fill(0.0f);
    nodeBoundsValid.fill(false);
    filterVizBoundsValid = false;
}

void SignalFlowOverlay::setNodePosition(Node node, juce::Point<float> position)
{
    auto index = static_cast<int>(node);
    nodes[static_cast<size_t>(index)] = position;
    nodeValid[static_cast<size_t>(index)] = true;
}

void SignalFlowOverlay::setNodeBounds(Node node, juce::Rectangle<float> bounds)
{
    auto index = static_cast<int>(node);
    nodeBounds[static_cast<size_t>(index)] = bounds;
    nodeBoundsValid[static_cast<size_t>(index)] = true;
}

void SignalFlowOverlay::resetNodes()
{
    nodeValid.fill(false);
    nodeBoundsValid.fill(false);
}

void SignalFlowOverlay::updatePaths()
{
    auto isValid = [this](Node node) {
        return nodeValid[static_cast<size_t>(node)];
    };
    auto nodePoint = [this](Node node) {
        return nodes[static_cast<size_t>(node)];
    };
    auto nodeRect = [this](Node node) {
        return nodeBounds[static_cast<size_t>(node)];
    };
    auto hasBounds = [this](Node node) {
        return nodeBoundsValid[static_cast<size_t>(node)];
    };

    auto edgePointForNode = [&](Node node, juce::Point<float> toward) {
        auto center = nodePoint(node);
        if (!hasBounds(node)) {
            return center;
        }
        auto rect = nodeRect(node);
        auto direction = toward - center;
        const float length = std::hypot(direction.x, direction.y);
        if (length <= 0.0001f) {
            return center;
        }
        direction /= length;

        float bestT = std::numeric_limits<float>::max();
        auto consider = [&](float t, float otherCoord, float minOther, float maxOther) {
            if (t > 0.0f && otherCoord >= minOther && otherCoord <= maxOther) {
                bestT = std::min(bestT, t);
            }
        };

        if (std::abs(direction.x) > 0.0001f) {
            float t = (rect.getX() - center.x) / direction.x;
            consider(t, center.y + direction.y * t, rect.getY(), rect.getBottom());
            t = (rect.getRight() - center.x) / direction.x;
            consider(t, center.y + direction.y * t, rect.getY(), rect.getBottom());
        }
        if (std::abs(direction.y) > 0.0001f) {
            float t = (rect.getY() - center.y) / direction.y;
            consider(t, center.x + direction.x * t, rect.getX(), rect.getRight());
            t = (rect.getBottom() - center.y) / direction.y;
            consider(t, center.x + direction.x * t, rect.getX(), rect.getRight());
        }

        if (bestT == std::numeric_limits<float>::max()) {
            return center;
        }
        return center + direction * bestT;
    };

    controlPaths.fill(false);
    controlLevels.fill(0.0f);

    auto clearPath = [&](int pathIndex) {
        paths[static_cast<size_t>(pathIndex)].path.clear();
    };

    auto setPathPoints = [&](int pathIndex,
                             juce::Point<float> start,
                             juce::Point<float> end,
                             float bend,
                             juce::Colour colour,
                             float thickness = 1.0f) {
        auto path = makePath(start, end, bend, colour);
        path.baseThickness = thickness;
        paths[static_cast<size_t>(pathIndex)] = path;
    };

    auto setPath = [&](int pathIndex, Node startNode, Node endNode, float bend, juce::Colour colour, float thickness = 1.0f) {
        if (!isValid(startNode) || !isValid(endNode)) {
            clearPath(pathIndex);
            return;
        }
        auto start = edgePointForNode(startNode, nodePoint(endNode));
        auto end = edgePointForNode(endNode, nodePoint(startNode));
        setPathPoints(pathIndex, start, end, bend, colour, thickness);
    };

    auto setControlPath = [&](int pathIndex,
                              Node startNode,
                              Node endNode,
                              float bend,
                              juce::Colour colour,
                              float thickness = 0.7f,
                              float level = 0.18f) {
        setPath(pathIndex, startNode, endNode, bend, colour, thickness);
        controlPaths[static_cast<size_t>(pathIndex)] = true;
        controlLevels[static_cast<size_t>(pathIndex)] = level;
    };

    const juce::Colour osc1Colour = juce::Colour::fromRGB(80, 170, 255);
    const juce::Colour osc2Colour = juce::Colour::fromRGB(70, 140, 255);
    const juce::Colour osc3Colour = juce::Colour::fromRGB(120, 120, 255);
    const juce::Colour noiseColour = juce::Colour::fromRGB(140, 145, 170);
    const juce::Colour mixColour = juce::Colour::fromRGB(60, 160, 255);
    const juce::Colour filterColour = juce::Colour::fromRGB(80, 190, 255);
    const juce::Colour modColour = juce::Colour::fromRGB(90, 210, 255);

    setPath(Osc1WaveToTune, Node::Osc1Wave, Node::Osc1Tune, 0.0f, osc1Colour);
    setPath(Osc1TuneToRange, Node::Osc1Tune, Node::Osc1Range, 0.0f, osc1Colour);
    setPath(Osc1RangeToVol, Node::Osc1Range, Node::Osc1Vol, 0.0f, osc1Colour);
    setPath(Osc1VolToMixer, Node::Osc1Vol, Node::Mixer, 0.0f, osc1Colour);

    setPath(Osc2WaveToFreq, Node::Osc2Wave, Node::Osc2Freq, 0.0f, osc2Colour);
    setPath(Osc2FreqToRange, Node::Osc2Freq, Node::Osc2Range, 0.0f, osc2Colour);
    setPath(Osc2RangeToVol, Node::Osc2Range, Node::Osc2Vol, 0.0f, osc2Colour);
    setPath(Osc2VolToMixer, Node::Osc2Vol, Node::Mixer, 0.0f, osc2Colour);

    setPath(Osc3WaveToFreq, Node::Osc3Wave, Node::Osc3Freq, 0.0f, osc3Colour);
    setPath(Osc3FreqToRange, Node::Osc3Freq, Node::Osc3Range, 0.0f, osc3Colour);
    setPath(Osc3RangeToVol, Node::Osc3Range, Node::Osc3Vol, 0.0f, osc3Colour);
    setPath(Osc3VolToMixer, Node::Osc3Vol, Node::Mixer, 0.0f, osc3Colour);
    setPath(NoiseToMixer, Node::NoiseVol, Node::Mixer, 0.0f, noiseColour);

    if (isValid(Node::FilterIn)) {
        const bool inletIsPanel = hasBounds(Node::FilterIn)
            && nodeRect(Node::FilterIn).getWidth() > (filterScopeWidth * 1.1f);
        auto filterInFromMixer = edgePointForNode(Node::FilterIn,
                                                  isValid(Node::Mixer) ? nodePoint(Node::Mixer) : nodePoint(Node::FilterIn));
        auto filterInToCore = edgePointForNode(Node::FilterIn,
                                               isValid(Node::FilterCore) ? nodePoint(Node::FilterCore) : nodePoint(Node::FilterIn));
        if (isValid(Node::Mixer)) {
            auto mixerEdge = edgePointForNode(Node::Mixer, filterInFromMixer);
            if (hasBounds(Node::Mixer)) {
                auto mixerBounds = nodeRect(Node::Mixer);
                auto direction = filterInFromMixer - mixerEdge;
                const float length = std::hypot(direction.x, direction.y);
                if (length > 0.001f) {
                    direction /= length;
                    mixerEdge -= direction * mixerBounds.getWidth();
                }
            }
            setPathPoints(MixerToFilterIn, mixerEdge, filterInFromMixer, 0.0f, mixColour);
        } else {
            clearPath(MixerToFilterIn);
        }
        if (!inletIsPanel && isValid(Node::FilterCore)) {
            auto coreEdge = edgePointForNode(Node::FilterCore, filterInToCore);
            auto direction = coreEdge - filterInToCore;
            const float length = std::hypot(direction.x, direction.y);
            const float maxLength = 18.0f;
            if (length > 0.001f) {
                direction /= length;
            }
            auto shortEnd = filterInToCore + direction * juce::jmin(length, maxLength);
            setPathPoints(FilterInToCore, filterInToCore, shortEnd, 0.0f, mixColour);
        } else {
            clearPath(FilterInToCore);
        }
    } else {
        clearPath(MixerToFilterIn);
        clearPath(FilterInToCore);
    }

    if (isValid(Node::FilterOut)) {
        auto filterOutFromCore = edgePointForNode(Node::FilterOut,
                                                  isValid(Node::FilterCore) ? nodePoint(Node::FilterCore) : nodePoint(Node::FilterOut));
        auto filterOutToVCA = edgePointForNode(Node::FilterOut,
                                               isValid(Node::LoudnessVCA) ? nodePoint(Node::LoudnessVCA) : nodePoint(Node::FilterOut));
        if (isValid(Node::FilterCore)) {
            auto coreEdge = edgePointForNode(Node::FilterCore, filterOutFromCore);
            setPathPoints(FilterCoreToOut, coreEdge, filterOutFromCore, 0.0f, filterColour);
        } else {
            clearPath(FilterCoreToOut);
        }
        if (isValid(Node::LoudnessVCA)) {
            auto vcaPoint = nodePoint(Node::LoudnessVCA);
            setPathPoints(FilterOutToVCA, filterOutToVCA, vcaPoint, 0.0f, filterColour);
        } else {
            clearPath(FilterOutToVCA);
        }
    } else {
        clearPath(FilterCoreToOut);
        clearPath(FilterOutToVCA);
    }

    if (isValid(Node::OutputPre)) {
        auto outputPreFromVCA = edgePointForNode(Node::OutputPre,
                                                 isValid(Node::LoudnessVCA) ? nodePoint(Node::LoudnessVCA) : nodePoint(Node::OutputPre));
        auto outputPreToOutput = edgePointForNode(Node::OutputPre,
                                                  isValid(Node::Output) ? nodePoint(Node::Output) : nodePoint(Node::OutputPre));
        if (isValid(Node::LoudnessVCA)) {
            auto vcaPoint = nodePoint(Node::LoudnessVCA);
            setPathPoints(VCAToOutputPre, vcaPoint, outputPreFromVCA, 0.0f, filterColour);
        } else {
            clearPath(VCAToOutputPre);
        }
        if (isValid(Node::Output)) {
            auto outputEdge = edgePointForNode(Node::Output, outputPreToOutput);
            setPathPoints(OutputPreToKnob, outputPreToOutput, outputEdge, 0.0f, filterColour);
        } else {
            clearPath(OutputPreToKnob);
        }
    } else {
        clearPath(OutputPreToKnob);
        if (isValid(Node::LoudnessVCA) && isValid(Node::Output)) {
            auto vcaPoint = nodePoint(Node::LoudnessVCA);
            auto outputEdge = edgePointForNode(Node::Output, vcaPoint);
            setPathPoints(VCAToOutputPre, vcaPoint, outputEdge, 0.0f, filterColour);
        } else {
            clearPath(VCAToOutputPre);
        }
    }

    if (isValid(Node::OutputPost) && isValid(Node::Output)) {
        auto outputPostFromOutput = edgePointForNode(Node::OutputPost, nodePoint(Node::Output));
        auto outputEdge = edgePointForNode(Node::Output, outputPostFromOutput);
        setPathPoints(KnobToOutputPost, outputEdge, outputPostFromOutput, 0.0f, filterColour);
    } else {
        clearPath(KnobToOutputPost);
    }
    if (isValid(Node::ModMix) && isValid(Node::ModOscTarget)) {
        auto start = edgePointForNode(Node::ModMix, nodePoint(Node::ModOscTarget));
        auto end = edgePointForNode(Node::ModOscTarget, nodePoint(Node::ModMix));
        juce::Rectangle<float> osc3Bounds;
        bool hasOsc3Bounds = false;
        auto expandBounds = [&](Node node) {
            if (!hasBounds(node)) {
                return;
            }
            if (!hasOsc3Bounds) {
                osc3Bounds = nodeRect(node);
                hasOsc3Bounds = true;
            } else {
                osc3Bounds = osc3Bounds.getUnion(nodeRect(node));
            }
        };
        expandBounds(Node::Osc3Wave);
        expandBounds(Node::Osc3Freq);
        expandBounds(Node::Osc3Range);
        expandBounds(Node::Osc3Vol);

        juce::Path modOscPath;
        modOscPath.startNewSubPath(start);
        if (hasOsc3Bounds) {
            const float pad = 18.0f;
            float leftX = osc3Bounds.getX() - pad;
            leftX = std::min(leftX, start.x - 12.0f);
            leftX = juce::jmax(leftX, 12.0f);
            modOscPath.lineTo(leftX, start.y);
            modOscPath.lineTo(leftX, end.y);
        }
        modOscPath.lineTo(end);
        modOscPath = modOscPath.createPathWithRoundedCorners(28.0f);

        FlowPath modPath;
        modPath.start = start;
        modPath.end = end;
        modPath.control = (start + end) * 0.5f;
        modPath.colour = modColour;
        modPath.baseThickness = 1.0f;
        modPath.path = modOscPath;
        modPath.usePathSampling = true;
        modPath.length = modPath.path.getLength();
        paths[static_cast<size_t>(ModToOsc)] = modPath;
    } else {
        clearPath(ModToOsc);
    }

    if (isValid(Node::ModMix) && isValid(Node::Mixer)) {
        auto start = edgePointForNode(Node::ModMix, nodePoint(Node::Mixer));
        auto end = edgePointForNode(Node::Mixer, nodePoint(Node::ModMix));

        juce::Path modPathShape;
        modPathShape.startNewSubPath(start);
        if (hasBounds(Node::NoiseVol)) {
            auto noiseBounds = nodeRect(Node::NoiseVol);
            const float keyboardTop = static_cast<float>(getLocalBounds().getBottom()) - 300.0f;
            const float pad = 18.0f;
            float routeY = noiseBounds.getBottom() + pad;
            const float maxRouteY = keyboardTop - 24.0f;
            if (routeY > maxRouteY) {
                routeY = maxRouteY;
            }
            float leftX = noiseBounds.getX() - pad;
            float rightX = noiseBounds.getRight() + pad;
            leftX = juce::jlimit(start.x + 20.0f, end.x - 20.0f, leftX);
            rightX = juce::jlimit(start.x + 40.0f, end.x - 20.0f, rightX);
            if (rightX < leftX + 10.0f) {
                rightX = leftX + 10.0f;
            }
            modPathShape.lineTo(leftX, routeY);
            modPathShape.lineTo(rightX, routeY);
        }
        modPathShape.lineTo(end);
        modPathShape = modPathShape.createPathWithRoundedCorners(28.0f);

        FlowPath modPath;
        modPath.start = start;
        modPath.end = end;
        modPath.control = (start + end) * 0.5f;
        modPath.colour = modColour;
        modPath.baseThickness = 1.0f;
        modPath.path = modPathShape;
        modPath.usePathSampling = true;
        modPath.length = modPath.path.getLength();
        paths[static_cast<size_t>(ModToFilter)] = modPath;
    } else {
        clearPath(ModToFilter);
    }

    setControlPath(CutoffToCore, Node::FilterCutoff, Node::FilterCore, -6.0f, filterColour, 0.6f, 0.2f);
    setControlPath(EmphasisToCore, Node::FilterEmphasis, Node::FilterCore, -4.0f, filterColour, 0.6f, 0.16f);
    setControlPath(ContourToCore, Node::FilterContour, Node::FilterCutoff, -2.0f, filterColour, 0.6f, 0.16f);
    setControlPath(FilterAttackToCutoff, Node::FilterAttack, Node::FilterCutoff, -10.0f, filterColour, 0.5f, 0.12f);
    setControlPath(FilterDecayToCutoff, Node::FilterDecay, Node::FilterCutoff, -8.0f, filterColour, 0.5f, 0.12f);
    setControlPath(FilterSustainToCutoff, Node::FilterSustain, Node::FilterCutoff, -6.0f, filterColour, 0.5f, 0.12f);
    setControlPath(Keys1ToCutoff, Node::FilterKey1, Node::FilterCutoff, 8.0f, filterColour, 0.5f, 0.12f);
    setControlPath(Keys2ToCutoff, Node::FilterKey2, Node::FilterCutoff, 10.0f, filterColour, 0.5f, 0.12f);
    setControlPath(LoudnessAttackToVCA, Node::LoudnessAttack, Node::LoudnessVCA, 10.0f, filterColour, 0.5f, 0.14f);
    setControlPath(LoudnessDecayToVCA, Node::LoudnessDecay, Node::LoudnessVCA, 8.0f, filterColour, 0.5f, 0.14f);
    setControlPath(LoudnessSustainToVCA, Node::LoudnessSustain, Node::LoudnessVCA, 6.0f, filterColour, 0.5f, 0.14f);
}

void SignalFlowOverlay::setLevels(const Levels& newLevels)
{
    updatePathLevel(Osc1WaveToTune, newLevels.osc1Raw);
    updatePathLevel(Osc1TuneToRange, newLevels.osc1Raw);
    updatePathLevel(Osc1RangeToVol, newLevels.osc1Raw);
    updatePathLevel(Osc1VolToMixer, newLevels.osc1);
    updatePathLevel(Osc2WaveToFreq, newLevels.osc2Raw);
    updatePathLevel(Osc2FreqToRange, newLevels.osc2Raw);
    updatePathLevel(Osc2RangeToVol, newLevels.osc2Raw);
    updatePathLevel(Osc2VolToMixer, newLevels.osc2);
    updatePathLevel(Osc3WaveToFreq, newLevels.osc3Raw);
    updatePathLevel(Osc3FreqToRange, newLevels.osc3Raw);
    updatePathLevel(Osc3RangeToVol, newLevels.osc3Raw);
    updatePathLevel(Osc3VolToMixer, newLevels.osc3);
    updatePathLevel(NoiseToMixer, newLevels.noise);
    updatePathLevel(MixerToFilterIn, newLevels.mix);
    updatePathLevel(FilterInToCore, newLevels.mix);
    updatePathLevel(FilterCoreToOut, newLevels.filter);
    updatePathLevel(FilterOutToVCA, newLevels.filter);
    updatePathLevel(VCAToOutputPre, newLevels.filter);
    updatePathLevel(OutputPreToKnob, newLevels.filter);
    updatePathLevel(KnobToOutputPost, newLevels.output);
    updatePathLevel(ModToOsc, newLevels.modOsc);
    updatePathLevel(ModToFilter, newLevels.modFilter);
}

void SignalFlowOverlay::setStageWaveforms(const StageWaveforms& waveforms)
{
    stageWaveforms = waveforms;

    updateStageStats(StageOsc1Raw, stageWaveforms.osc1Raw);
    updateStageStats(StageOsc2Raw, stageWaveforms.osc2Raw);
    updateStageStats(StageOsc3Raw, stageWaveforms.osc3Raw);
    updateStageStats(StageOsc1Post, stageWaveforms.osc1);
    updateStageStats(StageOsc2Post, stageWaveforms.osc2);
    updateStageStats(StageOsc3Post, stageWaveforms.osc3);
    updateStageStats(StageNoise, stageWaveforms.noise);
    updateStageStats(StageMix, stageWaveforms.mix);
    updateStageStats(StageFilter, stageWaveforms.filter);
    updateStageStats(StageOutput, stageWaveforms.output);
    updateStageStats(StageModOsc, stageWaveforms.modOsc);
    updateStageStats(StageModFilter, stageWaveforms.modFilter);
}

void SignalFlowOverlay::setFilterVizState(const FilterVizState& state)
{
    constexpr float envelopeEpsilon = 0.0005f;
    const float delta = state.filterEnvelope - lastContourEnvelope;
    contourEnvelopeRising = delta > envelopeEpsilon;
    contourEnvelopeFalling = delta < -envelopeEpsilon;
    const float sustainTarget = juce::jlimit(0.0f, 1.0f, state.contourSustainLevel);
    const float sustainThreshold = 0.03f;
    contourEnvelopeSustain = !contourEnvelopeRising
        && !contourEnvelopeFalling
        && state.filterEnvelope > 0.05f
        && std::abs(state.filterEnvelope - sustainTarget) < sustainThreshold;
    lastContourEnvelope = state.filterEnvelope;

    const float loudnessDelta = state.contourEnvelope - lastLoudnessEnvelope;
    loudnessEnvelopeRising = loudnessDelta > envelopeEpsilon;
    loudnessEnvelopeFalling = loudnessDelta < -envelopeEpsilon;
    const float loudnessSustainTarget = juce::jlimit(0.0f, 1.0f, state.loudnessSustainLevel);
    loudnessEnvelopeSustain = !loudnessEnvelopeRising
        && !loudnessEnvelopeFalling
        && state.contourEnvelope > 0.05f
        && std::abs(state.contourEnvelope - loudnessSustainTarget) < sustainThreshold;
    lastLoudnessEnvelope = state.contourEnvelope;
    filterVizState = state;
}

void SignalFlowOverlay::setFilterVizBounds(juce::Rectangle<float> bounds)
{
    filterVizBounds = bounds;
    filterVizBoundsValid = bounds.getWidth() > 2.0f && bounds.getHeight() > 2.0f;
}

void SignalFlowOverlay::setFilterVizCallbacks(const FilterVizCallbacks& callbacks)
{
    filterVizCallbacks = callbacks;
}

void SignalFlowOverlay::setSampleRate(double newSampleRate)
{
    if (newSampleRate > 0.0) {
        sampleRate = newSampleRate;
    }
}

void SignalFlowOverlay::setOscFrequencyScales(const OscFrequencyScales& scales)
{
    const float osc1PreTune = scales.osc1Tune * scales.osc1Range;
    const float osc1PreRange = scales.osc1Range;
    const float osc2PreFreq = scales.osc2Detune * scales.osc2Range;
    const float osc2PreRange = scales.osc2Range;
    const float osc3PreFreq = scales.osc3Detune * scales.osc3Range;
    const float osc3PreRange = scales.osc3Range;

    setPathTimeScale(Osc1WaveToTune, osc1PreTune, osc1PreRange);
    setPathTimeScale(Osc1TuneToRange, osc1PreRange, 1.0f);
    setPathTimeScale(Osc1RangeToVol, 1.0f, 1.0f);
    setPathTimeScale(Osc1VolToMixer, 1.0f, 1.0f);

    setPathTimeScale(Osc2WaveToFreq, osc2PreFreq, osc2PreRange);
    setPathTimeScale(Osc2FreqToRange, osc2PreRange, 1.0f);
    setPathTimeScale(Osc2RangeToVol, 1.0f, 1.0f);
    setPathTimeScale(Osc2VolToMixer, 1.0f, 1.0f);

    setPathTimeScale(Osc3WaveToFreq, osc3PreFreq, osc3PreRange);
    setPathTimeScale(Osc3FreqToRange, osc3PreRange, 1.0f);
    setPathTimeScale(Osc3RangeToVol, 1.0f, 1.0f);
    setPathTimeScale(Osc3VolToMixer, 1.0f, 1.0f);

    setPathTimeScale(NoiseToMixer, 1.0f, 1.0f);
    setPathTimeScale(MixerToFilterIn, 1.0f, 1.0f);
    setPathTimeScale(FilterInToCore, 1.0f, 1.0f);
    setPathTimeScale(FilterCoreToOut, 1.0f, 1.0f);
    setPathTimeScale(FilterOutToVCA, 1.0f, 1.0f);
    setPathTimeScale(VCAToOutputPre, 1.0f, 1.0f);
    setPathTimeScale(OutputPreToKnob, 1.0f, 1.0f);
    setPathTimeScale(KnobToOutputPost, 1.0f, 1.0f);
    setPathTimeScale(ModToOsc, 1.0f, 1.0f);
    setPathTimeScale(ModToFilter, 1.0f, 1.0f);
    setPathTimeScale(CutoffToCore, 1.0f, 1.0f);
    setPathTimeScale(EmphasisToCore, 1.0f, 1.0f);
    setPathTimeScale(ContourToCore, 1.0f, 1.0f);
    setPathTimeScale(FilterAttackToCutoff, 1.0f, 1.0f);
    setPathTimeScale(FilterDecayToCutoff, 1.0f, 1.0f);
    setPathTimeScale(FilterSustainToCutoff, 1.0f, 1.0f);
    setPathTimeScale(Keys1ToCutoff, 1.0f, 1.0f);
    setPathTimeScale(Keys2ToCutoff, 1.0f, 1.0f);
    setPathTimeScale(LoudnessAttackToVCA, 1.0f, 1.0f);
    setPathTimeScale(LoudnessDecayToVCA, 1.0f, 1.0f);
    setPathTimeScale(LoudnessSustainToVCA, 1.0f, 1.0f);
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

    auto advancePhase = [&](StageIndex stage, float baseRate) {
        if (baseRate <= 0.0f) {
            return;
        }
        const size_t index = static_cast<size_t>(stage);
        float advance = baseRate * static_cast<float>(deltaSeconds);
        float next = phaseOffsets[index] + advance;
        phaseOffsets[index] = static_cast<float>(std::fmod(next, 1.0f));
    };

    constexpr float baseDrift = 0.02f;
    advancePhase(StageOsc1Raw, baseDrift * 0.7f);
    advancePhase(StageOsc2Raw, baseDrift * 0.7f);
    advancePhase(StageOsc3Raw, baseDrift * 0.7f);
    advancePhase(StageOsc1Post, baseDrift * 0.8f);
    advancePhase(StageOsc2Post, baseDrift * 0.8f);
    advancePhase(StageOsc3Post, baseDrift * 0.8f);
    advancePhase(StageNoise, 0.0f);
    advancePhase(StageMix, baseDrift * 0.9f);
    advancePhase(StageFilter, baseDrift * 0.9f);
    advancePhase(StageOutput, baseDrift);
    advancePhase(StageModOsc, baseDrift * 0.6f);
    advancePhase(StageModFilter, baseDrift * 0.6f);
}

void SignalFlowOverlay::paint(juce::Graphics& g)
{
    constexpr bool drawPulses = false;
    constexpr bool drawThickGlow = false;

    for (size_t i = 0; i < paths.size(); ++i) {
        if (!controlPaths[i]) {
            continue;
        }
        const auto& path = paths[i];
        if (path.path.isEmpty()) {
            continue;
        }
        const float level = controlLevels[i];
        g.setColour(path.colour.withAlpha(0.08f + 0.18f * level));
        g.strokePath(path.path, juce::PathStrokeType(path.baseThickness));
    }

    for (size_t i = 0; i < paths.size(); ++i) {
        if (controlPaths[i]) {
            continue;
        }
        const auto& path = paths[i];
        if (path.path.isEmpty()) {
            continue;
        }

        const float level = pathLevels[i];
        const bool isModPath = (i == ModToOsc || i == ModToFilter);
        if (isModPath && level < 0.01f) {
            continue;
        }

        const std::array<float, waveformSize>* samples = nullptr;
        const std::array<float, waveformSize>* endSamples = nullptr;
        StageIndex stage = StageMix;
        StageIndex endStage = StageMix;
        float blendStart = 0.0f;

        switch (static_cast<int>(i)) {
            case Osc1WaveToTune:
                samples = &stageWaveforms.osc1Raw;
                stage = StageOsc1Raw;
                break;
            case Osc1TuneToRange:
            case Osc1RangeToVol:
                samples = &stageWaveforms.osc1Raw;
                stage = StageOsc1Raw;
                break;
            case Osc1VolToMixer:
                samples = &stageWaveforms.osc1;
                stage = StageOsc1Post;
                endSamples = &stageWaveforms.mix;
                endStage = StageMix;
                blendStart = 0.7f;
                break;
            case Osc2WaveToFreq:
                samples = &stageWaveforms.osc2Raw;
                stage = StageOsc2Raw;
                break;
            case Osc2FreqToRange:
            case Osc2RangeToVol:
                samples = &stageWaveforms.osc2Raw;
                stage = StageOsc2Raw;
                break;
            case Osc2VolToMixer:
                samples = &stageWaveforms.osc2;
                stage = StageOsc2Post;
                endSamples = &stageWaveforms.mix;
                endStage = StageMix;
                blendStart = 0.7f;
                break;
            case Osc3WaveToFreq:
                samples = &stageWaveforms.osc3Raw;
                stage = StageOsc3Raw;
                break;
            case Osc3FreqToRange:
            case Osc3RangeToVol:
                samples = &stageWaveforms.osc3Raw;
                stage = StageOsc3Raw;
                break;
            case Osc3VolToMixer:
                samples = &stageWaveforms.osc3;
                stage = StageOsc3Post;
                endSamples = &stageWaveforms.mix;
                endStage = StageMix;
                blendStart = 0.7f;
                break;
            case NoiseToMixer:
                samples = &stageWaveforms.noise;
                stage = StageNoise;
                endSamples = &stageWaveforms.mix;
                endStage = StageMix;
                blendStart = 0.7f;
                break;
            case MixerToFilterIn:
                samples = &stageWaveforms.mix;
                stage = StageMix;
                break;
            case FilterInToCore:
                samples = &stageWaveforms.mix;
                endSamples = &stageWaveforms.filter;
                stage = StageMix;
                endStage = StageFilter;
                blendStart = 0.35f;
                break;
            case FilterCoreToOut:
                samples = &stageWaveforms.filter;
                stage = StageFilter;
                break;
            case FilterOutToVCA:
                samples = &stageWaveforms.filter;
                stage = StageFilter;
                break;
            case VCAToOutputPre:
                samples = &stageWaveforms.filter;
                stage = StageFilter;
                break;
            case OutputPreToKnob:
                samples = &stageWaveforms.filter;
                stage = StageFilter;
                break;
            case KnobToOutputPost:
                samples = &stageWaveforms.output;
                stage = StageOutput;
                break;
            case ModToOsc:
                samples = &stageWaveforms.modOsc;
                stage = StageModOsc;
                break;
            case ModToFilter:
                samples = &stageWaveforms.modFilter;
                stage = StageModFilter;
                break;
            default:
                break;
        }

        if (samples == nullptr) {
            continue;
        }

        auto spectralColour = spectralColourForStage(stage);
        if (endSamples != nullptr) {
            spectralColour = spectralColour.interpolatedWith(spectralColourForStage(endStage), 0.5f);
        }
        const float rms = rmsForStage(stage);
        const float dynamics = juce::jlimit(0.0f, 1.0f, level * 0.8f + rms * 0.2f);
        auto glowColour = path.colour.interpolatedWith(spectralColour, 0.6f);

        const bool signalActive = (rms > 0.0002f) || (level > 0.005f);
        if (!signalActive) {
            const float guideFadeStart = 0.05f;
            const float guideFadeEnd = 0.2f;
            const float guideT = juce::jlimit(0.0f, 1.0f,
                                              (dynamics - guideFadeStart) / (guideFadeEnd - guideFadeStart));
            const float guideAlpha = 0.18f * (1.0f - guideT);
            if (guideAlpha > 0.001f) {
                g.setColour(glowColour.withAlpha(guideAlpha));
                g.strokePath(path.path, juce::PathStrokeType(path.baseThickness + dynamics * 1.0f));
            }
            continue;
        }

        if (drawThickGlow && level > 0.001f) {
            g.setColour(glowColour.withAlpha(0.15f + 0.5f * dynamics));
            g.strokePath(path.path, juce::PathStrokeType(path.baseThickness + 1.2f + dynamics * 1.2f));
        }

        float displayScale = displayScaleForStage(stage);
        float endDisplayScale = endSamples != nullptr ? displayScaleForStage(endStage) : displayScale;
        float autoScaleStart = timeScaleForStage(stage);
        float autoScaleEnd = endSamples != nullptr ? timeScaleForStage(endStage) : autoScaleStart;
        drawWaveformOnPath(g,
                           path,
                           *samples,
                           endSamples,
                           blendStart,
                           phaseOffsetForStage(stage),
                           pathTimeScaleStart[i] * autoScaleStart,
                           pathTimeScaleEnd[i] * autoScaleEnd,
                           level,
                           glowColour,
                           rms,
                           displayScale,
                           endDisplayScale);
    }

    if (drawPulses) {
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

    auto bounds = getLocalBounds().toFloat();
    auto constrain = [&](juce::Rectangle<float> rect) {
        return rect.constrainedWithin(bounds);
    };

    if (nodeValid[static_cast<size_t>(Node::FilterIn)] && nodeValid[static_cast<size_t>(Node::FilterOut)]) {
        const bool inletIsPanel = nodeBoundsValid[static_cast<size_t>(Node::FilterIn)]
            && nodeBounds[static_cast<size_t>(Node::FilterIn)].getWidth() > (filterScopeWidth * 1.1f);
        auto filterInPos = nodes[static_cast<size_t>(Node::FilterIn)];
        if (!inletIsPanel) {
            juce::Rectangle<float> preRect(filterInPos.x - filterScopeWidth * 0.5f,
                                           filterInPos.y - filterScopeHeight * 0.5f,
                                           filterScopeWidth,
                                           filterScopeHeight);
            drawMiniScope(g,
                          constrain(preRect),
                          stageWaveforms.mix,
                          phaseOffsetForStage(StageMix),
                          spectralColourForStage(StageMix),
                          rmsForStage(StageMix),
                          displayScaleForStage(StageMix));
        }
    } else if (nodeValid[static_cast<size_t>(Node::FilterCore)]) {
        auto filterPos = nodes[static_cast<size_t>(Node::FilterCore)];
        juce::Rectangle<float> preRect(filterPos.x - 110.0f, filterPos.y - 110.0f, 70.0f, 40.0f);
        drawMiniScope(g,
                      constrain(preRect),
                      stageWaveforms.mix,
                      phaseOffsetForStage(StageMix),
                      spectralColourForStage(StageMix),
                      rmsForStage(StageMix),
                      displayScaleForStage(StageMix));
    }

    if (filterVizBoundsValid) {
        drawFilterVizPanels(g);
    }
}

bool SignalFlowOverlay::hitTest(int x, int y)
{
    if (!filterVizBoundsValid) {
        return false;
    }
    return filterVizBounds.contains(static_cast<float>(x), static_cast<float>(y));
}

void SignalFlowOverlay::mouseDown(const juce::MouseEvent& event)
{
    auto layout = buildFilterVizLayout();
    if (!layout.valid) {
        return;
    }
    const auto position = event.position;
    const float handleRadius = 8.0f;
    auto within = [&](juce::Point<float> point) {
        return position.getDistanceFrom(point) <= handleRadius;
    };

    activeDragTarget = DragTarget::None;
    if (within(layout.resonanceHandle)) {
        activeDragTarget = DragTarget::Resonance;
    } else if (within(layout.contourHandle)) {
        activeDragTarget = DragTarget::ContourAmount;
    } else if (within(layout.cutoffHandle) || layout.topRect.contains(position)) {
        activeDragTarget = DragTarget::Cutoff;
    } else if (within(layout.contourEnv.attackHandle)) {
        activeDragTarget = DragTarget::ContourAttack;
    } else if (within(layout.contourEnv.decayHandle)) {
        activeDragTarget = DragTarget::ContourDecay;
    } else if (within(layout.contourEnv.sustainHandle)) {
        activeDragTarget = DragTarget::ContourSustain;
    } else if (within(layout.loudnessEnv.attackHandle)) {
        activeDragTarget = DragTarget::LoudnessAttack;
    } else if (within(layout.loudnessEnv.decayHandle)) {
        activeDragTarget = DragTarget::LoudnessDecay;
    } else if (within(layout.loudnessEnv.sustainHandle)) {
        activeDragTarget = DragTarget::LoudnessSustain;
    }

    if (activeDragTarget != DragTarget::None) {
        handleFilterVizDrag(position);
        repaint();
    }
}

void SignalFlowOverlay::mouseMove(const juce::MouseEvent& event)
{
    auto layout = buildFilterVizLayout();
    if (!layout.valid) {
        hoverTarget = DragTarget::None;
        return;
    }

    const auto position = event.position;
    const float handleRadius = 8.0f;
    auto within = [&](juce::Point<float> point) {
        return position.getDistanceFrom(point) <= handleRadius;
    };

    hoverTarget = DragTarget::None;
    if (within(layout.resonanceHandle)) {
        hoverTarget = DragTarget::Resonance;
        hoverPoint = layout.resonanceHandle;
    } else if (within(layout.contourHandle)) {
        hoverTarget = DragTarget::ContourAmount;
        hoverPoint = layout.contourHandle;
    } else if (within(layout.cutoffHandle)) {
        hoverTarget = DragTarget::Cutoff;
        hoverPoint = layout.cutoffHandle;
    } else if (within(layout.contourEnv.attackHandle)) {
        hoverTarget = DragTarget::ContourAttack;
        hoverPoint = layout.contourEnv.attackHandle;
    } else if (within(layout.contourEnv.decayHandle)) {
        hoverTarget = DragTarget::ContourDecay;
        hoverPoint = layout.contourEnv.decayHandle;
    } else if (within(layout.contourEnv.sustainHandle)) {
        hoverTarget = DragTarget::ContourSustain;
        hoverPoint = layout.contourEnv.sustainHandle;
    } else if (within(layout.loudnessEnv.attackHandle)) {
        hoverTarget = DragTarget::LoudnessAttack;
        hoverPoint = layout.loudnessEnv.attackHandle;
    } else if (within(layout.loudnessEnv.decayHandle)) {
        hoverTarget = DragTarget::LoudnessDecay;
        hoverPoint = layout.loudnessEnv.decayHandle;
    } else if (within(layout.loudnessEnv.sustainHandle)) {
        hoverTarget = DragTarget::LoudnessSustain;
        hoverPoint = layout.loudnessEnv.sustainHandle;
    }

    repaint();
}

void SignalFlowOverlay::mouseDrag(const juce::MouseEvent& event)
{
    if (activeDragTarget == DragTarget::None) {
        return;
    }
    handleFilterVizDrag(event.position);
    repaint();
}

void SignalFlowOverlay::mouseUp(const juce::MouseEvent&)
{
    activeDragTarget = DragTarget::None;
}

SignalFlowOverlay::FilterVizLayout SignalFlowOverlay::buildFilterVizLayout() const
{
    FilterVizLayout layout;
    if (!filterVizBoundsValid) {
        return layout;
    }

    auto panel = filterVizBounds;
    if (panel.getWidth() < 40.0f || panel.getHeight() < 40.0f) {
        return layout;
    }

    layout.valid = true;
    layout.panel = panel;

    auto inner = panel.reduced(8.0f);
    layout.headerRect = inner.removeFromTop(14.0f);

    const float topHeight = inner.getHeight() * 0.55f;
    layout.topRect = inner.removeFromTop(topHeight).reduced(2.0f);

    const float envGap = 6.0f;
    const float envHeight = (inner.getHeight() - envGap) * 0.5f;
    layout.contourRect = inner.removeFromTop(envHeight).reduced(2.0f);
    inner.removeFromTop(envGap);
    layout.loudnessRect = inner.removeFromTop(envHeight).reduced(2.0f);

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float logMin = std::log(minFreq);
    const float logRange = std::log(maxFreq) - logMin;
    auto freqToX = [&](float freq, juce::Rectangle<float> rect) {
        float clamped = juce::jlimit(minFreq, maxFreq, freq);
        float t = (std::log(clamped) - logMin) / logRange;
        return rect.getX() + t * rect.getWidth();
    };

    const float cutoffX = freqToX(filterVizState.cutoffHz, layout.topRect);
    const float resonance = juce::jlimit(0.0f, 1.0f, filterVizState.resonance);
    const float resonanceY = juce::jmap(resonance,
                                        layout.topRect.getBottom() - 6.0f,
                                        layout.topRect.getY() + 6.0f);
    layout.cutoffHandle = { cutoffX, layout.topRect.getCentreY() };
    layout.resonanceHandle = { cutoffX, resonanceY };
    const float contourAmount = juce::jlimit(0.0f, 1.0f, filterVizState.contourAmount);
    const float contourY = juce::jmap(contourAmount,
                                      layout.topRect.getBottom() - 6.0f,
                                      layout.topRect.getY() + 6.0f);
    layout.contourHandle = { layout.topRect.getRight() - 4.0f, contourY };

    const juce::Font labelFont(11.0f);
    const float labelPadding = 12.0f;
    const float contourLabelInset = labelFont.getStringWidthFloat("Contour Env") + labelPadding;
    const float loudnessLabelInset = labelFont.getStringWidthFloat("Loudness Env") + labelPadding;

    auto buildEnvelope = [&](juce::Rectangle<float> rect,
                             float attackNorm,
                             float decayNorm,
                             float sustainNorm,
                             float labelInset) {
        EnvelopeLayout env;
        env.rect = rect;
        env.innerRect = rect.reduced(8.0f);
        const float minWidth = 80.0f;
        const float maxInset = juce::jmax(0.0f, env.innerRect.getWidth() - minWidth);
        const float clampedInset = juce::jlimit(0.0f, maxInset, labelInset);
        env.innerRect.setLeft(env.innerRect.getX() + clampedInset);
        const float width = env.innerRect.getWidth();
        env.gap = width * 0.1f;
        env.leftSegment = width * 0.45f;
        env.rightSegment = width - env.leftSegment - env.gap;

        const float attackT = juce::jlimit(0.0f, 1.0f, attackNorm);
        const float decayT = juce::jlimit(0.0f, 1.0f, decayNorm);
        const float sustainT = juce::jlimit(0.0f, 1.0f, sustainNorm);

        const float attackX = env.innerRect.getX() + env.leftSegment * attackT;
        const float decayStartX = env.innerRect.getX() + env.leftSegment + env.gap;
        const float decayXRaw = decayStartX + env.rightSegment * decayT;
        const float minHandleSpacing = 7.0f;
        const float maxDecayX = env.innerRect.getRight() - minHandleSpacing;
        const float decayX = juce::jlimit(decayStartX, maxDecayX, decayXRaw);
        const float sustainY = env.innerRect.getBottom() - env.innerRect.getHeight() * sustainT;

        env.attackHandle = { attackX, env.innerRect.getY() };
        env.decayHandle = { decayX, sustainY };
        env.sustainHandle = { env.innerRect.getRight(), sustainY };
        return env;
    };

    layout.contourEnv = buildEnvelope(layout.contourRect,
                                      filterVizState.contourAttackNorm,
                                      filterVizState.contourDecayNorm,
                                      filterVizState.contourSustainLevel,
                                      contourLabelInset);
    layout.loudnessEnv = buildEnvelope(layout.loudnessRect,
                                       filterVizState.loudnessAttackNorm,
                                       filterVizState.loudnessDecayNorm,
                                       filterVizState.loudnessSustainLevel,
                                       loudnessLabelInset);

    return layout;
}

void SignalFlowOverlay::handleFilterVizDrag(juce::Point<float> position)
{
    auto layout = buildFilterVizLayout();
    if (!layout.valid) {
        return;
    }

    auto clamp01 = [](float value) {
        return juce::jlimit(0.0f, 1.0f, value);
    };

    switch (activeDragTarget) {
        case DragTarget::Cutoff: {
            const float t = clamp01((position.x - layout.topRect.getX()) / layout.topRect.getWidth());
            if (filterVizCallbacks.setCutoff) {
                filterVizCallbacks.setCutoff(t);
            }
            break;
        }
        case DragTarget::Resonance: {
            const float t = clamp01(1.0f - (position.y - layout.topRect.getY()) / layout.topRect.getHeight());
            if (filterVizCallbacks.setResonance) {
                filterVizCallbacks.setResonance(t);
            }
            break;
        }
        case DragTarget::ContourAmount: {
            const float t = clamp01(1.0f - (position.y - layout.topRect.getY()) / layout.topRect.getHeight());
            if (filterVizCallbacks.setContour) {
                filterVizCallbacks.setContour(t);
            }
            break;
        }
        case DragTarget::ContourAttack: {
            const float width = layout.contourEnv.leftSegment;
            if (width > 0.0f && filterVizCallbacks.setContourAttack) {
                const float t = clamp01((position.x - layout.contourEnv.innerRect.getX()) / width);
                filterVizCallbacks.setContourAttack(t);
            }
            break;
        }
        case DragTarget::ContourDecay: {
            const float width = layout.contourEnv.rightSegment;
            if (width > 0.0f && filterVizCallbacks.setContourDecay) {
                const float startX = layout.contourEnv.innerRect.getX() + layout.contourEnv.leftSegment + layout.contourEnv.gap;
                const float t = clamp01((position.x - startX) / width);
                filterVizCallbacks.setContourDecay(t);
            }
            break;
        }
        case DragTarget::ContourSustain: {
            if (filterVizCallbacks.setContourSustain) {
                const float t = clamp01(1.0f - (position.y - layout.contourEnv.innerRect.getY()) / layout.contourEnv.innerRect.getHeight());
                filterVizCallbacks.setContourSustain(t);
            }
            break;
        }
        case DragTarget::LoudnessAttack: {
            const float width = layout.loudnessEnv.leftSegment;
            if (width > 0.0f && filterVizCallbacks.setLoudnessAttack) {
                const float t = clamp01((position.x - layout.loudnessEnv.innerRect.getX()) / width);
                filterVizCallbacks.setLoudnessAttack(t);
            }
            break;
        }
        case DragTarget::LoudnessDecay: {
            const float width = layout.loudnessEnv.rightSegment;
            if (width > 0.0f && filterVizCallbacks.setLoudnessDecay) {
                const float startX = layout.loudnessEnv.innerRect.getX() + layout.loudnessEnv.leftSegment + layout.loudnessEnv.gap;
                const float t = clamp01((position.x - startX) / width);
                filterVizCallbacks.setLoudnessDecay(t);
            }
            break;
        }
        case DragTarget::LoudnessSustain: {
            if (filterVizCallbacks.setLoudnessSustain) {
                const float t = clamp01(1.0f - (position.y - layout.loudnessEnv.innerRect.getY()) / layout.loudnessEnv.innerRect.getHeight());
                filterVizCallbacks.setLoudnessSustain(t);
            }
            break;
        }
        case DragTarget::None:
        default:
            break;
    }
}

juce::Point<float> SignalFlowOverlay::pointOnPath(const FlowPath& path, float t)
{
    if (path.usePathSampling && path.length > 0.0f) {
        const float clamped = juce::jlimit(0.0f, 1.0f, t);
        const float distance = path.length * clamped;
        return path.path.getPointAlongPath(distance);
    }
    const float oneMinusT = 1.0f - t;
    const float oneMinusTSquared = oneMinusT * oneMinusT;
    const float tSquared = t * t;
    return path.start * oneMinusTSquared
        + path.control * (2.0f * oneMinusT * t)
        + path.end * tSquared;
}

juce::Point<float> SignalFlowOverlay::tangentOnPath(const FlowPath& path, float t)
{
    if (path.usePathSampling && path.length > 0.0f) {
        const float clamped = juce::jlimit(0.0f, 1.0f, t);
        const float distance = path.length * clamped;
        const float delta = juce::jmax(1.0f, path.length * 0.002f);
        const float d0 = juce::jlimit(0.0f, path.length, distance - delta);
        const float d1 = juce::jlimit(0.0f, path.length, distance + delta);
        const auto p0 = path.path.getPointAlongPath(d0);
        const auto p1 = path.path.getPointAlongPath(d1);
        return { p1.x - p0.x, p1.y - p0.y };
    }
    const float oneMinusT = 1.0f - t;
    auto a = (path.control - path.start) * (2.0f * oneMinusT);
    auto b = (path.end - path.control) * (2.0f * t);
    return a + b;
}

SignalFlowOverlay::FlowPath SignalFlowOverlay::makePath(juce::Point<float> start, juce::Point<float> end, float bend, juce::Colour colour)
{
    FlowPath path;
    path.start = start;
    path.end = end;
    path.colour = colour;
    path.baseThickness = 1.0f;
    path.length = 0.0f;
    path.usePathSampling = false;

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

void SignalFlowOverlay::drawWaveformOnPath(juce::Graphics& g,
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
                                           float endDisplayScale) const
{
    if (path.path.isEmpty()) {
        return;
    }

    const float rmsScaled = juce::jlimit(0.0f, 1.0f, rms * 0.1f);
    const float dynamics = juce::jlimit(0.0f, 1.0f, level * 0.7f + rmsScaled * 0.3f);
    const float amplitude = 3.0f + 8.4f * dynamics;
    const float offsetSamples = phaseOffset * static_cast<float>(waveformSize);
    const int windowSamples = windowSamplesForDisplay();
    const float maxScale =
        static_cast<float>(waveformSize - 1) / static_cast<float>(juce::jmax(1, windowSamples - 1));
    const float scaleStart = juce::jlimit(0.15f, 6.0f, timeScaleStart);
    const float scaleEnd = juce::jlimit(0.15f, 6.0f, timeScaleEnd);
    juce::Path wavePath;
    wavePath.preallocateSpace(windowSamples * 3);

    auto sampleAt = [](const std::array<float, waveformSize>& data, float pos) {
        float wrapped = std::fmod(pos, static_cast<float>(waveformSize));
        if (wrapped < 0.0f) {
            wrapped += static_cast<float>(waveformSize);
        }
        const int index0 = static_cast<int>(wrapped);
        const int index1 = (index0 + 1) % waveformSize;
        const float frac = wrapped - static_cast<float>(index0);
        const float a = data[static_cast<size_t>(index0)];
        const float b = data[static_cast<size_t>(index1)];
        return a + (b - a) * frac;
    };

    float samplePos = offsetSamples;
    for (int i = 0; i < windowSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(windowSamples - 1);
        float timeScale = scaleStart + (scaleEnd - scaleStart) * t;
        if (timeScale > 1.0f) {
            timeScale = 1.0f + (timeScale - 1.0f) * 0.35f;
        }
        timeScale = juce::jlimit(0.2f, maxScale, timeScale);
        auto basePoint = pointOnPath(path, t);
        auto tangent = tangentOnPath(path, t);
        float tangentLength = std::hypot(tangent.x, tangent.y);
        if (tangentLength < 0.0001f) {
            tangent = path.end - path.start;
            tangentLength = std::hypot(tangent.x, tangent.y);
        }
        if (tangentLength < 0.0001f) {
            continue;
        }
        juce::Point<float> normal(-tangent.y / tangentLength, tangent.x / tangentLength);

        float sampleValue = sampleAt(samples, samplePos) * displayScale;
        if (endSamples != nullptr) {
            float endValue = sampleAt(*endSamples, samplePos) * endDisplayScale;
            float blend = t;
            if (blendStart > 0.0f && blendStart < 1.0f) {
                blend = juce::jlimit(0.0f, 1.0f, (t - blendStart) / (1.0f - blendStart));
            }
            sampleValue = sampleValue + (endValue - sampleValue) * blend;
        }
        auto point = basePoint + normal * (sampleValue * amplitude);

        if (i == 0) {
            wavePath.startNewSubPath(point);
        } else {
            wavePath.lineTo(point);
        }

        samplePos += timeScale;
    }

    g.setColour(colour.withAlpha(0.2f + 0.5f * dynamics));
    g.strokePath(wavePath, juce::PathStrokeType(0.9f + dynamics * 1.4f));
}

void SignalFlowOverlay::drawMiniScope(juce::Graphics& g,
                                      juce::Rectangle<float> bounds,
                                      const std::array<float, waveformSize>& samples,
                                      float phaseOffset,
                                      juce::Colour colour,
                                      float rms,
                                      float displayScale) const
{
    if (bounds.getWidth() <= 2.0f || bounds.getHeight() <= 2.0f) {
        return;
    }

    const float rmsScaled = juce::jlimit(0.0f, 1.0f, rms * 0.1f);
    const float amplitude = bounds.getHeight() * (0.2f + 0.5f * rmsScaled);
    const float midY = bounds.getCentreY();
    const int offsetSamples = static_cast<int>(phaseOffset * waveformSize) % waveformSize;
    const int windowSamples = windowSamplesForDisplay();

    g.setColour(colour.withAlpha(0.25f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    juce::Path scopePath;
    scopePath.preallocateSpace(windowSamples * 3);

    for (int i = 0; i < windowSamples; ++i) {
        int sampleIndex = i + offsetSamples;
        if (sampleIndex >= waveformSize) {
            sampleIndex -= waveformSize;
        }
        float x = bounds.getX() + (bounds.getWidth() * static_cast<float>(i) / static_cast<float>(windowSamples - 1));
        float sampleValue = samples[static_cast<size_t>(sampleIndex)] * displayScale;
        float y = midY - sampleValue * amplitude;
        if (i == 0) {
            scopePath.startNewSubPath(x, y);
        } else {
            scopePath.lineTo(x, y);
        }
    }

    g.setColour(colour.withAlpha(0.35f + 0.45f * rmsScaled));
    g.strokePath(scopePath, juce::PathStrokeType(1.0f));
}

void SignalFlowOverlay::drawFilterVizPanels(juce::Graphics& g) const
{
    auto layout = buildFilterVizLayout();
    if (!layout.valid) {
        return;
    }

    const auto borderColour = juce::Colour::fromRGB(120, 150, 210).withAlpha(0.35f);
    const auto fillColour = juce::Colour::fromRGB(8, 12, 22).withAlpha(0.55f);
    const auto labelColour = juce::Colour::fromRGB(205, 220, 245).withAlpha(0.9f);
    const auto accentColour = juce::Colour::fromRGB(80, 180, 255);
    const auto contourColour = juce::Colour::fromRGB(90, 200, 255);
    const auto loudnessColour = juce::Colour::fromRGB(70, 140, 230);
    const auto attackColour = juce::Colour::fromRGB(90, 220, 150);
    const auto decayColour = juce::Colour::fromRGB(235, 90, 90);
    const auto sustainColour = juce::Colour::fromRGB(190, 120, 255);

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float logMin = std::log(minFreq);
    const float logRange = std::log(maxFreq) - logMin;
    auto freqToX = [&](float freq, juce::Rectangle<float> rect) {
        float clamped = juce::jlimit(minFreq, maxFreq, freq);
        float t = (std::log(clamped) - logMin) / logRange;
        return rect.getX() + t * rect.getWidth();
    };
    auto responseMag = [&](float freq, float cutoff, float resonance) {
        float safeCutoff = juce::jmax(minFreq, cutoff);
        float x = freq / safeCutoff;
        float mag = 1.0f / std::sqrt(1.0f + std::pow(x, 8.0f));
        float bump = resonance * std::exp(-std::pow((std::log(freq) - std::log(safeCutoff)) / 0.5f, 2.0f));
        return juce::jlimit(0.0f, 1.2f, mag + bump * 0.4f);
    };

    g.setColour(fillColour);
    g.fillRoundedRectangle(layout.panel, 8.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(layout.panel, 8.0f, 1.0f);

    g.setFont(11.0f);
    g.setColour(labelColour);
    g.drawText("Filter + Envelopes", layout.headerRect, juce::Justification::centredLeft);

    // Top spectrum + filter response
    const auto& pre = stageStats[static_cast<size_t>(StageMix)].spectrum;
    const auto& post = stageStats[static_cast<size_t>(StageFilter)].spectrum;

    juce::Path prePath;
    juce::Path postPath;
    for (int i = 0; i < spectrumBins; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(spectrumBins - 1);
        const float x = layout.topRect.getX() + t * layout.topRect.getWidth();
        const float preY = layout.topRect.getBottom() - pre[static_cast<size_t>(i)] * layout.topRect.getHeight();
        const float postY = layout.topRect.getBottom() - post[static_cast<size_t>(i)] * layout.topRect.getHeight();
        if (i == 0) {
            prePath.startNewSubPath(x, preY);
            postPath.startNewSubPath(x, postY);
        } else {
            prePath.lineTo(x, preY);
            postPath.lineTo(x, postY);
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));
    g.setColour(accentColour.withAlpha(0.5f));
    g.strokePath(postPath, juce::PathStrokeType(1.2f));

    const float cutoff = filterVizState.cutoffHz;
    const float resonance = juce::jlimit(0.0f, 1.0f, filterVizState.resonance);
    const float contourAmount = juce::jlimit(0.0f, 1.0f, filterVizState.contourAmount);
    float envCutoff = cutoff * (1.0f + contourAmount * filterVizState.filterEnvelope * 2.5f);
    envCutoff = juce::jlimit(minFreq, maxFreq, envCutoff);

    juce::Path response;
    const int steps = 80;
    for (int i = 0; i < steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps - 1);
        const float freq = minFreq * std::exp(logRange * t);
        const float mag = responseMag(freq, cutoff, resonance);
        const float x = layout.topRect.getX() + t * layout.topRect.getWidth();
        const float y = layout.topRect.getBottom() - mag * layout.topRect.getHeight() * 0.7f - layout.topRect.getHeight() * 0.1f;
        if (i == 0) {
            response.startNewSubPath(x, y);
        } else {
            response.lineTo(x, y);
        }
    }
    g.setColour(accentColour.withAlpha(0.4f));
    g.strokePath(response, juce::PathStrokeType(1.0f));

    const float cutoffX = freqToX(cutoff, layout.topRect);
    const float envX = freqToX(envCutoff, layout.topRect);
    if (std::abs(envX - cutoffX) > 1.0f) {
        auto band = layout.topRect.withLeft(std::min(cutoffX, envX)).withRight(std::max(cutoffX, envX));
        g.setColour(accentColour.withAlpha(0.08f));
        g.fillRect(band);
        g.setColour(accentColour.withAlpha(0.25f));
        g.drawLine(envX, layout.topRect.getY() + 1.0f, envX, layout.topRect.getBottom() - 1.0f, 1.0f);
    }

    g.setColour(accentColour.withAlpha(0.55f));
    g.drawLine(cutoffX, layout.topRect.getY() + 1.0f, cutoffX, layout.topRect.getBottom() - 1.0f, 1.2f);

    auto curtain = layout.topRect.withLeft(envX);
    juce::ColourGradient curtainGradient(juce::Colours::black.withAlpha(0.0f),
                                         envX, layout.topRect.getY(),
                                         juce::Colours::black.withAlpha(0.45f),
                                         layout.topRect.getRight(), layout.topRect.getY(),
                                         false);
    g.setGradientFill(curtainGradient);
    g.fillRect(curtain);

    const float modRms = stageStats[static_cast<size_t>(StageModFilter)].rms;
    const float modDepthHz = modRms * 5000.0f;
    if (modDepthHz > 1.0f) {
        const float modCenterHz = envCutoff;
        const float modMinHz = juce::jlimit(minFreq, maxFreq, modCenterHz - modDepthHz);
        const float modMaxHz = juce::jlimit(minFreq, maxFreq, modCenterHz + modDepthHz);
        const float modLeftX = freqToX(modMinHz, layout.topRect);
        const float modRightX = freqToX(modMaxHz, layout.topRect);
        juce::Rectangle<float> modBand = layout.topRect.withLeft(modLeftX).withRight(modRightX);
        g.setColour(accentColour.withAlpha(0.08f));
        g.fillRect(modBand);

        const float modSample = stageWaveforms.modFilter[waveformSize - 1];
        const float modInstantHz = juce::jlimit(minFreq, maxFreq, modCenterHz + modSample * 5000.0f);
        const float modX = freqToX(modInstantHz, layout.topRect);
        g.setColour(accentColour.withAlpha(0.45f));
        g.drawLine(modX, layout.topRect.getY() + 2.0f, modX, layout.topRect.getBottom() - 2.0f, 1.0f);
    }

    if (contourEnvelopeRising || contourEnvelopeFalling || contourEnvelopeSustain) {
        juce::Colour highlight = juce::Colours::transparentBlack;
        if (contourEnvelopeRising) {
            highlight = juce::Colour::fromRGB(90, 220, 150).withAlpha(0.18f);
        } else if (contourEnvelopeFalling) {
            highlight = juce::Colours::red.withAlpha(0.18f);
        } else if (contourEnvelopeSustain) {
            highlight = juce::Colour::fromRGB(190, 120, 255).withAlpha(0.18f);
        }

        if (highlight.getAlpha() > 0) {
            g.setColour(highlight);
            g.drawLine(envX, layout.topRect.getY() + 1.0f, envX, layout.topRect.getBottom() - 1.0f, 1.3f);
            juce::ColourGradient highlightCurtain(juce::Colours::transparentBlack,
                                                  envX, layout.topRect.getY(),
                                                  highlight,
                                                  layout.topRect.getRight(), layout.topRect.getY(),
                                                  false);
            g.setGradientFill(highlightCurtain);
            g.fillRect(curtain);
        }
    }

    const float resonanceRadius = 2.5f + resonance * 5.0f;
    g.setColour(accentColour.withAlpha(0.25f));
    g.fillEllipse(layout.resonanceHandle.x - resonanceRadius * 2.0f,
                  layout.resonanceHandle.y - resonanceRadius * 2.0f,
                  resonanceRadius * 4.0f,
                  resonanceRadius * 4.0f);
    g.setColour(accentColour.withAlpha(0.9f));
    g.fillEllipse(layout.resonanceHandle.x - resonanceRadius,
                  layout.resonanceHandle.y - resonanceRadius,
                  resonanceRadius * 2.0f,
                  resonanceRadius * 2.0f);

    // Contour depth slider
    const float contourTrackX = layout.contourHandle.x;
    g.setColour(borderColour.withAlpha(0.5f));
    g.drawLine(contourTrackX, layout.topRect.getY() + 2.0f, contourTrackX, layout.topRect.getBottom() - 2.0f, 1.0f);
    g.setColour(contourColour.withAlpha(0.9f));
    g.fillEllipse(contourTrackX - 3.5f, layout.contourHandle.y - 3.5f, 7.0f, 7.0f);

    // Envelope panels
    auto drawEnvelope = [&](const EnvelopeLayout& env,
                            juce::Colour colour,
                            const juce::String& label,
                            bool dashedLine) {
        g.setColour(borderColour.withAlpha(0.4f));
        g.drawRoundedRectangle(env.rect, 5.0f, 1.0f);
        auto labelArea = env.rect.withHeight(12.0f);
        g.setColour(labelColour.withAlpha(0.7f));
        g.drawText(label, labelArea, juce::Justification::centredLeft);

        const auto& r = env.innerRect;
        juce::Path path;
        path.startNewSubPath(r.getX(), r.getBottom());
        path.lineTo(env.attackHandle.x, env.attackHandle.y);
        path.lineTo(env.decayHandle.x, env.decayHandle.y);
        path.lineTo(r.getRight(), env.decayHandle.y);
        if (dashedLine) {
            const float dashPattern[] = { 4.0f, 3.0f };
            juce::Path glowPath;
            juce::PathStrokeType glowStroke(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            glowStroke.createDashedStroke(glowPath, path, dashPattern, 2);
            g.setColour(colour.withAlpha(0.12f));
            g.fillPath(glowPath);

            juce::Path dashedPath;
            juce::PathStrokeType dashStroke(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            dashStroke.createDashedStroke(dashedPath, path, dashPattern, 2);
            g.setColour(colour.withAlpha(0.65f));
            g.fillPath(dashedPath);

            const float tickTop = r.getBottom() - 6.0f;
            const float tickBottom = r.getBottom() - 1.0f;
            g.setColour(colour.withAlpha(0.55f));
            auto drawTick = [&](float x) {
                g.drawLine(x, tickTop, x, tickBottom, 1.0f);
            };
            drawTick(env.attackHandle.x);
            drawTick(env.decayHandle.x);
            drawTick(env.sustainHandle.x);
        } else {
            g.setColour(colour.withAlpha(0.65f));
            g.strokePath(path, juce::PathStrokeType(1.4f));
        }

        const float handleRadius = 3.4f;
        g.setColour(attackColour.withAlpha(0.9f));
        g.fillEllipse(env.attackHandle.x - handleRadius,
                      env.attackHandle.y - handleRadius,
                      handleRadius * 2.0f,
                      handleRadius * 2.0f);
        g.setColour(decayColour.withAlpha(0.9f));
        g.fillEllipse(env.decayHandle.x - handleRadius,
                      env.decayHandle.y - handleRadius,
                      handleRadius * 2.0f,
                      handleRadius * 2.0f);
        g.setColour(sustainColour.withAlpha(0.9f));
        g.fillEllipse(env.sustainHandle.x - handleRadius,
                      env.sustainHandle.y - handleRadius,
                      handleRadius * 2.0f,
                      handleRadius * 2.0f);

        if (dashedLine) {
            const float envelopeValue = juce::jlimit(0.0f, 1.0f, filterVizState.contourEnvelope);
            if (envelopeValue > 0.01f) {
                const float sustainLevel = juce::jlimit(0.0f, 1.0f, filterVizState.loudnessSustainLevel);
                const auto attackStart = juce::Point<float>(r.getX(), r.getBottom());
                juce::Point<float> indicatorPoint = env.decayHandle;
                if (loudnessEnvelopeRising) {
                    const float t = envelopeValue;
                    indicatorPoint = attackStart + (env.attackHandle - attackStart) * t;
                } else if (loudnessEnvelopeFalling) {
                    const float denom = juce::jmax(0.0001f, 1.0f - sustainLevel);
                    const float t = juce::jlimit(0.0f, 1.0f, (1.0f - envelopeValue) / denom);
                    indicatorPoint = env.attackHandle + (env.decayHandle - env.attackHandle) * t;
                } else {
                    indicatorPoint = env.decayHandle + (env.sustainHandle - env.decayHandle) * 0.7f;
                }

                const float intensity = juce::jlimit(0.0f, 1.0f, envelopeValue * 1.2f);
                const auto indicatorColour = colour.interpolatedWith(juce::Colours::white, 0.35f);
                const float glowRadius = 5.0f;
                const float dotRadius = 2.4f;
                g.setColour(indicatorColour.withAlpha(0.12f + 0.28f * intensity));
                g.fillEllipse(indicatorPoint.x - glowRadius,
                              indicatorPoint.y - glowRadius,
                              glowRadius * 2.0f,
                              glowRadius * 2.0f);
                g.setColour(indicatorColour.withAlpha(0.45f + 0.45f * intensity));
                g.fillEllipse(indicatorPoint.x - dotRadius,
                              indicatorPoint.y - dotRadius,
                              dotRadius * 2.0f,
                              dotRadius * 2.0f);
            }
        }
    };

    drawEnvelope(layout.contourEnv, contourColour, "Contour Env", false);
    drawEnvelope(layout.loudnessEnv, loudnessColour, "Loudness Env", true);

    if (hoverTarget != DragTarget::None) {
        auto toMs = [](float norm) {
            const float adjusted = (norm <= 0.0f ? 0.01f : norm);
            return adjusted * 10000.0f;
        };
        juce::String text;
        switch (hoverTarget) {
            case DragTarget::Cutoff:
                text = "Cutoff " + juce::String(filterVizState.cutoffHz, 0) + " Hz";
                break;
            case DragTarget::Resonance:
                text = "Emphasis " + juce::String(filterVizState.resonance * 10.0f, 1);
                break;
            case DragTarget::ContourAmount:
                text = "Contour " + juce::String(filterVizState.contourAmount * 10.0f, 1);
                break;
            case DragTarget::ContourAttack:
                text = "C Attack " + juce::String(toMs(filterVizState.contourAttackNorm), 0) + " ms";
                break;
            case DragTarget::ContourDecay:
                text = "C Decay " + juce::String(toMs(filterVizState.contourDecayNorm), 0) + " ms";
                break;
            case DragTarget::ContourSustain:
                text = "C Sustain " + juce::String(filterVizState.contourSustainLevel, 2);
                break;
            case DragTarget::LoudnessAttack:
                text = "Attack " + juce::String(toMs(filterVizState.loudnessAttackNorm), 0) + " ms";
                break;
            case DragTarget::LoudnessDecay:
                text = "Decay " + juce::String(toMs(filterVizState.loudnessDecayNorm), 0) + " ms";
                break;
            case DragTarget::LoudnessSustain:
                text = "Sustain " + juce::String(filterVizState.loudnessSustainLevel, 2);
                break;
            default:
                break;
        }

        if (text.isNotEmpty()) {
            const float paddingX = 6.0f;
            const float paddingY = 3.0f;
            g.setFont(11.0f);
            const float textWidth = g.getCurrentFont().getStringWidthFloat(text);
            const float textHeight = g.getCurrentFont().getHeight();
            juce::Rectangle<float> tipRect(hoverPoint.x + 10.0f,
                                           hoverPoint.y - textHeight - 12.0f,
                                           textWidth + paddingX * 2.0f,
                                           textHeight + paddingY * 2.0f);
            tipRect = tipRect.constrainedWithin(layout.panel);
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillRoundedRectangle(tipRect, 4.0f);
            g.setColour(labelColour.withAlpha(0.95f));
            g.drawText(text, tipRect, juce::Justification::centred);
        }
    }
}


void SignalFlowOverlay::updateStageStats(StageIndex stage, const std::array<float, waveformSize>& samples)
{
    float rmsSum = 0.0f;
    float peak = 0.0f;
    for (float sample : samples) {
        rmsSum += sample * sample;
        peak = juce::jmax(peak, std::abs(sample));
    }
    stageStats[static_cast<size_t>(stage)].rms = std::sqrt(rmsSum / static_cast<float>(waveformSize));
    stageStats[static_cast<size_t>(stage)].peak = peak;

    float previousOffset = stageStats[static_cast<size_t>(stage)].zeroCrossOffset;
    int bestIndex = -1;
    float bestDistance = 1.0f;
    int lastCross = -1;
    float periodSum = 0.0f;
    int periodCount = 0;
    for (int i = 0; i < waveformSize - 1; ++i) {
        if (samples[static_cast<size_t>(i)] <= 0.0f
            && samples[static_cast<size_t>(i + 1)] > 0.0f) {
            float candidate = static_cast<float>(i + 1) / static_cast<float>(waveformSize);
            float delta = candidate - previousOffset;
            if (delta > 0.5f) {
                delta -= 1.0f;
            } else if (delta < -0.5f) {
                delta += 1.0f;
            }
            float distance = std::abs(delta);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i + 1;
            }
            if (lastCross >= 0) {
                periodSum += static_cast<float>(i + 1 - lastCross);
                ++periodCount;
            }
            lastCross = i + 1;
        }
    }
    if (periodCount > 0) {
        stageStats[static_cast<size_t>(stage)].periodSamples = periodSum / static_cast<float>(periodCount);
    } else {
        stageStats[static_cast<size_t>(stage)].periodSamples = static_cast<float>(waveformSize);
    }
    if (bestIndex >= 0) {
        float candidate = static_cast<float>(bestIndex) / static_cast<float>(waveformSize);
        float delta = candidate - previousOffset;
        if (delta > 0.5f) {
            delta -= 1.0f;
        } else if (delta < -0.5f) {
            delta += 1.0f;
        }
        constexpr float smoothing = 0.08f;
        float smoothed = previousOffset + delta * smoothing;
        if (smoothed < 0.0f) {
            smoothed += 1.0f;
        } else if (smoothed >= 1.0f) {
            smoothed -= 1.0f;
        }
        stageStats[static_cast<size_t>(stage)].zeroCrossOffset = smoothed;
    }

    std::fill(fftScratch.begin(), fftScratch.end(), 0.0f);
    for (int i = 0; i < waveformSize; ++i) {
        fftScratch[static_cast<size_t>(i)] = samples[static_cast<size_t>(i)];
    }
    window.multiplyWithWindowingTable(fftScratch.data(), waveformSize);
    fft.performFrequencyOnlyForwardTransform(fftScratch.data());

    const int maxBin = waveformSize / 2;
    const int lowEnd = 4;
    const int midEnd = 16;
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
    float total = 0.0f;
    float centroidSum = 0.0f;

    auto& spectrum = stageStats[static_cast<size_t>(stage)].spectrum;
    spectrum.fill(0.0f);

    for (int i = 1; i < maxBin; ++i) {
        float magnitude = fftScratch[static_cast<size_t>(i)];
        total += magnitude;
        centroidSum += magnitude * static_cast<float>(i);
        if (i <= lowEnd) {
            low += magnitude;
        } else if (i <= midEnd) {
            mid += magnitude;
        } else {
            high += magnitude;
        }
        const int binIndex = (i * spectrumBins) / maxBin;
        if (binIndex >= 0 && binIndex < spectrumBins) {
            spectrum[static_cast<size_t>(binIndex)] += magnitude;
        }
    }

    float spectrumMax = 0.0f;
    for (float& value : spectrum) {
        value = std::log10(1.0f + value);
        spectrumMax = std::max(spectrumMax, value);
    }
    if (spectrumMax > 0.0f) {
        for (float& value : spectrum) {
            value /= spectrumMax;
        }
    }

    if (total > 0.0f) {
        float lowRatio = low / total;
        float midRatio = mid / total;
        float highRatio = high / total;
        float hue = juce::jmap(highRatio, 0.0f, 1.0f, 0.08f, 0.58f);
        float saturation = juce::jlimit(0.25f, 0.95f, 0.35f + midRatio * 0.6f);
        float brightness = juce::jlimit(0.25f, 1.0f, 0.3f + lowRatio * 0.7f);
        stageStats[static_cast<size_t>(stage)].colour = juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);

        float centroidBin = centroidSum / total;
        stageStats[static_cast<size_t>(stage)].centroidHz = centroidBin * static_cast<float>(sampleRate) / static_cast<float>(waveformSize);
    } else {
        stageStats[static_cast<size_t>(stage)].colour = juce::Colours::white;
        stageStats[static_cast<size_t>(stage)].centroidHz = 0.0f;
    }
}

juce::Colour SignalFlowOverlay::spectralColourForStage(StageIndex stage) const
{
    return stageStats[static_cast<size_t>(stage)].colour;
}

float SignalFlowOverlay::rmsForStage(StageIndex stage) const
{
    return stageStats[static_cast<size_t>(stage)].rms;
}

float SignalFlowOverlay::phaseOffsetForStage(StageIndex stage) const
{
    const auto& stats = stageStats[static_cast<size_t>(stage)];
    float offset = stats.zeroCrossOffset - phaseOffsets[static_cast<size_t>(stage)];
    offset = static_cast<float>(std::fmod(offset, 1.0f));
    if (offset < 0.0f) {
        offset += 1.0f;
    }
    return offset;
}

float SignalFlowOverlay::displayScaleForStage(StageIndex stage) const
{
    const float peak = stageStats[static_cast<size_t>(stage)].peak;
    if (peak <= 0.0001f) {
        return 1.0f;
    }
    const float targetPeak = 0.75f;
    float scale = targetPeak / peak;
    return juce::jlimit(0.2f, 1.4f, scale);
}

float SignalFlowOverlay::timeScaleForStage(StageIndex stage) const
{
    const float periodSamples = stageStats[static_cast<size_t>(stage)].periodSamples;
    const int windowSamples = windowSamplesForDisplay();
    if (windowSamples <= 0) {
        return 1.0f;
    }
    const float cycles = 1.0f;
    float scale = (periodSamples * cycles) / static_cast<float>(windowSamples);
    return juce::jlimit(0.2f, 10.0f, scale);
}

int SignalFlowOverlay::windowSamplesForDisplay() const
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int windowSamples = static_cast<int>(sr * 0.01f);
    return juce::jlimit(16, waveformSize, windowSamples);
}

void SignalFlowOverlay::setPathTimeScale(PathId path, float start, float end)
{
    const size_t index = static_cast<size_t>(path);
    pathTimeScaleStart[index] = start;
    pathTimeScaleEnd[index] = end;
}
