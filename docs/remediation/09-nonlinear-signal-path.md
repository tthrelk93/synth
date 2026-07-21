# Workstream 09 — Nonlinear Mixer, Feedback, VCA, and Output Path

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Filter](08-virtual-analog-ladder-filter.md) · Next: [UI](10-ui-correctness-authentic-panel.md)

## Goal and user-visible outcome

Oscillators, noise, external input, feedback, ladder, VCA, Main Output, Phones/Cue, and overload indication form one calibrated, stable Model D signal path. External audio is processable through the instrument; when no external bus is connected, output can be normalled back to External Input for authentic feedback without runaway or non-finite output.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| SIG-001 | Separate mixer drive, ladder nonlinearity, VCA gain/nonlinearity, and output stages with independent taps/tests. |
| SIG-002 | Run the nonlinear core at fixed 4× oversampling, report exact latency, and reset its state deterministically. |
| SIG-003 | Process the optional auxiliary External Input through the panel switch/volume and mono Model D path. |
| SIG-004 | Implement normalled post-Main-volume internal feedback when external input is absent, with explicit routing metadata. |
| SIG-005 | Bound feedback and all nonlinear stages against runaway, denormals, NaN/Inf, and unsafe recovery. |
| SIG-006 | Implement Main Output switch/volume without muting the independent Phones/Cue path. |
| SIG-007 | Implement Phones Volume on an optional Phones/Cue output; define disabled-bus/standalone behavior. |
| SIG-008 | Drive Overload from the documented external/mixer stage with measured threshold and attack/release ballistics. |
| SIG-009 | Calibrate gain staging, drive/saturation spectra, noise/external levels, feedback attenuation, and output levels from published specs/measurements. |
| SIG-010 | Pass bypass/switch, silence, feedback stability, latency, reset, and non-finite safety gates. |

## Current-code evidence

- `processBlock` linearly scales oscillator sends by 0…10, sums mono, applies a fixed `tanh(input*0.7)` inside `LadderFilter`, multiplies by a linear contour and Output Volume. Mixer, ladder, VCA, and output saturation cannot be isolated.
- There is no oversampler or latency reporting/reset.
- An input buffer can be summed when built as an effect, but the project is not an instrument and the bus is not an optional named auxiliary input.
- `feedbackKnob` is read and sent to `LadderFilter::setFeedback`; the filter uses it only in a dead `feedbackAmount` calculation. The UI feedback control is not constructed.
- No Main Output parameter exists; its layout entry/toggle construction is commented. `outputPhonesVolKnob` is never used in DSP and its position is commented.
- `overloadButton` is a clickable UI button, not a signal indicator. No detector or ballistics exist.

## Prerequisites, ownership, and merge conflicts

Requires buses/latency infrastructure (02), parameters/state (03), oscillators (06), contours (07), ladder (08), and analyzers/reference skeleton (12). It supplies signal taps and functional contracts to UI (10) and RT hardening (11).

Owns mixer/noise/external gain, oversampling wrapper, internal feedback routing, VCA/output/phones stages, overload detector, latency/reset, and signal-path calibration. Workstream 08 owns ladder internals.

Conflict hotspots: processor constructor/buses, almost all sample mixing/output in `processBlock`, filter call, UI feedback/output/overload, circular/overlay taps. Integrate through `NonlinearSignalPath`, not scattered multipliers.

## In scope

Five mixer sources, normalled feedback, auxiliary input, 4× nonlinear oversampling, ladder integration, VCA, main/phones output, A-440 routing, overload detector, state reset, gain/saturation/latency tests.

## Out of scope

A separate effect plug-in, stereo widening, arbitrary external effects, amplifier/cab/mic simulation (Recording Recreation), or guessed circuit gains/thresholds.

## Chosen architecture and data flow

```text
osc1/2/3 calibrated sends + noise + External source
  -> source gains/switches
  -> mixer summer + measured drive/nonlinearity [tap: mixer]
  -> 4x ModelDLadder [tap: filter]
  -> Loudness contour + VCA nonlinearity [tap: vca]
  -> pre-output/A440 tap
       +-> Phones gain/enable -> optional Phones/Cue bus
       +-> Main volume -> feedback tap -> Main switch -> host Main bus
                            |
                            +-> measured attenuation + one-internal-sample memory
                                -> External source when aux absent
```

Use one fixed 4× oversampled domain around mixer nonlinearity, ladder, feedback memory, and VCA nonlinearity. Use JUCE’s polyphase IIR oversampler or an equivalently tested fixed implementation. Report base-rate round-trip latency via `setLatencySamples`; store filter delay and dry/control alignment in the build/test manifest. This instrument has no dry bypass mix.

Main/Phones outputs are dual mono. Phones taps after VCA/A-440 but before Main Volume, so Phones Volume is independent. Main Output switch mutes only the Main host bus, not Phones or the internal feedback tap. Normalled feedback taps after Main Volume but before Main switch, matching the manual’s statement that Main Volume changes overload/feedback while allowing headphones when Main is muted.

## External input and feedback routing

```cpp
enum class ExternalSource : uint8_t { autoDetect, auxiliaryBus, internalFeedback };
struct SignalPathBuses { AudioBlock main; OptionalAudioBlock external; OptionalAudioBlock phones; };
```

- `autoDetect` (Authentic default): enabled auxiliary bus means external source; disabled bus means internal feedback.
- `auxiliaryBus`: requires enabled bus; if absent, source is silence and a non-RT diagnostic is shown.
- `internalFeedback`: ignores aux input explicitly for reproducible recreation presets.
- Stereo aux is downmixed `(L+R)/2` with headroom defined in the calibration manifest; mono is used directly.
- The External Input switch gates the source into the mixer; its Volume controls source gain. The overload detector monitors the documented pre-switch external/mixer point so the lamp can indicate overload as the manual describes even when the External switch is off.
- Feedback has one internal-sample causal memory at 4× rate. Attenuation, control taper, safe maximum, and saturation come from reference measurement. No zero-delay algebraic feedback path is allowed.

