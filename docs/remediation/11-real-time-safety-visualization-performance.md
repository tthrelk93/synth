# Workstream 11 — Real-Time Safety, Visualization, and Performance

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [UI](10-ui-correctness-authentic-panel.md) · Next: [Reference system](12-hardware-reference-regression-system.md)

## Goal and user-visible outcome

Audio remains glitch-free, race-free, finite, and predictably fast while editors open/close, visualizations run, automation changes, MIDI is dense, multiple instances play, and hosts vary sample rate/block size. Diagnostics never write to a user’s Desktop or mutate global process state.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| RT-001 | Remove audio-thread mutex/spinlock use, including MIDI and visualization paths. |
| RT-002 | Remove audio-thread allocations/deallocations, per-sample `juce::String`, formatting, and hidden container growth. |
| RT-003 | Remove unconditional Desktop logging and process-global logger mutation; make diagnostics per-instance, opt-in, off-thread, and bounded. |
| RT-004 | Replace unsynchronized waveform/stage arrays with a formally race-free fixed-capacity snapshot channel. |
| RT-005 | Bound capture/publication/UI visualization work independently of host block shape and drop frames safely instead of blocking. |
| RT-006 | Cache pitch/filter/contour/control calculations and prove no unnecessary expensive work in steady state. |
| RT-007 | Prove multi-instance state isolation, deterministic reset, and no project-owned data races with sanitizers where supported. |
| RT-008 | Establish allocation/lock instrumentation and approved CPU/latency benchmark gates across sample-rate/buffer/voice/UI matrices. |

## Current-code evidence

- `processBlock` acquires `midiCriticalSection`; UI note handlers acquire the same lock. `CircularBuffer` uses a `SpinLock` between audio/UI, even though the audio side uses try-lock and drops data.
- `EnvelopeGenerator::getNextSample` and `Oscillator::processNextSample` construct `juce::String` objects per sample.
- Processor construction creates `my_plugin_log.txt` on the Desktop, installs `FileLogger` as the process-global current logger, and the destructor clears the global logger, interfering with other instances/components.
- Audio writes `osc1Buffer/osc2Buffer/osc3Buffer` and twelve `stage*Buffer` arrays while UI reads/copies them. Publishing only `stageBufferWriteIndex` does not prevent concurrent element reads/writes or torn multi-array frames.
- `copyStageBuffers` copies every full stage ring; editor timer creates/copies additional large stage structures. Work is not coordinated through a frame protocol.
- Per-sample paths repeatedly set oscillator frequency/filter cutoff, evaluate powers, and call filter setters; sample rate/contour rates are also set repeatedly per block.

## Prerequisites, ownership, and merge conflicts

Requires event queue/lifecycle (05), DSP processing contracts (06–09), UI consumer (10), and test instrumentation/benchmark harness (12). It is a release-blocking cross-cutting workstream.

Owns real-time instrumentation, snapshot channel, diagnostic service, performance counters/benchmarks, and callback safety policy. Algorithm owners must fix failures inside their workstreams; RT-008 cannot waive them.

Conflict hotspots: processor members/callback, `CircularBuffer`, `WaveformDisplay`, `SignalFlowOverlay` data ingestion, logging, and every DSP inner loop. Land instrumentation early, then snapshot API, then optimize only from profiles.

## In scope

Audio-thread safety, SPSC events/snapshots, diagnostics, visualization capture/decimation, control caching, sanitizers, multi-instance tests, CPU/latency/denormal/stress benchmarks, and reset containment.

## Out of scope

DSP sound-design changes, GPU UI rendering, telemetry/network reporting, or inventing a CPU budget without a product performance manifest.

## Proposed architecture and data flow

```text
audio callback
  -> fixed UI EventQueue (consumer)
  -> ModelDEngine render
  -> VisualizationCollector (fixed decimation/storage)
  -> SpscSnapshotQueue<VisualizationSnapshot, 3>::tryPush
       [if full: drop new frame, increment atomic counter]

message thread timer
  -> tryPopLatest (discard older queued frames)
  -> paint immutable local snapshot

any thread diagnostics
  -> atomic counters / fixed records
  -> non-RT DiagnosticDrain -> opt-in file chosen by user
```

`VisualizationSnapshot` has fixed metadata, levels, contour/filter state, and fixed arrays of decimated stage samples. Capture uses a sample-rate-derived phase accumulator and immutable frame size/rate declared in a visualization manifest. It performs a bounded constant amount per rendered sample and publishes at the target frame cadence; it never copies a host-sized buffer. The queue never overwrites unread storage, so producer/consumer never access the same slot concurrently. UI drains to the newest complete frame.

The existing `CircularBuffer` and direct oscillator/stage getters are removed from cross-thread use. Atomics are used for small scalar counters only; arrays travel through the SPSC ownership protocol.

## Public interfaces and contracts

```cpp
struct VisualizationSnapshot { uint64_t sequence; double sampleRate; FixedLevels levels; FixedStageFrames stages; };
template <typename T, size_t Capacity> class SpscSnapshotQueue {
public: bool tryPush(const T&) noexcept; bool tryPopLatest(T&) noexcept;
};
struct RealtimeCounters { uint64_t allocations, deallocations, lockAttempts, snapshotDrops, invalidSamples, solverFallbacks; };
class DiagnosticDrain { void start(UserChosenDestination); void stop(); };
```

