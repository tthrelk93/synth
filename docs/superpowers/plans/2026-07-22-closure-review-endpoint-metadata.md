# Closure-Review Endpoint and Metadata Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the bounded endpoint, fixture-governance, and compiler-reproducibility findings without changing production DSP or acceptance status.

**Architecture:** Render-fixture loading becomes the authority for effective control endpoints, deriving them from restored v2 state plus ordered automation. Fixture governance and compiler identity flow through the same immutable fixture/render/metric artifacts already replayed by acceptance. Focused contracts drive three independent RED/GREEN slices before exact-head closure.

**Tech Stack:** C++20, JUCE 8.0.10, CMake 3.24+, CTest, canonical JSON, SHA-256.

## Global Constraints

- Work only in `/Users/agentt/.openclaw/workspace/Developer/synth/.worktrees/workstream-12-f0` from base `45dd246a1b578fbc64962476f95e24ab07563404`.
- Use genuine RED/GREEN TDD for every behavior change.
- Do not modify `Source/`, registry/state/contour semantics, acceptance approvals, or requirement statuses.
- Preserve all earlier identity, containment, typed-request, authoritative-replay, and no-`.git` fixes.
- Final verification uses tests and validators ON, warnings-as-errors, distribution identity validation, and exact JUCE `3af3ce009f6a02f6fa651008fffb5b41743a9fab`.

---

### Task 1: Exact control endpoints

**Files:**
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceData.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`

**Interfaces:**
- Consumes: restored `modelDState` v2 bytes, `ParameterRegistry::descriptor`, and automation ordered by `(sample, sequence)`.
- Produces: validated `FixtureAnalysisRequest::start` and `target` containing the derived float/domain endpoints used by dense control analysis.

- [x] **Step 1: Write failing endpoint contracts**

  Add fixture-loader cases for a wrong but in-range declared start, unrelated-event anchoring, multiple same-sample same-key events whose final sequence differs from JSON order, and valid `0.1`. Add a dense-analysis assertion whose declared start differs from the effective restored/prior-automation start.

- [x] **Step 2: Verify RED**

  Run `ModelDReferenceRendererContract` and `ModelDReferenceAnalyzerContract`. Expect the wrong start/unrelated anchor/final-sequence cases to load incorrectly and dense analysis to trust the declaration.

- [x] **Step 3: Implement minimal endpoint authority**

  Restore the fixture state through `MoogMiniAudioProcessor`, read the requested prepared parameter's normalized value, apply only earlier same-key automation in sorted `(sample,sequence)` order, select the final same-sample same-key target, compare declarations after conversion to the processor's float/domain representation with scale-aware float tolerance, then replace request endpoints with the derived values. Dense reconstruction reads only those derived request fields.

- [x] **Step 4: Verify GREEN**

  Rebuild and rerun both focused contracts; expect all endpoint and dense-input assertions to pass.

### Task 2: Fixture governance metadata

**Files:**
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/ReferenceData.cpp`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`
- Modify: `Tools/ReferenceHarness/AnalyzerRegistry.cpp`
- Modify: `Tools/ReferenceHarness/Acceptance.cpp`
- Modify: `Tests/reference/fixtures/native-v2-foundation.json`
- Modify: `Tests/reference/fixtures/migrated-v2-foundation.json`
- Modify: `Tests/reference/fixtures/legacy-contour-foundation.json`
- Modify: `Tests/reference/fixture-index-v1.json`

**Interfaces:**
- Consumes: required JSON strings `purpose` and `reviewStatus`.
- Produces: typed `RenderFixture` governance fields serialized into render manifests and render metric provenance, and checked during bound-candidate and authoritative replay equality.

- [x] **Step 1: Write failing metadata contracts**

  Require missing/empty/placeholder purpose and missing/unknown review status to reject. Require changed purpose/status in render manifest or metric provenance to invalidate evidence without changing any gate status.

- [x] **Step 2: Verify RED**

  Run renderer and requirement contracts; expect missing fields to load and forged governance to escape comparison.

- [x] **Step 3: Implement and seed metadata**

  Accept only non-placeholder purpose and supported status `foundation-reviewed`; add truthful purpose strings to all three fixtures, serialize the fields in render/metric provenance, and require exact equality during candidate binding and authoritative replay. Rehash the three fixtures and index.

- [x] **Step 4: Verify GREEN**

  Rebuild and rerun renderer and requirement contracts; expect strict governance and unchanged draft/open acceptance results.

### Task 3: Compiler reproducibility identity

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`
- Modify: `Tools/ReferenceHarness/AnalyzerRegistry.cpp`
- Modify: `Tools/ReferenceHarness/Acceptance.cpp`

