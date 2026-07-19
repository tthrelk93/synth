# Workstream 02 CTest and CI-Enforcement Evidence

Implementation commits: `d1a8628` and `abc4d31` on
`codex/workstream-02-build`.

Independent review: approved after two Important cross-platform issues were
fixed. PowerShell now exits immediately for every failed label invocation, and
format tests use JUCE's authoritative `JUCE_PLUGIN_ARTEFACT_FILE` property.

## Test-first evidence

The pre-change RED configure discovered only the single unlabeled
`ModelDProcessorContract`; each required label reported zero tests. The GREEN
suite initially registered seven independently meaningful cases and now
contains nine tests in validator-enabled builds:

| Label | Test | Existing contract exercised |
|---|---|---|
| `unit` | `ModelDUnitContract` | finite/non-silent foundational oscillator render |
| `state` | `ModelDStateSmoke` | existing serialize/restore smoke and host-restore marker |
| `dsp` | `ModelDDspContract` | deterministic silence, finite A440, named-bus processing |
| `midi` | `ModelDMidiSampleZero` | finite/non-silent sample-zero note-on under an existing patch |
| `realtime` | `ModelDRealtimeSmoke` | zero/short/bounded blocks plus reset/reprepare smoke; no Workstream 11 instrumentation claim |
| `host` | `ModelDHostContract` | wrapper flags, exact layout table, VST3 input role, named-bus routing |
| `artifact` | `ModelDFormatArtifacts` | JUCE-generated VST3/Standalone product and macOS AU product paths |
| `host` | `ModelDStandaloneLifecycle` | actual staged standalone lifecycle plus positive/negative evidence contracts |
| `artifact` | `ModelDValidatorPathSafety` | ownership, symlink-ancestor, zero-external-mutation, and alias-path contracts |

Every per-label CI execution uses `--no-tests=error`, and the PowerShell loop
checks `$LASTEXITCODE` immediately after discovery and execution so a later
label cannot mask an earlier failure.

## Local Debug/Release results

Fresh build paths contained spaces, used the exact JUCE checkout, and enabled
project warnings as errors. Debug and Release all-target builds passed; after
validator/lifecycle integration full CTest passed 9/9 in both. Each required
label independently discovered and ran at least one passing test; both
offline-render executions produced 480 finite samples.

Verbose artifact output resolved the current macOS products as:

```text
.../VST3/MiniMoog.vst3
.../Standalone/MiniMoog.app
.../AU/MiniMoog.component
```

JUCE's platform-aware property resolves the corresponding VST3 package and
native standalone product on Windows/Linux.

## Sentinel proof

`SYNTH_CI_INJECT_FAILING_SENTINEL` defaults OFF. With it enabled, the raw full
CTest command exited 8 and printed `MODEL_D_CI_SENTINEL_FAILURE`. Reconfiguring
the same tree with the option OFF returned the normal suite to 7/7 passing. A
manual workflow-dispatch boolean injects the same raw failure into a bounded
Linux Debug job; the CTest command is not converted to success.

## Supported workflow and remaining evidence

`.github/workflows/ci.yml` declares Debug and Release for Ubuntu 24.04 x86_64,
Windows 2025 x86_64, macOS 15 arm64, and macOS 15 Intel x86_64. It uses
`actions/checkout@v6.0.2`, space-containing build paths, Linux JUCE build
prerequisites, warnings-as-errors, full CTest, and fail-on-zero per-label runs.
YAML parsing and `git diff --check` passed.

The eight hosted jobs have not executed from this local task. BLD-010 therefore
remains `in-progress`, not `pass`; Windows, Linux, macOS Intel, and hosted
macOS arm64 results are `not-run` until a workflow run supplies durable logs.
