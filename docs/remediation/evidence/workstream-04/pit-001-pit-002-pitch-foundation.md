# Workstream 04 PIT-001/PIT-002 Pitch Foundation Evidence

Date: 2026-07-23

Status: PIT-001 and PIT-002 pass. Workstream 04 and the product remain
incomplete and non-release-ready.

## Claim boundary

This evidence closes only:

- PIT-001: Oscillator 2/3 selector indices `0...16` map exactly to
  `-8...+8` semitones before ratio conversion; and
- PIT-002: musical-range note, range, tune, oscillator offset, compatible bend,
  baseline calibration, and compatible modulation contributions are composed
  in one typed semitone domain before one hertz conversion.

It does not close LO calibration, nonzero modulation semantics, the published
pitch-bend contract, glide, caching/optimization, hardware calibration, host
validation, listening, or release. PIT-003 through PIT-008 remain open. The
acceptance manifest remains globally `draft`, the authoritative report remains
non-release-ready, and release verification exits `3` at BLD-006 with:

```text
release-not-ready: BLD-006 not-run requirement.not-run
```

For the Task 6 successor package, the user approved the explicit package
exclusion as the resolution of the raw archive-equality conflict:
`repository-context` comes from exact `git archive HEAD` content after
excluding only `.DS_Store`, `Builds/.DS_Store`, and `Source/.DS_Store`.
Package source equality is therefore checked against the archive with those
same three exclusions applied; no other committed path is omitted.

The musical-path work deliberately preserves the existing LO, bend,
modulation, and glide behavior until their owner requirements are implemented.
No hardware value or migration rule was inferred.

This slice centralizes musical-pitch authority but does not claim
snapshot-boundary static-term scheduling or caching. The Task 2 compatibility
path recomputes composition per sample. PIT-008 remains open and owns change
detection, exponent caching, counters, CPU acceptance, and exact
output-equivalence evidence.

Two nonblocking Task 5 review observations remain visible for final triage:
candidate-generation and release-replay evidence-appending logic is
duplicated, and the automated CLI contract asserts exit `3` while exact
stdout/stderr bytes are proven by the preserved manual exact-head probe. The
final-review correction changes neither authority path and does not weaken the
manual exact-output proof.

## Exact commit chronology

| Purpose | Commit |
|---|---|
| Approved implementation base | `648f2feda0692c2123e3431f66e3e464581f1614` — `Complete ultimate authority verification checklist` |
| Approved design | `c0989a05e4cd9e38d17c3edb0bec19d3d6341cc1` — `docs: design workstream 04 pitch foundation` |
| Design fixture alignment | `e3588785ab06bf295be500d1d9242227e351e27f` — `docs: align pitch fixtures with oscillator waveforms` |
| Implementation plan | `a6f87f187ab1aca423b99748e212410e9ff3373e` — `docs: plan workstream 04 pitch foundation` |
| Typed pitch domain | `b977afd240cdaccaf46ccb518759ee0c1b0fa438` — `feat: add typed pitch domain` |
| Musical processor path | `d1476cd16ec3371a9dfb2ecd7aad8bf8e1eaf3f8` — `fix: compose musical pitch in semitones` |
| Musical setter review fix | `5a153b0d2d02e5ace3a180fbc4940cd520abb8c8` — `fix: bypass legacy setters for musical pitch` |
| Versioned pitch analyzer | `83926fe18fa11849c923ce33d310820d97aee83d` — `feat: add versioned pitch analyzer` |
| Governed PIT fixtures | `65de36d72540dab70e14715c6b94d0b16da361ac` — `test: add governed pitch fixtures` |
| Bound acceptance/report authority | `6cf4699ec1b49f4961810605abae07791d606a90` — `feat: bind pitch evidence to acceptance` |
| Strongest-peak and pitch-contract correction | `b04fe5a26ccef619b66df4f143c402c5d91603ee` — `fix: select strongest pitch peak` |

The documentation-closeout commit contains this evidence. Its exact SHA, tree,
generated content identity, final verification results, and successor-package
facts are recorded without self-reference in the package Git state and the
ignored Task 6/final-review execution reports.

