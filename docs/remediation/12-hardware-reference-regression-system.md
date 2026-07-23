# Workstream 12 — Hardware-Reference and Regression Test System

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Real-time safety](11-real-time-safety-visualization-performance.md) · Next: [Presets](13-versioned-presets-official-library.md)

## Goal and user-visible outcome

Every fidelity, correctness, compatibility, real-time, and host claim is reproducible from versioned fixtures and evidence. Published specifications, software invariants, measured hardware bands, golden regression data, and subjective listening are distinct and cannot substitute for one another.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| TST-001 | Build a deterministic offline renderer for canonical state/presets and sample-timestamped MIDI/automation/audio input. |
| TST-002 | Provide analyzers for cents, spectra/aliasing, waveform/DC/pulse, contour timing/shape, cutoff/tracking/resonance, saturation/gain, latency, and sample-rate/block consistency. |
| TST-003 | Define versioned golden MIDI/state/audio fixtures with explicit generation, review, hash, and invalidation rules. |
| TST-004 | Separate hard software gates, published-spec gates, measurement-derived hardware bands, and supplemental listening results. |
| TST-005 | Store complete reference-instrument, calibration, environment, capture-chain, raw-file, repetition, uncertainty, and analysis provenance. |
| TST-006 | Create a machine-readable acceptance manifest; forbid invented hardware values/tolerances and make missing gates visible. |
| TST-007 | Map every traceability requirement to executable verification IDs and generate an aggregate pass/not-run/fail report. |
| TST-008 | Cover supported sample rates, host block partitions, resets, state versions, platforms/architectures, and reproducibility. |
| TST-009 | Define blinded, randomized, level-matched listening and hardware A/B protocols whose results cannot waive numerical failures. |

## Foundation closeout status

The Workstream 12 **F0 harness skeleton** is complete and verified; Workstream
12 as a whole is not complete. The durable command, hash, RED/GREEN, and status
record is the [F0 harness foundation evidence](evidence/workstream-12/f0-harness-foundation.md).

| ID | Status | F0 result / remaining acceptance |
|---|---|---|
| TST-001 | in-progress | Three deterministic state/MIDI/automation fixtures render 18 hashed candidate artifacts across three block patterns. Preset/audio-input and later owner-fixture coverage remains open. |
| TST-002 | in-progress | `signal.stats.v1`, `control.step.v1`, and `audio.click.v1` pass synthetic and finite-extreme contracts. The complete owner-analyzer catalog remains open. |
| TST-003 | in-progress | Frozen indexed inputs, candidate-only writes, exact hashes, and overwrite rejection are executable. An approved golden regeneration and two-reviewer freeze has not been exercised. |
| TST-004 | in-progress | Gate classifications and authority boundaries are executable and listening cannot waive numerical status. Later populated evidence classes remain open. |
| TST-005 | in-progress | Missing hardware provenance remains visible as `awaiting-approved-reference`; no hardware capture campaign or reference-instrument record has run. |
| TST-006 | in-progress | The machine-readable manifest remains globally `draft`; exactly four derived PAR-006 policies are approved. Published, measured-hardware, performance, and owner-DSP evidence remains incomplete. |
| TST-007 | in-progress | The map exactly covers 127 requirements and the canonical report/release verifier is authoritative. The F0 report is intentionally not release-ready. |
| TST-008 | in-progress | F0 proves repeat equality across three block partitions and records state/build provenance on Darwin arm64. The full sample-rate/reset/platform/architecture/host matrix remains open. |
| TST-009 | in-progress | The status model prevents listening from replacing numerical gates. No blinded, randomized, level-matched listening campaign has run. |

## Original pre-remediation code evidence

- At the planning baseline, the repository had no CMake/test target, test directory, offline renderer, fixtures, analyzers, reference manifest, hardware captures, or CI execution. Workstreams 02/03 and the F0 harness now supply the automated foundation; hardware captures remain absent.
- DSP and state logic are embedded in `MoogMiniAudioProcessor::processBlock`/APVTS, so algorithms cannot currently be exercised independently of a plug-in wrapper.
- The checked-in official manual supplies published constraints, but current code contains unsourced constants for waveform curvature, pulse width, contour time, filter modulation, mixer/noise scale, drive, feedback, glide, and cutoff limits.
- No provenance connects current panel behavior or presets to an identified reference instrument/recording chain.

