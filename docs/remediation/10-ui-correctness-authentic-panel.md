# Workstream 10 — UI Correctness and Authentic Panel

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Signal path](09-nonlinear-signal-path.md) · Next: [Real-time safety](11-real-time-safety-visualization-performance.md)

## Goal and user-visible outcome

The first launch is stable and every authentic Model D control is visible once, correctly labeled, correctly bound, resizable, automatable, and restorable. The existing signal-flow overlay remains an alternate educational view, not the only panel. Modern utility settings are clearly separated from the historical panel.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| UI-001 | Eliminate the overwritten `nestedMap[5][1]` entry and duplicate/missing layout keys. |
| UI-002 | Replace uninitialized position arrays and imperative scanning with validated declarative geometry. |
| UI-003 | Initialize `PianoKey` state and test mouse/drag/release/focus transitions. |
| UI-004 | Remove manual contour-ID inversion after canonical/legacy binding adapters exist. |
| UI-005 | Add a one-to-one Authentic Panel with historical controls/labels/order and explicit utility separation. |
| UI-006 | Retain Signal Flow as a saved alternate view using the same parameter/state contracts. |
| UI-007 | Make feedback routing, Main Output, Phones, overload, priority, and trigger controls/indicators functional and unambiguous. |
| UI-008 | Pass resize, HiDPI, first-launch, standalone visibility, focus, automation reflection, accessibility, and state-restoration gates. |

## Current-code evidence

- `PluginEditor` assigns `nestedMap[5][1]` first to `ctrlGlideKnob` and later to `osc3CtrlSwitch`, so Glide’s scanned position is never initialized.
- Dozens of local `int ...Pos[2]` arrays are uninitialized and only filled if a map/grid key is found. Missing/commented keys leave indeterminate bounds passed to constructors.
- Main Output and Phones entries are commented in `nestedMap`; feedback construction is commented. The overload control is a `TextButton`, not a DSP indicator.
- `PianoKey` constructor does not initialize `isKeyPressed`; paint and transitions read it.
- Overlay callbacks deliberately bind “Contour” controls to `loudness*` IDs, and timer visualization repeats the cross-inversion.
- The editor manually maps slider values/IDs, polls parameters, hides multiple authentic controls, and layers a large signal-flow overlay. There is no explicit Authentic/Signal Flow view state.

## Prerequisites, ownership, and merge conflicts

Requires build/editor lifecycle (02), registry/state and contour adapters (03), lifecycle/focus hooks (05), complete signal controls/taps (09), and snapshot API coordination (11). It exports geometry consumed later by Workstream 15.

Owns editor component hierarchy, geometry, bindings, view switching, focus/accessibility, overload display, and UI test IDs. It does not own parameter semantics, DSP routing, or image recognition.

High conflict: `PluginEditor.h/.cpp`, `PianoKey`, look-and-feel classes, `SignalFlowOverlay`, and parameter attachments. Freeze `PanelControlSpec` and automation bindings before photo-import work.

## In scope

Authentic/Signal Flow views, complete panel/control layout, modern utility strip, keyboard/pitch/mod controls, presets/recreation entry points, overload indicator, resize/HiDPI/accessibility, attachments/adapters, state restore, screenshot/UI automation tests.

## Out of scope

Changing DSP semantics, copying protected trade dress beyond approved assets, claiming a photo-perfect historical edition without license review, OCR/import, or adding controls not in the registry.

## Proposed architecture and data flow

```text
ParameterRegistry + StateBindingAdapters
  -> PanelControlSpec[] (semantic key, component type, geometry ID, label, accessibility, view)
  -> AuthenticPanel / UtilityStrip
  -> SignalFlowView (same bindings, read-only snapshots + gestures)
  -> EditorRoot view switch + resizable transform
```

Use normalized design coordinates in a declarative `PanelLayout` grouped as Controllers, Oscillator Bank, Mixer, Modifiers/Filter, Filter Contour, Loudness Contour, Output, Left-Hand controls, and Keyboard. Each geometry ID is unique and compile/test validated. Runtime never searches a sparse map to discover coordinates.

```cpp
enum class EditorView : uint8_t { authentic, signalFlow };
struct PanelControlSpec {
    SemanticParameter key;
    ComponentKind kind;
    GeometryId geometry;
    PanelSection section;
    const char* panelLabel;
    const char* accessibilityText;
};
struct PanelGeometry { GeometryId id; NormalizedRect bounds; ControlPose pose; };
```

The exact geometry dataset is exported as versioned `authentic-panel-geometry.json` from the same source used to construct components. Workstream 15 reads this export; it must not duplicate coordinates.

## Authentic panel and utility boundary

The Authentic view shows the documented panel controls once: Tune, Glide/Mod Mix, three oscillator Range/Frequency/Waveform controls, oscillator keyboard control/modulation switches, five-source Mixer switches/volumes, noise color, filter cutoff/emphasis/amount/key tracking/modulation, both contour triplets, Decay/Glide, Main Output Volume/Switch, Phones Volume, A-440, wheels, and keyboard.

