# Workstream 03 — Parameter, Automation, and State Contract

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Build](02-build-packaging-host-validation.md) · Next: [Pitch](04-pitch-tuning-glide-modulation.md)

## Goal and user-visible outcome

Make every control’s identity, display, automation, normalization, default, smoothing policy, and saved-state meaning stable and reviewable. Old projects retain their audible crossed-contour behavior; new projects expose correct Filter Contour and Loudness Contour semantics.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| PAR-001 | Centralize IDs and complete metadata in one registry consumed by processor, UI, state, presets, and tests. |
| PAR-002 | Give every parameter correct name, label, unit, range, step, skew/taper, default, and automation/discreteness flags. |
| PAR-003 | Default master Tune to `Zero`, not the current first choice. |
| PAR-004 | Correct crossed contour identities while preserving unversioned sessions and legacy host automation through explicit compatibility mode. |
| PAR-005 | Introduce host state v2, validated migration dispatch, and deterministic serialization. |
| PAR-006 | Define smoothing per audio-rate continuous parameter; never smooth switches or selectors. |
| PAR-007 | Add stable saved/automatable priority, trigger, main-output, view/recreation-facing contracts required by later plans. |
| PAR-008 | Preserve unknown safe extensions, reject unknown future major versions atomically, and test corrupt/partial states. |
| PAR-009 | Remove stringly typed parameter lookup from render loops and expose typed, prepared parameter snapshots. |

## Current-code evidence

- `MoogMiniAudioProcessor::createParameterLayout` is a long sequence of inline constructors. Six unrelated controls, including External Input, Glide, Mod Mix, Loudness Sustain, and Phones, are named “Output Volume.” Both keyboard-control switches share the same name.
- The `tune` choice lists `Zero` at index 5 but uses default index 0.
- `processBlock` reads parameter IDs by repeated strings and casts atomic floats to integers. It routes `loudness*` timing into the filter envelope and `filter*` timing into the output contour.
- `getStateInformation` serializes raw APVTS XML. `setStateInformation` accepts matching XML with no version, validation, migration report, or rollback.
- `PluginEditor` duplicates ID mapping in `getParameterID`, normalization in `getNormalizedValue`/`getSliderValueFromNormalized`, and manual timer synchronization instead of attachment/adaptor contracts.

## Prerequisites, ownership, and merge conflicts

Requires Workstream 02’s targets and test runner. This workstream exclusively owns parameter IDs, metadata, state root/version, migration dispatch, and contour compatibility policy. Workstreams 04–15 consume the registry and may not create ad hoc parameters.

High-conflict files: `PluginProcessor::createParameterLayout`, state methods, `PluginEditor` bindings, and `PresetManager`. Land the registry and adapters before DSP work. Workstream 13 owns preset-document schema, not host state.

## In scope

All current APVTS parameters; new priority/trigger/main-output parameters; UI-only saved properties; state validation/migration; normalized/display conversions; smoothing declarations; compatibility tests.

## Out of scope

Changing DSP algorithms, calibrating hardware tapers, implementing preset library storage, or rewriting DAW automation data.

## Proposed architecture and data flow

```text
ParameterRegistry (constexpr descriptors)
   -> APVTS ParameterLayout
   -> Host display/automation metadata
   -> UI binding adapters
   -> ParameterSnapshot prepared once per sub-block
   -> StateValidator -> StateMigrator(v0 -> v2)
   -> Preset schema mapping
```

Use stable ASCII IDs. Existing IDs remain stable; new IDs use dotted namespaces. Descriptor fields: ID, semantic key, display name, short label, unit, kind, physical min/max, default, interval, normalized mapping/taper key, automatable flag, smoothing class, and persistence scope.

New parameters:

| ID | Type / values | Default | Smoothing |
|---|---|---:|---|
| `keyboard.priorityMode` | choice `Low`, `High`, `Last` | `Low` | none |
| `keyboard.triggerMode` | choice `Single`, `Multi` | `Single` | none |
| `output.mainEnabled` | bool | on | none; click suppression is DSP-owned |
| `output.phonesEnabled` | bool | on | none; gain is smoothed |

UI view (`authentic`/`signalFlow`), selected calibration profile, recreation-enabled flag, and compatibility flags live under non-automatable state children, not audio parameters. Existing `outputPhonesVolKnob` remains the Phones gain parameter; existing `feedbackKnob` remains feedback amount.

`ParameterSnapshot` contains typed physical values, never `juce::String` keys:

```cpp
enum class PriorityMode : uint8_t { low, high, last };
enum class TriggerMode : uint8_t { single, multi };
enum class ContourContract : uint8_t { canonical, legacyCrossed };
struct ParameterSnapshot {
    PriorityMode priority;
    TriggerMode trigger;
    ContourContract contourContract;
    // typed oscillator, mixer, filter, contour and output structs
};
```

## State and migration contract

Canonical host-state root:

```text
modelDState { stateVersion=2, engineVersion, calibrationProfileId }
  parameters { APVTS values }
  compatibility { contourContract, sourceVersion, migratedAtVersion }
  ui { viewMode }
  recreation { enabled, linkedPresetId }
  extensions { preserved safe unknown values }
  migrationLog[] { from, to, action, warningCode }
```

Migration is parse → structural validation → version dispatch → semantic validation → build temporary canonical tree → commit once. A failure leaves the live state unchanged and reports a stable error code off the audio thread.

Version 0 rules:

1. Recognize the current `Parameters` APVTS XML root and legacy preset XML.
2. Copy all recognized values without swapping them.
3. Set `contourContract=legacyCrossed`; UI and DSP adapters preserve old routing and automation.
4. Fill missing new values with Low, Single, Main on, Phones on, Authentic view, recreation off; log each default.
5. Retain an opaque hash of the source tree for migration tests/support, not the entire host blob.

