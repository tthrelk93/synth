# Ultimate-Review Authority Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make control metrics event-local, bind authoritative evidence to a
clean current build/source identity, validate every frozen registry descriptor
field, and synchronize the exact-head closure package.

**Architecture:** Preserve absolute `control.step.v1` sample coordinates while
restricting dense reconstruction and response calculations to the requested
event. Generate source identity at every test-harness build, propagate it
through all evidence, and compare it to a fresh runtime Git inspection before
authoritative commands. Extend the existing pre-gate semantic registry seam to
compare the full live descriptor contract.

**Tech Stack:** C++20, JUCE 8.0.10, CMake 3.24+, Git, CTest, canonical JSON,
SHA-256.

## Global Constraints

- Work from approved design commit
  `8fbf68e424a1de719d84ec9488694a0617dcc827` in the Workstream 12 F0
  remediation worktree.
- Use genuine RED/GREEN TDD for every behavior change.
- Do not modify `Source/`, registry/state/contour semantics, acceptance
  approvals, or requirement statuses.
- Keep all source-identity and reference-harness targets inside
  `SYNTH_BUILD_TESTS=ON`.
- Preserve exact `control.step.v1` version and absolute sample semantics.
- Finish against exact JUCE commit
  `3af3ce009f6a02f6fa651008fffb5b41743a9fab` with tests, validators,
  warnings-as-errors, and distribution identity validation enabled.

---

### Task 1: Event-local control metrics

**Files:**
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/AnalyzerRegistry.cpp`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`

**Interfaces:**
- Consumes: `AnalysisRequest::eventSample`, effective request start/target, and
  ordered `RenderResult::controlTrace` points.
- Produces: absolute-sample `control.step.v1` metrics whose response window
  starts at the declared event and whose event movement includes `event - 1`.

- [x] **Step 1: Add the failing multi-prior-event contract**

  Construct one dense request with prior same-key values `0.9`, `0.1`, and
  final pre-event `0.4`, followed at the event by `0.6` and then target `0.8`.
  Assert exact first change at the event, settling at the next sample,
  monotonic `1`, overshoot `0`, and maximum movement `0.2`.

- [x] **Step 2: Run RED**

  Run the focused analyzer contract from the existing Release build:

  ```bash
  ctest --test-dir <release-build> -C Release -R '^ModelDReferenceAnalyzerContract$' --output-on-failure -j1
  ```

  Expected: FAIL because pre-event movement contributes to
  `maximum-per-sample-movement`.

- [x] **Step 3: Implement the minimum event-local fix**

  In `analyzeControlStep`, begin movement iteration at
  `max<uint64_t>(1, eventSample)`. In dense reconstruction, initialize from the
  effective request start, skip trace points before `eventSample`, and replay
  same-key points from the event onward in trace order.

- [x] **Step 4: Run GREEN**

  Rebuild `ModelDReferenceTests`, rerun analyzer and renderer contracts, and
  require both to pass with the exact assertions.

### Task 2: Build-generated and runtime source identity

