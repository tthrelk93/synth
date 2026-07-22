# Workstream 12 F0 Harness Skeleton Design

**Date:** 2026-07-22

**Status:** Approved in conversation

**Scope:** Complete the Workstream 12 Foundation-phase harness skeleton, beginning
with PAR-006, then hand off to Workstream 04. Workstream 12 remains active through
F4 for DSP-specific analyzers, hardware campaigns, approved hardware bands,
performance evidence, and listening governance.

## Outcome

The repository will gain a modular, deterministic, test-only reference harness
that consumes the frozen Workstream 03 registry and state contracts. It will:

- validate versioned render fixtures and their SHA-256 inputs;
- render exact-sample state, MIDI, automation, and optional-input scenarios;
- run versioned analyzers, beginning with signal statistics and PAR-006
  step/click analysis;
- evaluate classified gates from a machine-readable acceptance manifest;
- map every traceability requirement to honest executable evidence status; and
- emit canonical candidate artifacts and aggregate reports without changing
  approved fixtures in place.

The F0 skeleton is complete when these seams are implemented, calibrated on
synthetic signals, exercised against the frozen Workstream 03 artifacts, and
integrated with serial CTest. It does not require later DSP or hardware gates to
pass. Missing later evidence remains visible as `not-run` or
`awaiting-approved-reference`.

## Fixed Constraints

The work must preserve:

- JUCE 8.0.10 at `3af3ce009f6a02f6fa651008fffb5b41743a9fab`;
- TTH Audio / TTH Model One identity and existing target names;
- the one-instrument, no-effect wrapper and current bus contract;
- the exact ordered 48-key parameter registry and all legacy IDs;
- `modelDState` v2, v0 migration, safe extensions, and atomic rollback;
- canonical and `legacyCrossedContours` behavior, including explicit conversion;
- the prepared parameter snapshot and typed editor binding boundary;
- all nine immutable Workstream 03 fixtures; and
- all seven required CTest labels.

No DSP equation, parameter mapping, host automation lane, state meaning, preset
schema, UI layout, release status, or hardware tolerance may be changed or
invented as part of the F0 harness.

## Chosen Architecture

Add a test-only C++ target, `ModelDReferenceHarness`, with small components under
`Tools/ReferenceHarness/`. The library links the plug-in/core targets needed to
instantiate the processor, but production rendering does not depend on the
harness.

### Components

1. **Canonical JSON and hashing**
   - Parse versioned JSON with stable validation diagnostics.
   - Write canonical JSON with deterministic property ordering and formatting.
   - Compute SHA-256 for every declared regular-file input and output.
   - Resolve only bounded, repository-relative paths and reject unsafe or
     symlink-escaped inputs.

2. **Fixture loader**
   - Load `model-d.render-fixture.v1` documents.
   - Validate the state source, render configuration, exact-sample events,
     optional generated/file input, requested analyzer IDs, and owner
     requirement IDs.
   - Resolve parameter IDs exclusively through `ParameterRegistry`; no second
     normalization, ID, or smoothing table is allowed.

3. **Offline renderer**
   - Restore a validated state into a new processor before `prepareToPlay`.
   - Split declared host blocks at every MIDI or automation event boundary.
   - Apply each event at sample zero of the resulting sub-block, preserving
     stable same-sample input order.
   - Feed deterministic silence, DC, sine, or hash-verified audio input.
   - Return main/phones audio, parameter/control traces, event traces, and
     reproducibility metadata.

4. **Analyzer registry**
   - Register stable analyzer IDs and versions.
   - F0 supplies deterministic signal statistics, control-step response, and
     audio-click/discontinuity analyzers.
   - Each analyzer reports its units, settings, input hashes, result, and any
     uncertainty or floating-point policy.
   - Later workstreams add pitch, oscillator, contour, filter, nonlinear,
     latency, and performance analyzers without changing the runner contract.

5. **Acceptance manifest**
   - Load and validate `model-d.acceptance.v1`.
   - Classify every gate as hard software, published, derived software,
     measured hardware, or performance. Listening results are not acceptance
     gates.
   - Evaluate only gates whose required implementation and provenance exist.
   - Preserve open statuses rather than substituting a different evidence
     class.

6. **Requirement reporter**
   - Load the checked-in map of all 127 traceability IDs.
   - Reject missing, duplicate, or unknown IDs; duplicate owners; `waived`; and
     `pass` without an artifact.
   - Emit canonical JSON with `pass`, `fail`, `not-run`, or
     `awaiting-approved-reference` for every row.

