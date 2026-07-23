# Workstream 04 PIT-001/PIT-002 Pitch Foundation Design

**Date:** 2026-07-23

**Status:** Approved in conversation

**Base:** `648f2feda0692c2123e3431f66e3e464581f1614`

**Scope:** Implement only PIT-001 and PIT-002, including authoritative
renderer/analyzer/report evidence. PIT-003 through PIT-008 remain open under
their existing owners and dependencies.

**2026-07-23 approved correction:** `audio.pitch.v1` selects the globally
strongest valid local autocorrelation peak, with earliest lag used only for a
`0.00001` numerical tie. Static-term scheduling and caching are explicitly
deferred to PIT-008.

## Outcome

The processor will use one typed semitone-domain pitch model for every musical
oscillator range. Keyboard note, range, master tune, oscillator offset, pitch
wheel compatibility, calibration baseline, and oscillator-modulation
compatibility contributions will be summed in semitones before one final
equal-tempered conversion to hertz.

Oscillator 2 and 3 Frequency selector positions will map exactly from stored
index `0...16` to `-8...+8` semitones. The center position will produce a unity
ratio. Governed offline fixtures will prove the selector and composed pitch at
the actual processor output, and the authoritative requirement report will
promote PIT-001 and PIT-002 to `pass` without changing any unrelated status or
global release readiness.

## Fixed Constraints

The implementation must preserve:

- JUCE 8.0.10 at `3af3ce009f6a02f6fa651008fffb5b41743a9fab`;
- TTH Audio / TTH Model One identity and the existing target and bus contracts;
- all 48 parameter IDs, ordering, ranges, normalization, defaults, metadata,
  automation flags, smoothing classes, and persistence definitions;
- `modelDState` v2, v0 migration, safe extensions, atomic rollback, and all
  frozen Workstream 03 artifacts;
- canonical and `legacyCrossedContours` behavior and explicit conversion;
- the prepared parameter snapshot and attachment-only editor boundary;
- the F0 candidate-only output, source-identity, provenance, replay, and
  release-enforcement contracts;
- the globally `draft` acceptance manifest; and
- all open BLD, PAR, TST, PIT-003...008, host, hardware, listening, and release
  obligations.

The slice does not invent a Model D hardware calibration, LO mapping, glide
taper, modulation depth, pitch-wheel endpoint, or compatibility migration.

## Considered Approaches

### Chosen: typed pitch-domain component with staged compatibility

Add a focused production `PitchDomain` component. The processor constructs
typed semitone contributions, composes them, and performs the final hertz
conversion. Existing bend, modulation, and glide behavior is translated into
the shared domain rather than redesigned. This satisfies PIT-001/PIT-002 and
gives PIT-004...008 one stable extension seam.

### Rejected: patch `Oscillator` in place

Changing only `calculateDetunedFrequency` to subtract eight would close the
selector defect but leave note, range, tune, bend, and modulation split between
unrelated linear and ratio domains. That does not provide the analytic
PIT-002 model and would preserve the highest-conflict call structure.

### Rejected: complete Workstream 04 now

Implementing published bend endpoints, semitone modulation behavior, calibrated
LO, time-per-octave glide, transition semantics, and exponent caching in one
change would conflate PIT-004...008 with this handoff. Those requirements have
separate evidence and hardware dependencies.

## Production Components

Create `Source/PitchDomain.h` and `Source/PitchDomain.cpp` with small,
allocation-free value types:

```cpp
namespace PitchDomain {
struct Semitones { double value = 0.0; };
struct Hertz { double value = 440.0; };

enum class RangeMode { lowFrequency, musical };
struct RangeContribution {
    RangeMode mode = RangeMode::musical;
    Semitones semitones {};
};

struct Contributions {
    Semitones note;
    Semitones range;
    Semitones masterTune;
    Semitones oscillatorOffset;
    Semitones pitchWheel;
    Semitones calibration;
    Semitones modulation;
};

struct Result {
    Semitones coordinate;
    Hertz hertz;
    bool valid = false;
};
}
```