**Files:**
- Create: `cmake/GenerateSourceIdentity.cmake`
- Create: `cmake/SourceIdentity.generated.h.in`
- Create: `Tools/ReferenceHarness/SourceIdentity.h`
- Create: `Tools/ReferenceHarness/SourceIdentity.cpp`
- Modify: `CMakeLists.txt`
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`

**Interfaces:**
- Produces:
  `SourceIdentity { std::string commit, tree, content; bool dirty; }`,
  `builtSourceIdentity()`,
  `inspectSourceIdentity(const juce::File&, std::string_view)`, and
  `validateAuthoritativeSourceIdentity(const SourceIdentity&,
  const SourceIdentity&)`.
- Consumes: exact Git executable, source root, `HEAD`, `HEAD^{tree}`,
  `git diff --binary HEAD --`, and porcelain working-tree status.

- [x] **Step 1: Add failing identity contracts**

  Add compile-time assertions for `ReproducibilityInfo::sourceTree`,
  `sourceContent`, and `sourceDirty`. In isolated temporary Git repositories,
  commit a tracked file and assert: clean identity passes; an identity captured
  while dirty still rejects after revert; a clean identity rejects a current
  tracked modification; and a clean identity rejects after a new commit.
  Assert `run` and `verify-release` call the clean/current authority before
  authoritative work.

- [x] **Step 2: Run RED**

  Build `ModelDReferenceTests`.

  Expected: compilation fails for the missing fields and source-identity API.

- [x] **Step 3: Add the always-run build generator**

  The script must compute canonical values and configure the generated header
  only if bytes change. Add an always-run `ModelDGenerateSourceIdentity` target,
  generated-header byproduct, harness dependency, generated include directory,
  and `SourceIdentity.cpp` only inside `SYNTH_BUILD_TESTS`.

- [x] **Step 4: Implement Git inspection and pure authority validation**

  Execute Git with `juce::ChildProcess` argument arrays, require successful
  commands and correctly shaped IDs, compute the same SHA-256 content identity
  as CMake, and return stable diagnostics for built-dirty, current-dirty,
  stale/mismatched, and inspection failures.

- [x] **Step 5: Gate authoritative commands**

  Call the runtime validator in CLI `run` after argument validation but before
  source validation/output creation, and in `verify-release` before replay.
  Keep no-argument smoke and `validate` available while dirty.

- [x] **Step 6: Run focused GREEN where the dirty-build contract permits**

  Rebuild and run analyzer, renderer, and isolated source-identity assertions.
  The full CLI success path is deferred until the implementation is committed
  and rebuilt clean, because a binary built from dirty tracked bytes must
  reject by design.

### Task 3: Propagate complete source identity through evidence

**Files:**
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/OfflineRenderer.cpp`
- Modify: `Tools/ReferenceHarness/AnalyzerRegistry.cpp`
- Modify: `Tools/ReferenceHarness/Acceptance.cpp`
- Modify: `Tools/ReferenceHarness/RequirementReporter.h`
- Modify: `Tools/ReferenceHarness/RequirementReporter.cpp`

**Interfaces:**
- Consumes: `builtSourceIdentity()`.
- Produces: canonical `sourceCommit`, `sourceTree`, `sourceContent`, and
  `sourceDirty` fields in render results, render manifests, render metrics,
  registry metrics, and requirement reports.

- [x] **Step 1: Add failing serialization and forgery contracts**

  Require every evidence kind to contain all four fields. Clear or forge commit,
  tree, content, and dirty values independently in render results, render
  manifests, metrics, registry gate evidence, and reports; require stable
  rejection.

- [x] **Step 2: Run RED**

  Build the focused reference target and run renderer/requirement tests.

  Expected: missing serialization assertions fail and forged new fields are not
  rejected.

- [x] **Step 3: Implement canonical propagation and comparison**

  Populate evidence from `builtSourceIdentity()`, serialize all four fields,
  parse an exact JSON boolean for `sourceDirty`, and compare the full identity
  across block patterns, candidate binding, live-registry gate evidence,
  authoritative rerender, and report verification.

- [x] **Step 4: Run focused GREEN permitted by current identity**

  Rebuild and run non-authoritative serializer/comparison cases. Confirm dirty
  binaries reject CLI generation with the built-dirty diagnostic before output
  creation.

### Task 4: Complete frozen registry descriptor authority

