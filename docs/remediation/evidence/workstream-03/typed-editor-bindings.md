# Workstream 03 typed editor binding evidence

## Scope and ownership

`Source/ParameterBinding.h/.cpp` provides the single reusable editor binding.
Each instance retains a typed `ParameterRegistry::Key`, a prepared
`juce::RangedAudioParameter&`, and one `juce::ParameterAttachment`. The editor
resolves the prepared parameter once through
`MoogMiniAudioProcessor::getPreparedParameter(Key)` and owns every binding in a
`std::vector<std::unique_ptr<ParameterBinding>>` declared after the controls.
Bindings therefore stop listening before their target components are destroyed;
an active drag gesture is also ended during binding destruction.

The editor's 26 created `WaveformSlider` controls, two wheels, 14 created
`ToggleButton` controls, five embedded waveform/mixer toggles, and Signal Flow
cutoff/resonance/amount actions all use prepared typed bindings. Layout and
label keys remain presentation-only strings. Signal Flow contour writes remain
dynamic: every action calls `setSignalFlowContourControl`, which evaluates
`ContourRouting::parameterFor` against the current restored contour contract at
gesture time. Authentic panel contour controls retain their literal stable IDs.

## Denormalized conversions and gestures

`juce::ParameterAttachment` callbacks carry denormalized physical values.
Normal controls convert parameter physical values through
`RangedAudioParameter::convertTo0to1` and then through the component
`NormalisableRange`; UI values use the exact reverse path. This retains float
and choice endpoints, tune `-2.5..+2.5`, detune `-8..+8`, cutoff/pitch
`-5..+5`, panel indices `0..10`, and component interval snapping without an
editor-local metadata or normalization table.

The sole display exception is the four contour-time sliders: parameter physical
`0..1` maps to display milliseconds by `physical * 10000.0`, and UI display
maps by `value / 10000.0`. The existing Slider range retains the `10..10000`
display clamp and `0.01` interval.

Continuous drag callbacks produce one `beginGesture`, zero or more
`setValueAsPartOfGesture` calls, and one `endGesture`. Non-drag slider changes,
buttons, embedded toggles, and Signal Flow cutoff/resonance/amount actions use
one complete gesture. The pitch wheel now publishes its spring return as the
last part value before the base mouse-up ends the same drag, preventing a second
reset gesture. Parameter-originated callbacks use `dontSendNotification`, so
they never echo a component value or host gesture.

## Restore and visualization proof

The focused binding contract drives a real parameter from a worker after the
JUCE message queue is active, pumps JUCE's macOS CoreFoundation callback-message
run loop with a 500 ms condition bound, and observes the component update with
only the originating host value event and no echoed gesture/value. With a
binding alive, the same worker restores a successful v2 processor state; after
the message loop pumps, the bound component displays the restored value without
an editor timer poll. The cross-platform fallback uses the bounded JUCE dispatch
loop and the start event has shared ownership so a queued callback cannot retain
a stack reference.

At every editor timer tick, one `captureParameterSnapshot(timerParameterSnapshot)`
supplies all parameter-derived visualization state: embedded toggle states,
smoke enabled/color/alpha, cutoff/resonance/contour, stored semantic contours
through `mapStoredControls`, keyboard tracking, and oscillator tune/range/detune
scales. The prior coherent member advances only for a non-fallback capture.
Stage waveforms, signal levels, overlay animation, and repaint remain timer
driven. The timer contains no APVTS getter, parameter ID, or contour-only second
capture.

Source guards prove the editor contains no `getParameterID`,
`getNormalizedValue`, `getSliderValueFromNormalized`, `getEnumSizeLessOne`,
`sliderHasChanged`, ID-string APVTS callback chain, direct
`audioProcessor.apvts.get*`, or slider/toggle parameter map. Task 3B tests also
retain canonical/legacy dynamic writes and stored disabled-Decay sustain
readback rather than decay-gated physical `1.0` display values.

## TDD chronology

Before production edits, the test-only `ModelDParameterBindingContract` was
registered under both `unit` and `state`. `ModelDTests` compiled, then the
focused test failed 0/1 with six intended missing-feature assertions: absent
typed binding files/behavior, retained duplicate editor chains, direct APVTS
getters, timer polling/no complete snapshot, and absent typed editor retention.

After implementation, the focused binding contract passed. A self-review added
a second test-only pitch-wheel ordering check; its corrected focused RED failed
0/1 before the spring return was moved inside the drag gesture, then passed
after the production change. The combined binding, prepared snapshot, contour,
and state-v2 contracts passed 4/4. Required labels passed: unit 2/2, state 7/7,
and realtime 2/2.

The complete Release build produced Core, Assets, shared plug-in code,
Standalone, AU, VST3, actual-wrapper smoke, offline renderer, and tests. One
overlapping pair of full-suite runs collided in the reused mutable product
identity and standalone-evidence directories; the unrelated failures reproduced
only under overlap. With no concurrent runner, ProductIdentity passed 1/1,
StandaloneLifecycle passed 1/1, and a clean full CTest passed 16/16 with all
seven labels retained: artifact, dsp, host, midi, realtime, state, and unit.

## Production captures and immutable hashes

The following six regenerated production captures compared byte-for-byte with
`cmp`: parameter snapshot v2, registry v2, contour routing/conversion trace,
native default v2 state, migrated default v2 state, and migrated representative
v2 state.

```text
cbfefbf2e918818fed1fd6b340ca4c015981d6e020080a7b71bbfd006e398f16  parameter-snapshot-v2.json
7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d  legacy-parameter-inventory.json
2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284  parameter-registry-v2.json
07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21  legacy-default-state.xml
e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa  legacy-representative-state.xml
ff369e874e4c786830ea51731b8849e54c44f81151313cb1ceefdcab9b8f2507  native-default-state-v2.xml
4fa0dbbfec6b9816657f41d68411285e6d4e17e176d93c596141054c7a7d4958  migrated-default-state-v2.xml
f2ebb2afc79668c02ee9580f29fdc900a3545530e8175068690df5ebec8f9a40  migrated-representative-state-v2.xml
ce998775a66ac12a997dbfc613ee00a417c293836443fea5842b3df9d1dd8c0c  contour-routing-conversion-trace.json
```

## Deferrals

No DSP, contour algorithm/timing/calibration, smoothing, MIDI, output, preset
storage, layout, wrapper/bus, product identity, parameter descriptor, or state
schema change is included. DAW automation lanes are not rewritten. External
BLD-006/011/012 and unrun manual/host validation are not claimed.
