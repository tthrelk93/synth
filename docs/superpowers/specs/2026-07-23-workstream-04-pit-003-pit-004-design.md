# Workstream 04 PIT-003/PIT-004 Pitch Matrix and Bend Design

**Date:** 2026-07-23

**Status:** Approved in conversation

**Base:** `ab88a2197d7b0809d9554709594d62cabaaca88b`

**Scope:** Complete the evidence-backed baseline software portion of PIT-003
without inventing LO or hardware calibration, and fully implement PIT-004's
published centered symmetric pitch bend. PIT-003 remains
`awaiting-approved-reference`; PIT-004 advances to `pass` only through
authoritative processor-output evidence.

## Outcome

The five musical oscillator ranges will have factorized exhaustive pitch
coverage over the physical Model D keyboard span F0-C4, all master-tune and
Oscillator 2/3 detune positions, all three supported sample rates, and the
existing `baseline` calibration profile.

The normalized Pitch Wheel parameter will map continuously and linearly in the
semitone domain:

```text
pitchWheelSemitones = 14 * (normalizedValue - 0.5)
```

The exact endpoints are -7 and +7 semitones, the center is zero, and the
quarter positions are -3.5 and +3.5 semitones. There is no dead zone, taper, or
legacy asymmetric compatibility mode.

The bundled Model D manual is the published authority. Its PDF metadata records
a 2022-11-14 creation date and its exact SHA-256 is
`c7e6f1abd54999cad7aa232d782df397f974ff210db1e6a429a863af6e850b1f`:

- page 17 identifies five musical octave ranges plus the sixth LO setting;
- page 24 identifies the centered, spring-loaded wheel and fifth-up/fifth-down
  behavior; and
- page 80 specifies `Pitch Bend Range: (+/-) 7 semitones` and the overall
  six-range oscillator capability.

## Claim Boundary

This slice may claim:

- complete derived-software pitch math for MIDI notes 17-60, the physical
  F0-C4 keyboard span under the standard MIDI `C-1=0` note-name convention;
- exact five-range, master-tune, Oscillator 2/3 offset, baseline calibration,
  and pitch-wheel interval behavior;
- processor-output agreement at 44.1, 48, and 96 kHz;
- published centered equal-tempered pitch bend at -7/0/+7 semitones; and
- deterministic, block-partition-invariant governed evidence and replay.

This slice must not claim:

- calibrated LO frequency mapping;
- a hardware-derived profile, unit-specific offset, drift band, or tolerance;
- hardware verification of the five musical ranges;
- modulation, glide, caching, CPU, host, listening, or release completion; or
- hardware-calibrated pitch behavior outside the physical F0-C4 keyboard.

MIDI notes outside 17-60 remain valid software inputs and receive finite,
positive, diagnosed safety coverage over the full MIDI 0-127 domain. They are
not represented as calibrated physical-keyboard notes.

PIT-003 remains `awaiting-approved-reference` after its software gates pass
because LO and approved hardware/profile evidence are absent. PIT-004 may pass.

## Fixed Constraints

The implementation must preserve:

- JUCE 8.0.10 at `3af3ce009f6a02f6fa651008fffb5b41743a9fab`;
- TTH Audio / TTH Model One identity and all target, wrapper, and bus
  contracts;
- all 48 parameter IDs, ordering, ranges, normalization, defaults, metadata,
  automation flags, smoothing classes, and persistence definitions;
- `modelDState` v2, v0 migration, safe extensions, atomic rollback, and the
  strict existing `calibrationProfileId="baseline"` state rule;
- canonical and `legacyCrossedContours` behavior;
- the prepared parameter snapshot and attachment-only editor boundary;
- `audio.pitch.v1` version 1, including its corrected globally strongest peak
  and `0.00001` numerical-tie rule;
- candidate-only output, source identity, compiler provenance, authoritative
  replay, and release enforcement;
- the globally `draft` acceptance manifest; and
- every open BLD, PAR, TST, PIT-005...008, hardware, host, listening, and
  release obligation.

No new parameter or state field is authorized. The normalized Pitch Wheel
parameter remains `[0,1]` with default `0.5`. The visible wheel component may
display `-7...+7`, but its host normalization and saved bytes do not change.

## Considered Approaches

### Chosen: factorized exhaustive math plus governed boundary renders

Exhaustively enumerate each pitch factor independently around neutral values,
then render representative combinations and every important boundary at each
supported sample rate. This proves every dimension without a redundant
Cartesian explosion.

### Rejected: full Cartesian processor rendering

Rendering every note/range/tune/offset/bend/oscillator/sample-rate combination
would produce large, slow fixtures dominated by duplicate information. It
would add verification cost without increasing the authority of the analytic
factor contracts.