## Prerequisites, ownership, and merge conflicts

Requires Workstream 02’s test/offline targets and 03’s registry/state. The skeleton (renderer, manifest schema, core analyzers, requirement reporter) lands during Foundation. DSP-specific analyzers/measurements complete with Workstreams 04–11.

This workstream exclusively owns fixture formats, analyzers, reference/threshold manifests, golden regeneration, evidence hashes, aggregate requirement reporting, and listening protocol. Feature owners own test cases and passing results.

Likely conflicts: CMake/test targets, engine interfaces, calibration loaders, CI artifacts, and any test-only accessors. Prefer public test contracts/taps defined in owner plans; never expose mutable internals only for a test.

## In scope

Offline deterministic rendering, stimuli/fixtures, analysis libraries, hardware capture procedure/data model, calibration/acceptance manifests, golden governance, aggregate reporting, listening protocol, and cross-platform reproducibility.

## Out of scope

Fabricating raw hardware data, buying/choosing a reference unit without product approval, approving redistribution licenses, replacing host validators, or treating another software emulation as ground truth.

## Test-system architecture

```text
Fixture (state/preset + MIDI + automation + optional input + render config)
  -> ModelDOfflineRenderer -> WAV + lifecycle/control/stage traces
  -> AnalyzerRegistry -> metrics.json + plots
  -> acceptance-v1.json (published/derived/measured gates)
  -> requirement-map.json -> requirements-report.html/json

Hardware capture -> raw immutable files + provenance.json
  -> same AnalyzerRegistry -> measured distributions/uncertainty
  -> reviewed CalibrationProfile + acceptance bands
```

Use WAV/BWF or lossless FLAC for audio, Standard MIDI File plus a lossless JSON event representation for exact sample offsets, canonical JSON for manifests, and SHA-256 for every input/output. Store large raw captures in the repository’s approved large-file/object store with immutable content IDs; keep manifests/fixtures in Git. Never normalize or trim raw files in place.

## Public fixture and analyzer contracts

```cpp
struct FixtureAnalysisRequest { StableId id; AnalyzerIdentity analyzer; MetricId metric; TypedInput input; EventWindow event; OptionalEndpoints endpoints; };
struct RenderFixture { StateDocument state; EventTimeline midi; AutomationTimeline automation; OptionalAudio input; RenderConfig config; vector<FixtureAnalysisRequest> analysisRequests; };
struct RenderResult { AudioFile main, phones; TraceFiles traces; ReproducibilityInfo build; };
class Analyzer { virtual Metrics analyze(const RenderResult&, const ReferenceSet&) const = 0; };
```

Analyzer IDs are stable (`pitch.cents.v1`, `osc.alias.v1`, `contour.shape.v1`, `filter.sweep.v1`, `nonlinear.thdn.v1`, etc.). Metrics contain units, algorithm version, window/FFT/estimator settings, uncertainty, and source hashes. Any algorithm change creates a new analyzer version and requires deliberate golden/threshold review.

## Acceptance manifest

Planned `tests/reference/acceptance-v1.json` structure:

```json
{
  "manifestVersion": 1,
  "status": "draft|approved",
  "published": [{"id":"...","value":0,"unit":"...","source":"...","sourceVersion":"...","page":"80"}],
  "derivedSoftware": [{"id":"...","value":0,"derivation":"...","review":"..."}],
  "measuredHardware": [{"id":"...","referenceStatus":"approved","referenceSet":"...","bandArtifact":"...","rawSha256":["..."],"instrument":"...","environment":"...","captureChain":"...","repetitionCount":3,"repetitionStatistic":"...","uncertaintyMethod":"...","uncertaintyValue":0,"approver":"...","approvalDate":"YYYY-MM-DD"}],
  "performance": [{"id":"...","targetSystem":"...","budgetBasis":"...","rationale":"...","reviewStatus":"approved","reviewer":"...","reviewDate":"YYYY-MM-DD"}]
}
```

Placeholder zeros above illustrate schema only and are invalid in an approved manifest unless zero is the sourced/derived value. Approval validation requires non-placeholder values, units, provenance, analyzer versions, source/raw hashes, uncertainty method, approver, and date.

Hard software gates include exact event sample indices, enum/ID mappings, state atomicity, finite output, zero allocation/locks, and deterministic structural traces. Published gates copy primary-source values. Measured hardware gates are generated from captures. Performance gates come from an approved target-system/product budget. Listening is stored elsewhere.