## RED chronology

Every implementation task first demonstrated the missing behavior:

1. Typed-domain RED: building `ModelDTests` failed because `PitchDomain.h` did
   not exist.
2. Processor RED: `ModelDPitchDomainContract` exited `8` with exact failures
   `Oscillator 2 output selector must be centered at zero` and
   `Oscillator 3 output selector must be centered at zero`.
3. Review-fix RED: the same contract rejected redundant musical-path setters
   with `musical processor pitch paths must bypass legacy frequency setters`.
4. Analyzer RED: `ModelDReferenceTests` failed to compile because
   `AnalysisRequest::sampleRate` did not exist.
5. Window-propagation RED: a two-region fixture returned MIDI `41.0967`
   instead of `69`, proving that exact request windows and render sample rate
   were not yet propagated.
6. Fixture RED: the manifest reported three rather than five render fixtures,
   both PIT fixture IDs were absent, and the pitch fixture contract failed.
7. Acceptance RED: the checked-in manifest was rejected because only the
   prior four PAR-006 derived policies were permitted; incomplete binding
   cases returned the pre-binding diagnostic.
8. Whole-branch analyzer RED: at base
   `22d5e287ab8548c81f72342105bfc0ffb80e01b8`, a repeated 48 kHz signal
   containing a 200 Hz fundamental at amplitude `0.075` and a 400 Hz second
   harmonic at amplitude `0.75` failed the strict fundamental assertion with
   error `12.003704997` semitones. The current earliest-within-`0.02`
   selection had chosen the dominant-harmonic half-period.

These were feature-specific failures. The adjacent renderer contract remained
green during fixture RED, and no unrelated failure was reclassified as
success.

## Focused GREEN chronology

| Task | Command boundary | Result |
|---|---|---|
| Typed math | `ctest ... -R '^(ModelDPitchDomainContract|ModelDUnitContract|ModelDDspContract)$' -j1` | 3/3 pass |
| Processor integration | `ctest ... -R '^(ModelDPitchDomainContract|ModelDUnitContract|ModelDDspContract|ModelDMidiSampleZero|ModelDRealtimeSmoke)$' -j1` | 5/5 pass before and after the setter review fix |
| Pitch analyzer | `ctest ... -R '^(ModelDReferencePitchContract|ModelDReferenceAnalyzerContract|ModelDReferenceRendererContract)$' -j1` | 3/3 pass |
| Governed fixtures | `ctest ... -R '^(ModelDReferenceManifestContract|ModelDReferenceRendererContract|ModelDReferenceAnalyzerContract|ModelDReferencePitchContract)$' -j1` | 4/4 pass before and after commit; source-overwrite guard passes |
| Acceptance authority | `ModelDReferenceTests` categories `manifest`, `renderer`, `analyzers`, `pitch`, and `requirements` | 5/5 category invocations pass |
| Strongest-peak correction | `ctest ... -R '^ModelDReferencePitchContract$' -j1` | Pass; harmonic-rich fundamental error `0.000010641` semitone |
| Hardened production contract | `ctest ... -R '^(ModelDPitchDomainContract|ModelDUnitContract|ModelDDspContract|ModelDMidiSampleZero|ModelDRealtimeSmoke)$' -j1` | 5/5 pass; complete tune/adjacent-ratio/nonzero-composition/invalid-fallback proof |
| Correction reference slice | `ctest ... -R '^(ModelDReferenceManifestContract|ModelDReferenceRendererContract|ModelDReferenceAnalyzerContract|ModelDReferencePitchContract|ModelDReferenceRequirementContract)$' -j1` | 5/5 pass |

All final full-suite and label results at each exact documentation head are
retained in the ignored Task 6 or final-review execution report and successor
verification summary.

## Typed pitch and analyzer contract

The production authority is `PitchDomain::Checked<T>` with typed
`Semitones`, `Hertz`, `RangeContribution`, `Contributions`, and `Result`
values. MIDI note 69 is 440 Hz; musical range contributions are
`-24, -12, 0, +12, +24`; master tune uses half-semitone steps; and Oscillator
2/3 offsets are `index - 8`.

