# Workstream 04 — Pitch, Tuning, Glide, and Modulation

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Parameter/state](03-parameter-automation-state-contract.md) · Next: [MIDI lifecycle](05-sample-accurate-midi-monophonic-lifecycle.md)

## Goal and user-visible outcome

Panel tuning, oscillator ranges, fine frequency controls, pitch wheel, oscillator modulation, and glide operate in a unified semitone/octave control domain. A documented panel position produces the intended interval at every supported sample rate, and glide time depends on octave distance as the hardware specification states.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| PIT-001 | Map Oscillator 2/3 selector index 0…16 to −8…+8 semitones before any ratio calculation. |
| PIT-002 | Sum keyboard note, range, master tune, oscillator detune, pitch wheel, calibration, and modulation in semitone space, then convert once to hertz. |
| PIT-003 | Verify cents/interval correctness across all six ranges, notes, controls, sample rates, and calibration profiles. |
| PIT-004 | Implement centered pitch bend with published ±7-semitone endpoints and no asymmetric linear-ratio shortcut. |
| PIT-005 | Express audio-rate oscillator modulation in semitones/octaves with finite, positive, Nyquist-safe frequency behavior. |
| PIT-006 | Calibrate Glide as time per octave with published 1 ms–10 s one-octave endpoints. |
| PIT-007 | Define glide transitions for first note, legato, retrigger, bypass, target changes, priority fallback, and live knob changes. |
| PIT-008 | Remove repeated per-sample exponent/power calculations through cached or incremental ratios without changing the control contract. |

## Pitch progress

The [PIT-001/PIT-002 evidence](evidence/workstream-04/pit-001-pit-002-pitch-foundation.md)
records the exact commit chronology, RED/GREEN history, analyzer calibration,
fixture and candidate hashes, nine bound gates, authoritative report, negative
diagnostics, strongest-peak correction, scheduling clarification, and exact
claim boundary. The [PIT-003/PIT-004 evidence](evidence/workstream-04/pit-003-pit-004-pitch-matrix.md)
records baseline calibration and LO rejection, preserved v1 and new v2
analyzer contracts, all 54 cross-rate measured errors, published source
authority, the historical repeat-equal Task 7 candidates, honest requirement
reduction, and the retained non-claims. Its 2026-07-25 correction records the
independently reviewed musical-glide and bounded-v2 repair plus the fresh
exact-head candidate.

| ID | Status | Durable result / remaining boundary |
|---|---|---|
| PIT-001 | pass | Typed selector mapping is exactly `-8...+8`; six bound processor-output gates and all 34 governed selector records pass. |
| PIT-002 | pass | The musical path composes checked instantaneous glide position plus typed range/tune/offset/bend/calibration/modulation contributions through one authority and converts once; three bound composed-pitch gates pass. LO and keyboard-disabled Oscillator 3 compatibility behavior remains explicit. Static-term scheduling/caching is not claimed here. |
| PIT-003 | awaiting-approved-reference | All 39 five-musical-range software gates pass across 44.1/48/96 kHz within `0.01` semitone; the current maximum is `0.005411181877115`. Baseline calibration is exact zero for musical ranges and rejects LO. Approved LO/hardware calibration is absent, so PIT-003 is not pass. |
| PIT-004 | pass | Production maps normalized wheel position as `14 * (position - 0.5)` semitones. Center, intermediate symmetry, and published ±7 endpoints pass 15 governed gates across 44.1/48/96 kHz. |
| PIT-005 | not-started | Nonzero audio-rate modulation semantics and approved sideband/safety evidence are not implemented. |
| PIT-006 | not-started | Published time-per-octave glide endpoints and measured taper are not implemented. |
| PIT-007 | not-started | The complete glide transition table is not implemented. |
| PIT-008 | not-started | The Task 2 compatibility path currently recomputes composition per sample. Static-term change detection/caching, bounded audio-rate conversion, exponent counters, and CPU/equivalence evidence remain open under PIT-008. |

The live fixture-index schema uses `relativePath`. Task 4 Step 5's mistaken
`path` wording is corrected in the implementation plan under the user's
explicit compatibility approval; no schema migration is performed.

