# Workstream 07 — Model D Contour Generators

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Oscillators](06-band-limited-calibrated-oscillators.md) · Next: [Filter](08-virtual-analog-ladder-filter.md)

## Goal and user-visible outcome

Filter and Loudness contours respond like Model D contour generators: published time/level ranges, measured nonlinear knob tapers and curve shapes, classic single-trigger legato, optional multi-trigger, and release tails controlled by the Decay switch without corrupting held-note decay.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| ENV-001 | Replace the linear 0…10-second mapping and zero-position discontinuity. |
| ENV-002 | Cover published 1 ms–10 s attack, 4 ms–greater-than-35 s decay/release, and 0–100% sustain endpoints. |
| ENV-003 | Use measured nonlinear control tapers and analog-like stage curves with provenance. |
| ENV-004 | Implement stable Attack, Decay, Sustain, Release/Return, and Idle transitions, including zero/near-zero and live parameter changes. |
| ENV-005 | Make Decay switch affect post-key-release reuse of Decay Time only; held decay-to-sustain always occurs. |
| ENV-006 | Consume Single/Multi trigger actions exactly as defined by Workstream 05. |
| ENV-007 | Keep VCA release audible and report contour silence to oscillator/engine idling. |
| ENV-008 | Report a truthful host tail bound and reset behavior. |
| ENV-009 | Calibrate and test Filter and Loudness contours independently rather than cross-routing identities. |

## Current-code evidence

- `normalizedToMilliseconds(0)` returns 100 ms, while any positive value is `value * 10000`; the control jumps at zero, is linear, caps at 10 s, and misses the published endpoints.
- `EnvelopeGenerator::getNextSample` uses linear increments/decrements. `noteOn` always resets to zero; `noteOff` reuses `decayRate` as release with no Decay-switch policy inside the generator.
- `processBlock` changes contour sustain to 1.0 when Decay is off, preventing the normal held-note decay-to-sustain stage rather than only changing release behavior.
- Filter/Loudness parameter IDs are cross-routed between `setEnvelopeSettings`, `setContourEnvelopeSettings`, filter processing, and VCA multiplication.
- Oscillators are stopped before the output release can be heard, and `getTailLengthSeconds` reports 0.

## Prerequisites, ownership, and merge conflicts

Requires canonical/legacy contour adapters (03), gate/retrigger/release actions (05), and contour analyzer/capture fixtures (12). It supplies a stable `ContourPair` interface to the filter (08), VCA/output (09), and lifecycle idle contract.

Owns time/level mappings, stage equations, Decay-switch release semantics, contour calibration, silence reporting, and tail calculation. Workstream 03 owns identity/migration; 05 owns trigger decisions.

Conflict hotspots: `EnvelopeGenerator`, the two envelopes embedded in `LadderFilter`, contour setup/render in `processBlock`, editor time conversion, and tail reporting. Replace with a separate `ContourPair`; do not leave contours owned by the filter class.

## In scope

Filter/Loudness contour engines, panel time/sustain mappings, trigger modes, Decay switch, live automation, output-silence signal, tail metadata, reference capture/analysis.

## Out of scope

Filter cutoff mapping, VCA saturation/gain curve, velocity sensitivity, extra ADSR release knob, modulation destinations, or invented analog time constants.

## Proposed architecture and data flow

```text
GatePolicy VoiceAction
  -> ContourPair::apply(action)
ParameterSnapshot + ContourCalibration
  -> physical attack/decay/sustain/release-off behavior
  -> FilterContour sample (normalized 0..1)
  -> LoudnessContour sample (normalized control value)
  -> isSilent / maximumTailSamples
```

Use a target-ratio one-pole/exponential stage core with coefficients derived from physical time and measured stage target ratios. `ContourCalibration` provides separate Filter/Loudness knob-taper lookup curves, stage target ratios/shape residuals, Decay-off return behavior, and silence criterion, all versioned and provenance-linked. Monotone interpolation is required; no cubic overshoot.

```cpp
enum class ContourStage : uint8_t { idle, attack, decay, sustain, release };
struct ContourSettings { Seconds attack; Seconds decay; float sustain; bool releaseUsesDecay; };
struct ContourFrame { float filter; float loudness; ContourStage filterStage; ContourStage loudnessStage; };
class ContourPair {
public:
    void apply(VoiceAction) noexcept;
    ContourFrame next() noexcept;
    bool isLoudnessSilent() const noexcept;
    int maximumTailSamples() const noexcept;
};
```

## Stage behavior

- Gate open/retrigger in Multi starts Attack from the instantaneous level, unless the measured reference profile proves a different reset behavior; that behavior then becomes a profile field. Single legato pitch changes do not call the contour.
- Attack reaches peak under the profile’s measured time definition, then Decay always proceeds toward Sustain while the gate remains open.
- Sustain is held while gate remains open after Decay.
- Gate close with Decay on enters Release using that contour’s Decay Time and release curve.
- Gate close with Decay off enters the independently measured Decay-off return behavior. It does not force held Sustain to one and is not assumed instantaneous without measurements.
- Live time change preserves level and stage; coefficient changes at the next sub-block boundary. Sustain change during Decay changes the target without a level discontinuity. A target above current level does not invent a new attack; it holds/clamps according to the measured profile rule stored in calibration.
- Hard panic moves both contours to exact zero/idle and clears history.

