# Workstream 03 legacy parameter and unversioned-state inventory

This evidence freezes the pre-registry/pre-versioning APVTS contract at JUCE
8.0.10 commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`. It was captured from
a freshly instantiated real `MoogMiniAudioProcessor`; no production parameter
or state-serialization code was changed to create it.

## Checked-in fixtures

- `Tests/fixtures/parameters/legacy-parameter-inventory.json` is canonical,
  machine-readable host-order enumeration. It records each stable ID and
  version hint, concrete type, name, unit, physical range, normalized and
  physical default, choices, and host flags.
- `Tests/fixtures/state/legacy-default-state.xml` is the exact XML tree from a
  new processor's unversioned `Parameters` APVTS root.
- `Tests/fixtures/state/legacy-representative-state.xml` is the same root after
  setting every live parameter to normalized `0.75` through
  `setValueNotifyingHost`. The fixture records the resulting physical value for
  every ID (choices therefore store their snapped physical index, booleans
  store `1.0`, and floats store `0.75`).

| Fixture | SHA-256 |
| --- | --- |
| `Tests/fixtures/parameters/legacy-parameter-inventory.json` | `7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d` |
| `Tests/fixtures/state/legacy-default-state.xml` | `07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21` |
| `Tests/fixtures/state/legacy-representative-state.xml` | `e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa` |

## Reproducible capture and verification

From an already configured Release build directory (the command is safe to run
before changing the legacy production surface):

```sh
cmake --build . --config Release --target ModelDTests -j 4
./ModelDTests capture-parameters > /tmp/model-d-legacy-parameter-inventory.json
./ModelDTests capture-default-state > /tmp/model-d-legacy-default-state.xml
./ModelDTests capture-representative-state > /tmp/model-d-legacy-representative-state.xml
shasum -a 256 /tmp/model-d-legacy-parameter-inventory.json \
  /tmp/model-d-legacy-default-state.xml \
  /tmp/model-d-legacy-representative-state.xml
ctest -R '^ModelDLegacyParameterFixtures$' --output-on-failure
```

`ModelDLegacyParameterFixtures` has the existing `state` CTest label. It
instantiates the real processor, canonicalizes and compares the host inventory,
and compares both serialized XML trees with attribute and child order intact.
Consequently any ID, host order, type, name, label, range, default, choice,
flag, APVTS root, child order, or serialized value drift fails the test.

## Current ID use-site audit

The processor's layout and processing reads are in
`Source/PluginProcessor.cpp`; the editor uses explicit slider/toggle listeners
and `setValueNotifyingHost`, not JUCE `SliderAttachment` or `ButtonAttachment`.
The table distinguishes active manual controls from inactive/commented control
construction and helper references. No current ID is directly referenced by
`PresetManager`, `SynthPreset`, or `PresetLibrary`; those flows persist the
APVTS XML state as a whole.

| ID | Processor | Editor | Preset |
| --- | --- | --- | --- |
| `osc1Waveform` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc2Waveform` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc3Waveform` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc1Range` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc2Range` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc3Range` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc1Vol` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc2Vol` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc3Vol` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `tune` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc2Freq` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `osc3Freq` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterCutoff` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterEmphasis` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterContour` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `outputVolKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `extInputVolKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `ctrlGlideKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `ctrlModMixKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterAttackTimeKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterDecayTimeKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `loudnessAttackTimeKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `loudnessDecayTimeKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `filterSustainKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `noiseVolKnob` | layout + DSP read | live manual slider + toggle mapping | APVTS XML only |
| `loudnessSustainLevelKnob` | layout + DSP read | live manual slider mapping | APVTS XML only |
| `outputPhonesVolKnob` | layout only; no DSP consumer | inactive/commented control mapping; label/helper references | APVTS XML only |
| `feedbackKnob` | layout + DSP read | inactive/commented control mapping; label/helper references | APVTS XML only |
| `modWheelValue` | layout + DSP read | live wheel mapping | APVTS XML only |
| `pitchWheelValue` | layout + DSP read | live wheel mapping | APVTS XML only |
| `osc1OnOff` | layout + DSP read | live manual slider-toggle + toggle mapping | APVTS XML only |
| `osc2OnOff` | layout + DSP read | live manual slider-toggle + toggle mapping | APVTS XML only |
| `osc3OnOff` | layout + DSP read | live manual slider-toggle + toggle mapping | APVTS XML only |
| `a440HzOnOff` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `osc3CtrlMode` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `oscModSwitch` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `noiseOnOffSwitch` | layout + DSP read | live manual slider-toggle + toggle mapping | APVTS XML only |
| `extInputVolSwitch` | layout + DSP read | live manual slider-toggle + toggle mapping | APVTS XML only |
| `whitePinkSwitch` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `filterModSwitch` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `keyboardCtrlSwitch1` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `keyboardCtrlSwitch2` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `decaySwitch` | layout + DSP read | live manual toggle mapping | APVTS XML only |
| `glideSwitch` | layout + DSP read | live manual toggle mapping | APVTS XML only |
