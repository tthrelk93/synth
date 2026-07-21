# Workstream 03 parameter registry foundation

## Scope and compatibility boundary

This change centralizes the existing 44-parameter layout without correcting or
extending it. The legacy IDs, version hints, host order, parameter subclasses,
host-visible names and labels, physical ranges, defaults, choice text, and
automatable flags remain unchanged. State serialization and migration are not
changed.

The Task 1 compatibility inputs remain byte-identical:

| Fixture | SHA-256 |
| --- | --- |
| `Tests/fixtures/parameters/legacy-parameter-inventory.json` | `7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d` |
| `Tests/fixtures/state/legacy-default-state.xml` | `07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21` |
| `Tests/fixtures/state/legacy-representative-state.xml` | `e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa` |

## Architecture

`Source/ParameterRegistry.h` exposes two operations: read-only descriptor
access and APVTS layout construction. `Source/ParameterRegistry.cpp` owns the
single ordered 44-entry descriptor table and the focused shared choice arrays.
`MoogMiniAudioProcessor::createParameterLayout()` now delegates directly to
`ParameterRegistry::createParameterLayout()` and contains no per-parameter
metadata.

Each descriptor field has one responsibility:

- `id`: stable host/state identifier; all existing spellings are preserved.
- `semanticKey`: unique code-facing meaning independent of legacy host copy.
- `versionHint`: JUCE `ParameterID` version hint, currently `0` for every
  legacy parameter.
- `displayName`: full host-visible name.
- `shortLabel`: current host-visible value label; deliberately empty to
  preserve the legacy inventory.
- `unitKey`: typed semantic unit lookup key. `none` is explicit because this
  compatibility pass cannot add a host-visible unit.
- `kind`: JUCE construction family: choice, float, or bool.
- `rangeStart`, `rangeEnd`, `rangeInterval`, `rangeSkew`, and
  `symmetricSkew`: physical range contract captured by Task 1.
- `physicalDefault`: default in physical rather than normalized coordinates.
- `choiceValues`: ordered choice copy for choice parameters; empty for float
  and bool parameters. Identical legacy lists share storage.
- `mapping`: typed normalized mapping/taper key (`indexedChoice`, `linear`, or
  `boolean`).
- `automatable`: host automation flag.
- `smoothing`: typed downstream smoothing policy. `none` records current
  behavior rather than introducing DSP changes.
- `persistence`: typed persistence scope. `apvtsState` records the existing
  unversioned APVTS state behavior.

## TDD evidence

### RED

The focused test and CTest registration were added before any registry
implementation. A temporary `__has_include` seam made the missing unit an
intentional test assertion.

Command:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 4 && \
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^ModelDParameterRegistry$' --output-on-failure
```

Expected observed result: `ModelDParameterRegistry` failed with
`ParameterRegistry.h must provide the centralized legacy descriptor registry`;
0/1 tests passed. The build itself succeeded, confirming the failure was the
missing registry behavior rather than unrelated compilation.

### GREEN

After adding the registry and replacing the inline processor layout, the
focused registry test passed:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 2 && \
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^ModelDParameterRegistry$' --output-on-failure
```

Observed result: 1/1 passed, 0 failed.

The final focused registry and legacy-fixture run was:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 2 && \
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^(ModelDParameterRegistry|ModelDLegacyParameterFixtures)$' --output-on-failure
```

Observed result: 2/2 passed, 0 failed. The registry test checks descriptor
count, identity uniqueness, explicit typed metadata, finite ranges/defaults,
choice applicability, descriptor-to-live-layout equality, exact host inventory,
and both exact unversioned state fixtures.

## Full verification

After rebuilding the complete Release tree, full CTest was run once:

```sh
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release --output-on-failure
```

Observed result: 12/12 passed, 0 failed. The passing label summary retained all
seven required labels: `artifact`, `dsp`, `host`, `midi`, `realtime`, `state`,
and `unit`.