**Interfaces:**
- Consumes: `CMAKE_CXX_COMPILER_ID` and `CMAKE_CXX_COMPILER_VERSION`.
- Produces: `SYNTH_CONFIGURED_COMPILER_ID`, `SYNTH_CONFIGURED_COMPILER_VERSION`, and exact `ReproducibilityInfo::compilerId/compilerVersion` values in render and metric artifacts.

- [x] **Step 1: Write failing compiler identity contracts**

  Assert configured compiler fields are present in immutable render results, `render.json`, and render metric provenance. Mutate each field in candidate results/evidence and require stable rejection.

- [x] **Step 2: Verify RED**

  Build the focused reference target. Expect compilation failures for missing typed fields or assertions proving absent serialization.

- [x] **Step 3: Implement compiler propagation**

  Add both CMake definitions; populate render reproducibility; serialize them in render and metric JSON; include them in result equality, bound-candidate validation, and authoritative evidence comparison.

- [x] **Step 4: Verify GREEN**

  Reconfigure, rebuild, and run all four focused reference contracts; expect 4/4 pass.

### Task 4: Closeout and successor archive

**Files:**
- Modify: `docs/remediation/12-hardware-reference-regression-system.md`
- Modify: `docs/remediation/evidence/workstream-12/f0-harness-foundation.md`
- Modify: `docs/remediation/17-implementation-handoff.md`
- Modify: `docs/superpowers/plans/2026-07-22-workstream-12-f0-harness.md`
- Modify: `.superpowers/sdd/whole-branch-remediation-report.md` (ignored)
- Replace outside Git: `/Users/agentt/.openclaw/workspace/Developer/synth/Agent-08-Workstream-12-F0-Complete-Context.zip`

**Interfaces:**
- Consumes: committed implementation, refreshed hashes, exact-head candidates, and final CTest/CLI output.
- Produces: append-only durable closure history and a one-root verified successor ZIP.

- [x] **Step 1: Commit implementation and synchronized docs**

  Run focused verification before each commit; preserve prior handoff history and record no acceptance/status promotion.

- [x] **Step 2: Run exact-head verification**

  Configure a fresh path-with-spaces Release tree with validators/tests/warnings/distribution guards on; build all targets; pass 21/21 serial CTest and unit/state/DSP/MIDI/realtime/host/artifact labels; run smoke, validate, two 20-file candidates, expected release exit 3, tamper probes, 19 hash checks, 127-row map, links, history, `Source/` diff, and clean status.

- [x] **Step 3: Rebuild and verify package**

  Build the fixed ZIP from the exact closeout commit, exclude `.git`, `.DS_Store`, builds, credentials, and prior archives, verify one safe root, no duplicates/symlinks, full manifest coverage, committed/candidate equality, packaged release behavior, and extracted repository-context tests-off configuration with exact local JUCE.

- [x] **Step 4: Append ignored execution report**

  Record RED/GREEN chronology, commit hashes, exact commands/results, candidate/report hashes, ZIP size/hash/counts, and remaining open non-F0 obligations.

## Self-Review

- Spec coverage: Tasks 1–3 map exactly to endpoint authority, fixture governance, and compiler identity; Task 4 covers every required repeat-closure check.
- Placeholder scan: runtime paths are explicit bounded verification choices; no implementation behavior is deferred.
- Type consistency: derived endpoints remain `optional<double>` but store exact float/domain values; governance lives on `RenderFixture` and `MetricProvenance`; compiler identity lives only in `ReproducibilityInfo` and its canonical serializers.
- Scope check: no production DSP, parameter/state schema, approval, hardware, host, listening, or requirement-status change is included.
