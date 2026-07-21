# Workstream 03 state v2 and v0 migration evidence

## Scope and production seam

`Source/StateContract.h/.cpp` is the production host-state contract consumed by
`MoogMiniAudioProcessor`. It parses and validates host bytes into a temporary
`PreparedRestore`, constructs a complete canonical tree and a complete APVTS
tree, and returns a typed `RestoreResult`. The processor calls
`apvts.replaceState()` only after preparation succeeds, then installs the
already-built canonical metadata and sets `restoredStateFromHost`. A failed
attempt returns its own diagnostic directly and does not replace the previous
successful parameters, metadata, contour marker, or lifecycle flag.

`PresetManager` does not own or duplicate this schema. The v0 parser accepts a
bounded legacy `presetName` property and retains it under extensions so
Workstream 13 can route legacy preset documents through this same production
seam later.

## Exact canonical schema and ordering

Properties are created in the shown order. Children and repeated entries are
also emitted in the shown order.

```text
modelDState
  properties: stateVersion, engineVersion, calibrationProfileId
  parameters
    PARAM x48
      properties: id, value
  compatibility
    properties: contourContract, sourceVersion, migratedAtVersion, sourceHash
  ui
    properties: viewMode
  recreation
    properties: enabled, linkedPresetId
  extensions
  migrationLog
    ENTRY xN
      properties: from, to, action, warningCode
```

`stateVersion` is the strict integer `2`; only this property dispatches the
schema. `engineVersion` is the product version string. The 48 `PARAM` children
are always emitted in `ParameterRegistry::descriptors()` order and `value` is
the physical APVTS value. Native state is `canonicalContours`, source version
2, migrated-at version 0, empty source hash, `authentic`, recreation disabled,
empty linked preset, empty extensions/log, and calibration `baseline`.

Migrated state is `legacyCrossedContours`, source version 0, migrated-at
version 2, a lowercase SHA-256 source hash, and retains its extensions and
migration entries on every later v2 save. Serialization contains no time,
random identifier, platform path, or locale-dependent number formatting.

## Stable restore result codes

| Enum | Stable string | Meaning |
|---|---|---|
| `success` | `success` | Native v2 accepted. |
| `successMigratedV0` | `success_migrated_v0` | Unversioned `Parameters` accepted and migrated. |
| `malformedData` | `malformed_data` | Null, empty, corrupt, truncated, or non-JUCE XML binary. |
| `wrongRoot` | `wrong_root` | Root is neither `Parameters` nor `modelDState`. |
| `missingStateVersion` | `missing_state_version` | `modelDState` lacks `stateVersion`. |
| `invalidStateVersion` | `invalid_state_version` | Version is not a strict numeric integer token. |
| `nonIntegerStateVersion` | `non_integer_state_version` | Numeric version is fractional or non-integer lexical form. |
| `negativeStateVersion` | `negative_state_version` | Version is below zero. |
| `futureStateVersion` | `future_state_version` | Version is above 2. |
| `unsupportedStateVersion` | `unsupported_state_version` | Version is nonnegative but is not 2. |
| `missingRequiredProperty` | `missing_required_property` | A reserved node lacks a required property. |
| `missingRequiredChild` | `missing_required_child` | A canonical reserved child is absent. |
| `duplicateRequiredChild` | `duplicate_required_child` | A canonical reserved child occurs twice. |
| `duplicateParameterId` | `duplicate_parameter_id` | A parameter ID occurs twice. |
| `missingParameterId` | `missing_parameter_id` | A v2 registry parameter is absent. |
| `unknownParameterId` | `unknown_parameter_id` | An unknown ID occurs directly in v2 `parameters`. |
| `invalidNumericValue` | `invalid_numeric_value` | A numeric value has invalid syntax or trailing junk. |
| `nonFiniteValue` | `non_finite_value` | A value is NaN, infinity, or overflows finite binary32. |
| `outOfRangeValue` | `out_of_range_value` | A physical value is outside descriptor bounds. |
| `invalidDiscreteValue` | `invalid_discrete_value` | A choice is not an exact index or a bool is not exactly 0/1. |
| `unsafeExtensionStructure` | `unsafe_extension_structure` | Extension name, depth, node, value, total budget, text, or nesting is unsafe. |
| `unexpectedProperty` | `unexpected_property` | A reserved node has an unrecognized property. |
| `unexpectedChild` | `unexpected_child` | A reserved node has an unrecognized/nested child. |
| `invalidCompatibility` | `invalid_compatibility` | Compatibility tuple, hash, calibration, or engine metadata is invalid. |
| `invalidUi` | `invalid_ui` | View mode is not `authentic` or `signalFlow`. |
| `invalidRecreation` | `invalid_recreation` | Recreation enabled/link metadata is invalid. |
| `invalidMigrationLog` | `invalid_migration_log` | A log entry is malformed, unsafe, or invalid for native state. |

