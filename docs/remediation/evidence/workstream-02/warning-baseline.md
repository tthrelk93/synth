# Workstream 02 JUCE 8 and Warning-Baseline Evidence

Implementation commit: `5fe79c6` on `codex/workstream-02-build`.

Independent task review: approved for specification compliance and code quality with no Critical, Important, or Minor findings.

## RED inventory

A fresh Debug build with the exact JUCE override and `SYNTH_WARNINGS_AS_ERRORS=ON` failed on project-owned diagnostics in `ModWheel`, `Oscillator`, UI/look-and-feel callbacks, editor/processor unused parameters, `SignalFlowOverlay` deprecated font APIs, and other dead locals. The strict target-level options also initially reached JUCE module sources, which would have mixed third-party diagnostics with the project gate.

## Implementation

- Warning flags and optional `-Werror`/`/WX` are assigned only to explicit project-owned source lists. Pinned JUCE module sources do not inherit the project warning set.
- `SignalFlowOverlay` constructs its 11-point font through `FontOptions` with `TypefaceMetricsKind::legacy`, preserving the deprecated constructor's metrics.
- `GlyphArrangement::getStringWidth` replaces all three `getStringWidthFloat` calls.
- Remaining changes only remove unused declarations/parameter names or correct constructor initializer order; protected DSP, MIDI, state, parameter, preset, wrapper, bus, identity, and UI-geometry behavior is unchanged.

## GREEN verification

Both Debug and Release used paths containing spaces, the exact JUCE checkout, and:

```sh
cmake -S . -B '<build-dir>' -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE='<Debug or Release>' \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DSYNTH_JUCE_SOURCE_DIR='<JUCE at 3af3ce009f6a02f6fa651008fffb5b41743a9fab>' \
  -DSYNTH_WARNINGS_AS_ERRORS=ON
cmake --build '<build-dir>' \
  --target ModelDCore ModelDPlugin ModelDTests ModelDOfflineRenderer --parallel 4
ctest --test-dir '<build-dir>' --output-on-failure
'<build-dir>/ModelDOfflineRenderer'
```

For each configuration, all four targets built with zero project-owned warnings/errors, CTest passed 1/1, and the offline renderer produced 480 finite samples. Compile-command inspection showed `-Wall -Wextra -Wpedantic -Werror` on repository source files and not on JUCE module sources. A project-source scan found no remaining deprecated `Font(float)` construction or `getStringWidthFloat` call, and `git diff --check` passed.

This is a local macOS arm64 warning baseline. BLD-008 remains `in-progress` until supported-CI Debug/Release warning reports are durable.
