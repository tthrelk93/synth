# Workstream 12 F0 Harness Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete Workstream 12 Foundation-phase reference-harness skeleton, including deterministic fixture rendering, versioned analyzers, PAR-006 derived-software policy, acceptance-manifest validation, full requirement mapping/reporting, and a verified Workstream 04 successor package.

**Architecture:** A test-only `ModelDReferenceHarness` C++ library owns versioned reference data, exact-event offline rendering, analyzers, gate evaluation, and requirement reporting. `ModelDOfflineRenderer` remains a thin CLI and preserves its no-argument smoke behavior. The harness consumes the frozen Workstream 03 registry/state/contour artifacts and records later DSP or hardware work as open instead of modifying production DSP or inventing evidence.

**Tech Stack:** C++20, JUCE 8.0.10 at `3af3ce009f6a02f6fa651008fffb5b41743a9fab`, CMake 3.24+, CTest, JUCE JSON/audio-format/cryptography modules, canonical JSON, WAV, and SHA-256.

## Global Constraints

- Preserve all 48 parameter IDs, order, type, ranges, defaults, metadata, host flags, and nine immutable Workstream 03 fixtures.
- Preserve target names, wrappers/buses, JUCE commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`, and TTH Audio / TTH Model One identity.
- Preserve `modelDState` v2, atomic v0 migration/rollback, bounded safe extensions, canonical/legacy contour behavior, and explicit conversion semantics.
- Preserve the prepared parameter snapshot and attachment-only editor boundary; the harness may read public prepared/state surfaces but may not add mutable audio-thread test hooks.
- Do not change DSP equations, MIDI lifecycle, calibration/taper behavior, output policy, preset schema/storage, UI layout, or host automation lanes.
- `Tests/reference/acceptance-v1.json` remains globally `draft`; only the user-approved PAR-006 derived-software entries are individually approved.
- The F0 harness may complete while DSP cases remain `not-run` and hardware cases remain `awaiting-approved-reference`; release enforcement must still fail.
- PAR-002/004/007 and BLD-006/011/012 remain non-passing pre-release obligations.
- Run full CTest suites serially in one mutable build tree.
- Do not push, release, distribute, notarize, submit, purchase hosts, mutate accounts, or accept third-party terms.
- Starting implementation base is design commit `57e1c87` on `codex/workstream-02-build`; preserve all unrelated untracked predecessor archives and extracted orientation copies.

---

## File Structure

Create focused harness files with these responsibilities:

- `Tools/ReferenceHarness/ReferenceTypes.h` — shared immutable data types, status enum, diagnostics, and typed load results.
- `Tools/ReferenceHarness/ReferenceData.h/.cpp` — canonical JSON, SHA-256, bounded path resolution, artifact-index and render-fixture parsing/validation.
- `Tools/ReferenceHarness/OfflineRenderer.h/.cpp` — state restore, exact-event block splitting, generated input, audio/control traces, and candidate artifact writing.
- `Tools/ReferenceHarness/AnalyzerRegistry.h/.cpp` — analyzer registration, synthetic calibration, signal statistics, control-step, and audio-click metrics.
- `Tools/ReferenceHarness/Acceptance.h/.cpp` — acceptance-v1 parsing, provenance validation, smoothing-template expansion, and gate evaluation.
- `Tools/ReferenceHarness/RequirementReporter.h/.cpp` — 127-ID map validation, result aggregation, canonical report output, and release enforcement.
- `Tools/ModelDOfflineRenderer.cpp` — no-argument smoke plus `validate`, `run`, and `verify-release` CLI dispatch only.
- `Tests/ReferenceHarnessTests.cpp` — focused modes `manifest`, `renderer`, `analyzers`, and `requirements`.
- `Tests/reference/` — frozen artifact index, three foundation render fixtures, seven PAR-006 templates, acceptance manifest, requirement map, and expected canonical F0 report.
- `docs/remediation/evidence/workstream-12/f0-harness-foundation.md` — RED/GREEN chronology, hashes, commands, honest results, and open gates.

Do not add harness implementation to `Source/` or grow `Tests/ModelDTests.cpp`; production sources and the existing contract test executable remain unchanged unless a verified build-integration issue requires a narrowly documented correction.

---

### Task 1: Canonical reference data and frozen artifact index

**Files:**
- Create: `Tools/ReferenceHarness/ReferenceTypes.h`
- Create: `Tools/ReferenceHarness/ReferenceData.h`
- Create: `Tools/ReferenceHarness/ReferenceData.cpp`
- Create: `Tests/ReferenceHarnessTests.cpp`
- Create: `Tests/reference/fixture-index-v1.json`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ParameterRegistry::descriptors()`, `StateContract`, JUCE JSON, `juce::SHA256`, source root `SYNTH_SOURCE_ROOT`.
- Produces: `ReferenceHarness::Status`, `Diagnostic`, `LoadResult<T>`, `IndexedArtifact`, `FixtureIndex`, `sha256File`, `resolveBoundedRegularFile`, `canonicalJson`, and `loadFixtureIndex`.
- Task 2 consumes the same `LoadResult<T>` and bounded path/hash functions for render fixtures.

