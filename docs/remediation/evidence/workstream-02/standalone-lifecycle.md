# Workstream 02 Standalone Lifecycle Evidence

Implementation commits: `bddc704`, `b2ce79f`, `1864309`, `2164ff0`, and
`a42ec79` on `codex/workstream-02-build`.

Independent review approved the final lifecycle and evidence-ownership
contracts with no Critical, Important, or Minor findings.

## Implemented lifecycle

- The project-owned standalone shell creates the processor, audio-device
  manager, player, document window, and existing custom editor with explicit
  ownership and teardown order.
- The first window is shown after construction and supports editor-driven
  resizing through a JUCE constrainer. Options, audio settings, state/preset
  save/load/reset, close/system quit, callback wiring, and multiple instances
  remain available.
- `normal`, `invalid`, and `no-device` lifecycle modes run in fresh processes.
  No-device mode performs no device discovery, callback attachment, or MIDI
  enumeration.
- Test reports, screenshots, settings, presets, temporary files, and environment
  variables are isolated under a marked build-owned validation root. Raw CLI
  paths must be absolute before file construction, and lexical/canonical
  containment plus symlink-ancestor checks precede every mutation.
- Normal application startup does not probe lifecycle-test settings paths.
  The test seam does not read or write the real default settings path.

## Local evidence

Fresh macOS arm64 Debug and Release runs launched the staged standalone three
times in each device mode: 9/9 processes passed per configuration. Each run
asserted a visible non-empty window, expected editor bounds, resize propagation,
device-mode behavior, settings/preset isolation, deterministic close, and a
valid non-empty PNG screenshot.

The final Release report at commit `a42ec79` has SHA-256
`fabcdf09cef6ba36939cf080fe82c56fcd39da7d7169bf9696156143c8c31214`.
The three deterministic screenshot hashes were:

| Mode | SHA-256 |
|---|---|
| normal | `69664896c9038b095fd75c9996f915ec9c3320ac5c7bb1fc8bd94359489ab019` |
| invalid | `54293bbfef4a514ed6665680660d312daa34e560e95325130c6f16191d7f1c9e` |
| no-device | `fdf20d5086977d80dc67121de745efff404d5e056be098ab1202ea3351165158` |

Positive verification was paired with missing/corrupt report and screenshot,
hash, path, configuration, executable, resize, isolation, and symlink negative
tests. `ModelDValidatorPathSafety` additionally proves pluginval and standalone
orchestration cannot write through external symlink ancestors; alias-spelled
paths that resolve to the same owned build remain compatible.

BLD-009 remains `in-progress`, not `pass`, because only local macOS arm64
Debug/Release evidence exists. The checked-in hosted matrix has not executed on
macOS Intel, Windows, or Linux.