### Rejected: representative audio renders only

A small render set cannot justify the handoff's complete-matrix requirement.
It would leave gaps in adjacent-note, tune-step, selector-step, and finite
domain coverage.

## Production Components

### Typed pitch-wheel conversion

Extend `PitchDomain` with:

```cpp
Checked<Semitones> pitchWheel (double normalizedValue) noexcept;
```

It accepts only finite values in `[0,1]` and returns
`Semitones { 14.0 * (normalizedValue - 0.5) }`. The exact binary input values
`0.0`, `0.25`, `0.5`, `0.75`, and `1.0` therefore produce exact half-semitone
multiples. Invalid input returns `valid=false`.

### Baseline calibration boundary

Represent the existing supported profile without changing host state:

```cpp
enum class CalibrationProfile { baseline };

Checked<Semitones> calibration (
    CalibrationProfile profile,
    RangeContribution range) noexcept;
```

`baseline` returns exactly zero semitones for a musical range. It rejects LO
because no approved LO mapping exists. No string lookup, profile file, or
state access occurs on the audio thread. The existing state parser remains the
sole authority that accepts only `calibrationProfileId="baseline"` and rejects
missing or unsupported IDs atomically.

### Processor composition

`composeMusicalFrequency` consumes the normalized wheel value rather than a
legacy frequency ratio. It obtains the checked bend and baseline calibration
terms from `PitchDomain`, adds both exactly once to `Contributions`, and
performs the existing single final hertz conversion.

The block-level LO compatibility path derives its relative bend multiplier
from the same typed semitone value:

```text
pitchWheelRatio = exp2(pitchWheelSemitones / 12)
```

This removes the asymmetric wheel from LO-relative bending without claiming a
calibrated LO base frequency. Oscillator 3 with keyboard control disabled
continues to ignore keyboard Pitch Wheel behavior as before.

Invalid composition retains the last valid frequency and increments the
existing invalid-parameter diagnostic. LO never calls the musical calibration
function, so absent LO calibration cannot create a per-sample diagnostic
storm.

### Editor display

`PitchWheelSlider` changes only its component range from `-5...+5` to
`-7...+7`. `ParameterBinding::componentRange` continues to map that component
range bijectively to the unchanged normalized host parameter. Spring return
still writes the exact center value inside the active gesture.

## Production Data Flow

For a musical oscillator:

```text
selected note frequency
 -> relative note semitones
 + musical range semitones
 + master tune semitones
 + Oscillator 2/3 offset semitones
 + 14 * (normalized Pitch Wheel - 0.5)
 + baseline calibration zero
 + preserved modulation compatibility semitones
 = one instantaneous semitone coordinate
 -> one final equal-tempered hertz conversion
 -> oscillator
```

PIT-008 still owns change detection, static-term caching, exponent counters,
CPU acceptance, and exact output-equivalence evidence. This slice does not
claim to optimize the per-sample musical composition.

## Existing PIT-002 Evidence Transition

`pit-002-composed-pitch-v1.json` remains byte-for-byte unchanged. It is an
immutable input that legitimately exercises the corrected production curve.
Its expected processor-output records become:

| Request | Old compatibility expectation | Corrected expectation |
|---|---:|---:|
| `osc1-composed` | 72.5 | 72.5 |
| `osc2-composed` | 51.863137138648348 | 51.5 |
| `osc3-composed` | 87.343587129994475 | 87.0 |

The acceptance policy and exact production/reference assertions change to the
corrected analytic values. PIT-002 remains pass because it still proves a
single semitone-domain composition and conversion. The prior evidence report
is preserved; the PIT-003/PIT-004 evidence identifies the new commit and
values as a superseding behavior correction.

## Reference Analyzer

Keep `audio.pitch.v1` unchanged. Register `audio.pitch.v2` version 2 for the
physical-keyboard musical-range matrix. Version 2 shares the proven
normalized-autocorrelation, parabolic interpolation, global strongest-peak,
integer-recurrence, confidence, ambiguity, and stable-diagnostic rules, but
expands the deterministic lag search to `4 Hz...5 kHz`.

Version 2 uses:

```text
algorithm=normalized-autocorrelation-parabolic-v2
ambiguity-separation=0.02
confidence-threshold=0.80
lag-range=4Hz..5000Hz
minimum-periods=4
peak-tie-tolerance=0.00001
periodic-multiple-tolerance=0.05
```

The lower bound covers F0 at 32' (approximately 5.46 Hz). The upper bound
comfortably covers C4 at 2' (approximately 1046.5 Hz) without changing the v1
contract. Synthetic calibration covers the matrix boundaries and
representative notes at 44.1, 48, and 96 kHz under a `0.005`-semitone limit,
stricter than the `0.01`-semitone acceptance allowance. The v1 calibration
grid, harmonic-rich regression, settings, and stable diagnostics must remain
unchanged while the shared implementation is parameterized for v2.