Instrumentation tags the audio thread during callback and intercepts project allocation plus wrapped project locks. CI fails on any nonzero allocation/deallocation/lock count after warm-up/prepare. Third-party wrapper activity outside the tagged callback is reported separately.

## Diagnostics and logging policy

Default logging is memory-only bounded counters. No file is created without an explicit user/developer action. File drain runs on a background thread, uses a per-instance/session ID, never becomes JUCE’s global logger, redacts user paths unless requested, rotates to a configured bound, and stops before processor destruction. Release builds omit verbose sample/control tracing.

## Performance and threshold policy

Benchmark matrix includes supported sample rates, minimum/typical/maximum host block scenarios, dense MIDI/automation, silence/self-oscillation/max feedback, editor closed, Authentic open, Signal Flow open, and multiple instances. Measure wall time, callback percentile/max, CPU cycles if available, cache/profile hotspots, latency, snapshot drops, invalid/containment counts, and underruns in a controlled runner.

Numeric CPU budgets come only from an approved `performance` section in `acceptance-v1.json` that records target hardware, OS/power mode, compiler/configuration, measurement method, instance count, safety margin rationale, approver, and date. Until populated, benchmarks report values but RT-008 remains open; implementers do not choose a convenient budget.

## Backward compatibility and state

Visualization and diagnostic data are non-preset state. Removing Desktop logging changes no audio/preset contract. An opt-in diagnostic preference may be saved in application preferences, never host state. Snapshot format is internal/versioned; UI consumers reject mismatched versions and show no visualization rather than reading partial data.

## Real-time constraints

The [roadmap real-time contract](00-master-remediation-roadmap.md#real-time-contract) is normative. Additionally, snapshot drop/queue overflow, invalid-sample containment, and instrumentation must themselves be lock-free/bounded. No sanitizer runtime ships in Release artifacts.

## Edge cases and failure modes

Editor absent/closed mid-frame, UI stalled, snapshot queue full, giant/zero host block, sample-rate reprepare, diagnostic drain slow/unwritable, multiple processors destructing, denormals, NaN feedback, and ThreadSanitizer false positives from non-instrumented host/JUCE modules. Queue full drops frames; diagnostic failure disables the drain and records an in-memory error; neither affects audio.

## Implementation sequence

1. Add callback tagging and allocation/lock/invalid counters; record current failures.
2. Remove global/Desktop logging and per-sample strings/formatting.
3. Replace MIDI/CircularBuffer/stage-array sharing with fixed SPSC channels.
4. Add visualization decimation/publication and immutable UI consumption.
5. Profile and cache pitch/filter/contour/parameter work without changing outputs.
6. Add sanitizer, multi-instance, denormal, stress, and benchmark CI jobs; freeze performance manifest.

## Automated tests and measurable gates

- After prepare/warm-up, every approved callback scenario records exactly zero project allocations, deallocations, and lock attempts.
- ThreadSanitizer-supported standalone/test runs report zero project-owned data races; suppressions require source/owner/expiry and cannot hide project files.
- Snapshot sequence is monotonic; consumer receives only complete checksummed frames; stalled consumer causes drops but no producer wait/data race.
- Visualization disabled/enabled produces audio identical under shared floating-point policy and bounded capture work independent of editor/UI rate.
- Multi-instance randomized construction/render/editor/destruction shows no shared logger/state, cross-instance MIDI, race, crash, or leaked thread.
- Invalid finite/NaN-injection tests produce no non-finite host output; containment counters meet each DSP plan’s required zero normal-operation count.
- Performance matrix passes every approved CPU/callback/latency/snapshot-drop budget; missing manifest entry is `not-run`/open.
- Optimized output/lifecycle traces match pre-optimization golden fixtures within the appropriate hard/derived manifest.

## Manual and host validation

Stress small buffers/high rates, dense automation/MIDI, rapid editor view/open/close, window resizing, background/foreground, audio-device changes, multiple instances, offline bounce, and diagnostic enable/disable. Use host CPU/underrun meters only as supplemental data; the controlled benchmark remains authoritative.

## Definition of done

- [ ] RT-001 through RT-008 pass.
- [ ] No audio-thread lock/allocation/string/file/global-logger path remains.
- [ ] All visualization data crosses a verified SPSC ownership boundary.
- [ ] Sanitizer/multi-instance/invalid/denormal suites pass.
- [ ] Approved performance manifest and reports exist.

## Completion-report evidence

Include before/after instrumentation counters and profiles; snapshot ownership proof/test output; sequence/drop/checksum logs; TSAN/suppression report; multi-instance/destruction results; diagnostic destination/rotation failure tests; CPU/latency matrix with target-hardware provenance; output-equivalence hashes; and host stress notes.

## Primary technical references

- Ross Bencina, [Real-time audio programming 101: time waits for nothing](https://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing).
- JUCE [`AbstractFifo`](https://docs.juce.com/master/classAbstractFifo.html) and [`AudioProcessor`](https://docs.juce.com/master/classAudioProcessor.html).
- LLVM [ThreadSanitizer documentation](https://clang.llvm.org/docs/ThreadSanitizer.html).
- ISO C++ [memory model reference](https://en.cppreference.com/w/cpp/language/memory_model) (secondary language reference; implementation review should cite the applicable ISO standard section).
