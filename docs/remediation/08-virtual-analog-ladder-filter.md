# Workstream 08 — Virtual-Analog Ladder Filter and CV-Domain Cutoff

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Contours](07-model-d-contour-generators.md) · Next: [Nonlinear signal path](09-nonlinear-signal-path.md)

## Goal and user-visible outcome

The filter behaves as a stable nonlinear four-pole Model D ladder: correctly tuned cutoff and 24 dB/octave structure, musically mapped resonance/self-oscillation, exponential contour/key/modulation control in volts/octaves, and consistent behavior across sample rates.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| FLT-001 | Replace the current four Euler one-poles with a published virtual-analog whole-ladder topology. |
| FLT-002 | Apply one global ladder feedback loop with delay compensation/zero-delay solving, not per-stage repeated subtraction. |
| FLT-003 | Tune 10 Hz–20 kHz cutoff and 24 dB/octave response across supported sample rates/internal oversampling. |
| FLT-004 | Map emphasis, nonlinearities, resonance loss, and self-oscillation from approved measurements while remaining stable. |
| FLT-005 | Apply Filter Contour exponentially over the published 0–4-octave width. |
| FLT-006 | Apply keyboard tracking in CV space: 1/3, 2/3, and combined 1 octave/octave. |
| FLT-007 | Apply Osc3/noise modulation in octave/CV space and clamp only the final cutoff to an algorithm-safe range. |
| FLT-008 | Explicitly initialize/reset all filter/solver state and remove dead/misleading feedback calculations. |
| FLT-009 | Pass impulse, sweep, resonance/self-oscillation, CV tracking, modulation, non-finite, and sample-rate-independence gates. |

## Current-code evidence

- `LadderFilter::computeFilter` computes normalized cutoff as `modulatedCutoff/sampleRate`, applies Euler updates, and subtracts `resonance * stage[3]` inside every stage loop rather than once around the ladder.
- `setResonance` calculates `feedbackAmount`, but processing uses `resonance` directly; `feedbackAmount` is dead. `setFeedback` only stores a separate value.
- Constructor initializes stages/cutoff/resonance/envelope amount but does not explicitly initialize all fields including `feedback`, `feedbackAmount`, `contourEnvelopeAmount`, and `sampleRate` before some calculations may use them.
- Filter envelope adds up to a fixed 50,000 Hz; oscillator/noise modulation adds a fixed 5,000 Hz; neither is exponential CV-domain modulation.
- Key tracking uses a frequency ratio/power around MIDI note 69, while cutoff is repeatedly set per sample and clamped to 0…20 kHz without an algorithm-specific Nyquist/stability rule.

## Prerequisites, ownership, and merge conflicts

Requires parameter/state (03), pitch/CV units (04), lifecycle boundary updates (05), contour output (07), and filter analyzers/reference skeleton (12). It processes at the internal sample rate supplied by Workstream 09’s oversampled nonlinear domain.

Owns ladder equations, cutoff CV summation/conversion, resonance/nonlinearity/solver, filter state/reset, and filter calibration. It does not own mixer/VCA stages or oversampler implementation.

Conflict hotspots: `LadderFilter.h/.cpp`, filter/key/modulation sections of `processBlock`, and overlay visualization. Replace the class behind `ModelDLadder`; do not expose internal stage arrays to processor/UI.

## In scope

Four-pole low-pass ladder, TPT/ZDF integration, per-stage transistor-pair nonlinearity, cutoff/resonance/control mapping, self-oscillation, state reset, diagnostics, and reference analysis.

## Out of scope

Other filter modes, arbitrary circuit calibration values, mixer/VCA distortion, external EQ, or using listening to waive response/stability gates.

## Chosen architecture and data flow

Implement a topology-preserving transform (trapezoidal/TPT) four-stage ladder with a single whole-ladder negative feedback loop and per-stage `tanh` transistor-pair approximation, based on Huovilainen and Zavalishin. Solve the delay-free nonlinear loop with a bounded four-iteration Newton method per internal sample. Coefficients are prewarped at the internal sample rate.

If any Newton step is non-finite or leaves the solver’s analytically allowed state, restore the previous valid state, use a bounded one-step linearized fallback for that sample, increment a diagnostic counter, and continue. The acceptance gate requires zero fallbacks in the entire approved parameter/input matrix; fallback is containment, not normal operation.

```text
panel cutoff position -> measured log-frequency taper -> base cutoff octaves
 + keySemitones/12 * {0,1/3,2/3,1}
 + filterContour * contourWidthOctaves (0..4)
 + routed modulation * modulationDepthOctaves
 + calibration offset/drift
 -> exp2 once per control/sample as required
 -> safeCutoffHz(internalSampleRate, filterModelVersion)
 -> prewarped TPT coefficient -> nonlinear whole ladder
```

```cpp
struct CutoffCV { double baseOctaves, keyOctaves, contourOctaves, modulationOctaves, calibrationOctaves; };
struct LadderControls { CutoffCV cutoff; float emphasis; float drive; };
class ModelDLadder {
public:
    void prepare(double internalRate, const LadderCalibration&) noexcept;
    void reset(ResetReason) noexcept;
    float process(float input, const LadderControls&) noexcept;
    LadderDiagnostics diagnostics() const noexcept;
};
```