- [ ] **Step 1: Register the focused RED contract before implementation**

  Initially add only a `ModelDReferenceTests` executable containing
  `Tests/ReferenceHarnessTests.cpp`, give it the future
  `Tools/ReferenceHarness` include directory, link the same JUCE/plug-in
  dependencies the test will need, and include the still-missing
  `ReferenceData.h`. Do not declare the `ModelDReferenceHarness` library until
  Step 4, so the RED failure is the intended missing-header compiler error
  rather than a CMake missing-source error. Register:

  ```cmake
  synth_add_reference_test(ModelDReferenceManifestContract manifest)
  set_tests_properties(ModelDReferenceManifestContract PROPERTIES LABELS "unit;state")
  ```

  In `Tests/ReferenceHarnessTests.cpp`, include `ReferenceData.h` and require:

  ```cpp
  const auto index = ReferenceHarness::loadFixtureIndex (
      sourceRoot, sourceRoot.getChildFile ("Tests/reference/fixture-index-v1.json"));
  test.expect (index.ok(), "fixture index must validate");
  test.expect (index.value->frozenArtifacts.size() == 9,
               "fixture index must contain all nine frozen artifacts");
  ```

  Add negative cases that create bounded temporary copies and require stable codes for absolute path, `..`, backslash, missing file, symlink escape, SHA mismatch, duplicate ID/path, unknown schema, and non-regular input.

- [ ] **Step 2: Run the RED build**

  Run:

  ```sh
  cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDReferenceTests --parallel 2
  ```

  Expected: build fails because `ReferenceData.h` and the registered implementation do not yet exist. Record the exact compiler failure in the Workstream 12 evidence draft.

- [ ] **Step 3: Define the shared immutable types**

  Implement these exact public shapes in `ReferenceTypes.h`:

  ```cpp
  namespace ReferenceHarness {
  enum class Status { pass, fail, notRun, awaitingApprovedReference };

  struct Diagnostic {
      std::string code;
      std::string message;
  };

  template <typename T>
  struct LoadResult {
      std::optional<T> value;
      std::vector<Diagnostic> diagnostics;
      bool ok() const noexcept { return value.has_value() && diagnostics.empty(); }
  };

  struct IndexedArtifact {
      std::string id;
      std::string relativePath;
      std::string sha256;
      std::vector<std::string> requirements;
  };

  struct FixtureIndex {
      int version = 0;
      std::string schema;
      std::vector<IndexedArtifact> frozenArtifacts;
      std::vector<IndexedArtifact> renderFixtures;
      std::vector<IndexedArtifact> smoothingFixtures;
  };
  }
  ```

  Keep all harness types value-owned. Do not retain `juce::var`, `DynamicObject*`, APVTS objects, or file streams inside validated results.

- [ ] **Step 4: Implement bounded paths, hashing, canonical JSON, and index validation**

  Declare in `ReferenceData.h`:

  ```cpp
  std::string statusName (Status);
  std::string sha256File (const juce::File&);
  LoadResult<juce::File> resolveBoundedRegularFile (
      const juce::File& root, std::string_view relativePath);
  juce::String canonicalJson (const juce::var&);
  LoadResult<FixtureIndex> loadFixtureIndex (
      const juce::File& sourceRoot, const juce::File& indexFile);
  ```

  `resolveBoundedRegularFile` must reject empty/absolute paths, drive/UNC prefixes, `\\`, empty or `.`/`..` components, non-regular files, and canonical targets outside the canonical root. `loadFixtureIndex` must require exact schema `model-d.fixture-index.v1`, version 1, lowercase 64-hex hashes, unique IDs and paths across every section, nonempty requirements, byte-matching SHA-256, and exactly the expected nine `frozenArtifacts`. `renderFixtures` and `smoothingFixtures` may be empty in Task 1 and are populated by Tasks 2–3 without changing the frozen-artifact count.

  In this step, add the `ModelDReferenceHarness` static library with
  `ReferenceData.cpp`, relink `ModelDReferenceTests` to it, and apply project
  warnings to both new `.cpp` files. Define `SYNTH_SOURCE_ROOT`, the configured
  build type/platform/architecture, exact resolved JUCE commit, and the source
  commit captured by a checked `git rev-parse HEAD` at configure time for the
  harness target. A failed Git query is a configure error when tests are on.

  `canonicalJson` recursively sorts object properties lexicographically, retains array order, uses JUCE JSON compact spacing, and appends exactly one newline.

