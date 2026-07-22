# Workstream 03 complete prepared parameter snapshot evidence

## Scope and architecture

Task 3C1 adds `ParameterRegistry::Key` in the frozen 48-descriptor order,
`parameterCount == 48`, ordinal `index(Key)`, and `descriptor(Key)` backed by
the existing registry array. The focused test compares every ordinal to the
immutable 44 legacy plus four v2 ID oracle; no parameter ID, descriptor,
metadata, default, host flag, state representation, or predecessor fixture was
changed.

`MoogMiniAudioProcessor` resolves one 48-entry raw-atomic bank and one matching
prepared `RangedAudioParameter*` bank in its constructor. The public prepared
ranged access is typed by `Key`; mutable raw state is not exposed. The plain,
trivially-copyable `ParameterSnapshot` holds explicit oscillator, mixer,
filter, contour, modulation, keyboard, and output physical values, typed
priority/trigger modes, the contour contract, generation, coherence, and
fallback bookkeeping. It contains no string, tree, XML, APVTS, or owning type.

`processBlock` calls `captureParameterSnapshot` exactly once at the parameter
read boundary, updates its audio-thread prior only after a coherent capture,
and binds the existing DSP variables exclusively from typed snapshot fields.
The existing equations, scaling, MIDI behavior, output writes, timing mapping,
and canonical/legacy `ContourRouting::route` calls are unchanged. Task 3B's
public contour snapshot/trace API delegates to the complete reader rather than
performing a competing raw-load pass.

## Publication, memory ordering, and sanitization

The reader makes exactly three attempts. Each attempt acquire-loads the
generation, rejects odd publication, relaxed-loads all 48 prepared atomics,
acquire-loads the contour marker, then acquire-loads the generation again. It
accepts only equal even generations. Otherwise it returns the caller's prior
coherent complete snapshot, or the constructor-built native default, with
`usedFallback=true`.

Successful host restore, confirmed legacy conversion, and valid conversion
undo remain the only release-bracketed odd/even multi-field publishers. Direct
host automation remains an ordinary atomic value change. Concurrent coverage
alternates distinct native and legacy whole-state vectors and rejects every
mixed field/marker generation. Any fallback observed by that stress must equal
the prior snapshot field-for-field with the same generation and contract; the
exact three-attempt and fallback branches are also source guarded without
depending on scheduler timing.

After an accepted generation, the builder sanitizes without writing APVTS.
Non-finite values use the descriptor default and increment one relaxed atomic
diagnostic per field; floating values clamp to physical endpoints; choices
round and clamp; booleans become exact values; typed enums remain within their
domains. Tests cover every field at native default, a distinct vector,
`+FLT_MAX`, `-FLT_MAX`, and alternating NaN/+infinity/-infinity, including the
exact 48-count diagnostic increment. A real render with non-finite cutoff
proves `processBlock` consumes the sanitized snapshot and remains finite.

Source guards cover both builder and reader plus the complete `processBlock`.
The capture/builder contain no ID lookup, `juce::String`, ValueTree/XML access,
allocation, notification, lock, or unbounded loop. `processBlock` contains one
complete capture and no `getRawParameterValue`, `getParameter`, or string use.

## TDD chronology and verification

Before any production edit, the test-only `ModelDPreparedSnapshotContract`
built successfully and failed 0/1 with five intended assertions:

```text
FAIL: PAR-009 requires a checked typed key in frozen descriptor order
FAIL: processor must expose one complete typed prepared snapshot and diagnostic
FAIL: complete snapshot capture must use three bounded generation-bracketed attempts
FAIL: processBlock must capture once and contain no render-time parameter lookup/string
FAIL: prepared snapshot production capture fixture must exist
```

After implementation, the focused prepared-snapshot, Task 3B contour, and
state-v2 tests passed 3/3. State-labeled tests passed 6/6; realtime-labeled
tests passed 2/2. The complete Release build produced Core, Assets, shared
plug-in code, Standalone, AU, VST3, actual-wrapper smoke, offline renderer, and
tests. Full Release CTest passed 15/15 with all seven labels retained:

```text
artifact = 4.87 sec*proc (3 tests)
dsp = 0.03 sec*proc (1 test)
host = 19.28 sec*proc (2 tests)
midi = 0.01 sec*proc (1 test)
realtime = 0.10 sec*proc (2 tests)
state = 0.40 sec*proc (6 tests)
unit = 0.08 sec*proc (1 test)
Total Test time (real) = 24.70 sec
```

## Production capture and immutable hashes

`ModelDTests capture-parameter-snapshot-v2` emits all 48 ordered physical
fields, typed mode spellings, contour contract, and three-field sanitization
diagnostic. Regenerated stdout compares byte-for-byte with
`Tests/fixtures/parameters/parameter-snapshot-v2.json`.

```text
cbfefbf2e918818fed1fd6b340ca4c015981d6e020080a7b71bbfd006e398f16  parameter-snapshot-v2.json
```

All eight predecessor fixtures remain byte-identical:

```text
7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d  legacy-parameter-inventory.json
2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284  parameter-registry-v2.json
07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21  legacy-default-state.xml
e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa  legacy-representative-state.xml
ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507  native-default-state-v2.xml
4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958  migrated-default-state-v2.xml
f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40  migrated-representative-state-v2.xml
ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c  contour-routing-conversion-trace.json
```

## Explicit deferrals

Task 3C2 retains all general PluginEditor binding, attachment, map, and polling
work. Contour algorithms/calibration remain Workstream 07; DSP calibration,
smoothing algorithms, MIDI lifecycle, outputs, preset storage, layout,
wrappers/buses, and identity were not changed. External BLD-006, BLD-011, and
BLD-012 were not run or claimed.
