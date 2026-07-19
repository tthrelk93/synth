# Workstream 05 — Sample-Accurate MIDI and Monophonic Voice Lifecycle

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Pitch](04-pitch-tuning-glide-modulation.md) · Next: [Oscillators](06-band-limited-calibrated-oscillators.md)

## Goal and user-visible outcome

Notes begin, change priority, retrigger, fall back, sustain, release, and recover at their exact MIDI sample positions. Classic behavior is Low priority with single-trigger legato; saved High/Last and Multi modes behave as documented by the modern Model D manual.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| MID-001 | Render buffer subranges bounded by sorted MIDI event sample positions; never apply the entire buffer’s events before audio. |
| MID-002 | Maintain a duplicate-aware held-note model and select Low priority by default. |
| MID-003 | Implement saved High and Last priority with deterministic fallback ordering. |
| MID-004 | Implement classic Single trigger: legato pitch changes do not retrigger contours until all notes/gates are released. |
| MID-005 | Implement Multi trigger: each newly selected note-on retriggers both contours. |
| MID-006 | Define duplicate note-on, velocity-zero note-on, note-off ordering, same-sample ordering, channels, and running-status-equivalent behavior. |
| MID-007 | Handle sustain pedal, all-notes-off, all-sound-off, focus loss, transport/device reset, and stuck-note recovery. |
| MID-008 | Keep oscillators free-running through loudness release and idle only after the VCA contour is silent under its gate. |
| MID-009 | Make Oscillator 3 keyboard-control and mixer/modulation gating consistent in all note states. |
| MID-010 | Replace the audio-thread MIDI `CriticalSection` with a bounded real-time-safe UI event path and explicit overflow recovery. |
| MID-011 | Expose deterministic lifecycle diagnostics/fixtures without altering audio-thread behavior. |

## Current-code evidence

- `processBlock` merges UI MIDI under `midiCriticalSection`, iterates every `midiMessages` event, changes state, and only then renders the buffer, ignoring event sample positions.
- State is a single `currentNoteNumber`; releasing it stops oscillators and sets −1. There is no held-note stack, priority fallback, duplicate accounting, sustain, panic, or trigger-mode policy.
- Every note-on calls `ladderFilter.noteOn`; every note-off calls `noteOff`, including note-offs for notes that are not the selected note.
- `Oscillator::stop` sets active false and resets phase, so output release is silent regardless of contour time.
- Oscillator 3 starts/stops only when both its mixer switch and keyboard control are on, while modulation processing separately runs it when modulation switches are on. Mixer and control roles are conflated.
- `handleNoteOn`/`handleNoteOff` write sample-0 events into `incomingMidi` under a lock shared with the audio callback.

## Prerequisites, ownership, and merge conflicts

Requires typed parameters/state (03), pitch/glide transitions (04), and offline MIDI fixtures (12 skeleton). Freeze `MonoNoteStack`, `GatePolicy`, and `GateEvent` before oscillator/contour implementation.

Owns event ordering, note ownership, priority, gate/retrigger policy, panic, UI-MIDI queue, and lifecycle state. Workstream 06 owns oscillator phase/idling implementation; 07 owns contour stages; 10 owns focus-loss detection; 11 verifies real-time behavior.

High conflict: `processBlock`, note handlers, editor keyboard callbacks, oscillator start/stop, and `LadderFilter::noteOn/off`. Replace these with the event/render contract, not incremental conditionals.

## In scope

MIDI Note On/Off, CC64 sustain, CC120 All Sound Off, CC123 All Notes Off, UI keyboard events, sample offsets, monophonic priority/gate policies, velocity capture for future use, and deterministic reset behavior.

## Out of scope

Polyphony, MPE, MIDI 2.0 per-note control, arpeggiation, alternate tunings, velocity-to-VCA mapping, or waveform/filter algorithms.

## Proposed architecture and data flow

```text
host MidiBuffer + bounded UI SPSC queue
  -> normalize events {sampleOffset, sourceOrder, channel, type, data}
  -> stable order at each offset
  -> render audio [cursor, eventOffset)
  -> apply event(s) to MonoNoteStack + GatePolicy
  -> emit SelectedNoteChange / GateOpen / Retrigger / GateClose / Panic
  -> render next range
```

Event order at the same sample is fixed: (1) All Sound Off/reset, (2) note-offs and sustain releases, (3) sustain state changes, (4) note-ons, (5) continuous controllers. Within a class preserve source order. A Note On with velocity zero normalizes to Note Off before sorting.

`HeldNote` key is `(source, channel, note, pressSerial)`. Duplicate Note On increments a per-key press count and creates a new serial for Last priority. A matching Note Off releases the oldest outstanding press for that source/channel/note; the note remains held until the count is zero. Host and UI sources are distinct so a UI release cannot cancel a host-held note.

Priority selection:

- Low: lowest MIDI note; tie uses earliest still-held press.
- High: highest; tie uses earliest still-held press.
- Last: most recent still-held press serial.
- On selected-note release, immediately select the best remaining held or sustained note and issue a pitch transition; gate policy decides retrigger.

Sustain holds released notes in a `sustained` set until CC64 drops below 64. Priority includes physically held notes first; if none are physical, sustained notes participate under the selected mode. Re-pressing a sustained note creates a new physical instance.

## Trigger/gate contract

```cpp
enum class VoiceAction { pitchChange, gateOpen, retrigger, gateClose, hardSilence };
struct VoiceTransition { VoiceAction action; int note; float velocity; int sampleOffset; };
```