- [ ] **Step 5: Add the exact frozen artifact index**

  `Tests/reference/fixture-index-v1.json` must use the nine hashes from the Workstream 03 verification summary:

  ```text
  legacy-parameter-inventory  7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d
  parameter-registry-v2       2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284
  parameter-snapshot-v2       cbfefbf2e918818fed1fd6b340ca4c015981d6e020080a7b71bbfd006e398f16
  contour-conversion-trace    ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c
  legacy-default-state        07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21
  legacy-representative-state e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa
  native-default-state-v2     ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507
  migrated-default-state-v2   4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958
  migrated-representative-v2  f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40
  ```

  Use the existing repository-relative fixture paths and the owning PAR IDs. Do not copy or regenerate any frozen artifact.

- [ ] **Step 6: Run GREEN and negative contracts**

  Run:

  ```sh
  cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDReferenceTests --parallel 2
  ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^ModelDReferenceManifestContract$' --output-on-failure
  ```

  Expected: focused test passes; every negative mutation reports its exact stable diagnostic; the live nine-file index validates byte-for-byte.

- [ ] **Step 7: Verify and commit Task 1**

  Run `git diff --check`, rerun `ModelDParameterRegistry`, `ModelDStateV2Contract`, and the focused test, then commit only Task 1 paths:

  ```sh
  git commit -m 'test: add reference artifact contracts'
  ```

---

### Task 2: Versioned render fixtures and deterministic exact-event renderer