7. **Command-line front end**
   - Keep `ModelDOfflineRenderer` as the thin executable entry point.
   - Retain its no-argument foundation smoke behavior for compatibility.
   - Add explicit modes for manifest validation, rendering, report generation,
     and release enforcement.
   - Report generation succeeds when execution succeeds even if honest gates
     remain open. Release enforcement fails unless every required gate passes.

### Core interfaces

The harness boundary uses plain result types rather than exposing processor
internals:

```cpp
struct RenderFixture;
struct RenderResult {
    AudioArtifact main;
    AudioArtifact phones;
    TraceArtifacts traces;
    ReproducibilityInfo reproducibility;
};

struct MetricResult;
class Analyzer {
public:
    virtual ~Analyzer() = default;
    virtual AnalyzerIdentity identity() const = 0;
    virtual MetricResult analyze(const RenderResult&,
                                 const AnalysisRequest&) const = 0;
};

struct GateResult;
struct RequirementReport;
```

`FixtureLoader` returns either one fully validated immutable fixture or a
stable diagnostic; it never returns a partially usable object. `OfflineRenderer`
accepts only a validated fixture. Analyzers consume immutable render artifacts,
and manifest evaluation consumes immutable metric results. This keeps parsing,
rendering, measurement, policy, and reporting independently testable.

The exact initial command surface is:

```text
ModelDOfflineRenderer
ModelDOfflineRenderer validate --fixture-index FIXTURE_INDEX_PATH --acceptance ACCEPTANCE_PATH --requirements REQUIREMENTS_PATH
ModelDOfflineRenderer run --fixture-index FIXTURE_INDEX_PATH --acceptance ACCEPTANCE_PATH --requirements REQUIREMENTS_PATH --output NEW_CANDIDATE_DIRECTORY
ModelDOfflineRenderer verify-release --report REQUIREMENTS_REPORT_PATH
```

The no-argument command preserves the current 480-sample foundation smoke.
`validate` performs no rendering. `run` validates, renders every indexed F0
fixture, analyzes it, and writes candidate artifacts plus the aggregate report.
`verify-release` performs no regeneration and exits nonzero if any required row
is not `pass`.

## Data Flow

```text
fixture index + render fixture + frozen state/registry artifacts
                         |
                         v
               validate versions/hashes
                         |
                         v
              exact-event offline renderer
                         |
                         v
             audio + control/event traces
                         |
                         v
          versioned analyzers -> metrics.json
                         |
                         v
              acceptance-v1 gate results
                         |
                         v
        requirement-map -> requirements-report.json
```

Integrity or schema failures stop before evaluation. An ordinary numerical gate
failure is recorded as `fail`; absent owner implementation is `not-run`; absent
approved hardware evidence is `awaiting-approved-reference`.

## Render Fixture Contract

Each fixture contains:

- `schema`, stable fixture `id`, purpose, owner requirement IDs, and review
  status;
- state path, state schema/version expectation, and SHA-256;
- sample rate, total sample count, deterministic seed, and one or more declared
  host block patterns;
- stable-ordered MIDI events represented as exact bytes at exact sample indices;
- automation events identified by frozen parameter ID with an explicit
  normalized value at an exact sample index;
- optional input as silence, DC, sine, or a repository-relative lossless file
  with SHA-256;
- requested analyzer IDs and required render/control taps; and
- expected infrastructure disposition, never an expected numerical result used
  to excuse an analyzer failure.

Events must be within the render interval. Duplicate event sequence numbers,
unknown parameter IDs, non-finite values, invalid MIDI bytes, and inconsistent
same-sample ordering are validation errors.

F0 fixtures cover native v2 state, migrated v2 state, and the legacy contour
compatibility trace. Preset-document input remains unsupported until Workstream
13 defines preset schema v2; the fixture version reserves a typed source field
and rejects unsupported source kinds explicitly.

## Frozen Artifact Index

`Tests/reference/fixture-index-v1.json` will list all nine immutable Workstream
03 artifacts and their current SHA-256 values:

- legacy parameter inventory;
- parameter registry v2;
- parameter snapshot v2;
- contour routing/conversion trace;
- two legacy v0 state fixtures;
- native default state v2; and
- two migrated state v2 fixtures.

The index validator compares file bytes, hashes, registry descriptor order,
state root/version, and contour contract markers before a render can start.
This creates the F0 seam without copying or regenerating the frozen fixtures.

