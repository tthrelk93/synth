# Workstream 02 — Build, Packaging, and Host Validation

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · [Agent 01 kickoff](16-agent-01-workstream-02-kickoff.md) · [Implementation handoff](17-implementation-handoff.md) · Next: [Parameter/state contract](03-parameter-automation-state-contract.md)

## Goal and user-visible outcome

Produce reproducible Debug and Release builds of one Model D MIDI instrument whose VST3/AU/standalone identities, buses, editor, tests, and artifacts behave consistently on supported hosts. Users see the product under Instruments, can play it with MIDI, may connect an auxiliary External Input when the host supports it, and do not receive a separate effect plug-in.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| BLD-001 | Replace the missing CMake build with a checked-in, reproducible CMake project. |
| BLD-002 | Remove the machine-specific Projucer module-path dependency and stop treating generated projects as authoritative. |
| BLD-003 | Pin JUCE 8.0.10 at commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`; document the dependency/license review. |
| BLD-004 | Make README commands match commands exercised by CI. |
| BLD-005 | Build one MIDI instrument with an optional auxiliary audio input; do not build an initial effect target. |
| BLD-006 | Correct VST3/AU categories, bus declarations, MIDI capabilities, and layout negotiation. |
| BLD-007 | Replace manufacturer/bundle placeholders through a release-blocking product-identity contract. |
| BLD-008 | Replace removed/deprecated JUCE font calls and reach zero project-owned warnings. |
| BLD-009 | Diagnose and gate standalone first-window visibility and lifecycle. |
| BLD-010 | Execute unit tests through CTest on every supported CI platform/configuration. |
| BLD-011 | Validate artifacts with pluginval, auval, scanning, instantiation, editor, MIDI, state, and bus smoke tests. |
| BLD-012 | Package deterministic artifacts with version, architecture, dependency, and validation manifests. |

## Implementation progress

Use only `not-started`, `in-progress`, `blocked`, `fail`, or `pass`. A requirement may be `pass` only when its acceptance criterion and verification evidence are linked. Agent 02 owns these entries under the current handoff.

| Requirement | Status | Evidence | Notes |
|---|---|---|---|
| BLD-001 | in-progress | [Build foundation](evidence/workstream-02/build-foundation.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) · [Hosted CI attempt](evidence/workstream-02/hosted-ci-execution.md) | Fresh local Debug/Release remains green and the branch is published; GitHub Actions failed four push/PR triggers before job startup during a critical outage, so supported hosted fresh-clone evidence remains. |
| BLD-002 | in-progress | [Build foundation](evidence/workstream-02/build-foundation.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Supported CMake no longer uses Projucer, generated amalgamations, `JUCE_DIR`, or the machine path; hosted cross-platform dependency audit remains. |
| BLD-003 | in-progress | [Dependency/license review](evidence/workstream-02/dependency-license-review.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Exact fetched/local revision verification and local licence inventory are complete; hosted dependency evidence and the owner's distribution-licence decision remain. |
| BLD-004 | pass | [README clean-clone evidence](evidence/workstream-02/ctest-ci-enforcement.md#readme-clean-clone-proof) | All five committed README commands pass verbatim from a fresh local clone; the separate hosted-CI Definition-of-Done gate remains open. |
| BLD-005 | in-progress | [Wrapper and bus contract](evidence/workstream-02/wrapper-bus-contract.md) · [Validation](evidence/workstream-02/validation-evidence.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Local generated/runtime capability, exact artifact set, actual-wrapper, and pluginval evidence is green in fresh Debug/Release; supported hosted evidence remains. |
| BLD-006 | in-progress | [Wrapper and bus contract](evidence/workstream-02/wrapper-bus-contract.md) · [Validation](evidence/workstream-02/validation-evidence.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Local AU/VST3 metadata, exhaustive bus negotiation/routing, actual-wrapper scan, and pluginval are green; AU/commercial/hosted category evidence remains. |
| BLD-007 | blocked | [Preflight](evidence/workstream-02/preflight.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | A repository/remote refresh still found no distribution-history decision or approved legal identity. Product-owner confirmation and reviewed legal manufacturer name, codes, and reverse-DNS domain are required. |
| BLD-008 | in-progress | [Warning baseline](evidence/workstream-02/warning-baseline.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Fresh local macOS Debug/Release builds are project-warning-clean with strict flags scoped away from JUCE; supported-CI reports remain. |
| BLD-009 | in-progress | [Standalone lifecycle](evidence/workstream-02/standalone-lifecycle.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Fresh local macOS arm64 Debug/Release normal, invalid, and no-device evidence passes 9/9 per configuration; hosted macOS Intel/Windows/Linux rows remain. |
| BLD-010 | in-progress | [CTest and CI enforcement](evidence/workstream-02/ctest-ci-enforcement.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) · [Hosted CI attempt](evidence/workstream-02/hosted-ci-execution.md) | Nine tests across seven meaningful labels pass in fresh local Debug/Release, sentinel failure is proven, and `actionlint` passes; the published matrix has not started because GitHub Actions rejected four triggers before creating jobs. |
| BLD-011 | blocked | [Validator and linked-manifest evidence](evidence/workstream-02/validation-evidence.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) · [Hosted CI attempt](evidence/workstream-02/hosted-ci-execution.md) | Fresh local actual-wrapper and pluginval gates pass; AU registration, the VST3 SDK validator, commercial hosts, and a completed supported hosted run remain unavailable. |
| BLD-012 | in-progress | [Artifact staging and build manifest](evidence/workstream-02/artifact-staging-manifest.md) · [Linked validation evidence](evidence/workstream-02/validation-evidence.md) · [Agent 02 continuation](evidence/workstream-02/continuation-preflight.md) | Fresh Debug/Release linked manifests deep-verify locally; the release aggregate remains blocked by BLD-007, BLD-011, and hosted evidence. |

## Current-code evidence

- The historical missing-CMake deficit is reproduced in [Preflight](evidence/workstream-02/preflight.md); commit `42d42c2` provides the supported top-level build and the README now mirrors the checked-in CI command/path contract.
- `MiniMoog.jucer` still contains `../../Downloads/JUCE/modules`, but the supported CMake build does not read it or tracked generated amalgamations.
- Tracked `JuceLibraryCode/JucePluginDefines.h` remains historical/non-authoritative. JUCE 8 CMake generation now reports synth/MIDI instrument capabilities, VST3 `Instrument|Synth`, and AU `aumu` through generated `Defs.txt`, `moduleinfo.json`, and `Info.plist` evidence.
- Commits `1d5be04`, `6b8896e`, and `405a704` implement and test exactly one optional `External Input`, required `Main Output`, and optional `Phones/Cue`; all 18 allowed layouts and representative invalid layouts are executable tests.
- Commits `835b3d4` through `a42ec79` add explicit development staging, actual-wrapper/pluginval/auval/standalone evidence, cryptographically linked build/validation manifests, and hardened ownership/symlink-path contracts.
- Historical `SignalFlowOverlay::buildFilterVizLayout` and tooltip use of `juce::Font(float)`/`getStringWidthFloat` was removed in commit `5fe79c6`; legacy metrics are explicit and width measurement now uses `GlyphArrangement`.
- The project-owned standalone shell uses `MoogMiniAudioProcessor::createEditor`; three isolated device modes assert first-window visibility, editor bounds, resizing, shutdown, and screenshots without probing or mutating real user settings.
- Agent 02's [continuation preflight](evidence/workstream-02/continuation-preflight.md) reconfirmed fresh local Debug/Release 9/9 CTest and linked validation and passed static workflow audit. The later [hosted CI report](evidence/workstream-02/hosted-ci-execution.md) records the authorized branch/PR publication and the GitHub Actions outage that rejected four push/PR triggers before job startup; no hosted-dependent status was promoted.

## Prerequisites, ownership, and merge conflicts

No code prerequisite. This workstream must land before all others. It owns top-level CMake, dependency pinning, target definitions, CI, packaging, product identity configuration, compile-warning policy, and host-validation scripts. It does not own parameter semantics or DSP behavior.

Likely conflicts: `MiniMoog.jucer`, generated `JuceLibraryCode`, `PluginProcessor` constructor/bus methods, font construction in `SignalFlowOverlay`, and README. Freeze the CMake target names and compile definitions before Workstream 03 begins.

The exact legal manufacturer name, four-character manufacturer code, product code, and reverse-DNS domain are not present in the repository. They must be supplied in reviewed `cmake/ProductIdentity.cmake`. Distribution configuration must fail if any value remains a placeholder; the implementer must not invent them.

## In scope

- CMake 3.24+, C++20, Ninja/Xcode/MSVC generators, JUCE 8.0.10 exact commit.
- Shared-code library, VST3, AU on macOS, standalone, unit-test executable, offline-render executable, and validation helpers.
- Supported CI baseline: macOS arm64 and x86_64; Windows x86_64; Linux x86_64. AU is macOS-only. VST3/standalone build on all three.
- Required stereo Main Output; optional stereo External Input bus, disabled by default; optional stereo Phones/Cue output bus, disabled by default. Mono main output may be accepted and rendered dual-mono only if a wrapper requests mono.
- Artifact scan, instantiate, editor, render, state, MIDI, and bus validation.

## Out of scope

AAX/AUv3/LV2, installers/signing/notarization credentials, a separate effect or music-effect target, mobile, DSP remediation, and choosing the company’s legal identity.

## Proposed architecture and data flow

```text
CMakeLists.txt
  -> cmake/Dependencies.cmake (immutable JUCE commit)
  -> cmake/ProductIdentity.cmake (reviewed identity; no placeholders)
  -> ModelDCore (all project code except wrappers/editor shell)
  -> ModelDPlugin (juce_add_plugin: VST3, AU, Standalone)
  -> ModelDTests / ModelDOfflineRenderer
  -> CTest -> host validators -> artifact manifest
