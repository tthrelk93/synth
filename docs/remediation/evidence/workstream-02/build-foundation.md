# Workstream 02 Build-Foundation Evidence

Implementation commit: `42d42c2925227198bbc53d237c9656167ed01be6` on `codex/workstream-02-build`.

Independent task review: approved for specification compliance and code quality with no Critical, Important, or Minor findings.

## Implemented seams

- CMake 3.24+ and C++20 top-level project.
- Stable `ModelDCore`, `ModelDPlugin`, `ModelDTests`, and `ModelDOfflineRenderer` targets.
- JUCE 8.0.10 pinned to `3af3ce009f6a02f6fa651008fffb5b41743a9fab` through `FetchContent`.
- `SYNTH_JUCE_SOURCE_DIR` local override that rejects a non-Git root or any other `HEAD` revision before adding JUCE.
- Project-owned core/plugin compatibility headers and binary resources; supported compilation does not use `MiniMoog.jucer` or tracked `JuceLibraryCode` amalgamations.
- One plug-in declaration, plus real test and offline-render executables linked to project/JUCE code.
- Single unapproved development identity seam in `cmake/ProductIdentity.cmake`; opt-in distribution validation hard-fails while BLD-007 is blocked.

## Test-first evidence

RED before implementation:

```sh
red_build_dir=$(mktemp -d '/tmp/model-d-task2-red.XXXXXX')
cmake -S . -B "$red_build_dir"
```

Result: exit 1 because the source directory did not contain `CMakeLists.txt`.

GREEN used source and build paths containing spaces. Both Debug and Release ran:

```sh
cmake -S '<Model D Source>' -B '<Debug or Release Build>' \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE='<Debug or Release>' \
  -DSYNTH_JUCE_SOURCE_DIR='<exact-revision JUCE checkout>'
cmake --build '<build-dir>' \
  --target ModelDCore ModelDPlugin ModelDTests ModelDOfflineRenderer -j 4
ctest --test-dir '<build-dir>' --output-on-failure
'<build-dir>/ModelDOfflineRenderer'
```

Results for each configuration: configure exit 0; all four targets built; CTest 1/1 passed; offline renderer reported `Rendered 480 ModelDCore samples`.

Configure recorded:

```text
Resolved JUCE 8.0.10 at commit 3af3ce009f6a02f6fa651008fffb5b41743a9fab
```

A local override at JUCE parent commit `8146e30d8b23150e33b0f24386a09d5689a8b621` exited 1 before JUCE addition with the expected revision-mismatch error. `SYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON` exited 1 before dependency setup because manufacturer/domain values are placeholders and identity is unapproved. Normal Debug and Release compilation remain available; this is not distribution approval.

The task commit passed `git diff --check`. Existing project warnings remain deliberately visible for BLD-008; this milestone does not claim a zero-warning baseline, cross-platform CI, dependency license review, or any BLD requirement pass.