The public operations are pure and `noexcept`:

- `midiNote(int)` returns a coordinate relative to A4, so note 69 is zero;
- `range(int)` returns LO as `lowFrequency` and musical indices as
  `-24, -12, 0, +12, +24` semitones;
- `masterTune(int)` returns `-2.5...+2.5` in exact half-semitone steps;
- `oscillatorOffset(int)` returns `index - 8` for indices `0...16`;
- `ratioToSemitones(double)` converts a positive finite compatibility ratio
  with `12 * log2(ratio)`;
- `fromHertz(double)` translates the existing glide state to the common
  coordinate relative to 440 Hz;
- `compose(const Contributions&)` adds every finite term; and
- `toHertz(Semitones)` performs `440 * exp2(semitones / 12)` once and validates
  the result.

Invalid indices, non-finite terms, non-positive ratios, or unsafe output return
`valid=false`. The processor then uses the last valid or declared baseline
pitch and increments the existing invalid-parameter diagnostic. There are no
exceptions, allocations, locks, strings, logging calls, or parameter lookups
on the sample path.

## Production Data Flow

For a musical range, the processor performs:

```text
legacy glide frequency
 -> relative note semitones
 + musical range semitones
 + master tune semitones
 + oscillator 2/3 selector semitones
 + preserved pitch-wheel ratio expressed as semitones
 + explicit zero calibration baseline
 + preserved oscillator-modulation ratio expressed as semitones
 = final oscillator semitone coordinate
 -> one exp2 conversion
 -> final oscillator hertz
```

This slice centralizes ownership of the static range, master-tune, selector,
pitch-wheel, calibration, and modulation terms in one typed composition. The
implemented Task 2 compatibility path currently recomputes that composition
per sample. It does not claim a static-term scheduling or caching result.
PIT-008 owns change detection, cached/incremental conversion, exponent
counters, CPU acceptance, and output-equivalence proof. Oscillator 1 uses a
zero oscillator-offset term. Oscillator 2 and 3 use the exact selector mapping.

The existing pitch-wheel ratio curve is not redefined. Its current `2/3...1.5`
ratio is translated with `ratioToSemitones`; PIT-004 later replaces only that
contribution generator with the published centered `-7...+7` mapping.

The existing modulation ratio is not redefined. The current positive
`1 + effect` ratio is translated with `ratioToSemitones`; PIT-005 later
replaces only that contribution generator with the approved semitone/octave
depth model.

The existing glide trajectory remains in its current hertz state. Each current
glide value is translated with `fromHertz` before composition. PIT-006 later
moves the trajectory itself into semitone space without changing the
composition interface.

The calibration term is explicitly zero until an approved profile exists. No
profile identifier or unit-specific value is inferred.

`Oscillator` receives the fully composed musical-range hertz value and does not
reapply range, master tune, oscillator offset, or modulation. Its waveform,
phase, activity, volume, and control-role behavior are otherwise unchanged.
Legacy setters may remain temporarily for direct compatibility callers, but
the processor's musical path has one pitch authority.

## LO Boundary

LO is a typed special mode, not an invented musical-range offset. Its existing
behavior remains isolated from the PIT-001/PIT-002 passing evidence. PIT-003
and OSC-005 retain responsibility for measured LO mapping, calibration, and
cross-rate acceptance. The musical ranges `32'`, `16'`, `8'`, `4'`, and `2'`
are the exact coarse-range scope of this slice.

## Reference Analyzer

Register `audio.pitch.v1` version 1 in the existing analyzer registry. Extend
`AnalysisRequest` with the render sample rate, populated from the immutable
`RenderResult`.

For a finite mono single-pitch periodic analysis window, the analyzer:

1. removes the mean;
2. rejects silence, non-finite samples, insufficient duration, and fewer than
   four credible periods;