**Files:**
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceData.cpp`

**Interfaces:**
- Consumes: the `parameters` array from
  `parameter-registry-v2.json` and live
  `ParameterRegistry::descriptors()`.
- Produces: `semantic.registry` before fixed-set SHA validation or PAR-001 gate
  construction whenever any descriptor field differs.

- [x] **Step 1: Add SHA-coordinated mutation RED cases**

  Copy the frozen files, mutate one field, update the copied index with the new
  file SHA, and require `semantic.registry` for index, semantic key, version,
  display/short/unit metadata, kind, every float-range/default member,
  symmetric skew, ordered choices, mapping, automatable, smoothing, and
  persistence.

- [x] **Step 2: Run RED**

  Run `ModelDReferenceManifestContract`.

  Expected: the new non-ID mutations reach only the later fixed-SHA failure,
  not `semantic.registry`.

- [x] **Step 3: Implement exact full-field comparison**

  Add exhaustive enum-to-fixture-name helpers; exact typed property readers;
  float-representation comparison; and ordered choice comparison. Apply the
  full contract to `parameter-registry-v2.json`, while retaining the legacy
  inventory and snapshot-specific contracts.

- [x] **Step 4: Run GREEN**

  Rebuild and rerun the manifest contract. Require every coordinated mutation
  to fail with `semantic.registry` and the live frozen index to pass.

### Task 5: Clean implementation commit and full GREEN

**Files:**
- Modify: plan checkboxes for Tasks 1–4 after evidence exists.

**Interfaces:**
- Consumes: focused RED/GREEN evidence and all implementation changes.
- Produces: a clean exact implementation commit whose next ordinary build
  refreshes the generated identity without reconfiguration.

- [x] **Step 1: Verify scope and commit implementation**

  Run `git diff --check`, confirm `git diff -- Source` is empty, review every
  changed file, then commit implementation and completed Task 1–4 checkboxes.

- [x] **Step 2: Rebuild without CMake reconfigure**

  Run the existing build command. Inspect the generated header and binary
  candidate provenance to prove it moved from dirty pre-commit identity to the
  clean exact implementation commit automatically.

- [x] **Step 3: Run focused and serial full GREEN**

  Run all four reference contracts, then serial full CTest and all seven
  labels. Require 21/21 full tests and nonzero exact label counts.

- [x] **Step 4: Run authoritative CLI and mutation GREEN**

  Run no-argument smoke, `validate`, two fresh candidate generations, all
  20-file comparisons, expected release exit 3, missing/tampered metric probes,
  old-binary stale-HEAD, current-dirty, built-dirty/reverted, coordinated
  registry mutation, hash, matrix/map, link, history, source-diff, diff, and
  status guards.

### Task 6: Documentation, package, and honest checklist synchronization

**Files:**
- Modify: `docs/remediation/01-traceability-matrix.md`
- Modify: `docs/remediation/12-hardware-reference-regression-system.md`
- Modify: `docs/remediation/evidence/workstream-12/f0-harness-foundation.md`
- Modify: `docs/remediation/17-implementation-handoff.md`
- Modify: `docs/superpowers/plans/2026-07-22-closure-review-endpoint-metadata.md`
- Modify: `docs/superpowers/plans/2026-07-22-ultimate-review-authority-remediation.md`
- Modify: `.superpowers/sdd/whole-branch-remediation-report.md` (ignored)
- Replace outside Git:
  `/Users/agentt/.openclaw/workspace/Developer/synth/Agent-08-Workstream-12-F0-Complete-Context.zip`

**Interfaces:**
- Consumes: exact clean build/test/CLI evidence and candidate/package hashes.
- Produces: truthful 18-per-render/20-total documentation, checked prior
  closure steps, append-only evidence, and an exact post-checklist-head ZIP.

- [x] **Step 1: Synchronize durable documentation without status changes**

  Replace the stale TST-001 15-artifact statement and make every summary say
  18 render artifacts plus root metric and report, 20 candidate files total.
  Append ultimate-remediation history and exact evidence, preserving prior
  history and all statuses. Commit these documentation changes.

- [x] **Step 2: Run fresh exact-head verification and first package boundary**

  Configure a fresh path-with-spaces Release tree, build all targets, run the
  strict 21-test and label matrix plus all CLI/mutation/guard checks, and build
  and fully verify a one-root safe ZIP from that exact commit.

- [x] **Step 3: Complete prior closure checkboxes and append report**

  Check the prior closure plan's exact-head verification, package, and report
  steps only now that their evidence exists. Append ignored execution evidence
  with commits, RED/GREEN chronology, commands, counts, hashes, package size,
  entry/manifest counts, and remaining open obligations. Commit the tracked
  checklist update.

- [ ] **Step 4: Rebuild automatically and reverify the final exact head**

  Without manual reconfigure, rebuild so generated source identity binds the
  post-checklist commit. Rerun authoritative CLI/release checks, then rebuild
  and verify the fixed-path ZIP from this final exact head, including archive
  integrity, one safe root, no duplicate/symlink/unsafe entries, complete
  `MANIFEST.sha256`, committed/candidate equality, packaged release behavior,
  required exclusions, and extracted no-`.git` tests-off configuration.

## Self-Review

- Spec coverage: Tasks 1–6 cover every approved design section and every item
  in the ultimate-review authority brief.
- Placeholder scan: all implementation APIs, files, expected diagnostics,
  test boundaries, counts, and final checks are explicit.
- Type consistency: build/current identity uses the same four fields throughout
  CMake, C++, JSON, reports, tests, and runtime validation.
- Scope check: `Source/`, frozen registry definitions, state/contour behavior,
  approval data, and requirement statuses are excluded from changes.