**Files:**
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/ReferenceData.h`
- Modify: `Tools/ReferenceHarness/ReferenceData.cpp`
- Create: `Tools/ReferenceHarness/OfflineRenderer.h`
- Create: `Tools/ReferenceHarness/OfflineRenderer.cpp`
- Modify: `Tools/ModelDOfflineRenderer.cpp`
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Create: `Tests/reference/fixtures/native-v2-foundation.json`
- Create: `Tests/reference/fixtures/migrated-v2-foundation.json`
- Create: `Tests/reference/fixtures/legacy-contour-foundation.json`
- Modify: `Tests/reference/fixture-index-v1.json`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `LoadResult<T>`, bounded input/hash validation, `MoogMiniAudioProcessor::restoreState`, `getPreparedParameter`, `captureParameterSnapshot`, and the three frozen v2/compatibility artifacts.
- Produces: `RenderFixture`, `AutomationEvent`, `MidiEvent`, `RenderResult`, `loadRenderFixture`, `renderFixture`, and `writeCandidateArtifacts`.
- Task 3 consumes `RenderResult::main`, `RenderResult::controlTraces`, and reproducibility metadata.

- [ ] **Step 1: Add the focused renderer RED contract**

  Register:

  ```cmake
  synth_add_reference_test(ModelDReferenceRendererContract renderer)
  set_tests_properties(ModelDReferenceRendererContract PROPERTIES LABELS "dsp;state")
  ```

  Require all three fixtures to validate, native/migrated roots to restore successfully, the legacy fixture to retain `legacyCrossedContours`, automation to apply at exact sample indices, same-sample ordering to be stable, all samples finite, and repeated runs under block patterns `[128]`, `[17,31,64,127]`, and `[512]` to produce identical local-policy audio/control hashes.

  Add negative fixtures for unsupported `preset` source, sample rate outside `{44100,48000,96000}`, zero/negative render length, empty/zero block size, event beyond render length, duplicate sequence, invalid MIDI bytes, unknown parameter ID, non-finite normalized value, input hash mismatch, and existing candidate output directory.

- [ ] **Step 2: Run RED**

  Run the reference-test build and focused CTest. Expected: compile/test failure because `OfflineRenderer` and `model-d.render-fixture.v1` parsing do not exist.

- [ ] **Step 3: Add exact render and trace types**

  Extend `ReferenceTypes.h` with:

  ```cpp
  struct AutomationEvent {
      std::uint64_t sample = 0;
      std::uint32_t sequence = 0;
      ParameterRegistry::Key key {};
      float normalizedValue = 0.0f;
  };

  struct MidiEvent {
      std::uint64_t sample = 0;
      std::uint32_t sequence = 0;
      std::vector<std::uint8_t> bytes;
  };

  struct RenderConfig {
      double sampleRate = 0.0;
      std::uint64_t totalSamples = 0;
      std::uint64_t seed = 0;
      std::vector<std::vector<int>> blockPatterns;
  };

  enum class InputKind { silence, dc, sine, wav };
  struct InputDefinition {
      InputKind kind = InputKind::silence;
      double value = 0.0;
      double frequencyHz = 0.0;
      juce::File file;
      std::string sha256;
  };

  struct ReproducibilityInfo {
      std::string sourceCommit;
      std::string juceCommit;
      std::string buildType;
      std::string platform;
      std::string architecture;
      std::string fixtureSha256;
      std::map<std::string, std::string> inputHashes;
      std::map<std::string, std::string> outputHashes;
      std::uint64_t seed = 0;
  };

  struct RenderFixture {
      std::string id;
      juce::File stateFile;
      std::string stateSha256;
      StateContract::ContourContract expectedContourContract {};
      RenderConfig config;
      std::vector<AutomationEvent> automation;
      std::vector<MidiEvent> midi;
      InputDefinition input;
      std::vector<std::string> analyzers;
      std::vector<std::string> requirements;
  };

  struct ControlTracePoint {
      std::uint64_t sample = 0;
      ParameterRegistry::Key key {};
      float normalizedValue = 0.0f;
      float physicalValue = 0.0f;
  };

  struct RenderResult {
      double sampleRate = 0.0;
      int mainChannels = 0;
      int phonesChannels = 0;
      std::vector<float> main;
      std::vector<float> phones;
      std::vector<ControlTracePoint> controlTrace;
      std::vector<std::string> eventTrace;
      std::vector<int> blockPattern;
      ReproducibilityInfo reproducibility;
  };
  ```

  Store interleaved audio with exact channel counts. `ReproducibilityInfo` records source commit passed from CMake, exact JUCE revision, build type, platform, architecture, fixture/input hashes, sample rate, block pattern, seed, and output hashes.

- [ ] **Step 4: Implement render-fixture parsing and validation**

  Add:

  ```cpp
  LoadResult<RenderFixture> loadRenderFixture (
      const juce::File& sourceRoot, const juce::File& fixtureFile);
  ```

  Require schema `model-d.render-fixture.v1`, state source kind `hostState`, exact state path/hash/version/contour expectation, supported sample rate, positive bounded sample count, nonempty positive block pattern, stable sequence ordering, normalized values in `[0,1]`, MIDI messages of 1–3 bytes accepted by JUCE, and input kind `silence`, `dc`, `sine`, or hash-verified `wav`. Resolve parameter IDs by walking `ParameterRegistry::descriptors()` and storing only typed keys.

- [ ] **Step 5: Implement exact-event rendering**

  Declare:

  ```cpp
  LoadResult<std::vector<RenderResult>> renderFixture (const RenderFixture& fixture);
  LoadResult<std::vector<juce::File>> writeCandidateArtifacts (
      const RenderFixture&, std::span<const RenderResult>,
      const juce::File& newDirectory);
  ```

  The render loop must:

  1. construct a new processor;
  2. read and restore the validated state before prepare;
  3. configure buses required by the fixture without altering the wrapper contract;
  4. call `setRateAndBufferSizeDetails` and `prepareToPlay` once;
  5. choose the next boundary as the minimum of host-block end, event sample, and render end;
  6. at each boundary apply stable-ordered automation through `getPreparedParameter(key)->setValueNotifyingHost`, insert MIDI at sub-block offset zero, and record the control/event trace;
  7. generate the declared input for the sub-block, call `processBlock`, and append main/phones samples; and
  8. release/reset deterministically.

  Reject a candidate directory that already exists. Write canonical `render.json`, `control-trace.json`, `event-trace.json`, and deterministic float WAV artifacts with SHA-256 recorded in `render.json`.

- [ ] **Step 6: Convert the CLI without growing business logic in `main`**

  Preserve the current no-argument output `Rendered 480 ModelDCore samples`. Add exact dispatch:

  ```text
  validate --fixture-index PATH --acceptance PATH --requirements PATH
  run --fixture-index PATH --acceptance PATH --requirements PATH --output NEW_DIRECTORY
  verify-release --report PATH
  ```

  Task 2 implements fixture-index and render-fixture validation plus render execution. Until Tasks 3–4 land, `validate` must report stable `missing-acceptance-implementation` or `missing-requirement-implementation`, and `run` must refuse partial reporting rather than silently skip those stages.

- [ ] **Step 7: Add the three F0 fixtures**

  Use native default v2, migrated default v2, and migrated representative v2/legacy-contour inputs with their frozen hashes. Each fixture renders 2048 samples at 48 kHz, declares all three block patterns, sets `outputVolKnob` and `a440HzOnOff` at sample zero, performs one valid parameter transition at sample 512, and requests exact control/event trace capture. The legacy fixture requires `legacyCrossedContours`; no static value swap is permitted.

- [ ] **Step 8: Run GREEN and determinism checks**

  Build and run `ModelDReferenceRendererContract` serially. Run the no-argument CLI and compare its exact stdout. Run a fixture twice to two new `/private/tmp` candidate directories and use `cmp` on canonical JSON and SHA-256 on WAV outputs. Expected: focused test and repeats pass; unsupported preset source and every negative fixture fail with stable codes.

- [ ] **Step 9: Verify and commit Task 2**

  Run focused manifest/renderer tests, existing `state`, `dsp`, and `midi` labels, `git diff --check`, then commit:

  ```sh
  git commit -m 'feat: add deterministic reference rendering'
  ```

---

### Task 3: Versioned analyzers, PAR-006 templates, and acceptance manifest

**Files:**
- Create: `Tools/ReferenceHarness/AnalyzerRegistry.h`
- Create: `Tools/ReferenceHarness/AnalyzerRegistry.cpp`
- Create: `Tools/ReferenceHarness/Acceptance.h`
- Create: `Tools/ReferenceHarness/Acceptance.cpp`
- Modify: `Tools/ReferenceHarness/ReferenceTypes.h`
- Modify: `Tools/ReferenceHarness/ReferenceData.h`
- Modify: `Tools/ReferenceHarness/ReferenceData.cpp`
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Create: `Tests/reference/acceptance-v1.json`
- Create: `Tests/reference/fixtures/par-006/none-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/gain-control-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/control-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/dedicated-pitch-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/dedicated-cutoff-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/dedicated-glide-step-v1.json`
- Create: `Tests/reference/fixtures/par-006/contour-stage-step-v1.json`
- Modify: `Tests/reference/fixture-index-v1.json`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 `RenderResult`, control/event traces, registry smoothing classes, and candidate hashes.
- Produces: `AnalyzerIdentity`, `MetricResult`, `AnalysisRequest`, `AnalyzerRegistry`, `AcceptanceManifest`, `GateDefinition`, `GateResult`, `loadAcceptanceManifest`, `expandSmoothingFixtures`, and `evaluateAcceptance`.
- Task 4 consumes gate results and artifact hashes without reinterpreting analyzer values.

- [ ] **Step 1: Add analyzer/acceptance RED contracts**

  Register:

  ```cmake
  synth_add_reference_test(ModelDReferenceAnalyzerContract analyzers)
  set_tests_properties(ModelDReferenceAnalyzerContract PROPERTIES LABELS "unit;dsp")
  ```

  Require registered IDs `signal.stats.v1`, `control.step.v1`, and `audio.click.v1`; exact algorithm settings and units; analytic results for constant, impulse, upward/downward step, 5 ms ramp, 10 ms ramp, overshoot, late settling, click, NaN, and empty input; and deterministic metric JSON.

  Require acceptance validation to reject unknown schema/status/classification/analyzer/metric/unit, duplicate gate ID, missing requirement, missing derivation/source/reference/performance provenance, globally approved incomplete manifest, bare approved zero, unapproved derived policy, stale analyzer version, and `pass` without artifact hash.

  Require exactly seven smoothing templates, runtime coverage of all 48 descriptors by registry class, exact 5 ms/10 ms policies, `none` exact-step behavior, and delegated classes reported `not-run` without generic-ramp substitution.

- [ ] **Step 2: Run RED**

  Build and run the new focused test. Expected: failure because analyzers, acceptance validation, and PAR-006 templates are absent.

- [ ] **Step 3: Define analyzer and gate types**

  Add exact types:

  ```cpp
  struct AnalyzerIdentity { std::string id; int version = 0; };
  struct AnalysisRequest {
      std::string metric;
      std::uint64_t eventSample = 0;
      std::uint64_t durationSamples = 0;
      double start = 0.0;
      double target = 0.0;
      std::span<const double> control;
      std::span<const float> audio;
  };
  struct MetricResult {
      AnalyzerIdentity analyzer;
      std::string metric;
      std::string unit;
      double value = 0.0;
      double allowance = 0.0;
      bool finite = false;
      std::map<std::string, std::string> settings;
  };

  class AnalyzerRegistry {
  public:
      static AnalyzerRegistry withFoundationAnalyzers();
      LoadResult<std::vector<MetricResult>> analyze (
          std::string_view analyzerId, const AnalysisRequest&) const;
  };

  struct GateDefinition;
  struct GateResult {
      std::string id;
      Status status = Status::notRun;
      std::string reasonCode;
      std::vector<std::string> requirements;
      std::optional<MetricResult> metric;
      std::string artifactPath;
      std::string artifactSha256;
  };
  ```

  `AnalyzerRegistry::find(id)` returns a stable diagnostic for unknown IDs. Analyzer changes require a new ID/version; never mutate `*.v1` semantics silently.

- [ ] **Step 4: Implement synthetic-calibrated analyzers**

  `signal.stats.v1` reports sample count, finite count, min, max, peak absolute value, mean, RMS, and maximum first difference.

  `control.step.v1` reports first-change sample, settled sample, monotonicity, overshoot, maximum per-sample movement, and evaluated floating allowance. Use:

  ```cpp
  const auto travel = std::abs (request.target - request.start);
  const auto epsilonAllowance = 8.0 * std::numeric_limits<double>::epsilon()
                              * std::max (1.0, travel);
  const auto idealIncrement = request.durationSamples == 0
                            ? travel
                            : travel / static_cast<double> (request.durationSamples);
  ```

  `audio.click.v1` reports maximum absolute first difference in the declared event window, pre/post RMS, peak-over-steady-state dB, and finite count. Empty/non-finite inputs return `fail` metrics with stable codes, never NaN JSON.

- [ ] **Step 5: Implement the seven registry-expanded PAR-006 templates**

  Each template uses schema `model-d.smoothing-fixture.v1`, names one exact registry class, declares normalized start/end, sample rate matrix `{44100,48000,96000}`, event sample, analysis window, required analyzer, required control/audio tap, and owner workstream.

  `expandSmoothingFixtures` iterates `ParameterRegistry::descriptors()` once, matches the descriptor's enum class, and produces exactly one case per descriptor. It must not contain a second parameter-ID list. Assert class counts from the frozen registry and total count 48.

  Expected dispositions:

  - `none`: executable exact control step; path-owned audio click may remain `not-run`;
  - `gainControl`: approved 5 ms linear-amplitude policy, actual DSP tap currently `not-run`;
  - `control`: approved 10 ms owner-declared control-domain policy, actual DSP tap currently `not-run`;
  - dedicated pitch/cutoff/glide/contour: required future owner tap and `not-run`;
  - hardware taper/shape: `awaiting-approved-reference`.

- [ ] **Step 6: Implement and seed `acceptance-v1.json`**

  Use schema `model-d.acceptance.v1`, manifest version 1, global status `draft`, and separate arrays for hard software, published, derived software, measured hardware, and performance gates. In F0, measured-hardware and performance arrays are empty, published entries are not promoted beyond sourced fields already present in the planning suite, and the approved derived entries record:

  ```text
  par006.gain-control.duration   0.005 seconds
  par006.control.duration       0.010 seconds
  par006.settling.allowance     +1 sample after ceil(duration * sampleRate)
  par006.none.intermediate      0 intermediate values
  ```

  Record review status `approved`, review basis `2026-07-22 user-approved design`, and date `2026-07-22`. State explicitly that these are software safety policies, not hardware measurements.

- [ ] **Step 7: Run GREEN, synthetic calibration, and honest live evaluation**

  Run `ModelDReferenceAnalyzerContract` and the prior focused tests. Generate an F0 candidate result. Expected: all synthetic analyzer calibrations pass; manifest validation passes; every registry descriptor is covered; only available `none` control-trace gates can pass; missing owner-DSP taps are `not-run`; missing hardware references are `awaiting-approved-reference`; no open gate is converted to pass.

- [ ] **Step 8: Verify and commit Task 3**

  Run unit/dsp/state labels serially, canonical capture repeat/`cmp`, all reference hashes, and `git diff --check`, then commit:

  ```sh
  git commit -m 'feat: define reference acceptance policies'
  ```

---

### Task 4: Full requirement map, aggregate report, and CLI enforcement

**Files:**
- Create: `Tools/ReferenceHarness/RequirementReporter.h`
- Create: `Tools/ReferenceHarness/RequirementReporter.cpp`
- Modify: `Tools/ModelDOfflineRenderer.cpp`
- Modify: `Tests/ReferenceHarnessTests.cpp`
- Create: `Tests/reference/requirement-map.json`
- Create: `Tests/reference/expected-f0-requirements-report.json`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 `GateResult`, manifest classifications, durable artifact hashes, and `docs/remediation/01-traceability-matrix.md`.
- Produces: `RequirementDefinition`, `RequirementResult`, `RequirementReport`, `loadRequirementMap`, `validateRequirementSet`, `buildRequirementReport`, `writeRequirementReport`, and `verifyReleaseReady`.
- Task 5 consumes the exact canonical report and command log as durable planning evidence.

- [ ] **Step 1: Add the requirement/report RED contract**

  Register:

  ```cmake
  synth_add_reference_test(ModelDReferenceRequirementContract requirements)
  set_tests_properties(ModelDReferenceRequirementContract PROPERTIES LABELS "artifact;state")
  ```

  Parse requirement tokens from matrix data rows and require exact set equality with `requirement-map.json`: 127 unique IDs, one owner each, nonempty verification list, valid status, and no unknown/duplicate/missing row. Require stable failures for `waived`, `pass` without existing hashed artifact, unknown verification code, missing gate mapping, duplicate owner, and a changed matrix ID.

  Require two generated reports to be byte-identical; `run` must exit 0 after producing an honest mixed-status report; `verify-release` must exit nonzero and identify the first non-pass requirement while any required row is open.

- [ ] **Step 2: Run RED**

  Build and run the focused requirement test. Expected: failure because the complete map, reporter, and CLI enforcement are absent.

- [ ] **Step 3: Define the reporting types and status reduction**

  Add:

  ```cpp
  struct RequirementDefinition {
      std::string id;
      std::string owner;
      std::vector<std::string> verification;
      std::vector<std::string> gateIds;
      std::vector<std::string> artifactPaths;
      Status status = Status::notRun;
  };

  struct RequirementResult {
      RequirementDefinition definition;
      Status status = Status::notRun;
      std::vector<std::string> reasons;
      std::vector<std::string> artifactHashes;
  };

  struct RequirementReport {
      int manifestVersion = 1;
      std::vector<RequirementResult> requirements;
      bool releaseReady = false;
  };
  ```

  Reduction rules are exact: any executed failing required gate makes the row `fail`; otherwise any missing measured-hardware gate makes it `awaitingApprovedReference`; otherwise any unrun required gate makes it `notRun`; only all required gates/artifacts passing makes it `pass`. Listening never participates.

- [ ] **Step 4: Create and validate the complete checked-in map**

  Extract every matrix data-row ID, owner-plan link, and verification code into `model-d.requirement-map.v1`. Seed existing BLD/PAR pass rows with their committed evidence paths/hashes; translate still-open in-progress/external rows to `not-run`; seed future rows as `not-run` or `awaiting-approved-reference` only where the owner plan explicitly requires missing hardware data.

  The validator must independently parse the Markdown matrix ID set at test time and compare it with JSON. Do not maintain a second C++ array of 127 IDs. The checked-in JSON is the data source; the matrix parser is the drift oracle.

- [ ] **Step 5: Implement canonical aggregate reporting**

  `buildRequirementReport` merges definitions with gate results, verifies every referenced artifact exists beneath the source or candidate root and matches its hash, applies the reduction rules, sorts rows by matrix order, and sets `releaseReady` only when all 127 rows pass.

  `writeRequirementReport` writes canonical JSON only into a new candidate directory. Include manifest/fixture/analyzer versions, source/build provenance, per-status counts, every requirement row, gate/artifact links, and explicit open-gate reason codes.

- [ ] **Step 6: Complete CLI modes**

  `validate` validates the artifact index, all render/smoothing fixtures, acceptance manifest, requirement map, analyzer registry, and set equality without rendering.

  `run` performs validation, renders the three foundation fixtures, runs analyzers/PAR-006 expansion, writes all candidate artifacts, and writes `requirements-report.json`. It exits 0 if infrastructure succeeds even though the report is not release-ready.

  `verify-release --report PATH` validates the canonical report and artifacts, exits 0 only for all-pass, otherwise exits 3 and prints stable `release-not-ready: ID STATUS REASON` to stderr.

- [ ] **Step 7: Capture and freeze the expected F0 report**

  Run `validate`, then `run` into a fresh bounded candidate directory. Review that:

  - the set contains all 127 IDs;
  - Workstream 03 and Workstream 02 retained statuses are honest;
  - the F0 harness artifacts are present and hashed;
  - later DSP cases are `not-run`;
  - hardware cases are `awaiting-approved-reference` only when the plans require hardware; and
  - `releaseReady` is false.

  Copy the reviewed canonical report to `Tests/reference/expected-f0-requirements-report.json`; rerun capture and require `cmp` byte equality.

- [ ] **Step 8: Run GREEN and negative enforcement**

  Run all four reference contracts. Run `validate` expecting exit 0, `run` expecting exit 0, and `verify-release` expecting exit 3. Mutate a candidate report to false pass and require rejection. Expected: reference tests pass and release remains truthfully non-ready.

- [ ] **Step 9: Verify and commit Task 4**

  Run all unit/state/dsp/artifact labels serially, old CLI smoke, canonical report repeat, `git diff --check`, and immutable fixture hashes, then commit:

  ```sh
  git commit -m 'feat: report complete reference requirements'
  ```

---

### Task 5: F0 closeout, full verification, and Workstream 04 successor package

**Files:**
- Modify: `docs/remediation/00-master-remediation-roadmap.md`
- Modify: `docs/remediation/01-traceability-matrix.md`
- Modify: `docs/remediation/03-parameter-automation-state-contract.md`
- Modify: `docs/remediation/12-hardware-reference-regression-system.md`
- Modify: `docs/remediation/17-implementation-handoff.md`
- Create: `docs/remediation/evidence/workstream-12/f0-harness-foundation.md`
- Modify: `docs/superpowers/plans/2026-07-22-workstream-12-f0-harness.md`
- Create outside Git after final commit: `Agent-08-Workstream-12-F0-Complete-Context.zip`

**Interfaces:**
- Consumes: Tasks 1–4 commits, reference candidate artifacts, complete requirement report, and existing Workstream 03/02 evidence.
- Produces: an honest F0-complete ledger, durable verification report, exact final commit, and verified one-root successor package whose next action is Workstream 04 pitch/glide implementation.

- [ ] **Step 1: Audit requirement statuses against artifacts**

  Reconcile TST-001–009 without claiming full Workstream 12 completion. Record the F0 skeleton as complete while individual final requirements remain `in-progress`/non-pass as their owner plan requires. PAR-006 advances from no executable measurement seam to approved executable policy, but remains `in-progress` because non-`none` DSP trajectories and hardware-dependent cases are open. Preserve PAR-002/004/007 and BLD-006/011/012 as non-passing.

- [ ] **Step 2: Write the durable evidence report**

  Record:

  - archive preflight and exact starting commit;
  - baseline 16/16 serial CTest;
  - every task RED failure and GREEN command;
  - all new schemas, analyzer versions, fixture/report hashes;
  - deterministic repeat results and negative-contract codes;
  - exact current gate/status counts;
  - explicit no-DSP/no-hardware/no-host claims; and
  - exact commands to reproduce validation, rendering, reporting, and release-enforcement failure.

- [ ] **Step 3: Synchronize roadmap, matrix, owner plans, and handoff**

  Update the F0 phase table, PAR-006 and TST rows, Workstream 03/12 status and definition-of-done text, current handoff snapshot, change inventory, verification ledger, blockers, exact resumption point, and append-only history. Historical entries remain byte-for-byte unchanged. Set the next engineering action to Workstream 04's PIT-001/PIT-002 semitone-domain foundation using the new fixture/analyzer contracts.

- [ ] **Step 4: Run fresh final serial verification**

  Configure a new path-with-spaces Release tree against exact JUCE with tests, validators, warnings-as-errors, and distribution identity validation enabled. Run:

  ```sh
  cmake --build FINAL_BUILD --config Release --parallel 2
  ctest --test-dir FINAL_BUILD -C Release -j1 --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L unit --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L state --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L dsp --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L midi --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L realtime --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L host --output-on-failure
  ctest --test-dir FINAL_BUILD -C Release -j1 -L artifact --output-on-failure
  ModelDOfflineRenderer validate ...
  ModelDOfflineRenderer run ... --output NEW_CANDIDATE
  ModelDOfflineRenderer verify-release --report NEW_CANDIDATE/requirements-report.json
  ```

  Expected: build and every CTest pass; validate/run exit 0; release verify exits 3 for the first honest open requirement; repeated candidate canonical outputs compare byte-equal; all frozen/new hashes, source guards, `git diff --check`, Markdown links, and repository status checks pass.

- [ ] **Step 5: Commit the synchronized F0 closeout**

  Commit every tracked implementation-plan completion mark, planning/evidence update, and no unrelated path:

  ```sh
  git commit -m 'docs: close workstream 12 f0 harness'
  ```

  Do not push.

- [ ] **Step 6: Build the exact successor context**

  Construct `Agent-08-Workstream-12-F0-Complete-Context.zip` from the exact final commit with one top-level root. Include:

  - `START-HERE.md` and current `HANDOFF.md`;
  - complete `docs/remediation` planning/evidence suite;
  - this design and implementation plan;
  - the official Model D manual and approved original-audit text;
  - exact harness/repository sources, CMake, all reference and frozen fixtures;
  - exact repository state and verification summary; and
  - `MANIFEST.sha256` covering every other regular file.

  Exclude `.git`, builds/caches, dependencies, downloaded tools/validators, credentials, settings, recovery files, predecessor archives, and unrelated workspace paths.

- [ ] **Step 7: Verify the successor package and record it**

  Run ZIP integrity; reject absolute, drive/UNC, traversal, and symlink entries; extract into a fresh bounded directory; verify every internal SHA-256; compare packaged sources/planning/reference files with the committed originals; verify the packaged final commit; and run semantic spot checks for F0-complete status, draft acceptance manifest, open release report, retained Workstream 03 fixtures, and Workstream 04 first action.

  Add a final tracked plan mark only if package verification status must be recorded, commit that one documentation change, rebuild the package from the new exact commit, and repeat every archive verification. Leave only the final verified ZIP untracked.

## Self-Review

- Spec coverage: Task 1 implements canonical data, hashing, path safety, and the frozen artifact seam; Task 2 implements fixture parsing, deterministic exact-event rendering, generated inputs, output governance, and CLI compatibility; Task 3 implements analyzers, synthetic calibration, seven registry-derived PAR-006 templates, approved derived policy, and honest open results; Task 4 implements all-127 requirement mapping, canonical reports, and release enforcement; Task 5 implements evidence, planning synchronization, full serial verification, and the successor chain.
- Placeholder scan: every implementation step names exact paths, interfaces, commands, expected RED/GREEN outcomes, status semantics, and commit boundaries. Command metavariables such as `FINAL_BUILD` and `NEW_CANDIDATE` are explicitly runtime-chosen bounded paths, not unresolved design decisions.
- Type consistency: all loaders return `LoadResult<T>`; the renderer produces `RenderResult`; analyzers produce `MetricResult`; acceptance produces `GateResult`; reporting produces `RequirementReport`. Parameter identity always resolves through `ParameterRegistry::Key`; report status always uses the single `Status` enum.
- Scope check: the five tasks form one F0 harness subsystem with sequential dependencies and independently reviewable commits. Later DSP analyzers, hardware campaigns, full TST completion, host validation, presets, UI, release, and distribution remain out of scope.
