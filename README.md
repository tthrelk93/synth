# MiniMoog

A JUCE 8 Model D–inspired MIDI instrument. The supported build produces one
instrument product as VST3 and Standalone on macOS, Windows, and Linux, plus AU
on macOS. It does not produce a separate effect plug-in.

The checked-in product identity is development-only and is not approved for
distribution. Release packaging remains blocked until the manufacturer name,
four-character codes, reverse-DNS domain, and distribution history are reviewed
in `cmake/ProductIdentity.cmake`.

## Prerequisites

- CMake 3.24 or newer
- Git; network access unless an exact pinned JUCE checkout is supplied
- A C++20 toolchain: Xcode/AppleClang, MSVC, or GCC/Clang
- On Ubuntu, the packages installed by `.github/workflows/ci.yml`: ALSA, JACK,
  LADSPA, FreeType, Fontconfig, X11 development libraries, Xvfb, and Xauth

JUCE 8.0.10 is fetched and verified at commit
`3af3ce009f6a02f6fa651008fffb5b41743a9fab`; `JUCE_DIR`, Projucer, and the
tracked generated amalgamations are not part of the supported build.

## Build and test

These Release commands are the same configure, build, test, lifecycle, and
validation commands exercised by the Release rows in CI:

```sh
cmake -S . -B "build with spaces/Release" -DCMAKE_BUILD_TYPE=Release -DSYNTH_WARNINGS_AS_ERRORS=ON -DSYNTH_BUILD_TESTS=ON -DSYNTH_BUILD_VALIDATORS=ON
cmake --build "build with spaces/Release" --config Release --parallel 2
ctest --test-dir "build with spaces/Release" -C Release --output-on-failure
cmake --build "build with spaces/Release" --config Release --target ModelDRunStandaloneLifecycle --parallel 2
cmake --build "build with spaces/Release" --config Release --target ModelDVerifyValidationEvidence --parallel 2
```

Use `Debug` in both the directory and configuration arguments for the CI Debug
variant. The validation target records unavailable external gates as `blocked`
or `not-run`; a successful evidence-verifier invocation is not, by itself, a
distribution approval.

CI also runs every required CTest label independently with fail-on-zero
enforcement: `unit`, `state`, `dsp`, `midi`, `realtime`, `host`, and `artifact`.

## Development artifacts

CI stages products and manifests beneath:

```text
build with spaces/<config>/development-stage/<config>-<system>-<architecture>/
  VST3/MiniMoog.vst3
  Standalone/MiniMoog.app | MiniMoog.exe | MiniMoog
  AU/MiniMoog.component                 # macOS only
  build-manifest.json
  validation-manifest.json
```

Validator logs, lifecycle reports, and lifecycle screenshots are under
`build with spaces/<config>/validation/`. The staged manifest contains their
relative paths and SHA-256 hashes.

## Dependency licensing

JUCE modules are dual-licensed under AGPLv3 and the commercial JUCE licence.
The build pin and dependency inventory are recorded in
`docs/remediation/evidence/workstream-02/dependency-license-review.md`; the
project owner must confirm the applicable JUCE licensing path before
distribution.