## Current and original code evidence

- The canonical musical path now bypasses legacy setters, maps the Oscillator
  2/3 selector through `index - 8`, and composes pitch through `PitchDomain`.
  Oscillators 1/2 and keyboard-controlled Oscillator 3 derive their note term
  from checked instantaneous glide frequency; all static terms remain single
  contributions. Keyboard-disabled Oscillator 3 remains reference-based and
  wheel-neutral. The five musical ranges use exact-zero baseline software
  calibration. The deliberately preserved LO compatibility path rejects
  musical calibration and remains pending approved reference evidence.
- `audio.pitch.v1` now chooses the globally strongest parabolically
  interpolated local peak, using earliest lag only inside a `0.00001`
  numerical tie. A weak 200 Hz fundamental with a ten-times-stronger 400 Hz
  harmonic reports the fundamental within `0.000010641` semitone.
- `audio.pitch.v2` preserves all v1 settings except its explicit v2 identity
  and 4 Hz lower boundary. Its overflow-safe pure planner structurally caps
  exact coarse plus conservative refinement work at `64000000` pairs, uses
  the largest deterministic safe tail subwindow, preserves the full
  configured band when feasible, and returns `analyzer.work-budget` when the
  minimum full-band/four-period request cannot fit. A tied maximum-boundary
  peak is accepted only when it is a periodic multiple of the selected
  interior fundamental. The synthetic grid maximum remains `0.001699433`
  semitone; the current governed matrix maximum is
  `0.005411181877115` semitone.
- `MoogMiniAudioProcessor::processBlock` now maps Pitch Wheel as
  `14 * (normalized - 0.5)` semitones, centered at zero with published
  `-7/+7` endpoints.
- Oscillator modulation multiplies frequency by `1 + modulationEffect`, limited to −0.9…0.9; the domain is neither semitones nor volts/octave.
- `calculateGlideRate` says seconds per semitone, but the render loop uses it as a generic first-order denominator `(target-current)/(rate*sampleRate)`, so travel time is not the published time per octave and never completes by defined duration.
- Per-sample rendering calls `setFrequency`, oscillator detune `pow`, keyboard-tracking `pow`, and filter setters repeatedly.

## Prerequisites, ownership, and merge conflicts

Requires Workstream 03’s typed registry/snapshot and Workstream 12’s pitch/glide analyzer skeleton. Its `PitchControl` interface must freeze before Workstream 05 integrates note transitions and Workstream 06 integrates oscillators.

Owns pitch-domain math and glide trajectories. Workstream 05 owns which note is selected and when a transition/retrigger occurs; 06 owns waveform phase; 08 owns conversion from pitch/CV modulation to filter cutoff.

High conflict: `processBlock`, `Oscillator::calculateFrequencyForRange`, `calculateTuneForOsc1`, `calculateDetunedFrequency`, and `calculateGlideRate`. Move the math into new classes rather than patching all call sites.

## In scope

Equal-tempered note conversion, six range offsets, master/fine tune, pitch wheel, calibration offsets, oscillator modulation depth, glide trajectory, cache/update boundaries, and diagnostic pitch values.

## Out of scope

Waveform generation/aliasing, note-priority selection, contour retriggering, filter modulation implementation, MPE/polyphony, alternate tuning tables, or guessing analog drift.

## Proposed architecture and data flow

```text
selected MIDI note
 + rangeSemitones
 + masterTuneSemitones
 + oscDetuneSemitones
 + pitchWheelSemitones
 + calibration key/osc offset
 + audio modulation semitones
 = instantaneous semitone coordinate
 -> exp2(semitones / 12) using cached control ratio + bounded modulator
 -> oscillator hertz / phase increment
```

Use MIDI note 69 = 440 Hz as the mathematical reference. Range offsets relative to 8′ are LO (special mode), 32′ = −24, 16′ = −12, 8′ = 0, 4′ = +12, 2′ = +24 semitones. LO uses the calibration profile’s documented free-running/LFO mapping and the published total 0.1 Hz lower capability; do not infer a fixed `/256` relationship from current code without measurement.

