# Workstream 06 — Band-Limited and Calibrated Oscillators

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [MIDI lifecycle](05-sample-accurate-midi-monophonic-lifecycle.md) · Next: [Contours](07-model-d-contour-generators.md)

## Goal and user-visible outcome

All Model D oscillator waveforms retain their characteristic measured shapes while avoiding objectionable digital aliasing, DC errors, pitch discontinuities, and phase resets. Oscillator 3 works consistently as an audible oscillator or a free-running modulation source.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| OSC-001 | Replace direct discontinuous waveform sampling with a band-limited, audio-rate-modulation-capable oscillator. |
| OSC-002 | Calibrate Triangle, Triangle/Saw, Reverse Saw, Saw, Rectangle, Wide Pulse, and Narrow Pulse from approved reference captures. |
| OSC-003 | Use independently measured Wide/Narrow pulse widths; do not retain complementary 70%/30% placeholders. |
| OSC-004 | Preserve authentic free-running phase; define initialization, reset, sample-rate change, transport, release, and idle behavior. |
| OSC-005 | Define Oscillator 3 keyboard-control, LO/free-run, modulation, and mixer use against the lifecycle contract. |
| OSC-006 | Gate alias energy and spectral-shape error at all supported pitches, sample rates, ranges, and modulation conditions. |
| OSC-007 | Gate DC offset, pulse duty, level, phase continuity, sample-rate consistency, and deterministic test mode. |
| OSC-008 | Provide a bounded, allocation-free oscillator-bank API with cached static pitch values. |

## Current-code evidence

- `Oscillator::generateWaveform` and `processNextSample` sample saw, reverse saw, square, and pulse discontinuities directly from phase; no BLEP, wavetable mip selection, or oversampling correction is present.
- Wide/Narrow Rectangle use phase thresholds 0.7 and 0.3, so their duty cycles are complementary and differ mainly by polarity/DC behavior.
- Triangle and “Sharktooth” use hand-selected curvature/blend constants with no measurement provenance.
- `Oscillator::stop` resets phase to zero; ordinary note-off therefore phase-locks the next attack instead of modeling free-running analog oscillators.
- `processNextSample` constructs `juce::String isGap` per sample and evaluates power functions through pitch/detune paths.
- Oscillator 3 activity is split among start/stop, mixer on/off, keyboard control, and modulation switches in `processBlock`, with inconsistent ownership.

## Prerequisites, ownership, and merge conflicts

Requires the parameter registry (03), PitchControl and positive-frequency contract (04), lifecycle/Osc3 truth table (05), and oscillator analysis/capture fixtures (12 skeleton). Its processor-facing API freezes at F1.

Owns oscillator phase, waveform generation, band limiting, per-waveform calibration, and oscillator idle behavior. It does not select notes, define filter modulation depth, or own downstream saturation.

Conflict hotspots: `Oscillator.h/.cpp`, oscillator sections of `processBlock`, UI waveform names, and visualization taps. Introduce `OscillatorBank` behind the frozen interface; do not preserve the current enum arithmetic internally.

## In scope

Three audio oscillators, their seven panel waveform variants, six ranges including LO, waveform levels/DC correction, phase, pitch/audio modulation input, deterministic offline mode, and raw/mixer visualization taps.

## Out of scope

Filter, noise spectra, mixer distortion, alternate oscillator models, hard sync, PWM not on the authentic panel, or fabricating calibration from screenshots/listening.

## Proposed architecture and data flow

Use a double-precision normalized phase accumulator and an event-corrected piecewise waveform engine:

- PolyBLEP/minBLEP correction for value discontinuities (saw/pulse edges);
- PolyBLAMP or analytically integrated band-limited correction for slope discontinuities (triangle and triangle/saw segments);
- fixed, versioned correction tables generated offline and compiled as immutable data;
- phase increment supplied by Workstream 04, recalculated from cached static ratio plus bounded audio-rate semitone modulation;
- optional waveform calibration transfer (piecewise polynomial/harmonic residual) selected by `CalibrationProfile`, constrained to preserve band limiting.

The implementation must compare this fixed design against a high-rate offline reference before freeze. It may not switch to naive fixed wavetables or oversampling-only without updating this plan and alias/modulation fixtures.

```cpp
enum class OscWave : uint8_t { triangle, triangleSaw, reverseSaw, saw, rectangle, widePulse, narrowPulse };
enum class OscRole : uint8_t { audioAndMod, modulationOnly, idleEligible };
struct OscRenderInput { Hertz base; Semitones audioMod; OscWave wave; OscRole role; };
class OscillatorBank {
public:
    void prepare(const ProcessSpec&, const OscillatorCalibration&) noexcept;
    void reset(ResetReason, uint64_t deterministicSeed) noexcept;
    OscillatorFrame render(const OscRenderInput[3]) noexcept;
};
```

`OscillatorFrame` returns raw normalized sample and calibrated mixer-send sample for each oscillator. Volume switch/knob are applied by Workstream 09; modulation taps remain available when the audio send is off.

## Phase and reset contract