`safeCutoffHz` is a versioned derived-software table produced by stability sweeps and solver analysis in Workstream 12. It must respect the published 20 kHz endpoint where the internal rate can support it; it is not a hand-chosen fraction of Nyquist.

## Calibration and self-oscillation

Published constraints: low-pass Moog ladder, 10 Hz–20 kHz cutoff, 24 dB/octave, resonance at cutoff, contour width 0–4 octaves. Required measurements: cutoff knob taper/tuning, passband loss versus emphasis, onset/amplitude/pitch of self-oscillation, nonlinear transfer versus level/cutoff/resonance, temperature/calibration drift, and modulation response.

Workstream 12 records reference unit/version, warm-up/tuning, input waveform/level/impedance, panel positions, output/load, capture interface/calibration, sample rate, raw hashes, repetitions, analysis, and uncertainty. The resulting `LadderCalibration` and acceptance bands are immutable versioned inputs.

Self-oscillation is allowed and must remain finite. When no input is present, its onset and pitch-tracking band must match approved reference data. Do not normalize resonance to “0…1 means feedback 0…4” unless the measurement profile establishes that mapping.

## Backward compatibility and preset behavior

Existing cutoff/emphasis/contour parameter IDs and normalized positions remain. Canonical semantic mapping changes intentionally from the inaccurate digital implementation to the documented panel behavior. Calibration profile ID records which measured mapping is used. No “old Euler filter” mode is exposed; golden migration tests focus on state/control preservation, not preserving defective DSP sound.

## Real-time audio constraints

All state/calibration/coefficients are preallocated. Control-rate terms update at MIDI/automation sub-block boundaries; audio-rate modulation performs fixed bounded work. Four solver iterations are the absolute maximum. No allocation, locks, strings, logging, table parsing, exceptions, or unbounded convergence loop. Denormals are suppressed and all state/output remains finite.

## Edge cases and failure modes

Cutoff below/above published range, modulation crossing bounds, emphasis automation at self-oscillation, silence, impulses, huge finite input, NaN controls, sample-rate/internal-rate changes, reset while resonating, calibration absence/corruption, and solver failure. Invalid controls use last valid/baseline values; state containment prevents NaN propagation.

## Implementation sequence

1. Build current impulse/sweep/self-oscillation failure baselines.
2. Implement linear TPT ladder and analytic cutoff/prewarp tests.
3. Add whole-ladder feedback, nonlinear stages, bounded solver, and containment diagnostics.
4. Implement `CutoffCV` summation, contour 0–4 octaves, key tracking, modulation, and safe clamp.
5. Integrate oversampled domain and calibration profile.
6. Run response/stability/reference/CPU gates; freeze diagnostics and UI tap.

## Automated tests and measurable gates

- Impulse/sweep confirms four-pole structure and published 24 dB/octave band under Workstream 12’s measurement-derived analysis uncertainty.
- Cutoff endpoints/knob positions and sample-rate consistency pass published/measured manifest bands.
- Key tracking produces exact CV coefficients 0, 1/3, 2/3, 1 and full tracking yields one cutoff octave per played octave before final clamp.
- Full Filter Contour maps to the published profile width up to 4 octaves; monotonicity and exponential ratio hold.
- Modulation is symmetric in octaves before clamp and never creates negative/non-finite cutoff.
- Resonance onset, self-oscillation pitch/amplitude, passband loss, and nonlinear spectra pass approved hardware bands.
- Approved input/parameter/sample-rate sweeps produce finite bounded output, zero solver fallbacks, and no state explosion.
- Reset yields exact baseline state; block partitioning and repeated prepare are deterministic under manifest policy.
- CPU/real-time gates pass with fixed four-iteration upper bound.

## Manual, listening, and hardware validation

Measure logarithmic sweeps at cutoff/emphasis positions, impulses, self-oscillation tuning with tracking switches, contour sweeps, Osc3/noise modulation, drive levels, sample rates, and thermal repeats. Blind listening evaluates resonance character and sweeps only after numerical gates. Hardware/reference differences must be labeled by unit/profile, not “fixed” by unsourced tuning.

## Definition of done

- [ ] FLT-001 through FLT-009 pass.
- [ ] Whole-ladder/TPT/ZDF design and fixed solver are documented/tested.
- [ ] Published cutoff/slope/contour/tracking constraints pass.
- [ ] Measured resonance/nonlinearity gates have full provenance.
- [ ] No dead feedback fields or uninitialized filter state remain.

## Completion-report evidence

Include topology/solver derivation and source citations; coefficient and safe-cutoff tables; impulse/sweep plots; CV component traces; self-oscillation maps; nonlinear spectra; solver-fallback counter; non-finite/stability fuzz report; sample-rate/block comparisons; reference provenance/uncertainty; and CPU/real-time output.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 28–31, 36, 80–81.
- Tim Stilson and Julius O. Smith, [Analyzing the Moog VCF for Digital Implementation](https://quod.lib.umich.edu/i/icmc/bbp2372.1996.123/1/--analyzing-the-moog-vcf-for-digital-implementation?page=root;size=100;view=text).
- Antti Huovilainen, [Non-Linear Digital Implementation of the Moog Ladder Filter](https://dafx.de/paper-archive/2004/P_061.PDF).
- Vadim Zavalishin, [The Art of VA Filter Design](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf).
