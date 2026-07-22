# Workstream 03 contour compatibility, coherent publication, and conversion evidence

## Scope and physical-port truth table

`Source/ContourRouting.h/.cpp` is the single typed, allocation-free adapter used
by processor DSP and Signal Flow semantic controls. `LadderFilter` retains its
legacy method names: `setEnvelopeSettings` configures the physical Filter
envelope, while `setContourEnvelopeSettings` configures the envelope multiplied
into the physical VCA/Loudness path.

| Contract | Filter A/D/S IDs feed | Loudness A/D/S IDs feed |
|---|---|---|
| `canonicalContours` | physical Filter | physical VCA/Loudness |
| `legacyCrossedContours` | physical VCA/Loudness | physical Filter |

The six-distinct vector is Filter `0.11/0.22/0.30` and Loudness
`0.66/0.77/0.90`. Canonical output is Filter `0.11/0.22/0.30`, Loudness
`0.66/0.77/0.90`; legacy output is Filter `0.66/0.77/0.90`, Loudness
`0.11/0.22/0.30`. With Decay disabled both routed sustains are exactly `1.0`.
No timing mapping, envelope stage, release, or smoothing code changed.

Signal Flow reads route a typed snapshot through the adapter. Writes call
`parameterFor(contract, semanticContour, stage)` at gesture time, so a later
restore cannot leave stale constructor-captured IDs. Authentic panel bindings
keep their literal stable host IDs.

## Generation publication and bounded reader

The processor resolves all six contour atomic handles during construction. A
successful restore, confirmed conversion, or undo is serialized by
`statePublicationLock`: release-store odd generation; replace APVTS values and
publish canonical metadata, contour marker, lifecycle/undo and conversion
metadata; then release-store even generation. Parse failure, cancellation,
`already_canonical`, and `no_undo_available` return before the odd store.

The reader makes exactly three attempts. It acquire-loads generation, rejects
odd, loads the six prepared atomics plus Decay and the atomic contract, then
acquire-loads generation again and accepts only the same even value. If all
attempts overlap publication it returns the prior coherent per-block snapshot.
The capture path contains no unbounded loop, lock, `juce::String`, APVTS ID
lookup, `ValueTree`, XML, or allocation. Direct host automation remains an
ordinary per-block atomic change.

Concurrent coverage alternates native and migrated restores with different
six-value vectors while reading 1,000 snapshots. Every accepted/fallback result
is a complete known even generation; mixed contract/value traces fail.

## Explicit conversion, warning, and undo contract

| Operation | Stable result | Stable UI warning |
|---|---|---|
| unconfirmed legacy, automation unknown | `cancelled` | `host_automation_not_rewritten` |
| confirmed legacy, automation unknown/present | `converted` | `host_automation_not_rewritten` |
| confirmed legacy, automation known absent | `converted` | `none` |
| canonical request | `already_canonical` | `none` |
| first valid undo | `undo_restored` | `none` |
| unavailable/second undo | `no_undo_available` | `none` |

Every confirmed conversion, including known-absent automation, appends exactly
`ENTRY { from=2, to=2, action="convertLegacyContours",
warningCode="legacy.hostAutomationNotRewritten" }`.

| Left stable ID | Right stable ID |
|---|---|
| `filterAttackTimeKnob` | `loudnessAttackTimeKnob` |
| `filterDecayTimeKnob` | `loudnessDecayTimeKnob` |
| `filterSustainKnob` | `loudnessSustainLevelKnob` |

Only those static pairs and the marker mutate. Tests compare source
version/hash, engine/calibration, retained extensions, UI, recreation, an
unrelated parameter, and every prior log entry before/after; all are equal.
The swapped static values plus canonical marker preserve both immediate
physical traces. DAW automation lanes cannot be queried or rewritten; explicit
confirmation acknowledges that limitation and no lane is fabricated.

Preparation rejects non-legacy/incomplete input and the processor gates
publication on its typed success. Validation accepts source-0 canonical state
only with one exact conversion entry as the final log entry. Missing/malformed
provenance and conversion history under a legacy marker reject atomically.
Undo restores the complete exact pre-conversion binary and legacy marker,
clears the one-level snapshot, and a later successful external restore also
clears it.

## Production fixture and immutable hashes

`ModelDTests capture-contour-routing-trace` regenerated stdout compares equal
with `cmp` to `Tests/fixtures/state/contour-routing-conversion-trace.json`.

```text
ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c  contour-routing-conversion-trace.json
```

All predecessor hashes remain unchanged:

```text
7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d  legacy-parameter-inventory.json
2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284  parameter-registry-v2.json
07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21  legacy-default-state.xml
e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa  legacy-representative-state.xml
ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507  native-default-state-v2.xml
4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958  migrated-default-state-v2.xml
f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40  migrated-representative-state-v2.xml
```

## RED, GREEN, and verification

Before production edits the test-only focused executable built, then
`ModelDContourContract` failed 0/1 with seven expected assertions: absent
adapter, snapshot/conversion/undo, generation bracket, warning conversion,
processor routing, dynamic Signal Flow mapping, and stale crossed callbacks.

Focused GREEN passed `ModelDStateV2Contract` and `ModelDContourContract` 2/2;
all state-labeled tests passed 5/5. The complete Release build exited zero and
built Assets, Core, OfflineRenderer, ActualWrapperSmoke, plugin shared code,
tests, Standalone, AU, and VST3 targets.

Full-suite command:

```sh
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release --output-on-failure
```

The completed run passed 14/14 with zero failures and retained all seven
labels:

```text
artifact = 4.89 sec*proc (3 tests)
dsp = 0.03 sec*proc
host = 29.97 sec*proc (2 tests)
midi = 0.01 sec*proc
realtime = 0.01 sec*proc
state = 0.32 sec*proc (5 tests)
unit = 0.38 sec*proc
```

Total real time was `35.61 sec`.

## Explicit deferrals

Contour algorithms, timing/calibration, stage transitions, and measured
hardware comparison remain Workstream 07. The prepared snapshot stays limited
to six contour controls plus existing Decay; the complete 48-parameter snapshot
and remaining general editor bindings remain Task 3C. External
BLD-006/011/012 results are not claimed.