Public contract:

```cpp
using Semitones = StrongFloat<struct SemitoneTag>;
using Hertz = StrongFloat<struct HertzTag>;
struct StaticPitch {
    Semitones note, range, masterTune, oscillatorTune, pitchWheel, calibration;
};
class PitchControl {
public:
    void setStaticPitch(const StaticPitch&) noexcept;
    void setTargetNote(Semitones note, GlideTransition) noexcept;
    Semitones nextNoteSemitones() noexcept;
    Hertz toHertz(Semitones audioModulation) const noexcept;
};
```

The completed PIT-001/PIT-002 slice centralizes the authority represented by
this interface but does not yet implement this scheduling target: its
compatibility path recomputes composition per sample. PIT-008 owns static-ratio
change detection/caching and the required counters, CPU, and output-equivalence
evidence. Audio-rate modulation may use a precomputed/interpolated `exp2`
strategy chosen with Workstream 06, but error must pass the derived-software
pitch gate and measured modulation spectra.

## Glide contract

Glide parameter represents seconds per octave. For a transition of `d` semitones, duration is `abs(d)/12 * secondsPerOctave`. Semitone position follows a constant-rate linear ramp in log-frequency/semitone space so equal octave distances take equal time. Published one-octave endpoints are 1 ms and 10 s; the knob taper between endpoints comes from the approved measurement manifest.

- First note after no held notes: no glide; start at target.
- Legato target change: glide from instantaneous current semitone position to newly selected priority note.
- Multi-trigger note change: trigger policy does not change glide; use the same trajectory unless Glide is off.
- Priority fallback on note-off: glide to fallback note when Glide is on.
- Glide switched off: jump to current target at the exact event/control boundary.
- Glide switched on mid-note: do not move until the next selected-note change.
- Time knob changed mid-glide: retain instantaneous pitch and recompute remaining slope using the new seconds-per-octave; no discontinuity.
- New target during glide: start from instantaneous pitch, not the previous source/target.
- Zero-distance transition: complete immediately, no divide-by-zero.

## Backward compatibility and preset migration

Workstream 03 owns Tune’s new-instance default. Existing normalized selector values remain unchanged. Correcting Osc2/3 from index-as-semitone to `(index−8)` is an intentional bug fix for canonical state. A v0 compatibility profile may reproduce the old error only when a saved state is explicitly marked `legacyPitchIndexing`; migration tests determine whether shipped presets relied on it. Do not silently insert that flag for all legacy state: add it only if an audible legacy-render fixture demonstrates the behavior was released and compatibility is required. Canonical saved panel positions always mean −8…+8.

Calibration offsets are referenced by profile ID and never baked into panel parameters. Missing profiles fall back to the declared baseline profile with a warning in state/preset metadata.

## Real-time audio constraints

No allocation, locks, strings, logging, generic `pow`, or parameter lookup per sample. `exp2`/approximation work is bounded and vectorizable; static terms update at sub-block boundaries. All hertz and phase increments are finite, positive, and clamped before oscillator use to the sample-rate-aware limit declared by Workstream 06.

## Edge cases and failure modes

NaN/Inf parameters, MIDI outside 0–127, extreme calibration, negative modulation, target above safe oscillator range, sample-rate changes, zero sample rate before prepare, block-size changes, and rapid automation. Invalid inputs increment a diagnostic and use the last valid/baseline value; they never reach the oscillator as negative/non-finite hertz.

## Implementation sequence

1. Capture failing interval/pitch-wheel/glide fixtures from current code.
2. Add strong units, selector tables, and static pitch sum.
3. Implement centered ±7-semitone bend and positive/safe hertz conversion.
4. Implement semitone-domain audio modulation contract.
5. Implement time-per-octave glide state machine and all transition types.
6. Add cache/change detection and benchmark hooks.
7. Freeze interface with Workstreams 05/06/08 and publish diagnostic trace format.

## Automated tests and measurable gates

