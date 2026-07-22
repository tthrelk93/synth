# Workstream 12 F0 Harness Foundation Evidence

Date: 2026-07-22

Platform: macOS Darwin arm64

Task 5 starting commit: `f67f4f74c31550194c699a8249ff141fc1e616d8`

Exact JUCE checkout: `/private/tmp/model-d-agent03-final.NECHVL/Release build with spaces/_deps/juce-src` at `3af3ce009f6a02f6fa651008fffb5b41743a9fab`

## Claim boundary

This report closes only the Workstream 12 **F0 harness skeleton**. It does not
close Workstream 12, approve `acceptance-v1`, promote any open requirement, or
claim a release. PAR-002/004/006/007 and BLD-006/011/012 remain non-passing.
No designated host, new production DSP trajectory, reference hardware,
hardware capture, or listening campaign was used. The honest release command
must fail while these obligations remain open.

## Archive and repository preflight

Task 5 began from a clean `codex/workstream-12-f0-harness` worktree at the exact
commit above. The predecessor
`Agent-07-Workstream-03-Complete-Context.zip` remained unmodified. Its preflight
passed `unzip -t`, one-root/path/symlink inspection, fresh extraction, and all
80 internal manifest hashes. It contains 99 ZIP entries, zero symlinks, and the
root `Agent-07-Workstream-03-Complete-Context`; its outer SHA-256 is
`2d89824455484cca8e8cd2b8a9d36d149024ecfe484ebbc99288d4cb6cd92eb9`.
The approved original-audit text copied forward from that verified input has
SHA-256 `35aa992c29a6872f8c7d171a70614db050bc98dbdba73f5ef7b740cf63cf7b50`.

The predecessor's fresh Workstream 03 exact-JUCE, warnings-as-errors Release
baseline remains 16/16 serial CTest in 36.95 seconds. That durable baseline is
recorded in the [Workstream 03 verification summary](../workstream-03/workstream-03-verification-summary.md);
it is not represented as a new Task 5 execution. The F0 harness adds four
reference contracts, producing the fresh 20-test closeout suite recorded below.

## RED/GREEN chronology

The ignored execution reports under `.superpowers/sdd/` retain the complete
development transcript. This durable summary records each required RED and its
GREEN proof without treating a failed intermediate command as final evidence.

| Task | Required RED evidence | GREEN command/result | Implementation commits |
|---|---|---|---|
| 1 — canonical reference data | `ModelDReferenceTests` failed to compile with `fatal error: 'ReferenceData.h' file not found`; review probes then exposed missing exact frozen ownership/semantic checks and the temporary-root race. | `ctest --test-dir <build> -C Release -R '^(ModelDParameterRegistry|ModelDStateV2Contract|ModelDReferenceManifestContract)$' --output-on-failure` passed 3/3 after each correction; `git diff --check` passed. | `e0bc0c4`, `6c4d015`, `aa6c9a3` |
| 2 — deterministic renderer | The focused target failed to compile with `fatal error: 'OfflineRenderer.h' file not found`. Later REDs exposed all-fixture nondeterminism, 27 validation/writer omissions, 24 provenance omissions, and four stochastic-interval assertions. | `ctest --test-dir <build> -C Release -R '^ModelDReference(Manifest|Renderer)Contract$' --output-on-failure -j1` passed 2/2; serial state/DSP/MIDI labels and repeated five-file candidate comparisons passed. | `b81fc0e`, `052a497`, `2019fbf`, `3ebd279` |
| 3 — analyzers and acceptance | The target failed to compile with `fatal error: 'Acceptance.h' file not found`. Review REDs exposed declaration-only passes, incomplete global approval, weak policy binding, hand-built evidence, and 27 finite-extreme analyzer failures. | The three focused reference contracts passed 3/3; serial unit/DSP/state labels passed; every indexed hash and five repeated candidate bytes matched. | `babda92`, `301a0a6`, `dd94309`, `bd69b5f` |
| 4 — requirement reporting | The target failed to compile because `RequirementReporter.h` did not exist. Subsequent REDs exercised the provisional map/report/CLI, missing projection, wrong report set, reciprocal ownership/order/status/gate forgery, and missing/tampered candidate authority. | `ModelDReferenceRequirementContract` passed after each slice; the four reference contracts passed 4/4. CLI `validate` and `run` exit 0; authoritative `verify-release` exits 3 on the first honest open row. | `eaf838d`, `f67f4f7` |

## Frozen schemas, analyzers, and hashes