## Acceptance Manifest Contract

`Tests/reference/acceptance-v1.json` remains globally `draft` during F0 because
later DSP, hardware, performance, and host evidence is absent. Individual
derived-software policies may be approved within the draft manifest.

Each gate declares:

- stable gate ID and owner requirement IDs;
- evidence classification;
- analyzer ID/version and metric name;
- value or lower/upper bound with unit;
- derivation, primary source, measurement-band artifact, or performance-budget
  provenance appropriate to the classification;
- applicability, required fixture/tap, review status, reviewer basis, and date;
- artifact path/hash when evaluated; and
- honest current status.

Zero is valid only when the manifest states why zero is the sourced or derived
value. A bare zero in an approved entry is rejected as a placeholder.
Published entries require source/version/page metadata. Measured-hardware
entries require approved reference-set, raw hashes, analyzer version,
repetition statistics, uncertainty method, approver, and date. Performance
entries require an approved target-system budget. No evidence class may waive
another.

## PAR-006 Smoothing and Step/Click Policy

Seven fixture templates, one for each declared smoothing class, expand from the
registry at runtime. This covers every descriptor without introducing another
hard-coded parameter list.

### `none`

- Applicable switches and selectors change discretely at the event sample.
- The control trace contains no interpolated values.
- Event index, finite output, and valid enum/range behavior remain hard gates.
- Audio-path click suppression for output switches is owned by the applicable
  later signal-path workstream and remains `not-run` until that path exists.

### `gainControl`

- Software safety policy: 5 ms linear-amplitude ramp.
- Required metrics: first change at the event sample, monotonic travel, no
  overshoot, finite samples, settling within
  `ceil(0.005 * sampleRate) + 1` samples, and per-sample movement no greater
  than the ideal ramp increment plus the floating-point allowance.

### `control`

- Software safety policy: 10 ms ramp in the owner-declared control domain.
- The same structural metrics apply with
  `ceil(0.010 * sampleRate) + 1` settling.
- The manifest records the required domain; an absent domain-specific tap is
  `not-run`, not silently evaluated in normalized or linear-Hz space.

### Dedicated classes

- `dedicatedPitch`, `dedicatedCutoff`, `dedicatedGlide`, and `contourStage`
  receive fixture, analyzer, metric, and owner-workstream contracts in F0.
- They do not receive invented generic ramps. Their results remain `not-run`
  until Workstreams 04, 07, and 08 implement and expose the specified
  semitone/octave/stage trajectories.
- Hardware-dependent taper and time-shape gates remain
  `awaiting-approved-reference` until approved campaigns exist.

The floating allowance is recorded as a formula based on the metric precision,
travel magnitude, and a small fixed multiple of machine epsilon. The analyzer
must publish the evaluated allowance in each metric artifact. Sample tolerance
is at most one sample beyond the exact ceiling above.

`control.step.v1` analyzes generated control trajectories. `audio.click.v1`
analyzes first differences and local peak/RMS behavior on synthetic and routed
DC/sine fixtures. Both analyzers must pass synthetic analytic calibration
before processor results are accepted. A missing stable production tap or
route yields `not-run`, not a fabricated pass.

These durations and bounds are approved software safety policy from the design
review. They are not Model D hardware claims.

## Requirement Map and Status Semantics

`Tests/reference/requirement-map.json` contains all 127 matrix IDs exactly once,
with one owner, declared verification methods, artifact paths, and current
status. The F0 implementation may automate only a subset, but no requirement
is omitted.

Allowed statuses are:

- `pass`: every required gate in current scope has durable passing artifacts;
- `fail`: an executed acceptance gate failed;
- `not-run`: implementation, route, tap, external host, or execution is absent;
- `awaiting-approved-reference`: the executable gate exists but approved
  hardware/reference evidence does not.

`waived` is forbidden. Listening is supplemental and cannot change a numerical
status. Report generation and release enforcement are separate commands so CI
can prove the harness while the product truthfully remains pre-release.

## Determinism and Output Governance

- A render records source commit, build type, compiler/platform/architecture,
  JUCE revision, fixture hashes, registry/state versions, sample rate, block
  pattern, seed, analyzer versions, and output hashes.
- Candidate output directories must not already exist. The harness never
  overwrites an approved fixture, metric, golden, or report in place.
- JSON output is canonical and byte-stable for repeated runs with the same
  declared platform policy.