## Complete v0 parameter migration/default table

Every recognized ID that is present is strictly validated and copied under the
same ID. This includes all 48 current registry IDs if a newer raw
`Parameters` tree contains them. No Filter/Loudness pair is swapped. If an ID
is missing, these physical defaults are used and one
`defaulted.parameter.<id>` entry is appended in registry order.

| Registry ID | Missing v0 physical default | Note |
|---|---:|---|
| `osc1Waveform` | 0 | |
| `osc2Waveform` | 0 | |
| `osc3Waveform` | 0 | |
| `osc1Range` | 2 | |
| `osc2Range` | 2 | |
| `osc3Range` | 2 | |
| `osc1Vol` | 0 | |
| `osc2Vol` | 0 | |
| `osc3Vol` | 0 | |
| `tune` | 0 | Historical v0 default; new v2 instances use 5 (`Zero`). |
| `osc2Freq` | 8 | |
| `osc3Freq` | 8 | |
| `filterCutoff` | 0.5 | |
| `filterEmphasis` | 0 | |
| `filterContour` | 0 | |
| `outputVolKnob` | 0 | |
| `extInputVolKnob` | 0 | |
| `ctrlGlideKnob` | 0 | |
| `ctrlModMixKnob` | 0 | |
| `filterAttackTimeKnob` | 0 | Retained under this exact ID. |
| `filterDecayTimeKnob` | 0 | Retained under this exact ID. |
| `loudnessAttackTimeKnob` | 0.5 | Retained under this exact ID. |
| `loudnessDecayTimeKnob` | 0.5 | Retained under this exact ID. |
| `filterSustainKnob` | 0 | Retained under this exact ID. |
| `noiseVolKnob` | 0 | |
| `loudnessSustainLevelKnob` | 0 | Retained under this exact ID. |
| `outputPhonesVolKnob` | 0 | |
| `feedbackKnob` | 0 | |
| `modWheelValue` | 0 | |
| `pitchWheelValue` | 0.5 | |
| `osc1OnOff` | 0 | |
| `osc2OnOff` | 0 | |
| `osc3OnOff` | 0 | |
| `a440HzOnOff` | 0 | |
| `osc3CtrlMode` | 0 | |
| `oscModSwitch` | 0 | |
| `noiseOnOffSwitch` | 0 | |
| `extInputVolSwitch` | 0 | |
| `whitePinkSwitch` | 0 | |
| `filterModSwitch` | 0 | |
| `keyboardCtrlSwitch1` | 0 | |
| `keyboardCtrlSwitch2` | 0 | |
| `decaySwitch` | 0 | |
| `glideSwitch` | 0 | |
| `keyboard.priorityMode` | 0 | Low. |
| `keyboard.triggerMode` | 0 | Single. |
| `output.mainEnabled` | 1 | Main on. |
| `output.phonesEnabled` | 1 | Phones on. |

Metadata defaults are calibration `baseline`, UI `authentic`, recreation
enabled 0, and empty linked preset. Their warning codes are
`defaulted.calibrationProfileId`, `defaulted.ui.viewMode`,
`defaulted.recreation.enabled`, and
`defaulted.recreation.linkedPresetId`. The final deterministic entry is action
`preserveLegacyCrossedContours` with warning
`legacy.contourRoutingPreserved`.

A safe unknown legacy `PARAM` is sorted by ID and retained as
`extensions/legacyParameters/legacyParameter { id, value }`; it is never
applied to APVTS. A present legacy `presetName` is retained as
`extensions/legacyPreset { presetName }`. Unknown root properties, unsafe IDs,
extra `PARAM` properties/children, invalid numbers, and non-finite values are
rejected before migration.

## Source hash canonicalization

The hash input is UTF-8 and is built only after the complete source validates:

1. Append `model-d-v0-parameters\n`.
2. Append `presetName-present:` or `presetName-absent:`, its UTF-8 byte count,
   `:`, the decoded value, then `\n`. Presence and present-empty are distinct.
3. Normalize numeric tokens to finite binary32 physical values, including
   signed zero to positive zero.