## Factorized Exhaustive Production Contract

The production test enumerates:

1. MIDI notes 17-60 and every adjacent ratio at neutral controls;
2. all five musical ranges and their exact `-24,-12,0,+12,+24` offsets;
3. all 11 master-tune positions and adjacent half-semitone ratios;
4. all 17 Oscillator 2/3 offsets and adjacent semitone ratios;
5. all three oscillator paths at representative nonzero compositions;
6. baseline calibration for every musical range and explicit LO rejection;
7. a dense normalized wheel grid proving linearity, monotonicity, center, exact
   endpoints, and cents symmetry;
8. processor output for the bend endpoints and intermediate quarter points;
9. full MIDI 0-127 finite/positive software safety; and
10. invalid note, range, tune, offset, wheel, calibration, and non-finite
    fallback with exact diagnostic deltas.

The test is factorized rather than Cartesian: one factor varies while the
others remain at declared neutral values, followed by representative nonzero
composition cases that reject omission or double application.

## Governed Render Fixtures

Add three immutable `model-d.render-fixture.v1` files:

- `pit-003-pit-004-pitch-matrix-44100-v1.json`;
- `pit-003-pit-004-pitch-matrix-48000-v1.json`; and
- `pit-003-pit-004-pitch-matrix-96000-v1.json`.

Each restores `native-default-state-v2.xml`, whose indexed bytes bind the
baseline calibration profile. Each declares PIT-003 and PIT-004 ownership,
uses block patterns `[[128], [17,31,64,127], [512]]`, and contains
`audio.pitch.v2` requests for:

- MIDI 17 / F0 at 32' and MIDI 60 / C4 at 2';
- a neutral MIDI 45 / A2 case at each of 32', 16', 8', 4', and 2';
- the adjacent MIDI 45 / A2 and MIDI 46 / A-sharp2 pair at 8';
- master-tune -2.5 and +2.5 semitones;
- Oscillator 2 offsets -8 and +8 semitones;
- Oscillator 3 offsets -8 and +8 semitones; and
- normalized wheel positions `0`, `0.25`, `0.5`, `0.75`, and `1`.

Control sweeps use MIDI 45, 8', neutral master tune, neutral wheel, and
Oscillator 1 unless the request explicitly names Oscillator 2 or 3. Each
oscillator uses the existing stable triangle waveform in isolation.
Analysis windows are long enough for at least four periods at the declared
frequency. Every request must remain deterministic and block-partition
invariant.

The exact request IDs in each sample-rate fixture are:

```text
boundary-f0-range32
boundary-c4-range2
range32-neutral
range16-neutral
range8-neutral
range4-neutral
range2-neutral
note-adjacent-lower
note-adjacent-upper
tune-minus2p5
tune-plus2p5
osc2-minus8
osc2-plus8
osc3-minus8
osc3-plus8
bend-minus7
bend-minus3p5
bend-center
bend-plus3p5
bend-plus7
```

The adjacent-note records are retained for governed review and analyzer
cross-rate coverage; the 54 passing gates bind the remaining 18 records per
fixture.

## Acceptance and Report Authority

Add 54 render-backed gates.

Extend `PublishedProvenance` with a mandatory lowercase `sourceSha256` field.
Because the current published section is empty, this adds no compatibility
case or migration. Every new published gate must bind the exact bundled manual
hash above in addition to its source, source version, and page. The `source`
field is the bounded repository path `Minimoog_Model_D_Manual.pdf`; manifest
validation resolves that exact regular file under the source root and compares
its bytes to `sourceSha256`.

PIT-003 receives 39 derived-software gates:

- five musical ranges at each of three sample rates: 15;
- F0/32' and C4/2' at each sample rate: 6;
- both master-tune endpoints at each sample rate: 6; and
- both offset endpoints for Oscillators 2 and 3 at each sample rate: 12.

PIT-004 receives 15 gates:

- published -7/+7 endpoints at each sample rate: 6; and
- derived -3.5/0/+3.5 symmetry points at each sample rate: 9.

Gate IDs use the exact deterministic forms below, where `<rate>` is one of
`44100`, `48000`, or `96000`:

```text
pit003.sr<rate>.boundary.f0-range32
pit003.sr<rate>.boundary.c4-range2
pit003.sr<rate>.range32
pit003.sr<rate>.range16
pit003.sr<rate>.range8
pit003.sr<rate>.range4
pit003.sr<rate>.range2
pit003.sr<rate>.tune.minus2p5
pit003.sr<rate>.tune.plus2p5
pit003.sr<rate>.osc2.minus8
pit003.sr<rate>.osc2.plus8
pit003.sr<rate>.osc3.minus8
pit003.sr<rate>.osc3.plus8
pit004.sr<rate>.bend.minus7
pit004.sr<rate>.bend.minus3p5
pit004.sr<rate>.bend.center
pit004.sr<rate>.bend.plus3p5
pit004.sr<rate>.bend.plus7
```

The published endpoint gates cite the bundled manual, the source version
`PDF created 2022-11-14`, its packaged SHA-256, and page 80 through typed
published provenance. Derived gates retain the `0.01`-semitone allowance and
state explicitly that they are equal-tempered software behavior, not hardware
calibration.

Exact policy allowlists cover the new derived and published gates. Every gate
is reciprocal across requirement map, fixture ownership, request identity,
analyzer/version/metric/unit, baseline state bytes, complete metric artifact,
source identity, and compiler identity.

The requirement map keeps PIT-003's seed status
`awaiting-approved-reference`, now with the 39 software gate IDs. PIT-004
retains a `not-run` seed and gains its 15 gate IDs; authoritative passing
evidence reduces it to `pass`.

Expected final inventory:

```text
render fixtures                         8
candidate files                        50
requirements                          127
gates                                 164
pass requirements                      17
not-run requirements                   91
awaiting-approved-reference            19
fail requirements                       0
releaseReady                        false
```

The acceptance manifest remains globally `draft`. `verify-release` continues
to exit `3` with zero stdout and exactly:

```text
release-not-ready: BLD-006 not-run requirement.not-run
```

## Failure and Negative Authority Tests

The implementation must reject:

- non-finite or out-of-range normalized wheel values;
- a missing, altered, or unsupported calibration profile in canonical state;
- an attempt to treat LO as baseline-calibrated;
- analyzer v1/v2 substitution;
- a changed sample rate, request window, fixture requirement owner, or
  baseline state hash;
- published gates with altered source, version, page, classification, or
  endpoint;
- derived gates with altered review basis, value, allowance, or claim scope;
- missing, moved, altered, or hash-forged metric artifacts;
- stale source/compiler identity;
- a report that promotes PIT-003 to pass without approved reference evidence;
  and
- a coordinated false report that promotes release readiness.

Missing LO/hardware evidence remains `awaiting-approved-reference`; it is not a
software failure. A numerical software bound violation is a failure.

## Verification

Use genuine RED/GREEN TDD for every behavior:

1. pitch-wheel math and processor endpoints fail against the old asymmetric
   curve;
2. baseline/LO profile boundaries fail before typed calibration exists;
3. `audio.pitch.v2` requests fail before the analyzer is registered;
4. the three fixtures and 54 policies fail before indexing and authority
   support;
5. PIT-004 remains non-passing before complete evidence; and
6. forged PIT-003 pass/release reports reject throughout.

Final verification includes:

- strict Release all-target build with warnings as errors and validators on;
- serial full CTest and every nonempty label;
- focused pitch-domain and reference analyzer/fixture/requirement contracts;
- CLI validation and two fresh candidate generations;
- exact 50-file inventory and byte-for-byte candidate equality;
- authoritative release replay and negative mutations;
- source, fixture, map, link, append-only history, state/registry/contour
  freeze, and status guards; and
- extracted no-`.git` tests-off configuration against exact local JUCE.

## Documentation and Successor Package

Closeout synchronizes the roadmap, traceability matrix, Workstream 04 and 12
owner plans, canonical handoff, expected status projection, prior pitch
evidence, and a new PIT-003/PIT-004 evidence report. It preserves PIT-003's
hardware boundary and every unrelated open requirement.

After all tracked implementation, test, evidence, and documentation changes
are committed, construct one untracked successor ZIP from the exact clean
commit. Continue the established one-root protocol: exact committed source,
orientation, handoff, planning/evidence, manual, Git state, verification
summary, exact candidate, internal SHA-256 manifest, safe-path/no-symlink
checks, committed-source equality after only the established three
`.DS_Store` exclusions, candidate equality, honest report, and extracted
no-`.git` configuration.

## Explicit Non-Goals

- An approved LO mapping or 0.1 Hz endpoint proof.
- Multiple selectable calibration profiles or a state-schema change.
- Unit-specific tuning offsets, drift, uncertainty, or hardware bands.
- PIT-005 oscillator-modulation semantics.
- PIT-006/PIT-007 glide behavior.
- PIT-008 caching, exponent counters, CPU acceptance, or scheduling.
- Waveform, MIDI lifecycle, filter, contour, signal-path, UI-layout, preset,
  host, listening, distribution, or release work.