- Audio uses a deterministic lossless format and explicit channel/sample
  format. Cross-platform byte identity is claimed only where demonstrated;
  otherwise the report localizes differences through metrics and records the
  platform policy.
- Offline code may allocate. No measurement, formatting, file I/O, or mutable
  test hook is added to the audio callback.

## Failure Behavior

The command fails before rendering for:

- unknown or future unsupported schema versions;
- duplicate fixture, gate, analyzer, event, or requirement IDs;
- absolute, drive/UNC, backslash-ambiguous, traversal, or symlink-escaped paths;
- missing regular files or SHA-256 mismatches;
- invalid state root/version or unknown frozen parameter IDs;
- out-of-range/non-finite event data or unstable event ordering;
- unregistered or stale analyzer versions;
- unclassified gates, missing required provenance, placeholder approved values;
- missing/duplicate traceability rows, unknown owners, `waived`; or
- a `pass` result with no durable artifact and hash.

Numerical acceptance failures do not masquerade as infrastructure errors. The
report is still written to the new candidate directory with status `fail`.

## CTest and Verification Design

Add focused labeled tests for:

1. **Reference manifest contract** (`unit;state`)
   - schema/version/provenance/status validation;
   - frozen artifact index and registry-derived smoothing coverage; and
   - all negative corruption, placeholder, ID, path, and hash cases.

2. **Offline fixture contract** (`dsp;state`)
   - native/migrated/legacy inputs;
   - exact event splitting and stable same-sample ordering;
   - block-pattern repeatability; and
   - deterministic candidate output/hashes under the declared local policy.

3. **Analyzer calibration contract** (`unit;dsp`)
   - analytically known constant, impulse, step, ramp, overshoot, NaN, click,
     and settling signals;
   - exact analyzer IDs/settings/units; and
   - correct pass/fail/not-run classification.

4. **Requirement report contract** (`artifact;state`)
   - exact equality with all 127 matrix IDs;
   - unique owner and verification/artifact validation;
   - deterministic report bytes; and
   - release enforcement failure while any required row remains open.

Implementation follows test-driven development: each contract is first added
and observed failing for the missing surface, then production/test-tool code is
added to make the focused contract pass. Final verification runs serially in a
fresh Release tree and includes every CTest label, capture/repeat comparisons,
fixture hashes, source guards, `git diff --check`, and documentation link audit.

## Planning and Evidence Updates

The F0 closeout will:

- mark the Workstream 12 F0 skeleton complete while leaving TST-001–009 statuses
  honest for their remaining full-workstream acceptance;
- update PAR-006 from a wholly unimplemented measurement obligation to a
  defined/executable policy whose owner-DSP cases remain open;
- retain PAR-002/004/007 and BLD-006/011/012 as non-passing pre-release gates;
- synchronize the roadmap, traceability matrix, Workstream 03 and 12 plans,
  canonical handoff, and append-only history;
- add a durable Workstream 12 F0 evidence report with commands, hashes, and
  exact open-gate results; and
- commit all tracked implementation/planning/evidence changes before packaging.

## Successor Package and Next Action

Build one untracked, one-root successor ZIP from the exact final commit. Include
`START-HERE.md`, current `HANDOFF.md`, the complete planning/evidence suite,
required primary references, exact repository context, verification summary,
and `MANIFEST.sha256`. Exclude Git metadata, build/cache trees, tools,
credentials, settings, recovery content, predecessor archives, and unrelated
workspace files.

Verify ZIP integrity, safe paths, absence of symlinks, fresh extraction,
internal hashes, source equality, and final-commit equality. Report the
clickable absolute path, size, outer SHA-256, verification result, and exact
next action.

After the F0 harness seam is accepted, hand off to Workstream 04. Workstream 04
must implement the semitone-domain pitch/glide contracts and populate their
registered fixtures/analyzers without changing the frozen registry or harness
schemas. Workstream 12 then continues alongside Workstreams 04–11 toward the
full F4 acceptance system.

## Explicit Non-Goals

- Completing all TST-001–009 final release acceptance in F0.
- Inventing hardware captures, calibration profiles, tolerances, or approvers.
- Implementing pitch, glide, contour, filter, oscillator, or signal-path DSP.
- Changing state/preset/UI/host contracts or rewriting automation lanes.
- Publishing a factory library, recreation content, release, distribution,
  notarization, store submission, host purchase, or account mutation.