3. computes normalized autocorrelation over the deterministic 20 Hz...5 kHz
   lag search;
4. uses deterministic parabolic peak interpolation to select the globally
   strongest valid local peak, with the earliest lag winning only when peak
   strengths are within the explicit `0.00001` numerical-tie tolerance;
5. applies deterministic parabolic lag interpolation;
6. rejects boundary or ambiguous peaks and confidence below the calibrated
   threshold while retaining integer-periodic-recurrence handling; and
7. reports `frequency-hz`, `midi-semitones`, and `confidence`.

`midi-semitones` is `69 + 12 * log2(frequency / 440)`. The derived-software
acceptance allowance is `0.01` semitone, or one cent. Synthetic analytic tones
at 44.1, 48, and 96 kHz must demonstrate a stricter maximum error before any
processor evidence may pass. Analyzer settings, sample rate, input hash,
allowance, and confidence policy are serialized in the canonical metric
artifact.

A 48 kHz regression repeats a 240-sample period containing a 200 Hz
fundamental at amplitude `0.075` and a ten-times-stronger 400 Hz second
harmonic at amplitude `0.75`. The half-period correlation is about `0.9802`
and the full-period correlation is `1.0`; the analyzer must report the 200 Hz
fundamental within the existing `0.005`-semitone synthetic limit.

## Governed Render Fixtures

Add two `model-d.render-fixture.v1` files and index them immutably.

### PIT-001 oscillator selector sweep

`pit-001-oscillator-offset-sweep-v1.json` renders Oscillator 2 and Oscillator 3
individually with the existing stable triangle waveform under MIDI control at
8', zero master tune, neutral pitch wheel, zero modulation, and the zero
calibration baseline. It
contains all 17 selector positions for both oscillators. Every position has a
settling prefix followed by a same-value event anchor and an
`audio.pitch.v1` analysis window.

The metric artifact therefore contains the complete 34-window audio sweep.
Acceptance gates bind the lower, center, and upper records for each oscillator;
the focused production contract proves the complete exact table and the
artifact retains every intermediate record for review.

### PIT-002 composed pitch

`pit-002-composed-pitch-v1.json` renders representative Oscillator 1, 2, and 3
cases with nontrivial MIDI notes, musical ranges, master-tune positions,
Oscillator 2/3 offsets, and non-neutral preserved bend ratios. Modulation and
calibration are explicit zero-baseline terms in the audio cases. Pure
production tests separately compose nonzero values for every term, including
modulation and calibration, without claiming their later hardware or musical
mapping requirements.

Every case has a settling prefix, an exact event anchor, and an
`audio.pitch.v1` request whose expected coordinate is calculated independently
from the typed analytic model.

Both fixtures use `foundation-reviewed` only as fixture-readiness metadata. It
does not approve hardware, listening, calibration, release, or the global
manifest.

## Acceptance and Authority

Add nine approved derived-software gates:

- PIT-001 Oscillator 2 lower, center, and upper selector positions;
- PIT-001 Oscillator 3 lower, center, and upper selector positions; and
- PIT-002 representative Oscillator 1, 2, and 3 compositions.

Each render-backed gate includes its exact fixture ID and request ID in the
existing v1 manifest entry. This is an additive v1 field used only by new
render-backed gates; existing manifests and F0 gates retain their semantics.
The loader requires the bound fixture/request to exist, match the analyzer
identity/version/metric/unit, and own the gate's requirements.

Candidate generation selects the bound record from the complete fixture
`metrics.json`, attaches the whole artifact and its SHA-256, and passes it to
the existing metric-evidence evaluator. Evaluation independently reloads the
canonical index and fixture, rerenders every block pattern, reanalyzes every
request, compares the selected record and full metric artifact, and validates
source/build/compiler identity before a gate can pass.

No hand-built metric or report promotion is accepted. Missing evidence remains
`not-run`; malformed, stale, mismatched, or out-of-bound evidence is `fail`.

