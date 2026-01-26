
# MiniMoog

A JUCE-based Model D–inspired synthesizer with a standalone UI.

## Prerequisites

- CMake 3.22+
- C++17 toolchain (Xcode/clang, MSVC, or GCC)
- JUCE (download from https://juce.com/get-juce or the JUCE GitHub repo)

## Build and Run (CMake)

1. Clone the repo.
2. Make JUCE available by setting `JUCE_DIR` or passing it to CMake:
   - `export JUCE_DIR=/path/to/JUCE`
   - or `cmake -S . -B build -DJUCE_DIR=/path/to/JUCE`
3. Configure and build:
   - `cmake -S . -B build -DJUCE_DIR="$JUCE_DIR"`
   - `cmake --build build --config Release`
4. Run the standalone app:
   - macOS: `build/MiniMoog_artefacts/Release/Standalone/MiniMoog.app`
   - Windows: `build/MiniMoog_artefacts/Release/Standalone/MiniMoog.exe`
   - Linux: `build/MiniMoog_artefacts/Release/Standalone/MiniMoog`

## Build with Projucer (Optional)

1. Open `MiniMoog.jucer` in Projucer.
2. Set the JUCE modules path.
3. Save and export, then build the generated project in your IDE.
