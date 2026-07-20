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

## First corrective run and second portability pass

Commit `ddbb50ea2a5774a3fc1f95b7a915f23b3df0a8d5` published the first
root-cause corrections. Push run
[`29717442208`](https://github.com/tthrelk93/synth/actions/runs/29717442208)
materialized all eight rows at that exact commit. The duplicate
pull-request-synchronize run `29717443835` was cancelled before jobs started.

The first three deterministic failures in the retained push run were sufficient
to identify two additional portability boundaries, so Agent 03 cancelled the
remaining rows rather than consume hosted capacity after the result could no
longer become green:

- Windows Debug and Release reached the project build and MSVC `/WX` exposed
  additional double-to-float, atomic-float-to-choice-index, height-to-float,
  and parameter-hiding diagnostics in `PluginProcessor`, `SmokeComponent`, and
  `WaveformDisplay`.
- Linux Debug compiled beyond the former curl-configuration defect, then
  `cc1plus` was killed and the runner shut down while the unconstrained
  all-target parallel build was active. The prior Linux Release row had already
  shown that the source compiles on the same image; the new failure is bounded
  build-memory pressure, not a missing source dependency.
- The other five rows were cancelled after the deterministic failures. Their
  partial logs and six available validator uploads were retained rather than
  represented as test results.

The combined run log SHA-256 is
`50dc9fd0b17b1327dcb05ae8983aa306b4dbf7bc1b6962eaa7b6159d7d6b1d44`.
All nine job-log files and 76 files from the six available uploads were retained
under `/private/tmp/model-d-agent03-corrective.DfIpdC`; a generated
`SHA256SUMS.txt` covers 89 regular evidence files. This cancelled diagnostic run
is not used to promote any BLD status.

The second portability pass makes every intentional numeric conversion explicit,
renames the remaining waveform count argument that hid a member under MSVC, and
bounds each workflow build invocation to `--parallel 2`. It does not weaken the
warnings-as-errors policy, CTest matrix, validation targets, or release gates.

After those changes, the independent space-bearing
`/private/tmp/model-d-agent03-final.NECHVL` Debug and Release trees again passed
complete all-target strict builds, 9/9 CTest, every required label,
actual-wrapper 3/3, pluginval 1.0.4 strictness-10 in 3/3 isolated processes,
standalone lifecycle 9/9, and linked-manifest deep verification. The following
hashes supersede the earlier values from those same trees because the build and
validation evidence was regenerated after the second pass:

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `1bbd79bd8fe8a220b010dada26fa106dba3d9a1c9237c9b5fe999c999dda8d69` | `f3d00b9e89c6d9598a6a571229b1f1acebb21474929e3092ab6647e26e1d2ed5` | `1afe4b3197fdd454b10ea11bc339a48ba07df4a14d6edadd2e329cd326c627b2` |
| Release | `44b7830973d3e12695cdaf0659e4192f4d263b2123cf888979550414dbbc5f5f` | `27ff012ef9b81a383ce36544e99a672838a75d9f83d179e34ba92f2b244686a9` | `0bacedc9c6757f74244a01789cc3af7b74d4dee34e43738292b099e5d4e75ced` |

These local results authorize publication of the second correction; they do not
substitute for the required all-green supported matrix.

## Second corrective run and final compatibility sweep

Commit `4314fd15716920907064cbd8b892e8c85be5059c` published the second pass.
Push run
[`29717915136`](https://github.com/tthrelk93/synth/actions/runs/29717915136)
materialized all eight exact-head rows; duplicate pull-request run `29717916604`
was cancelled. Windows Debug and Release then reached later translation units
that the previous warning failures had prevented MSVC from compiling, exposing
the remaining project-owned UI numeric-boundary and local-hiding warnings in
`CustomSliderLookAndFeel`, `PitchWheelLookAndFeel`, and `PluginEditor`. Linux
Debug completed the full bounded compile through 98% and then rejected the
test-only multi-character AU literal under GCC `-Werror=multichar`. This proves
that `--parallel 2` resolved the earlier hosted Linux memory exhaustion.

Agent 03 retained the terminal run before changing source. The combined log
SHA-256 is
`c8f8c6fa6d1b547df1e50dc1c8b989cf6e07150f559da42c0f7a35e3607fdaaf`.
All nine job logs and 87 files from all eight partial uploads are under
`/private/tmp/model-d-agent03-corrective2.RAKp7d`; its `SHA256SUMS.txt` covers
98 evidence files. As with the prior diagnostic attempt, no BLD status is
promoted from a cancelled non-green run.

The final compatibility sweep:

- makes all remaining UI layout, opacity, geometry, and slider-value
  conversions explicit at their existing truncation boundaries;
- stringizes the generated AU type token in the unit contract, preserving the
  exact `aumu` assertion without compiling a non-portable multi-character
  integer literal;
- updates the three documented build commands to the same `--parallel 2`
  policy exercised by CI; and
- isolates the editor-startup regression in a unique temporary preset
  directory, verifies the editor actually used it, restores the prior override,
  and removes the temporary directory so the test neither reads nor writes the
  real user preset area.

An independent read-only review found no Critical issue in the production
fixes and confirmed that authoritative APVTS initialization, public JUCE
definitions, explicit conversions, bounded workflow builds, and strict gates
are technically sound. Its three Important findings were the README mismatch,
test preset isolation, and stale final handoff state; the first two are fixed in
this sweep and the handoff is reconciled only after the final hosted result is
known.

The final local regeneration produced these superseding hashes:

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `c69e0204d246d203a61e635331eb1b406ded3347ec722b425e87ba73b78e38b2` | `02e04dedaa3cfb5ec6426b611d0b39ba1ea57784174b77ea8fdaabd8b60fd0d7` | `caf698e34ceb9dfe383fcfeea937f97124918cae13dd6e408815c834f21fa058` |
| Release | `dece4a18a6b0897544b233c098f57efb039d65e2d6efd0edf2b9962a9c1c2b49` | `be8395e398800c01c227097d955ce971e6aa88201f48a8eb28121290228622f0` | `e6cfa6e2f53b6cb271cb04d4a5e9332781e6a001d498ca43616809ac527259cd` |

## Third corrective run and verified Linux display

Commit `12f225c14369b0ee2916bc65f7eb4b4a785e4a9e` published the final
compatibility sweep. Push run
[`29718676605`](https://github.com/tthrelk93/synth/actions/runs/29718676605)
materialized all eight exact-head rows; duplicate PR run `29718678657` was
cancelled. Both macOS arm64 rows completed the entire validation chain. Windows
Debug and Release reached the last two previously uncompiled warning sites:
one test vector index converted from `size_t` to JUCE's `int` index, and the
actual-wrapper tool's global `sampleRate` hid a JUCE parameter under MSVC.

Both Linux rows completed their strict builds, then all nine standalone
lifecycle processes reached JUCE but reported no display under individual
`xvfb-run -a` launches. The retained process logs contain the JUCE banner and
ALSA discovery output before the display rejection, proving this was X11
discovery rather than an application crash or screenshot mismatch. Agent 03
cancelled the remaining two macOS x86_64 rows once the exact-head run could no
longer become green; cancelled rows are not represented as failures of their
unfinished validation steps.

The combined run log SHA-256 is
`54f669ff45500f33b200a4775c70362a59b70cd0e6db351fafb786ff22449951`.
All eight uploads were retained under
`/private/tmp/model-d-agent03-corrective3.QhB1TT`. The checksum index covers
177 regular evidence files and has SHA-256
`ff5e01531e1e299f8d896c520725a2b8724149d86e505aa113d307fcdc62bd79`;
independent `shasum -c` verification passes.

The remaining Windows conversions are now explicit. Hosted Linux now starts
one job-scoped Xvfb server without authentication, polls it with `xdpyinfo`,
and only exports `DISPLAY` after readiness succeeds. Lifecycle, actual-wrapper,
and pluginval validators reuse that display only under CI's explicit verified
reuse marker; ordinary Linux invocations retain the former isolated `xvfb-run`
behavior. Standalone, actual-wrapper, and pluginval
reports record `native`, `inherited-x11`, or `xvfb-run` as appropriate, and the
deep verifiers bind exact normalized commands to that declared provider. A
contract test requires the hosted readiness step and the provider evidence.

Fresh local strict Debug and Release builds then passed, each with 9/9 CTest,
standalone lifecycle 9/9, actual-wrapper 3/3, pluginval 1.0.4 strictness-10 in
3/3 isolated processes, and linked-manifest deep verification. The local
reports truthfully record the native macOS display provider. Their hashes are:

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `1306365dada2c876affd71015698950c1ff99b54286acb0221c4d7b58079c5cf` | `26c882620d7a6c425199b32aa79c15e6861f0a60765a11c5cc1a9b7e1214dc07` | `fb5151825a7c7e29e6708f62ed502059ecd1b7ec348707dd8f843039fe5de62b` |
| Release | `833eef3e843d025b61e9f08c16e198229884a3e5de6e9e670d7865abaabec26d` | `246932c0493e208eb7f6be79518d4fdbf8b755fd37bbdbade5ebf267e502bdd2` | `84bb0644582390fbd759d5c91b8735b106b37e73deeaf846e31295796b8db4e6` |

## Fourth corrective run and final lifecycle findings

Commit `f2fbc9210db7201c167e45c8c7522e1c5f0d97a5` published the verified
job-scoped display implementation and remaining Windows conversions. Push run
[`29720578388`](https://github.com/tthrelk93/synth/actions/runs/29720578388)
materialized all eight exact-head matrix rows; duplicate pull-request run
`29720580019` was cancelled. All four macOS rows completed the entire strict
build, test, validator, ephemeral-AU, and linked-manifest chain. Both Linux
rows completed their strict builds, 9/9 CTest, every separately required
label, and the direct standalone lifecycle gate on the verified inherited X11
display. Both Windows rows completed strict warnings-as-errors builds and the
first six CTest contracts.

The retained artifacts isolated three final lifecycle boundaries:

- Linux actual-wrapper validation entered JUCE's borderless temporary-window
  path on bare Xvfb. JUCE 8.0.10 then issued `X_ChangeProperty` through an
  absent `_NET_WM_WINDOW_TYPE` atom and X11 terminated the process with
  `BadAtom`. The wrapper smoke now uses a normal titled, resizable host-style
  peer; pluginval continues to exercise its independent hosted-editor path.
- Windows Debug's first normal screenshot differed from repeats two and three
  by only 25 decoded text-antialias pixels while all application reports and
  assertions were byte-identical. A discarded full offscreen render now proves
  that the in-process renderer is usable before the retained screenshot; the
  following hosted run established that process-external cache priming also is
  required on a cold Windows runner.
- Windows Release additionally exposed 79,261 changed decoded pixels bounded
  entirely to the keyboard. `PianoKey::isKeyPressed` had no initializer, so a
  release build could paint arbitrary keys as pressed. It now begins explicitly
  released; the lifecycle report asserts that initial state before performing
  the note-on/off round trip, and the source contract prevents regression.

The completed diagnostic run log has SHA-256
`afd0f6980e40f10c84e77ae1eaecb2776cb93cd150ea8de46dd821027d583676`.
All eight uploads and the complete run metadata/log were retained under
`/private/tmp/model-d-agent03-corrective4.lqw1Lx`. Its checksum index covers
296 regular evidence files and has SHA-256
`77c98e54921ca3f2e6204a13a56103f1bd39e39a26d60d017364dd9300985108`.
No BLD status is promoted from this non-green diagnostic run.

After the three fixes, the space-bearing local Debug and Release trees again
passed strict all-target builds, 9/9 CTest, standalone lifecycle 9/9 with
identical per-mode evidence across fresh processes, actual-wrapper 3/3,
pluginval 1.0.4 strictness-10 in 3/3 isolated processes, and linked-manifest
deep verification. Local auval and the aggregate release status remain
truthfully blocked on the declared external gates.

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `64992941832f29f1c0ccf466a8cbb0696695911eb48299b1148a4f2c0add0c10` | `c9e039cb627c4959ad2ea15a185a019611debe29cd68709e945c19d0bb402d8e` | `bca92415798a3c3d4a2c788509258321e9633482dfa363dfb9a81f434f7ddd05` |
| Release | `692782ba79eaeaf3731d2a644733b144eb7fc43e77995b0ec0ed5ef5802f556f` | `8dbd45284a2455020b231789df5bff980ab87ba3fc4c631a6b7b739d6d2e62b1` | `e797967f899065cfc854253a8f386b185c8831b5b57ce882f36c535aea9f3d6c` |

## Fifth corrective run and verified renderer/window-manager boundaries

Commit `f13df447f14232bdaa71149a6bb7d8a08bb2ceda` published the deterministic
piano-key state, in-process render assertion, and host-style wrapper peer. Push
run [`29721931520`](https://github.com/tthrelk93/synth/actions/runs/29721931520)
materialized all eight exact-head rows; duplicate pull-request run `29721933451`
was cancelled. All four macOS rows completed the full chain. Both Linux rows
completed the strict build, 9/9 CTest, every required label, and direct
standalone lifecycle validation. Both Windows rows completed their strict
warnings-as-errors builds and the first six CTest contracts.

The terminal artifacts then bounded the remaining failures without weakening
any gate:

- Both Linux actual-wrapper processes reached the second editor-open case and
  failed identically with `BadAtom`, major opcode `X_ChangeProperty`, atom
  `0x0`, and serial 75. A titled host-style peer therefore is necessary but
  not sufficient on bare Xvfb: JUCE requires the EWMH atoms published by a real
  window manager. Hosted Linux now installs and starts Openbox only after Xvfb
  passes `xdpyinfo`, then withholds the verified-display marker until `xprop`
  confirms `_NET_SUPPORTING_WM_CHECK`.
- Every Windows application report was `pass`, with empty failures and true
  initial-key and renderer assertions. The harness nevertheless rejected all
  nine runs because native backslash paths were compared lexically against
  CMake-style forward-slash paths. Reported native paths are now normalized
  before the existing absolute-path, existence, symlink, and containment
  checks rewrite them to build-relative evidence.
- After that independent path defect, the only byte divergence remained the
  first normal screenshot. Debug repeat 1 was
  `7696186f48d637f0648cc517f2874980fa6d54818ffb2ddeb4cc5916e2046a8b`
  versus stable repeats 2/3
  `c248cf3c5d2ff5b444f0c6f76c3b5da88ff80be72dc08bd8386747439d44aedf`;
  Release repeat 1 was
  `dab4acef700024ac889df8d9d5d132e12165058487d760d29251624cbc50b764`
  before converging on the same stable hash. The runner now launches and
  validates one disposable normal-mode process, removes all of its build-owned
  output, then starts the unchanged measured 3-by-3 fresh-process set. The
  aggregate records `renderer_warmup_provider: fresh-process`, and the deep
  verifier and source contract require it.

The complete run metadata, combined log, and all eight uploads are retained
under `/private/tmp/model-d-agent03-corrective5.noBJta`. Its checksum index
covers 296 regular files and has SHA-256
`5341a604a3ef7eb4b3a8ce61ad2c4a1efa1ad7906c1e434023bcfe52fb851ac6`;
independent `shasum -c` verification passes. The combined run log SHA-256 is
`af54d0cc9848045a7c94cb128fcfd76d9c38d46e2e9835e8c3e60cf2ee029065`
and the run-summary SHA-256 is
`51ebab9702c0c548e01c0dfbcb615a98777819af619b466950b32111c726a47c`.
This non-green diagnostic run does not promote a BLD status.

Regression contracts were observed failing before each correction and passing
afterward. The workflow also parses as YAML. The space-bearing local Debug and
Release trees then passed strict all-target builds, 9/9 CTest, every required
label with fail-on-zero behavior, standalone lifecycle 9/9 with identical
per-mode evidence, actual-wrapper 3/3, pluginval 1.0.4 strictness-10 in 3/3
isolated processes, and linked-manifest deep verification. The superseding
local hashes are:

| Configuration | Build manifest SHA-256 | Validation manifest SHA-256 | Standalone report SHA-256 |
|---|---|---|---|
| Debug | `4dac4421dac0faf0f81bcede3f6f03c6e984770cc75115882633722e94f3d92f` | `099bb067df209fcf3946274cedad1cc0e4b52fab77c485aa7b0bbad820fba11d` | `4afa1972bf9fbb8348a1bbe7bc3c6da0a92f6089c66b052e8370767151900dbe` |
| Release | `c281e8db48ae2d261e555f903b536a508d833680c6e3daeabaa68d5dcaa2c87f` | `6e158581e00364c4fcc3021465900e1355357694530c6330025b3f4b76075997` | `947de977fbe5adf9b660c2cd0d1b78c64b68bb6120a1a82266536f0e61578c10` |

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

BLD statuses remain unchanged through these diagnostic attempts. BLD-004
remains `pass`; BLD-007 and BLD-011 remain `blocked`; all other rows remain
`in-progress`. The fifth correction is locally green in Debug and Release
all-target, 9/9 CTest, lifecycle, actual-wrapper, pluginval, and linked-validation
execution. Publish that correction and require one exact-head hosted run with
all eight matrix rows green before promoting any hosted-dependent BLD row.