- Selector table is exactly `[−8, …, 0, …, +8]`; center ratio is exactly unity and all 16 adjacent ratios are equal-tempered under the shared floating-point policy.
- Master tune covers the exact 11-entry `-2.5...+2.5` half-semitone table; a nonzero processor composition applies note/range/tune/offset/bend once, and invalid processor input uses the diagnosed fallback.
- Each adjacent keyboard note and fine selector step has the analytic equal-tempered ratio; range offsets produce exact octave ratios before calibration.
- Pitch wheel center is 0 semitones; endpoints are −7/+7 semitones and symmetric in cents.
- Across supported sample rates/notes, finite input never produces non-positive, NaN, or infinite frequency/phase increment.
- A one-octave glide at endpoint settings reaches the target at the sample count derived from 1 ms or 10 s; intermediate knob tests consume measurement-derived taper entries.
- Glide duration scales exactly with semitone distance; mid-glide target/knob changes preserve sample-to-sample pitch continuity under the manifest’s derived-software bound.
- Rendered pitch is invariant to host block partitioning for identical sample-timestamped events.
- Performance counters show no generic exponent evaluation for unchanged static controls; per-sample conversion stays within the Workstream 11 approved CPU manifest.

## Manual, listening, and hardware validation

Compare tuner readings for all A/C notes across ranges, center/end fine tune, bend endpoints, and one-octave glide. Record reference unit identity, warm-up, room temperature, tuning procedure, output/capture chain, repetition count, and raw files per Workstream 12. Derive LO mapping, glide taper, and drift acceptance from those captures. Listening may assess glide feel and modulation musicality only after numerical pitch gates pass.

## Definition of done

- [x] PIT-001 and PIT-002 pass with governed processor-output evidence and authoritative report replay.
- [x] PIT-003 five-musical-range software matrix is complete; missing LO/hardware evidence remains explicitly `awaiting-approved-reference`.
- [x] PIT-004 passes centered, symmetric, published ±7-semitone gates.
- [ ] PIT-003 receives approved LO/hardware evidence and PIT-005 through PIT-008 pass.
- [ ] Pitch/glide interfaces are frozen and used by the engine.
- [x] Published bend endpoints pass.
- [ ] Published glide endpoints pass.
- [ ] No unit-specific calibration value was guessed.
- [ ] Block partition and real-time gates pass.

## Completion-report evidence

Include selector/range tables; pitch traces in semitones/cents/hertz; pitch-wheel symmetry results; glide sample-index plots for every transition; block-partition hashes; invalid-input tests; exponent counters/benchmarks; reference-capture manifest or `awaiting-approved-reference` status; and any legacy-pitch compatibility evidence.

## 2026-07-25 whole-branch repair correction

Initial Task 7 documentation head
`457ceebbda0d85fda39042557f52ee46252ba096` remains historical. Repair commit
`cf5fb99ba1b0cc2626a4d9de7a65bbf5fc9a7064` makes musical Oscillators 1/2 and
keyboard-controlled Oscillator 3 consume checked instantaneous glide
frequency, leaves keyboard-disabled Oscillator 3 reference-based and
wheel-neutral, hardens v2 boundary recurrence classification, and enforces the
`64000000`-pair work plan. Range-zero wheel outputs at `0`, `0.5`, and `1`
prove relative bend once, musical-calibration bypass, and zero diagnostics
without claiming an LO base map.

The strict all-target repair build passed with tests, validators, and
warnings-as-errors enabled; focused verification passed 9/9, full serial CTest
passed 23/23 in `740.16` seconds, and independent review returned READY with
zero findings. PIT-003 remains `awaiting-approved-reference`, PIT-004 remains
`pass`, and PIT-005...008 remain open. Task 8 packaging is still prospective;
the stale Agent 11 ZIP must be replaced only after the documentation repair
commit.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 18–24, 43–45, 49–51, 80–81.
- MIDI Association, [MIDI 1.0 detailed specification resources](https://midi.org/specifications-old/item/table-1-summary-of-midi-message).
- Julius O. Smith, [Mathematics of the Discrete Fourier Transform / sinusoidal frequency concepts](https://ccrma.stanford.edu/~jos/mdft/).