Historical feedback is operated by External Input switch/volume when routing is `internalFeedback`. The existing dedicated `feedbackKnob` remains compatibility-addressable but appears in a clearly labeled **Utility — not an original panel control** section as a calibrated feedback trim; it is not drawn as a historical Model D knob. Low/High/Last priority, Single/Multi trigger, external-source override, view selector, calibration profile, and recreation controls also live in Utility/Settings because modern Model D exposes priority/trigger as global settings rather than front-panel controls.

Overload is a non-clickable lamp bound to snapshot state with DSP-defined ballistics. Main Output switch/volume and Phones controls show host routing status: if Phones/Cue bus is disabled, the control remains saved but shows “Enable Phones/Cue bus in host”; standalone exposes the cue route when the device configuration supports it.

## Bindings and state

Use APVTS attachments or typed binding adapters generated from Workstream 03. Every gesture issues begin/change/end notifications and supports host automation/undo. `legacyCrossed` adapters map physical contour controls without changing component code. UI-only `EditorView` and window size are saved under `ui`; default is Authentic. Recreation enable never changes control bindings or the authentic panel snapshot.

## Backward compatibility and migration

Existing parameter IDs remain. First v2 launch defaults Authentic; legacy state without view also defaults Authentic. Preserve old editor size only if valid/in bounds; otherwise use design default. The Signal Flow view remains available and saved. Correct labels may change, but automation IDs and legacy contour adapter behavior remain per Workstream 03.

## Real-time audio constraints

The UI never reads mutable DSP arrays directly or calls processor/DSP mutation outside parameter/event contracts. Timer/paint consumes Workstream 11 snapshots only. No message-thread operation blocks the audio thread. Screenshot/layout/state work may allocate on the message thread; it must not enable additional audio-thread work beyond the bounded snapshot producer.

## Edge cases and failure modes

Zero/very small/very large editor bounds, non-integer scaling, multiple monitors, HiDPI changes, view switch during automation, editor close while audio runs, missing Phones bus, legacy mode, corrupt geometry export, focus loss mid-key/drag, overlapping black/white keys, mouse leaving window, touch, host generic UI, and multiple editor instances. Invalid geometry fails tests/build; runtime fallback uses safe default layout, never uninitialized memory.

## Implementation sequence

1. Inventory all physical/utility controls and assign semantic/geometry/test IDs.
2. Add declarative layout validation and initialize/fix `PianoKey` behavior.
3. Build Authentic Panel using typed attachments/adapters; remove map/array scanning.
4. Refactor Signal Flow to consume the same bindings/snapshots; add saved view switch.
5. Add Utility settings, routing status, overload lamp, accessibility, focus/panic hooks.
6. Add resize/HiDPI/screenshot/host automation/state tests and export geometry v1.

## Automated tests and measurable gates

- Every required authentic/utility semantic key maps to exactly one visible control in its designated view; every geometry/test ID is unique; no unbound registry parameter intended for UI remains.
- Layout validation rejects missing, duplicate, non-finite, negative-size, and out-of-design bounds before component construction.
- `PianoKey` starts unpressed; exact mouse-down/drag-between-keys/mouse-up/focus-loss sequences yield matching UI event traces and no stuck key.
- Canonical and legacy contour binding fixtures show correct physical-control routing without per-view ID inversion.
- Golden screenshots at approved sizes/scales pass structural image-diff regions/manifest thresholds; all hit targets stay within bounds and do not overlap incorrectly.
- Parameter automation changes both views; gestures update host values; save/reload restores values, view, modes, and valid size.
- First editor creation before/after prepare, repeated open/close, multiple instances, and standalone first window produce visible nonempty bounds with no crash.
- Accessibility audit finds a nonempty role/name/value/action for every interactive control and no keyboard-focus trap.

## Manual and host validation

Compare Authentic layout/control inventory against manual panel diagrams. Test mouse, keyboard, touch/trackpad, scaling, multiple displays, focus loss, host automation write/read, undo, generic host UI, view switching, state/preset reload, Phones bus absent/present, Main mute cueing, overload indication, and standalone first launch. Capture screenshots and exact host/OS versions.

## Definition of done

- [ ] UI-001 through UI-008 pass.
- [ ] No sparse layout map/uninitialized position array remains.
- [ ] Authentic controls are complete; utility controls are visibly non-historical.
- [ ] Both views share typed bindings and restore correctly.
- [ ] Geometry v1 and UI/accessibility/host evidence are published.

## Completion-report evidence

Include control inventory mapped to manual sections; layout validation output; geometry export/hash; screenshots at all approved sizes/scales/views; binding coverage; PianoKey/focus traces; automation/restore captures; accessibility report; overload/feedback/Main/Phones/priority/trigger demonstrations; standalone window evidence; and conflict notes.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 14–36, 47, 52.
- JUCE [`Component`](https://docs.juce.com/master/classComponent.html), [`AudioProcessorValueTreeState` attachments](https://docs.juce.com/master/classAudioProcessorValueTreeState.html), and [accessibility classes](https://docs.juce.com/master/classAccessibilityHandler.html).