The explicit conversion action snapshots state, swaps the three Filter/Loudness static value pairs, changes to canonical, and warns that the plug-in cannot rewrite host automation lanes. Cancel is the default when host automation presence is unknown. Legacy mode remains saveable indefinitely.

## Parameter smoothing contract

- No smoothing: bools, waveform/range/tuning choices, priority, trigger, noise color, view mode.
- Gain-domain smoothing: oscillator/noise/external/mixer/output/phones levels, modulation depth, feedback, contour amount, resonance/drive. Interpolate in the appropriate linear-gain or control domain declared by the descriptor.
- Pitch/cutoff/glide: use their dedicated semitone/octave trajectories from Workstreams 04/08, not generic linear-Hz smoothing.
- Contour times/levels: stage logic in Workstream 07 handles mid-stage changes; APVTS values are not independently smoothed.
- Smoothing durations are named fields in the acceptance manifest. Hardware-dependent values require measurement; safety click-suppression bounds may be derived and documented by Workstream 12.

## Backward compatibility and preset behavior

No existing ID is removed, renamed, or reused. User-facing names may be corrected because host identity is the ID, but host scan snapshots must confirm automation continuity. Legacy mode is visible in diagnostics and preset metadata, not as an alarming panel control. Saving v2 never discards the compatibility marker. Workstream 13 maps unversioned preset XML through the same migrator.

## Real-time audio constraints

Parameter pointers are resolved during construction/prepare. At each MIDI-delimited sub-block, atomics are read once into a stack/preallocated typed snapshot. No APVTS tree copy, lookup by `String`, migration, XML, listener allocation, or notification occurs on the audio thread. UI gestures use begin/set/end host notification correctly.

## Edge cases and failure modes

- Missing/duplicate descriptor ID: compile-time/static registry test fails.
- Host presents normalized values outside [0,1] or NaN: reject/sanitize before physical conversion and report a diagnostic counter.
- State version is non-integer, negative, or newer major: no partial apply.
- v0 state has one contour field missing: preserve present values, fill only missing fields, keep legacy mode, log warning.
- Legacy session with automation: compatibility routing remains; conversion warning prevents silent lane reinterpretation.
- Parameter added later: explicit descriptor default and migration rule required in the same change.

## Implementation sequence

1. Inventory all current IDs and capture a host enumeration/state fixture before changes.
2. Add descriptor registry, typed enums/units, and registry validation tests.
3. Generate APVTS layout and host text conversion from descriptors; correct names and Tune default for new instances.
4. Add v2 state model, validator, v0 migration, rollback, and migration log.
5. Add canonical/legacy contour adapters and explicit conversion command.
6. Replace processor/UI string maps with prepared handles and bindings.
7. Publish registry JSON for test/report tooling and freeze at checkpoint F1.

## Automated tests and measurable gates

- Registry IDs are unique/nonempty; every descriptor has unit, mapping, default, and smoothing class; current IDs match the captured legacy inventory exactly.
- New instance yields Tune `Zero`, Low priority, Single trigger, Main/Phones on, canonical contours.
- Every choice’s normalized endpoints and round trip map to the intended index; continuous mappings are finite, monotonic, and hit published endpoints.
- v0 fixture renders the same control-routing trace before and after migration, including automated legacy contour lanes; no value is silently swapped.
- Canonical state routes Filter controls only to Filter Contour and Loudness controls only to VCA contour.
- Repeated v2 serialize→parse→serialize is canonical-byte identical after documented ordering normalization.
- Corrupt, truncated, wrong-root, duplicate-ID, and future-major states leave a sentinel live state byte-for-byte unchanged.
- Parameter fuzzing produces no NaN, invalid enum, crash, or out-of-range physical value.
- Audio-thread instrumentation sees zero string lookups/allocations from parameter snapshot creation.

Floating-point comparison policy comes from Workstream 12’s derived-software manifest; hardware taper comparisons wait for approved measured entries.

## Manual and host validation

In designated VST3/AU hosts, capture automation lists before/after, reopen a legacy project, automate each contour control, save/reload, and compare audible routing plus displayed values. Verify corrected names/units in generic host UI, UI gestures, undo, latch/write/read automation, legacy-mode diagnostics, and the explicit conversion warning/cancel path.

## Definition of done

- [ ] PAR-001 through PAR-009 pass.
- [ ] Registry is the only metadata source.
- [ ] Legacy IDs and automation remain addressable.
- [ ] v0 migration is atomic and visibly marked legacy; v2 is deterministic.
- [ ] New instances have canonical contour semantics and correct defaults.
- [ ] All later workstreams can consume typed snapshots without inventing mappings.

## Completion-report evidence

Include legacy/current registry exports and diff; descriptor validation output; default snapshot; v0/v2 fixtures and hashes; routing-trace comparisons; failure/rollback test output; host automation screenshots; conversion-warning capture; real-time instrumentation; and a migration table listing every added/defaulted property.

## Primary technical references

- JUCE [`AudioProcessorValueTreeState`](https://docs.juce.com/master/classAudioProcessorValueTreeState.html), [`RangedAudioParameter`](https://docs.juce.com/master/classRangedAudioParameter.html), and [`SmoothedValue`](https://docs.juce.com/master/classSmoothedValue.html).
- Steinberg [VST3 parameter concepts](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html).
- Apple [Audio Unit parameter concepts](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins/parameter_implementation).
- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 28–33, 47, 80.