## Tests

Implementation uses genuine RED/GREEN TDD.

### `ModelDPitchDomainContract`

The production contract proves:

- the exact 17-entry `-8...+8` selector table;
- exact center unity and adjacent equal-tempered ratios;
- musical range offsets `-24, -12, 0, +12, +24`;
- master tune `-2.5...+2.5` in half-semitone steps;
- arbitrary nonzero contributions add exactly in semitone space;
- compatibility ratios round-trip through semitones within the declared
  floating policy;
- a nonzero processor render does not apply note/range/tune/offset/bend twice;
- Oscillator 2 and 3 use the same selector mapping;
- invalid indices, ratios, non-finite terms, unsafe results, and invalid
  processor input take the diagnostic fallback; and
- the code path is allocation-, lock-, string-, and exception-free.

### `ModelDReferencePitchContract`

The reference contract proves:

- synthetic analyzer accuracy at every supported sample rate and pitch range;
- the harmonic-rich weak-fundamental regression selects the strongest
  full-period peak rather than the earlier dominant-harmonic half-period;
- deterministic failure for silence, ambiguity, short windows, non-finite
  input, unknown metrics, and invalid sample rates;
- both new fixtures validate and remain block-partition invariant;
- repeat renders and metrics are byte-identical at one clean exact source
  identity;
- every fixture/request/gate binding is reciprocal;
- missing, forged, stale, moved, tampered, or numerically wrong evidence
  rejects; and
- PIT-001/PIT-002 pass only through authoritative render metrics.

Final verification runs the complete Release build and CTest suite serially,
all seven labels, CLI smoke and validation, two candidate generations, exact
file comparison, expected release exit 3, source/hash/map/link/history/status
guards, `git diff --check`, and extracted tests-off archive configuration.

## Report and Artifact Changes

Two fixtures add twelve candidate files. The exact candidate inventory becomes:

- five render fixtures at six files each: 30 files;
- one root live-registry `metrics.json`; and
- one `requirements-report.json`.

Each candidate therefore contains exactly 32 files. The immutable source index
grows from 19 to 21 regular artifacts.

Nine new acceptance gates grow the report from 101 to 110 gates. PIT-001 and
PIT-002 move from `not-run` to `pass`, producing:

- 127 requirements;
- 16 `pass`;
- 92 `not-run`;
- 19 `awaiting-approved-reference`;
- 0 `fail`; and
- `releaseReady=false`.

Authoritative `verify-release` must continue to exit 3 at the honest first-open
BLD-006 row.

## Planning, Evidence, and Successor Package

Closeout synchronizes the roadmap, traceability matrix, Workstream 04 owner
plan, Workstream 12 artifact inventory, canonical handoff, expected status
projection, and a new Workstream 04 evidence report. PIT-003...008 and every
unrelated open gate remain explicit.

After all tracked changes and evidence are committed, construct one untracked
successor ZIP from the exact clean commit. It must include orientation,
handoff, complete planning/evidence, required primary references, repository
state, verification summary, exact candidate artifacts, and a SHA-256 manifest.
It must exclude Git metadata, builds, caches, dependencies, tools, credentials,
settings, recovery data, predecessor archives, and unrelated workspace paths.
Verify ZIP integrity, safe one-root paths, zero symlinks, fresh extraction,
every internal hash, committed-source equality, exact final commit identity,
candidate equality, and the packaged non-release-ready result.

## Explicit Non-Goals

- PIT-003 cents/hardware/calibration completion.
- PIT-004 published `-7...+7` pitch-wheel behavior.
- PIT-005 approved semitone/octave modulation depth and sideband gates.
- PIT-006/PIT-007 glide mapping and transition state machine.
- PIT-008 exponent caching and CPU acceptance.
- Measured LO behavior or compatibility migration.
- MIDI priority/lifecycle, oscillator waveform, filter, contour, UI, preset,
  host, listening, hardware, distribution, or release work.