## Public interfaces and DSP contracts

```cpp
struct SignalPathControls { MixerControls mixer; LadderControls ladder; VcaControls vca; OutputControls output; ExternalSource source; };
struct SignalPathFrame { float main; float phones; SignalTaps taps; OverloadState overload; };
class NonlinearSignalPath {
public:
    void prepare(const ProcessSpec&, const SignalPathCalibration&) noexcept;
    void reset(ResetReason) noexcept;
    void process(RenderRange, const Sources&, const SignalPathControls&, SignalPathBuses&) noexcept;
    int latencySamples() const noexcept;
};
```

Each stage accepts finite bounded input, produces finite bounded output, exposes a test tap through Workstream 11’s snapshot mechanism, and has an isolated offline test entry point.

## Published versus measured calibration

Manual p. 80 publishes external input ranges/impedances, high/low/headphone output typical/maximum levels, and Loudness dynamic range. These constrain the measurement setup and endpoint performance. Mixer gain curves, source balance, feedback attenuation, overload threshold/ballistics, VCA curve, saturation spectra, internal headroom, and output-knob tapers require reference measurements with unit/version/warm-up/load/capture-chain/calibration/repetition/raw-hash provenance.

The fixed 4× choice is a software architecture decision; alias/reconstruction/latency performance must still pass Workstream 12’s derived and measured manifest.

## Backward compatibility and preset behavior

Existing source switches/volumes, Output Volume, Phones Volume, and Feedback IDs remain. Add `output.mainEnabled`, `output.phonesEnabled`, and non-automatable `routing.externalSource`. Missing legacy state defaults Main/Phones on and `autoDetect`. Old inaudible feedback becomes functional intentionally; no compatibility mode preserves the defect. Presets that require feedback explicitly store `internalFeedback` and routing notes.

## Real-time audio constraints

Oversampler/filter/delay/taps allocate only in prepare. Processing has no locks, allocation, strings, logging, dynamic bus discovery, or unbounded feedback loop. Parameter gains are smoothed by registry policy. Non-finite detection uses bounded counters/containment; it never writes files or notifies UI from the callback.

## Edge cases and failure modes

Aux bus enabled with silence, bus enable change/reprepare, Main muted with feedback/phones, Phones bus absent, max feedback/source gains, feedback after panic, NaN/Inf input, sample-rate/block changes, oversampler reset, A-440 with gate closed, and overload detector at block boundaries. Containment fades/zeros unsafe output, clears nonlinear/feedback state at a safe boundary, increments diagnostics, and requires a failing test/report; it is not a fidelity behavior.

## Implementation sequence

1. Add isolated linear stage/tap skeleton and current gain baseline.
2. Add 4× oversampling, latency reporting, reset/alignment tests.
3. Integrate calibrated mixer, ladder, contours/VCA, A-440, Main and Phones paths.
4. Implement aux-source selection and causal internal feedback.
5. Add overload detector/ballistics and signal snapshot taps.
6. Measure/calibrate gains/nonlinearities/feedback; run stability/host/CPU gates.

## Automated tests and measurable gates

- Each source switch/volume affects only its mixer send; isolated stage taps match the signal-flow contract.
- Reported latency equals measured impulse displacement for every supported sample rate/block; reset clears delayed energy except intentional contour tail behavior.
- External aux routing/downmix, `autoDetect`, forced aux, and forced feedback truth table pass exactly.
- Main switch mutes Main only; Phones remains independent; Main Volume changes Main and feedback tap but not Phones; A-440 reaches both enabled outputs.
- Feedback parameter/input matrix produces finite bounded output and zero containment events; control-off produces no feedback contribution.
- Nonlinear spectra, gain staging, output levels, feedback attenuation, VCA range, and overload onset/ballistics fall within approved reference bands.
- Overload indication is driven by the declared stage and remains block-partition invariant under its manifest policy.
- Silence remains silence absent enabled A-440/self-oscillation/feedback; finite inputs never produce NaN/Inf.
- 4× alias/reconstruction and CPU gates pass approved manifests.

## Manual, host, listening, and hardware validation

In hosts, enable/disable aux input and Phones/Cue buses, route external audio, mute Main while monitoring Phones, automate gains/switches, bounce offline, and reload state. On hardware, measure source/mixer/output gains, external input, normalled feedback, overload lamp, VCA, and saturation at documented panel positions/loads. Blind listening compares drive/feedback only after numerical gates.

## Definition of done

- [ ] SIG-001 through SIG-010 pass.
- [ ] Aux/feedback/Main/Phones/overload truth tables and host routing pass.
- [ ] Latency/reset/non-finite/stability gates pass.
- [ ] Every analog gain/nonlinearity threshold has approved provenance.
- [ ] No separate effect target exists.

## Completion-report evidence

Include signal-flow diagram and tap captures; oversampler configuration/impulse/latency; routing truth tables; feedback stability heat map and containment count; gain/spectrum/output/overload measurements with provenance; switch/bypass tests; host bus screenshots; CPU/real-time report; and separate listening results.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 26–27, 32–36, 52, 80–81.
- JUCE [`Oversampling`](https://docs.juce.com/master/classdsp_1_1Oversampling.html) and [`AudioProcessor` latency/bus APIs](https://docs.juce.com/master/classAudioProcessor.html).
- Antti Huovilainen, [Non-Linear Digital Implementation of the Moog Ladder Filter](https://dafx.de/paper-archive/2004/P_061.PDF).