```

Use `FetchContent` for the exact JUCE commit with `SYNTH_JUCE_SOURCE_DIR` as an offline/local override that must resolve to the same commit. Do not regenerate or edit `JuceLibraryCode` as part of normal builds. After the CMake target is accepted, remove generated amalgamation files from compilation; removal from version control is a separately reviewed cleanup within this implementation workstream.

`juce_add_plugin(ModelDPlugin)` contract:

- `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT FALSE`, `IS_MIDI_EFFECT FALSE`;
- `FORMATS VST3 AU Standalone` on macOS, `VST3 Standalone` elsewhere;
- VST3 category `Instrument|Synth`; AU type `aumu`;
- product name chosen by product identity file; plug-in/manufacturer codes are stable once released;
- no `COPY_PLUGIN_AFTER_BUILD` in CI; staging is explicit and logged.

The processor exposes Main Out, External Input, and Phones/Cue as named buses. Layout support is table-driven: main output mono/stereo; External Input disabled/mono/stereo (mono duplicated); Phones disabled/mono/stereo; no main input bus. Unsupported combinations return false without assertions.

## Public interfaces and build contracts

- Cache variables: `SYNTH_JUCE_SOURCE_DIR`, `SYNTH_BUILD_TESTS` (default ON for developer/CI), `SYNTH_BUILD_VALIDATORS`, `SYNTH_WARNINGS_AS_ERRORS` (ON in CI for project sources), `SYNTH_ENABLE_SANITIZERS`.
- Targets: `ModelDCore`, `ModelDPlugin`, `ModelDTests`, `ModelDOfflineRenderer`.
- CTest labels: `unit`, `state`, `dsp`, `midi`, `realtime`, `host`, `artifact`.
- `build-manifest.json`: product version, git commit/dirty flag, JUCE commit, compiler/SDK, configuration, architecture, artifact hashes, enabled formats, identity fields, test-report hashes.
- Release configure rejects empty values, `yourcompany`, example domains, duplicate/reserved four-character codes, and a version without an approved migration note.

## Backward compatibility

Keep the existing plug-in product code only if the product owner confirms shipped sessions exist under it; otherwise assign the reviewed product code before the first remediated release. The decision and evidence go in `ProductIdentity.cmake`. Never change a released product/manufacturer code or bundle ID casually: a change requires an explicit side-by-side migration strategy and host scan test.

Changing category from effect to instrument can make existing effect-slot sessions undiscoverable. If the current binary has been distributed, ship one compatibility release whose old identifier loads state and presents a migration notice, but do not develop an ongoing separate effect product. If there is no distribution evidence, document that fact and change the existing identity in place. This decision is made from release records, not implementer preference.

## Real-time audio constraints

Bus negotiation, identity checks, file-system staging, manifests, logging, and validator orchestration occur off the audio thread. Artifact smoke tests must instrument the render callback and fail on allocation or lock attempts; Workstream 11 provides the complete instrumentation.

## Edge cases and failure modes

- Network unavailable: local JUCE override is accepted only after commit verification.
- Host enables aux input after prepare: wrapper re-prepares cleanly; no stale pointers.
- Host supplies zero channels or oversized blocks: no crash; unsupported layout rejected or bounded render performed.
- Host lacks aux buses: instrument remains fully usable with internal normalled feedback.
- Standalone has no MIDI device/audio input: window still appears and on-screen keyboard works; clear device status is shown.
- Duplicate plug-in codes or placeholder identity: Release configure fails.
- Validator unavailable on a platform: job is `not-run`, never `pass`; release aggregate fails.

## Implementation sequence

1. Record whether any existing binary has shipped and obtain reviewed product identity.
2. Add pinned dependency and top-level targets; compile the current code without behavior edits.
3. Replace generated-module compilation and JUCE 8 font APIs; clean project-owned warnings.
4. Convert wrapper flags/categories and processor bus declarations to the fixed instrument topology.
5. Add test/offline targets and CTest discovery; make a deliberately failing test prove CI execution.
6. Add platform CI, artifact staging, manifests, pluginval/auval, and host smoke harnesses.
7. Reproduce standalone visibility at normal and 0/invalid audio-device states; fix lifecycle and add screenshot/window assertions.
8. Update README only after commands pass from a clean clone.

## Automated tests and measurable gates

- Clean configure/build from a path containing spaces succeeds with no undeclared local dependency.
- Dependency check resolves exactly commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`.
- Debug and Release compile with zero project-owned warnings; warnings from pinned third-party sources are captured separately and not promoted by blanket suppression.
- CTest discovers and executes at least one test per required label; injected sentinel failure makes CI fail.
- Processor reports synth=true, accepts MIDI, produces no MIDI, is not a MIDI effect, and returns the exact accepted/rejected bus-layout table.
- Note-on at sample 0 yields finite non-silent output under a known test patch; no MIDI yields deterministic silence where the patch has no free-running audible source.
- pluginval strictness 10 passes the VST3 in isolated process; AU passes `auval -v aumu <subtype> <manufacturer>`.
- Artifact scan/instantiate/editor-open/editor-close/state-restore is repeated without crash or leaked instance; repeat count is stored in the validator manifest rather than hidden in CI code.
- Build manifest hashes match staged files and a rebuild from identical inputs produces identical metadata other than explicitly excluded timestamps/signatures.