4. Sort all source `PARAM` records lexicographically by bytewise ID.
5. For each, append `known:` or `unknown:`, the ID byte count, `:`, the ID,
   `:`, exactly eight lowercase hexadecimal digits containing its IEEE-754
   binary32 bits, then `\n`.
6. Store the lowercase SHA-256 of those bytes.

Thus child order and equivalent number spellings such as `0.5` and `0.500000`
produce the same source hash and the same canonical v2 state.

## Safe-extension policy

The dedicated `extensions` node itself has no properties. Its contents retain
attribute and child order through successful v2 parse/serialize when all of
these limits hold:

- maximum descendant depth: 8;
- maximum descendant element count: 128;
- maximum element or property-name length: 64 UTF-8 bytes;
- maximum individual property-value length: 1024 UTF-8 bytes;
- maximum combined property-value bytes: 16384;
- names use conservative ASCII XML identifiers;
- non-whitespace text and nested reserved schema names are forbidden.

Focused rejection tests cross every numeric limit, the reserved-name rule, and
the deepest boundary. A non-empty two-level safe vendor subtree is preserved
byte-for-byte and the full canonical binary remains identical after two v2
round trips.

## Fixtures and immutable hashes

Production capture commands (run from the configured Release build) are:

```sh
ModelDTests capture-state-v2-native
ModelDTests capture-state-v2-migrated-default
ModelDTests capture-state-v2-migrated-representative
```

Each command's stdout compared equal with `cmp` to its checked fixture.

```text
ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507  Tests/fixtures/state/native-default-state-v2.xml
4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958  Tests/fixtures/state/migrated-default-state-v2.xml
f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40  Tests/fixtures/state/migrated-representative-state-v2.xml
```

The immutable predecessor/registry fixture hashes remain:

```text
7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d  Tests/fixtures/parameters/legacy-parameter-inventory.json
2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284  Tests/fixtures/parameters/parameter-registry-v2.json
07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21  Tests/fixtures/state/legacy-default-state.xml
e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa  Tests/fixtures/state/legacy-representative-state.xml
```

## Rollback and determinism corpus

One sentinel first completes a successful native restore. Each later rejected
attempt compares the full canonical binary to the saved sentinel bytes and
also checks the successful lifecycle and contour metadata remain unchanged.
The corpus covers malformed and half-truncated binary, wrong root, every
version category, missing/extra properties and children, duplicate reserved
children, duplicate/missing/unknown parameters, invalid/trailing/non-finite/
out-of-range/non-discrete/bool values, invalid compatibility/UI/recreation/log,
the final parameter, and every safe-extension limit. All rollback comparisons
are byte-equal.

The migration corpus proves both immutable v0 fixtures map to exact v2
fixtures, repeated migrations are byte-identical, v2 reload/save is
byte-identical, missing Tune uses historical index 0, one missing contour alone
defaults and logs, all four supplied appended IDs survive, safe unknown data
survives, and a synthetic six-distinct-value contour state retains every value
under its original ID.

## TDD and verification

Valid RED, before production state code:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 4 &&
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^ModelDStateV2Contract$' --output-on-failure
```

The executable built; CTest failed 0/1 with 11 assertions identifying the raw
`Parameters` root and absent canonical properties, children, ordered parameter
container, and compatibility metadata.

Focused GREEN after the complete corpus:

```text
1/1 Test #3: ModelDStateV2Contract ............   Passed    0.37 sec
100% tests passed, 0 tests failed out of 1
```

Final Release build:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release -j 4
```

Exit code 0. Standalone, VST3, AU, tests, offline renderer, and wrapper smoke
targets built.

The one completed-state full CTest execution was:

```sh
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release --output-on-failure
```

```text
100% tests passed, 0 tests failed out of 13

Label Time Summary:
artifact    =   5.00 sec*proc (3 tests)
dsp         =   0.03 sec*proc (1 test)
host        =  29.95 sec*proc (2 tests)
midi        =   0.01 sec*proc (1 test)
realtime    =   0.01 sec*proc (1 test)
state       =   0.10 sec*proc (4 tests)
unit        =   0.17 sec*proc (1 test)

Total Test time (real) = 35.28 sec
```

## Deferred behavior

Migration never swaps v0 contour values. Runtime crossed routing, the explicit
static-value conversion command/undo, and automation warning UX remain Task
3B. This task makes no contour DSP/UI routing change and introduces no preset
schema, prepared snapshot, MIDI, wrapper, bus, identity, or DSP behavior.