`ModelDPitchDomainContract` now enumerates the full 11-entry master-tune table,
proves all 16 adjacent selector ratios, observes a nonzero note/range/tune/
offset/bend composition through processor audio to prevent double application,
and injects a non-finite master-tune value through the existing production
parameter path. The invalid render remains finite at the declared neutral
fallback and increments the public invalid-parameter diagnostic exactly once.
No test-only processor hook exists.

`audio.pitch.v1` is analyzer version `1`. It emits `frequency-hz` (`Hz`),
`midi-semitones` (`semitones`, allowance `0.01`), and `confidence` (`ratio`)
with these exact settings:

```text
algorithm=normalized-autocorrelation-parabolic-v1
ambiguity-separation=0.02
confidence-threshold=0.80
lag-range=20Hz..5000Hz
minimum-periods=4
peak-tie-tolerance=0.00001
periodic-multiple-tolerance=0.05
```

The analyzer compares parabolically interpolated local-peak strengths, selects
the global maximum, and uses ascending lag only inside the explicit
`0.00001` numerical-tie tolerance. The `0.02` separation remains solely an
ambiguity boundary. Integer-periodic-recurrence handling remains anchored to
the earliest confidence-qualified member of the comparable peak family, while
the reported pitch comes from the globally strongest peak.

The synthetic grid covers MIDI 36, 60, 69, 77, and 96 at 44.1, 48, and
96 kHz. Its maximum absolute error is `0.000419994` semitone at 44.1 kHz /
MIDI 96, below the `0.005` synthetic-calibration limit. The harmonic-rich
weak-fundamental regression reports error `0.000010641` semitone under the
same limit.

Stable analyzer rejection diagnostics are:

| Invalid input | Diagnostic |
|---|---|
| zero sample rate | `analyzer.sample-rate` |
| silence | `analyzer.pitch-silence` |
| insufficient or boundary window | `analyzer.pitch-window` |
| competing non-periodic peaks | `analyzer.pitch-ambiguous` |
| non-finite sample | `analyzer.non-finite` |

## Governed fixtures and compatibility decision

| Artifact | SHA-256 |
|---|---|
| `Tests/reference/fixtures/pit-001-oscillator-offset-sweep-v1.json` | `99dd9fc5935ab43442dceee59a1d6268bc9fe4e2695c2f3c51338cc86dd2ad34` |
| `Tests/reference/fixtures/pit-002-composed-pitch-v1.json` | `63a2a95b9caee4799d814abe647e4c2433b82eb59fddebf502f38f68864e12ee` |
| `Tests/reference/fixture-index-v1.json` | `72ab68c7b63d22409a699571d7c12114924646ee6f6643a5acb9ac0f4dedfb9a` |

The index contains five render fixtures and 21 total governed artifacts. The
PIT fixtures provide 37 finite pitch records: 34 Oscillator 2/3 selector
records and three composed-pitch records. Repeated renders under all three
block patterns produce byte-identical canonical metric records. The largest
selector error is `0.000348182990` semitone; the largest composed error is
`0.000307297084` semitone.

The live validated fixture-index schema names its bounded file field
`relativePath`. Task 4 Step 5 mistakenly said `path`; the user explicitly
approved following the live `relativePath` field. The implementation, plan
correction, and this evidence preserve that schema. No schema migration or
compatibility rewrite is authorized or required.

## Nine authoritative gates

All gates use `audio.pitch.v1` version `1`, metric `midi-semitones`, unit
`semitones`, allowance `0.01`, and the approved 2026-07-23 review metadata.

