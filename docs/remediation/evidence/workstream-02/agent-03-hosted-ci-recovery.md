# Workstream 02 Agent 03 Hosted-CI Recovery

Captured from 2026-07-19 19:06 PDT on macOS arm64. This report is updated
through the completion of Agent 03's recovery attempt.

## Repository and continuation preflight

The supplied `Agent-03-Workstream-02-Continuation-Context.zip` passed
`unzip -t`, and every regular file listed in its `MANIFEST.sha256` verified
after extraction. The live checkout was clean for tracked files on
`codex/workstream-02-build` at
`68748da89e4cfe4cf78d3bec66d513c5da8015f0`. `origin` resolved the same branch
to that exact commit, and draft PR
[#1](https://github.com/tthrelk93/synth/pull/1) remained open against `main`.
GitHub CLI authentication included `repo` and `workflow` scopes.

The only untracked repository entries were the three supplied predecessor ZIPs
and their extracted orientation copies. None was staged, modified, or removed.
The parent workspace `AGENTS.md` explicitly grants authority only over another
repository and therefore does not apply here.

## Official incident state and trigger discipline

The official GitHub Statuspage API was queried directly before any workflow
mutation. At 2026-07-20 02:06 UTC it reported incident `8vfyvq16hzh9`,
`Incident with GitHub Actions`, as `investigating` with `critical` impact and
the Actions component in `partial_outage`. GitHub's latest update said it was
still restoring Actions workflow runners. Bounded polling continued to report
`partial_outage` until the component returned to `operational` at 03:35:09 UTC.

No recovery trigger was issued while the component remained non-operational.
Read-only rehearsal established that manual dispatch is unavailable: GitHub
returns 404 because `.github/workflows/ci.yml` is introduced by this branch and
does not yet exist on the default branch. This matches GitHub's documented rule
that `workflow_dispatch` can be triggered only when its workflow exists on the
default branch; it is not a project-workflow defect.

The primary single-run recovery path is therefore to re-run exact-commit push
run `29711636690` after the Actions component is operational. GitHub documents
that a re-run retains the original `GITHUB_SHA` and `GITHUB_REF`; this run is
bound to branch `codex/workstream-02-build` and commit `68748da`. If GitHub
continues to reject re-running a jobless `startup_failure`, the bounded fallback
is to close and reopen draft PR #1. The workflow's default `pull_request` event
types include `reopened` but not `closed`, so that sequence creates one fresh
pull-request run without a duplicate push run. Earlier `startup_failure` runs
remain infrastructure evidence, not project failures.

After the component became operational, `gh run rerun 29711636690` returned
`This workflow run cannot be retried`; read-back confirmed run attempt 1 and an
empty job list, so no new trigger or attempt was created. Agent 03 then used the
documented fallback: close and immediately reopen draft PR #1. The PR returned
to open/draft state at the same head commit, and exactly one new
`pull_request/reopened` run was created:

```text
run: 29714998121
url: https://github.com/tthrelk93/synth/actions/runs/29714998121
event: pull_request
head: 68748da89e4cfe4cf78d3bec66d513c5da8015f0
workflow: .github/workflows/ci.yml
```

GitHub materialized all eight required matrix jobs. The optional raw failing
sentinel job was correctly `skipped`. All eight jobs then reached project
commands and were terminal by 2026-07-20 04:19:05 UTC:

| Matrix row | Result | Exact observation |
|---|---|---|
| Windows x86_64 / Debug | fail | MSVC `/WX` rejected project code: `M_PI` was undeclared, one double-to-float conversion was implicit, and `LadderFilter::setResonance` hid the member named `resonance`. |
| Windows x86_64 / Release | fail | Same project-owned MSVC diagnostics as Debug. |
| Linux x86_64 / Debug | fail | `ModelDOfflineRenderer` compiled JUCE core without the intended `JUCE_USE_CURL=0`, reached missing `curl/curl.h`, and the hosted runner later received a shutdown signal while parallel work drained. |
| Linux x86_64 / Release | fail | Same missing curl configuration propagation plus GCC `-Werror` findings for two dead editor-position variables and one potentially uninitialised position. |
| macOS x86_64 / Debug | pass | Complete strict build, 9/9 tests, required labels, standalone lifecycle, actual-wrapper 3/3, pluginval 3/3, ephemeral AU validation, and linked-manifest verification passed. |
| macOS x86_64 / Release | pass | Same complete result as Debug. |
| macOS arm64 / Debug | fail | Build and the first six tests passed; the standalone lifecycle gate rejected different normal-mode screenshots across fresh repeats. |
| macOS arm64 / Release | fail | Same lifecycle determinism failure; retained screenshots proved real control-value differences rather than PNG metadata noise. |

The full combined log has SHA-256
`1dc6aab7c23808c2e53a20bacabec63ebade84d4d86842522c6a56eb56f2aaac`.
Every terminal job log was retained separately:

| Job log | SHA-256 |
|---|---|
| Windows x86_64 / Debug | `09c9fe1566934eaf844f87be0ccf11e10c179432ad026bc370f45e9060a272f6` |
| Windows x86_64 / Release | `b7ce2c872da4a27abe94cc51a39f83096b7dbedb02436b217e0dfc5485443242` |
| Linux x86_64 / Debug | `59bfb9d882f8b0e090f6d66352907025b6524d030353eda1cfe373a981cc0a49` |
| Linux x86_64 / Release | `07f11574c40fbdb3adb1a78e0a2f5e824992604d585903ff22a65d88d1fb29b7` |
| macOS x86_64 / Debug | `5bb8b8787315764452de595e9b8cec761eeefb2c0cfb1280f4179b1a5830cae4` |
| macOS x86_64 / Release | `b946ddeb850d37ec7dacd752a14531004ee732eac8f1a2a55e7a8140cb0efe79` |
| macOS arm64 / Debug | `c7bfc4ebd6edea8ff3cd1df58f975525dc5e5669001574cc9fb3e54655c74656` |
| macOS arm64 / Release | `869efa533a4cad1dd2517ad28beccb0bcaf094c467a2f0a97b782633e2783312` |

Seven `model-d-validation-*` uploads were retained and their 147 regular files
were individually hashed. Linux Debug produced no upload because the hosted
runner shutdown interrupted the always-run upload step; its full job log still
preserves both the project failure and the later infrastructure termination.

## Fresh local baseline during the outage

Agent 03 configured fresh Release and Debug build directories under
`/private/tmp/model-d-agent03-local.D2YYJs`, both with paths containing spaces.
Release fetched JUCE normally; Debug used Release's fetched checkout through
the supported `SYNTH_JUCE_SOURCE_DIR` override. Both resolved JUCE exactly at
`3af3ce009f6a02f6fa651008fffb5b41743a9fab` and enabled tests, validators, and
project warnings as errors.

For both configurations, Agent 03 ran:

```sh
cmake -S . -B <build> -DCMAKE_BUILD_TYPE=<config> \
  -DSYNTH_WARNINGS_AS_ERRORS=ON \
  -DSYNTH_BUILD_TESTS=ON \
  -DSYNTH_BUILD_VALIDATORS=ON
cmake --build <build> --config <config> --parallel 4
ctest --test-dir <build> -C <config> --output-on-failure
cmake --build <build> --config <config> \
  --target ModelDVerifyValidationEvidence --parallel 4
```

Both strict builds completed, and each configuration passed 9/9 CTest with all
seven required labels. Each linked-validation run recorded actual-wrapper 3/3,
pluginval 1.0.4 strictness-10 in 3/3 isolated processes, standalone lifecycle
9/9, and deep manifest verification as passing. `auval` remained truthfully
`blocked` because the AU was not registered in the real account, and the
release aggregate remained `blocked` on the declared external gates.

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `0c0779e283c3c9d25cb1b6009b8afa42607b039d8ae83af3ba2bd5a35c45afb8` | `aa800be0ebc778dd1e90b5c73e6c6e14cb17e919148bab793b279b63dda38cbc` | `61caf20cd41dede0d59a7147e829dcb8b201b261a423becc0dac276242e23898` |
| Release | `854bc1fae049844d3306d51314a6d0961dab81cf5f704a5126cb196220e45d51` | `5c8ab388936545b7673e17218e41c1e3226be36133085a5fb3b883ddfdb6e0e5` | `d35e14a3bcfa74ba8fb1180959957349b6f8af7b457ed2ce9ed24111af5483e5` |

These hashes identify the fresh temporary Agent 03 trees. They are local
continuation evidence and do not substitute for supported hosted execution.

## Root-cause remediation

The failures were corrected at their causes rather than by weakening strict
build or determinism gates:

- `Source/Oscillator.cpp` now uses the standard C++20 `std::numbers` constant
  instead of non-portable `M_PI`, with explicit narrowing at the intended float
  boundary.
- `Source/LadderFilter.{h,cpp}` names the setter input `newResonance`, removing
  the project-owned MSVC hiding warning without changing filter arithmetic.
- JUCE module-wide `JUCE_WEB_BROWSER=0` and `JUCE_USE_CURL=0` definitions are
  public on `ModelDCore`, so consumer targets that compile JUCE sources inherit
  the same configuration. Fresh Debug and Release `ModelDOfflineRenderer`
  flags explicitly contain both definitions.
- `Source/PluginEditor.cpp` removes positions for controls that are not
  constructed and initializes the remaining optional position before use,
  satisfying GCC's strict dataflow diagnostics without adding a UI control.
- Slider initialization now converts from the authoritative normalized
  parameter and suppresses notification before registering the listener. The
  former path read APVTS's asynchronously mirrored ValueTree and registered the
  listener first, allowing transient startup UI values to feed back into real
  parameters. That race exactly accounts for the hosted arm64 fresh-process
  screenshot divergence. `ModelDUnitContract` now asserts that constructing and
  dispatching the editor preserves the complete parameter inventory even when
  raw parameters intentionally lead the mirrored ValueTree.

Three forced post-fix Release lifecycle executions, nine fresh processes per
execution, passed with one normal-mode screenshot SHA-256 across all repeats:
`b98583ac9cee5ab86f41eaff964660f8bcf9159868f22e82a9b39fd5ebe7b3b9`.

Agent 03 then configured a second independent pair under
`/private/tmp/model-d-agent03-final.NECHVL`, again with space-bearing build
paths, strict project warnings, tests, and validators. Release fetched the
exact JUCE revision and Debug used its supported local override. Both complete
all-target builds passed, both CTest suites passed 9/9, every required label was
run separately with fail-on-zero behavior, actual-wrapper passed 3/3,
pluginval 1.0.4 strictness-10 passed 3/3 isolated processes, standalone passed
9/9, and linked evidence deep-verified. The expected non-mutating local auval
and aggregate release states remain `blocked`.

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `b2fbdacab34c043ec8771fad0cb2a12786c0c68267c6b439741fea1d7bb5a9d7` | `0124f255df63def39c4778758abeff200638cff27c6c9e3ff58523c16edc588c` | `ee0467b16989d20d3ee5b12ec5a4526512925c451a4fa8d11ba7cba3b2cf692a` |
| Release | `8770981a299f9a3f407c0ae681b2ff08c194d705d9587a2b735b6f75eb1624e3` | `2eabd99f6d3b01c05430ff5f77372060759fe3092a03e7bd817a30fc1e990019` | `6a624782e3a6ac994e2800b225fff2deffdcb3afce08a7d5e8bf4e8fb361d42f` |

The corrective hosted-run result is appended after its eight rows become
terminal; these local results do not substitute for that supported matrix.

## Factual external-gate refresh

Read-only inspection found no new input that can truthfully close the remaining
owner/access gates:

- the repository still has no Git tags or GitHub Releases;
- `cmake/ProductIdentity.cmake` still contains the explicit unapproved
  `yourcompany`, `Manu`, `Via9`, and `com.yourcompany` development values;
- no product-owner distribution-history, legal-identity, or JUCE licensing
  decision is present in the repository or PR context;
- `auval -a` does not list the MiniMoog AU in this account;
- no Steinberg VST3 SDK validator executable is discoverable; and
- Logic Pro, Ableton Live, REAPER, Cubase, and Nuendo are absent from the
  standard application locations.

Absence of tags, releases, or local artifacts is not proof that a historical
binary never shipped. BLD-007 still requires the product owner's factual
confirmation and reviewed identity. BLD-011 still requires ephemeral AU
registration, the SDK validator, and the designated commercial hosts.

## Current status and next action

BLD statuses are unchanged at this milestone. BLD-004 remains `pass`;
BLD-007 and BLD-011 remain `blocked`; all other rows remain `in-progress`.
Finish fresh local Debug/Release verification, publish the corrective commit,
then require one exact-head hosted run with all eight matrix rows green before
promoting any hosted-dependent BLD row.