## Published versus measured values

Published endpoints (manual p. 80) are hard constraints: Attack 1 ms–10 s; Decay/Release 4 ms–>35 s; Sustain 0–100%. The exact maximum above 35 s, knob tapers, stage time definition (percentage reached), curve shape, retrigger from nonzero, Decay-off return, and silence threshold require hardware capture.

Capture both contours as control effects: Filter Contour through a calibrated filter-tracking setup and Loudness Contour through a fixed carrier/VCA measurement. Record repeated gates, panel positions, unit/calibration/temperature/warm-up, gate source/length, capture chain, raw hashes, analysis version, and uncertainty. Store derived bands in Workstream 12’s manifest.

## Backward compatibility and preset migration

Workstream 03’s `legacyCrossed` adapter determines which saved IDs feed each physical contour; `ContourPair` itself always exposes correct Filter/Loudness ports. Canonical presets use correct identities. Time parameter normalization remains stable, but physical mapping becomes profile/version aware; presets record the calibration profile. Legacy linear contour behavior is not a user mode.

Host tail equals the greater published/profile maximum Loudness release plus nonlinear/oversampler settling from Workstream 09, converted to seconds. If the profile’s “>35 s” maximum is not yet measured, report the conservative approved bound stored in the manifest; never report zero.

## Real-time audio constraints

Precompute coefficients at sub-block boundaries. Sample processing has fixed arithmetic, no division by zero, denormals, allocation, locks, strings, logging, table loading, or unbounded solving. Calibration tables are validated/preloaded. Stage values remain finite and within the profile’s documented bound.

## Edge cases and failure modes

Minimum/maximum time, zero sustain, gate shorter than one sample, retrigger during each stage, release then re-gate, Single/Multi automation, Decay switch change during release, sample-rate change, corrupt calibration, and render after panic. Missing measured data blocks fidelity sign-off but loads the baseline published-range model safely.

## Implementation sequence

1. Capture current mapping/stage/release failures as fixtures.
2. Separate contours from `LadderFilter`; implement typed stage core and published endpoints.
3. Integrate VoiceAction Single/Multi behavior and Decay-switch release state.
4. Add live-change, silence, reset, and tail contracts.
5. Add calibration mapping/shape loader and reference measurements.
6. Integrate Filter/Loudness consumers and remove crossed workaround from engine paths.

## Automated tests and measurable gates

- Normalized endpoints map to published Attack/Decay/Sustain ranges; mappings are finite and monotonic. Top Decay is strictly greater than 35 s once the reference profile is approved.
- Held gate always executes Attack→Decay→Sustain regardless of Decay switch.
- Gate close uses Decay Time only when Decay is on; Decay-off behavior matches its approved reference band.
- Single/Multi lifecycle trace exactly matches Workstream 05; no unintended retrigger.
- Every stage transition is level-continuous under the derived-software manifest, except explicit hard panic.
- Release remains nonzero until the declared silence criterion and reaches idle within the approved tail bound.
- Filter and Loudness parameter perturbation affects only its intended contour in canonical mode; legacy adapter routing passes v0 fixtures.
- Timing/shape error at required panel positions/sample rates falls inside approved hardware bands; absent bands remain `awaiting-approved-reference`.
- Block partitioning does not alter contour output for identical sample-timestamped actions.

## Manual, listening, and hardware validation

Measure minimum/mid/maximum Attack and Decay, multiple Sustain levels, short gates, legato/multi retrigger, Decay off/on, release/re-gate, and sample rates. Blind, level-matched listening may judge articulation after numerical curve/timing gates; it cannot approve missing measurements.

## Definition of done

- [ ] ENV-001 through ENV-009 pass.
- [ ] Published endpoints and measured curves/tapers are provenance-backed.
- [ ] Decay switch and Single/Multi truth tables pass.
- [ ] Release is audible and tail reporting is nonzero/truthful.
- [ ] Filter/Loudness ports are semantically independent.

## Completion-report evidence

Include mapping tables; stage diagrams/traces; endpoint sample counts; Decay on/off and trigger truth tables; reference capture manifests/uncertainty; curve overlays; block-partition hashes; tail report/impulse decay; real-time instrumentation; and separated listening notes.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 23, 28–31, 47, 80.
- Will Pirkle, *Designing Software Synthesizer Plug-Ins in C++*, contour-generator chapters (implementation background; hardware claims still require primary measurements).
- JUCE [`AudioProcessor::getTailLengthSeconds`](https://docs.juce.com/master/classAudioProcessor.html).