The harness defines `model-d.fixture-index.v1`,
`model-d.render-fixture.v1`, canonical render/control/event outputs,
`model-d.smoothing-fixture.v1`, `model-d.acceptance.v1`,
`model-d.requirement-map.v1`, `model-d.requirements-report.v1`, and the
non-provenance expected F0 status projection. Analyzer identities are
`signal.stats.v1`, `control.step.v1`, and `audio.click.v1`, each at version 1.

The immutable Workstream 03 frozen set remains nine files with the hashes in
`Tests/reference/fixture-index-v1.json`. New input hashes are:

| Artifact | SHA-256 |
|---|---|
| `native-v2-foundation.json` | `86a06f1dacabff5cc3e346a0549446fff154f79c8fc8263beeadbf439d614c79` |
| `migrated-v2-foundation.json` | `4c298c51d760e6b0ed27db97e777c25cabd6b4f3ad737bddc9db71ac7bfd8740` |
| `legacy-contour-foundation.json` | `9decfa1f8cf93b57656c2c2035cd244619c985ef183172003833de3f17feb830` |
| `par-006/none-step-v1.json` | `e2346232c6d4ad68621231982c35de64377789ed83678d6dd0509dfbe1f9b6b3` |
| `par-006/gain-control-step-v1.json` | `e44a0d7953c79f2bc8604d9b8addb527e986da73f43cd155c91e9050459ebfcf` |
| `par-006/control-step-v1.json` | `43dded14ef88f7d7fc1b43cffd69e15fc498af95d4e59b5aff44cd258613684f` |
| `par-006/dedicated-pitch-step-v1.json` | `34426fd74ba02378daf201246195471808a72da6a8d57d50c7d38c0e17f9e70c` |
| `par-006/dedicated-cutoff-step-v1.json` | `e3199bbaea44488f31d62354f6257d68ed67e2bb38ae6f7abd6c12da258d1677` |
| `par-006/dedicated-glide-step-v1.json` | `116777cf514b05bf1c96bb6a883bbd68fec48244f82b13ccd84807887d8035fb` |
| `par-006/contour-stage-step-v1.json` | `4a982469f00b68b01276017a6afac32f765221892b8da7062ff64d5770677bb4` |
| `acceptance-v1.json` | `7ebe0d765301125165726884f0e424105f502b0fb718002af7c0c9e2867e0216` |
| `requirement-map.json` | `d1a23e8b70fe94d044cb5827338ebd36648066820836c70c06c9e4ddfe50a8b9` |
| `expected-f0-requirement-statuses.json` | `179ec17e454bfaa6d14059fe8e426bab86f0633ac18a6a58886a20d72ea12c46` |

## Determinism and negative contracts

Each native, migrated, and legacy-contour fixture renders twice and across its
declared `[128]`, `[17,31,64,127]`, or `[512]` host partition. Main audio,
Phones audio, control traces, event traces, and canonical render manifests are
byte-equal for repeated candidates at the same exact configured commit. The
final retained repeat hashes are recorded in the closeout verification below.

Stable negative diagnostics cover `path.*`, `index.*`, `semantic.*`,
`fixture.*`, `render.*`, `output.*`, `analyzer.*`, `acceptance.*`,
`smoothing.*`, `requirement.*`, and `release.*` contracts. In particular,
existing candidate outputs, path escape/symlink inputs, frozen-byte or semantic
drift, invalid event sequencing/MIDI/numerics, stochastic positive intervals,
non-finite/overflow analysis, unapproved or incomplete acceptance, missing or
forged requirement ownership/evidence, report reordering/status forgery, and
candidate-byte/hash coordination are rejected with stable codes. A release
report with honest open requirements returns `release.not-ready`, not success.

## Fresh closeout verification

The first configure-only attempt requested Ninja and stopped before generation
because Ninja is unavailable in this environment. No test or build claim is
based on that discarded tree. The successful, genuinely fresh default-generator
tree is:

`/private/tmp/model-d-task5-final.cUgN5c/Release build with spaces`

It resolved exact JUCE `3af3ce009f6a02f6fa651008fffb5b41743a9fab`
and enabled Release, tests, validators, warnings as errors, and distribution
identity validation. The complete pre-commit build exited 0. Serial CTest
passed 20/20 in 38.75 seconds. The required label reruns passed: unit 4/4,
state 10/10, DSP 3/3, MIDI 1/1, realtime 2/2, host 2/2, and artifact 4/4.

CLI `validate`, candidate A `run`, and candidate B `run` each exited 0.
All 16 files in A compare byte-equal to B. `verify-release` exited 3 with the
exact expected first-open diagnostic:

```text
release-not-ready: BLD-006 not-run requirement.not-run
```

The report's configured provenance is exact source commit
`f67f4f74c31550194c699a8249ff141fc1e616d8`; its canonical projection is 127
requirements, 101 gates, 14 pass, 94 not-run, 19 awaiting approved reference,
0 fail, and `releaseReady=false`.

Pre-commit repeat hashes:

| Fixture | `render.json` | `control-trace.json` | `event-trace.json` | `main.wav` / `phones.wav` |
|---|---|---|---|---|
| native v2 | `20fb40633e7120d0f3f15931c0483eddb0eeee605aa874f456ed953685c0f44b` | `867e3c974d1126698b06fdc9c82cb6f00db28bf24c27d9f27dd08935c5f33a13` | `8f0e584133cfe1abbc6bc40c72fe6c7871dade5129203b084f030225d445a42c` | `5824a7fa979b042bd93ee5f3eed64fd6e54f2ff93f29bf46bb375f5180fe2cd8` |
| migrated v2 | `d67fe44591c2a100ef88abe1bdbe4a3ab7fd7d9adc429d5b72b51d173095aa45` | `867e3c974d1126698b06fdc9c82cb6f00db28bf24c27d9f27dd08935c5f33a13` | `8f0e584133cfe1abbc6bc40c72fe6c7871dade5129203b084f030225d445a42c` | `5824a7fa979b042bd93ee5f3eed64fd6e54f2ff93f29bf46bb375f5180fe2cd8` |
| legacy contour | `39bd0b59869906734dea00cbcb123ad9932441453bd1a5db0efdbb3cc6678d92` | `81fb80a44ead765ffadf894e0bd9874a215f0036c06c54f99d2de98d341ccbdb` | `4ebeb8ec96bafe195eb4281b824b7f3136be9d42e01977b24e98b4bcc1b18a32` | `2005419d9ac4c37e8f8f7271170bc9bb19683aae05b6af248b4053bdfec427a0` |

The canonical pre-commit requirement report hash is
`5d4be504c45e8f8006071baffd3c8b1580c9f1fb013a6aec50e9049d45229783`.
Every one of 19 indexed source artifacts rehashed to its declared SHA-256.
Workstream 12 changed no production `Source/` file. `git diff --check`, the
127-ID matrix/map audit, local Markdown links, append-only history comparison,
and tracked-scope/status checks passed before commit.

After the documentation closeout commit, the same tree is explicitly
reconfigured to embed that exact commit, rebuilt, rerun 20/20 serially, and
exercised through CLI validate/run/verify again before packaging. The exact
post-commit SHA is intentionally recorded in the package's generated
`repository-context/GIT-STATE.md`, avoiding a self-referential tracked file.

Reproduction commands:

```sh
cmake -S "$SOURCE" -B "$FINAL_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSYNTH_JUCE_SOURCE_DIR="$EXACT_JUCE" \
  -DSYNTH_BUILD_TESTS=ON -DSYNTH_BUILD_VALIDATORS=ON \
  -DSYNTH_WARNINGS_AS_ERRORS=ON \
  -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON
cmake --build "$FINAL_BUILD" --config Release --parallel 2
ctest --test-dir "$FINAL_BUILD" -C Release -j1 --output-on-failure
ctest --test-dir "$FINAL_BUILD" -C Release -j1 -L <label> --output-on-failure
ModelDOfflineRenderer validate --fixture-index ... --acceptance ... --requirements ...
ModelDOfflineRenderer run --fixture-index ... --acceptance ... --requirements ... --output <new-candidate>
ModelDOfflineRenderer verify-release --report <new-candidate>/requirements-report.json
```

## Honest F0 projection and next action

The canonical F0 report contains 127 requirements and 101 gate payloads:
14 `pass`, 94 `not-run`, 19 `awaiting-approved-reference`, 0 `fail`, and
`releaseReady=false`. The first open row is BLD-006. PAR-006 and TST-006 reduce
to `awaiting-approved-reference` because software evidence and hardware-owned
evidence remain mixed and open; in the planning ledger they remain
`in-progress`, not passed.

Workstream 04 is the next engineering owner. Start with PIT-001/PIT-002:
introduce the semitone-domain pitch foundation and the exact coarse-range plus
−8…+8 semitone oscillator offset mapping, then bind its fixtures/results to the
existing renderer, analyzer, manifest, and requirement-report contracts. Do
not infer hardware calibration, host validation, or release readiness from F0.