- Production oscillators begin at independent phase offsets derived off-thread from a saved instance seed; exact phase is not a panel setting.
- They advance continuously after prepare whenever required by audio, modulation, release, feedback, or visualization. Ordinary Note On/Off, legato, priority change, DAW transport start/stop, and preset load do not reset phase.
- A sample-rate change preserves normalized phase and recalculates increments/correction widths.
- `ResetReason::hardPanic` silences downstream gates but does not phase-reset. `ResetReason::newInstance` and an explicit test-only deterministic reset may initialize phase.
- Idle optimization may skip computation only by analytically advancing phase by the skipped sample count. Resumption must match continuously rendered phase under the derived-software bound.
- Oscillator 3 with keyboard control off uses the calibrated panel frequency/range independent of selected note and pitch wheel. LO remains free-running without a gate.

## Hardware measurement and calibration

Workstream 12 captures each waveform from each oscillator/range at multiple panel frequency positions and supported sample-rate-equivalent analysis rates. Record unit serial/version, warm-up, tuning, room temperature, output used, mixer/output positions, load, interface, calibration certificate, gain, sample rate/bit depth, and raw hashes.

Phase-align and amplitude-normalize repeated periods without removing measured DC until DC is characterized. Store median cycle, harmonic amplitudes/phases, duty-cycle distribution, level, and repeatability/uncertainty in the calibration profile. Wide/Narrow pulse positions are separate measured fields and must not be algebraically derived from one another. The acceptance manifest generates waveform/spectrum bands from repeated captures plus measurement uncertainty.

## Backward compatibility and preset behavior

Panel waveform/range choice indices remain stable. Rename “Sharktooth” display text to the documented “Triangle/Sawtooth” without changing its host ID/index. Oscillator phase is not serialized as a preset parameter; the instance seed is state metadata for repeatable diagnostics. Legacy direct-waveform rendering is not offered as a product mode.

## Real-time audio constraints

All correction tables/calibration data load and validate before prepare. Render uses fixed storage, no strings, logging, locks, allocation, virtual dispatch in the sample loop, or unbounded iteration. No generic exponent is evaluated for unchanged static pitch. Every output is finite and bounded before downstream nonlinear stages.

## Edge cases and failure modes

Frequency near zero/Nyquist, negative modulation request, phase increment crossing multiple edges, rapid waveform/range automation, sample-rate change, LO-to-audio transition, calibration data absent/corrupt/future-version, idle/resume, and oscillator used only as modulation. Missing calibration loads the declared baseline model with a diagnostic; corrupt data never partially applies.

## Implementation sequence

1. Add current direct-oscillator spectral/phase/DC baselines and high-rate reference renderer.
2. Implement phase core and event corrections for saw/pulse, then triangle/slope shapes.
3. Integrate PitchControl/OscillatorBank and continuous-phase lifecycle.
4. Implement Osc3 role/LO behavior and analytic idle advance.
5. Add calibration-profile loader and measured residual shaping.
6. Run spectral, modulation, sample-rate, and CPU gates; freeze raw/mixer tap contracts.

## Automated tests and measurable gates

- Analytic phase increment and fundamental pitch pass Workstream 04 gates across ranges/sample rates.
- No phase discontinuity occurs on Note On/Off, waveform automation crossfade, transport, priority, sample-rate change, or idle/resume beyond the derived-software manifest.
- Alias-energy ratio, harmonic-shape distance, and audio-rate modulation sidebands pass `acceptance-v1.json`; a missing hardware-derived threshold reports `awaiting-approved-reference`, never pass.
- DC, duty cycle, peak/RMS level, and median period shape fall inside approved reference bands for each oscillator/waveform.
- Wide and Narrow pulse profile fields are independently sourced and unequal to a forced complementary relationship.
- Identical deterministic seed/events produce identical offline render; different production seeds alter phase without altering long-term spectrum.
- Finite input produces finite bounded output at all fuzzed frequencies/modulation depths.
- Callback instrumentation reports zero allocations/locks/strings; CPU passes Workstream 11’s approved matrix.

## Manual, listening, and hardware validation

Level-match and compare sustained single oscillators, interval beating, LO modulation, high-note alias audibility, waveform switching, and attacks at randomized phases. Use blinded listening only after numerical spectra/shape gates. Document every hardware capture; no photographed waveform or secondary emulator substitutes for the reference set.

## Definition of done

- [ ] OSC-001 through OSC-008 pass.
- [ ] All seven waveform contracts are measured or explicitly awaiting an approved capture.
- [ ] No direct uncorrected discontinuity or per-sample string remains.
- [ ] Phase/Osc3/idle truth tables pass.
- [ ] Alias, calibration, sample-rate, and real-time reports are attached.

## Completion-report evidence

Include algorithm/design comparison; correction-table generator hash; spectra and alias maps; phase/idle traces; DC/duty/level tables; reference provenance and uncertainty; calibration profile diff; deterministic render hashes; Osc3 truth table; CPU/real-time output; and listening report separated from hard gates.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 16–20, 41–45, 80.
- Vesa Välimäki and Antti Huovilainen, [Oscillator and Filter Algorithms for Virtual Analog Synthesis](https://doi.org/10.1162/comj.2006.30.2.19).
- Vesa Välimäki et al., [Antialiasing Oscillators in Subtractive Synthesis](https://doi.org/10.1109/MSP.2012.2205954).