## Manual and host validation

Test at least one current supported version of Logic Pro, Ableton Live, Reaper, Cubase/Nuendo, and the standalone shell, recording exact version/OS/architecture. Confirm instrument menus, MIDI play, automation enumeration, aux-input discovery, optional Phones/Cue routing, editor visibility/resizing, save/reload, offline bounce, and multiple instances. Listening checks only confirm routing; no fidelity claim closes here.

## Definition of done

- [ ] BLD-001 through BLD-012 pass in the trace report.
- [ ] No machine-specific path is required or documented.
- [ ] Product identity is reviewed and contains no placeholder.
- [ ] No separate effect artifact is generated.
- [ ] CI, CTest, pluginval, auval, host matrix, and standalone-window evidence are attached.
- [ ] README commands were copied into and executed by CI.
- [ ] Generated project files are no longer authoritative.

## Completion-report evidence

Include clean-clone commands and logs; CMake/JUCE/compiler versions; exact dependency commit; product-identity approval; warning report; CTest XML; pluginval and auval logs; bus-layout table output; artifact hashes; screenshots of instrument categorization and the first standalone window; host/version matrix; shipped-identity investigation; and `git diff --check`.

## Primary technical references

- JUCE [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md) and [8.0.10 release](https://github.com/juce-framework/JUCE/releases/tag/8.0.10).
- JUCE [`AudioProcessor`](https://docs.juce.com/master/classAudioProcessor.html).
- Steinberg [VST3 categories](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Miscellaneous.html).
- Apple [Audio Unit types and auval](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/AudioUnitDevelopmentFundamentals/AudioUnitDevelopmentFundamentals.html).
- Tracktion [pluginval](https://github.com/Tracktion/pluginval).