- Single: first eligible note opens/retriggers contours. Additional legato note-ons and priority fallbacks change pitch/glide only. Gate closes when no physical or sustained note remains.
- Multi: each note-on that becomes selected emits retrigger. A lower/higher non-selected note-on does not retrigger until it becomes selected through a later event. Priority fallback on note-off changes pitch without retrigger, matching “each new note played,” not release.
- Note-off for an unselected note never closes the gate.
- Panic/All Sound Off emits hard silence, clears all sets/queue, resets contours and feedback immediately with click-safe output handling owned by Workstream 09.
- All Notes Off acts as release for all physical notes and observes sustain; a second panic path ignores sustain.

## Oscillator 3 and release contract

Oscillators are continuous signal sources once prepared. The voice lifecycle never resets phase on ordinary Note Off. During VCA release, oscillators and filter continue rendering. When VCA reports `isSilent()` and there is no audible free-running/modulation/feedback need, the engine may enter a deterministic idle optimization without changing phase semantics; resumption state is defined by Workstream 06.

Oscillator 3 keyboard control determines whether selected pitch/pitch wheel drive it. Its Mixer switch only controls audio contribution. Its modulation source runs whenever a routed modulation destination needs it, regardless of Mixer switch. LO/free-run operation is independent of held notes.

## UI event queue contract

Use a fixed-capacity SPSC queue from message thread to audio thread. Each event has a monotonic sequence and target sample time relative to the next callback; if exact device time is unavailable, assign sample 0 of the next block. Producer never blocks. On overflow, set an atomic overflow flag; the audio thread consumes it first and performs a source-scoped UI panic so no stuck UI note survives. Capacity and overflow counters are published through diagnostics and acceptance manifest.

## Backward compatibility and preset behavior

Workstream 03 adds saved priority/trigger parameters. Missing state defaults Low/Single. No legacy single-current-note behavior is preserved because it is defective and cannot represent a coherent Model D policy. MIDI input channel remains Omni initially; channel-filter settings are future schema additions.

## Real-time audio constraints

Event arrays and note storage are fixed-capacity and preallocated. Sorting is bounded by event capacity; prefer stable insertion/bucket processing by sample offset. No locks, allocations, dynamic containers, exceptions, strings, listener calls, or UI repaint from the callback. Overflow has the deterministic panic behavior above.

## Edge cases and failure modes

Same-sample note-on/off, duplicate presses, out-of-order note-off, velocity zero, events at block end, invalid offsets, more events than capacity, sustain during priority changes, mode automation at the same sample, host stop without note-offs, UI focus loss, sample-rate reprepare, and multiple channels playing the same note.

Mode changes take effect at their sub-block boundary. Changing priority recalculates selected note and emits pitch change without retrigger. Changing Single→Multi does not retroactively retrigger; the next qualifying note-on does. Multi→Single retains the current gate.

## Implementation sequence

1. Create exact MIDI fixtures that expose current block-early behavior and stuck/release failures.
2. Implement bounded event normalization/order and range renderer with a silent test engine.
3. Implement duplicate-aware note stack and Low/High/Last selection.
4. Implement Single/Multi gate policy, sustain, CC120/123, resets, and diagnostics.
5. Add SPSC UI path and overflow panic; update editor keyboard/focus hooks.
6. Integrate PitchControl transitions, then contour/oscillator contracts behind test doubles.
7. Freeze lifecycle trace format and run block-partition invariance tests.

## Automated tests and measurable gates

- An event at sample `N` changes a sentinel render beginning exactly at `N`; samples before `N` match the no-event render exactly.
- Identical absolute event times rendered with different host block partitions produce identical lifecycle traces and audio under the shared floating-point policy.
- Table-driven fixtures pass every Low/High/Last press/release permutation, duplicate count, same-note multi-channel/source case, and priority-mode change.
- Single mode emits no retrigger during legato; after full gate close the next note emits gate open. Multi emits retrigger only for qualifying selected note-ons.
- Sustain, CC120, CC123, focus loss, transport reset, queue overflow, and reprepare leave no held notes or stuck audible gate under their defined semantics.
- VCA release fixture contains oscillator/filter signal until contour silence; ordinary note-off does not reset oscillator phase.
- Osc3 mixer/control/modulation truth table passes all switch/note combinations.
- Audio-thread instrumentation reports zero lock attempts and allocations in event ingestion/dispatch.

## Manual, host, and listening validation

Play overlapping scales/trills in Low, High, and Last; legato in Single/Multi; sustain interactions; repeated same notes; DAW loop boundaries; stop/restart; all-notes-off; on-screen keyboard drags/focus loss; and dense MIDI at several buffer sizes. Record MIDI/audio and compare event transients at sample positions. Listening confirms phrasing after numerical event/trace tests pass.

## Definition of done

- [ ] MID-001 through MID-011 pass.
- [ ] Low/Single are saved defaults; High/Last/Multi restore correctly.
- [ ] Range renderer respects every MIDI sample offset.
- [ ] No audio-thread MIDI lock or stuck-note path remains.
- [ ] Release and Osc3 truth-table tests pass.

## Completion-report evidence

Include lifecycle state diagram; event-order table; note-stack/trigger trace outputs; sample-index audio plots; block-partition hashes; sustain/panic/overflow/focus-loss results; Osc3 truth table; release-tail phase evidence; lock/allocation instrumentation; and host/manual MIDI matrix.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 19, 23–25, 47–50, 80–81.
- JUCE [`MidiBuffer`](https://docs.juce.com/master/classMidiBuffer.html), [`MidiMessage`](https://docs.juce.com/master/classMidiMessage.html), and [`AbstractFifo`](https://docs.juce.com/master/classAbstractFifo.html).
- MIDI Association [MIDI 1.0 message summary](https://midi.org/specifications-old/item/table-1-summary-of-midi-message).