## Hardware measurement procedure

For each campaign:

1. Create a `ReferenceInstrument` record: manufacturer/model/version, serial or privacy-preserving unique ID, production era, modifications/service history, calibration/tuning procedure/results, warm-up, supply, room temperature/humidity, and operator.
2. Create `CaptureChain`: exact output/input used, cables/load/DI/attenuation, interface/preamp/ADC model/serial/firmware, gain, sample rate/bit depth, clocking, calibration certificate/date, noise floor, latency, and channel map.
3. Use analyzer-generated stimuli and a photographed/signed panel worksheet. Record performance/gate/MIDI/CV source and timing.
4. Capture repeated runs without overwriting. Hash raw files immediately. Record failed/aborted runs rather than deleting them from the log.
5. Analyze blind to implementation output. Estimate central curve/value, repeated-capture variation, known measurement uncertainty, and any unit-to-unit scope.
6. Generate an acceptance band from the approved repeated-capture distribution plus recorded measurement uncertainty using the metrology method named in the manifest. A reviewer approves the method/output; implementers do not hand-enter a wider band.
7. A single unit supports claims only for that calibration profile. A generalized “Model D population” profile requires a separately approved sampling/power design; do not generalize from one unit.

Waveforms, knob tapers, contours, filter/self-oscillation, gain/saturation, feedback, overload, output levels, glide, and drift each have an owner-plan procedure layered on this common record.

## Golden fixture governance

- Fixtures have purpose, owner requirement IDs, generator version, source hash, expected analyzers, and review status.
- Golden audio is not the hardware truth; it detects unintended code changes after an approved baseline.
- Regeneration requires a command that writes to a candidate directory, produces metric diffs and listening candidates, and never overwrites approved data in place.
- Approval requires feature-owner plus independent DSP/test reviewer, reason/change link, threshold impact, and updated hash manifest.
- A changed golden cannot make a failing acceptance metric pass by itself. Threshold changes follow acceptance-manifest review.

## Requirement trace contract

`requirement-map.json` has exactly one row per ID in [01-traceability-matrix.md](01-traceability-matrix.md), with owner plan, verification IDs, artifact paths, and status. CI rejects missing/duplicate/unknown IDs, a pass with no artifact, or `waived` status. Allowed statuses: `pass`, `fail`, `not-run`, `awaiting-approved-reference`; only `pass` satisfies release.

## Backward compatibility

Keep v0 state/preset fixtures forever. Analyzer/fixture schema readers support their own prior major version or provide a checked-in migration tool. Golden data identifies engine/calibration/state versions. Never delete a fixture solely because the corrected DSP sounds different; reclassify it as legacy compatibility or archive with manifest history.

## Real-time constraints

Offline tools may allocate and use high precision. Test hooks used by the real-time engine are fixed/bounded and compile out or remain no-op without affecting Release audio. Measurement/logging never runs in the callback except fixed counters/taps approved by Workstream 11.

## Edge cases and failure modes

Missing raw object, hash mismatch, analyzer version mismatch, stale threshold, clock drift, clipped capture, wrong panel state, calibration expired, incomplete repetition, future schema, cross-platform floating differences, and listening identity leak. Any integrity/provenance failure invalidates the affected gate; it does not silently fall back to subjective review.

## Implementation sequence

1. Add test/offline targets, canonical fixture/manifest schemas, hash utility, and sentinel test.
2. Add event/state/render reproducibility plus core pitch/waveform/contour/filter/gain analyzers.
3. Add requirement-map validator and aggregate report seeded from the traceability matrix.
4. Capture/version legacy baselines and published manual constraints.
5. Run approved hardware campaigns and generate calibration/acceptance candidates.
6. Review/freeze acceptance v1 and golden v1; integrate host/RT/performance evidence.
7. Add blinded listening runner/report and full release report.

## Automated tests and measurable gates