| Gate | Expected | Fixture / request |
|---|---:|---|
| `pit001.osc2.minus8` | 61.0 | `pit-001-oscillator-offset-sweep-v1` / `osc2-offset-m08` |
| `pit001.osc2.center` | 69.0 | `pit-001-oscillator-offset-sweep-v1` / `osc2-offset-z00` |
| `pit001.osc2.plus8` | 77.0 | `pit-001-oscillator-offset-sweep-v1` / `osc2-offset-p08` |
| `pit001.osc3.minus8` | 61.0 | `pit-001-oscillator-offset-sweep-v1` / `osc3-offset-m08` |
| `pit001.osc3.center` | 69.0 | `pit-001-oscillator-offset-sweep-v1` / `osc3-offset-z00` |
| `pit001.osc3.plus8` | 77.0 | `pit-001-oscillator-offset-sweep-v1` / `osc3-offset-p08` |
| `pit002.osc1.composed` | 72.5 | `pit-002-composed-pitch-v1` / `osc1-composed` |
| `pit002.osc2.composed` | 51.863137138648348 | `pit-002-composed-pitch-v1` / `osc2-composed` |
| `pit002.osc3.composed` | 87.343587129994475 | `pit-002-composed-pitch-v1` / `osc3-composed` |

Each binding is reciprocal across the acceptance manifest, fixture ownership,
request identity, analyzer/version/metric/unit, and requirement map. Candidate
and release paths retain and hash the full canonical fixture metric artifact;
they do not manufacture selected values.

## Authoritative Task 5 candidate and report

At clean implementation commit `6cf4699ec1b49f4961810605abae07791d606a90`,
two independent candidate runs each contained 32 files and all corresponding
bytes matched. Important hashes were:

| Artifact | SHA-256 |
|---|---|
| root `metrics.json` | `0c452f3108ca283b8bee9dc1da9dc01caec6e038f11c9e3ad6c5d37b4878fd73` |
| PIT-001 render `metrics.json` | `9e5e347872eeadb79398e54cbf64906410d04b0bc643c0600a28629ed2121959` |
| PIT-002 render `metrics.json` | `2ab3b8c0a27940b479115e9e2713f0c9168a7d4dca2673880d171a7c96529702` |
| `requirements-report.json` | `8c04128ecfa7e6b62dfe66a5e14f4d4ef1d858f24f1138341fe72e37ffe8438d` |
| checked-in status projection | `3fc01201f34f52ce8412e47f772cd826a28cbed4d001b9283b0157d1275c0f83` |

The report has exactly 127 requirements and 110 gates:

```text
pass=16
not-run=92
awaiting-approved-reference=19
fail=0
releaseReady=false
```

Only PIT-001 and PIT-002 move to `pass`. No unrelated requirement is promoted.
The exact documentation-head candidate necessarily has a new clean source
identity; its hashes are recorded after that commit in the ignored Task 6
report and packaged verification summary.

## Stable negative authority diagnostics

The authority contracts reject incomplete render bindings with:

```text
acceptance.metric-binding: render metric binding requires both fixture and request IDs
```

Invalid approved-policy mutations reject through the stable
`acceptance.derived-policy` boundary; an unknown analyzer metric reports
`acceptance.metric: acceptance metric is unknown for the analyzer version`.
Invalid or forged evidence reduces to
`acceptance.metric-evidence-invalid`; a numerical bound violation reduces to
`acceptance.metric-out-of-bound`.

Release mutations reject with the following stable diagnostics:

```text
release.gate-evidence: submitted gate evidence is not authoritative
release.report-mismatch: report provenance is not the current build provenance
release.report-artifact: report artifact hash changed
release.report-mismatch: candidate evidence placement is not authoritative
release.report-mismatch: submitted report does not match authoritative reduction
```

Task 6 and the final-review closeout repeat the tampered pitch metric, missing
render metric, forged request binding, dirty-current-tree, stale-binary, and
false-report probes at their exact documentation heads. Their commands and
observed outputs are retained in ignored execution reports so the tracked
evidence does not create an exact-head self-reference.

## Scope and next action

Frozen parameter registry, state, contour compatibility, source-identity,
candidate-only, and release-authority contracts remain unchanged. The next
roadmap action is to plan and implement PIT-003 then PIT-004 while retaining
PIT-005 through PIT-008, PAR-002/004/006/007, TST-001 through TST-009, and
BLD-006/011/012 as open obligations.