- Same fixture/build/profile renders deterministically under its declared platform policy; differences outside policy fail with localized metrics.
- Every analyzer passes synthetic calibration signals with analytically known results under a documented derived-software error bound.
- Hash corruption, missing provenance, placeholders, unapproved status, stale analyzer version, and unauthorized golden overwrite are rejected.
- Requirement-map validator reports exactly the full matrix ID set, one owner each, at least one verification/artifact each, and no pass without evidence.
- v0/v2, sample-rate, block-partition, reset/reprepare, platform/architecture, and deterministic-seed matrices are represented.
- Candidate golden/threshold updates generate reviewable diffs and cannot modify approved data in place.
- Listening randomization/level matching/blinding logs are complete; numerical failure remains failure regardless of preference score.

## Manual, host, listening, and hardware validation

Audit a campaign from panel worksheet through raw hash, analyzer, band, calibration profile, implementation render, and requirement report. Independently reproduce a fixture on a clean machine. Listening sessions use randomized hidden labels, calibrated level matching, repeated trials, declared participants/monitoring/environment, confidence intervals, and predeclared questions; they report similarity/preferences, never “identical.”

## Definition of done

- [ ] TST-001 through TST-009 pass.
- [x] F0 renderer/analyzer/fixture/manifest/map/report/release-enforcement skeleton is accepted.
- [x] Every matrix ID has an executable verification mapping and evidence schema; current non-pass states remain explicit.
- [ ] Acceptance v1 is approved with no invented/missing release gate.
- [ ] Hardware captures/calibrations have full provenance/integrity.
- [ ] Golden/listening governance is exercised, not only documented.

## Completion-report evidence

The F0 subset is recorded in the [foundation evidence](evidence/workstream-12/f0-harness-foundation.md). Final Workstream 12 completion must additionally include schema files/examples; renderer reproducibility hashes; analyzer synthetic-validation results; requirement coverage report; published-source extract/page map; raw/reference/calibration/uncertainty manifests; approved acceptance/golden review records; cross-platform diffs; listening protocol/log/report; and commands to reproduce all artifacts.

## Second senior-review remediation

Implementation commit `5d7e6213f6163fad62b74d6ec7688b81a8eccd0c`
binds every render metric to a versioned fixture-owned request. Requests declare
the exact analyzer/metric, audio tap and deinterleaved channel or registry-backed
control parameter/domain, event origin/window, and required endpoints. Control
analysis reconstructs the full sample-domain signal from exact trace indices;
sparse events and interleaved channels are never passed off as analyzer samples.

Fixture IDs are conservative portable filename components. Index and fixture
IDs, ordered requirement ownership, and bytes must match exactly, and candidate
output is proven to be one canonical direct child of `renders` before creation.
Render-kind gate evidence reloads the checked index, rerenders every block
pattern, rederives the named request, and compares the full multi-record metric
artifact. The live-registry evidence path and globally draft manifest remain
unchanged.

Git source identity and all reference-harness targets are now test-only. The
artifact contract creates a source archive without `.git`: distribution
configuration with tests and validators off succeeds against exact local JUCE,
while tests-on configuration retains the stable mandatory-Git failure. No
production `Source/` file or requirement status changed.

## Closure-review endpoint and provenance remediation

Implementation commit `0f223df2bc54a6039e352cb457095da0650aa7fe`
makes fixture loading authoritative for control-analysis endpoints. The loader
restores the declared v2 state, applies prior same-key automation in exact
`(sample,sequence)` order, requires same-key automation at the event origin,
and selects the final same-sample target. Endpoint comparison uses the live
parameter's float/domain representation, including ordinary values such as
`0.1`, and dense analysis consumes only the derived endpoints.

Every foundation render fixture now requires a meaningful purpose and the
supported typed readiness status `foundation-reviewed`. Both fields are bound
through render and metric provenance and fixture hashes. Compiler ID and
version are likewise configured by CMake and bound through reproducibility,
serialization, candidate validation, and authoritative replay. Missing,
placeholder, unsupported, or forged metadata rejects. These readiness and
reproducibility fields do not approve the globally draft acceptance manifest
or change any requirement, hardware, host, listening, or release status.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), especially pp. 43–51 and 80–81.
- Audio Engineering Society, [AES17 standard overview](https://www.aes.org/publications/standards/search.cfm?docID=21) (use the licensed standard text where applicable).
- EBU [Tech 3285: BWF specification](https://tech.ebu.ch/docs/tech/tech3285.pdf).
- ITU-R [BS.1116 methods for subjective assessment](https://www.itu.int/rec/R-REC-BS.1116) and [BS.1534 MUSHRA](https://www.itu.int/rec/R-REC-BS.1534).
