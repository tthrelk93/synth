# Workstream 03 parameter registry v2

## Contract delta

The production registry advances from 44 to 48 descriptors. The exact legacy
ID prefix remains, in order and with JUCE version hint 0:

```text
osc1Waveform, osc2Waveform, osc3Waveform, osc1Range, osc2Range, osc3Range,
osc1Vol, osc2Vol, osc3Vol, tune, osc2Freq, osc3Freq, filterCutoff,
filterEmphasis, filterContour, outputVolKnob, extInputVolKnob, ctrlGlideKnob,
ctrlModMixKnob, filterAttackTimeKnob, filterDecayTimeKnob,
loudnessAttackTimeKnob, loudnessDecayTimeKnob, filterSustainKnob,
noiseVolKnob, loudnessSustainLevelKnob, outputPhonesVolKnob, feedbackKnob,
modWheelValue, pitchWheelValue, osc1OnOff, osc2OnOff, osc3OnOff,
a440HzOnOff, osc3CtrlMode, oscModSwitch, noiseOnOffSwitch,
extInputVolSwitch, whitePinkSwitch, filterModSwitch, keyboardCtrlSwitch1,
keyboardCtrlSwitch2, decaySwitch, glideSwitch
```

Exactly four saved, automatable descriptors follow that prefix:

| Index | ID | Version | Kind / choices | Display name | Default |
| ---: | --- | ---: | --- | --- | --- |
| 44 | `keyboard.priorityMode` | 1 | choice: `Low`, `High`, `Last` | `Note Priority` | `Low` |
| 45 | `keyboard.triggerMode` | 1 | choice: `Single`, `Multi` | `Trigger Mode` | `Single` |
| 46 | `output.mainEnabled` | 1 | bool | `Main Output Enabled` | on |
| 47 | `output.phonesEnabled` | 1 | bool | `Phones Output Enabled` | on |

The exact legacy display-name corrections are:

| ID | Legacy display name | Registry v2 display name |
| --- | --- | --- |
| `tune` | `Tune` | `Master Tune` |
| `osc2Freq` | `Oscillator 2 Frequency` | `Oscillator 2 Frequency Offset` |
| `osc3Freq` | `Oscillator 3 Frequency` | `Oscillator 3 Frequency Offset` |
| `filterCutoff` | `Filter Cutoff Frequency` | `Filter Cutoff` |
| `filterContour` | `Filter Contour` | `Filter Amount of Contour` |
| `outputVolKnob` | `Output Volume` | `Main Output Volume` |
| `extInputVolKnob` | `Output Volume` | `External Input Volume` |
| `ctrlGlideKnob` | `Output Volume` | `Glide Time` |
| `ctrlModMixKnob` | `Output Volume` | `Modulation Mix` |
| `filterAttackTimeKnob` | `Filter Attack Time` | `Filter Contour Attack Time` |
| `filterDecayTimeKnob` | `Filter Decay Time` | `Filter Contour Decay Time` |
| `loudnessAttackTimeKnob` | `Loudness Attack Time` | `Loudness Contour Attack Time` |
| `loudnessDecayTimeKnob` | `Loudness Decay Time` | `Loudness Contour Decay Time` |
| `filterSustainKnob` | `Filter Sustain` | `Filter Contour Sustain Level` |
| `loudnessSustainLevelKnob` | `Output Volume` | `Loudness Contour Sustain Level` |
| `outputPhonesVolKnob` | `Output Volume` | `Phones Volume` |
| `feedbackKnob` | `Feedback Knob` | `Feedback Amount` |
| `modWheelValue` | `Mod Wheel Value` | `Modulation Wheel` |
| `pitchWheelValue` | `Pitch Wheel Value` | `Pitch Wheel` |
| `osc1OnOff` | `Oscillator 1 On/Off` | `Oscillator 1 Enabled` |
| `osc2OnOff` | `Oscillator 2 On/Off` | `Oscillator 2 Enabled` |
| `osc3OnOff` | `Oscillator 3 On/Off` | `Oscillator 3 Enabled` |
| `a440HzOnOff` | `A440Hz On/Off` | `A-440 Tuner Enabled` |
| `osc3CtrlMode` | `Oscillator 3 Control Mode` | `Oscillator 3 Keyboard Control` |
| `oscModSwitch` | `Oscillator Mod Switch` | `Oscillator Modulation Enabled` |
| `noiseOnOffSwitch` | `Noise On/Off` | `Noise Enabled` |
| `extInputVolSwitch` | `External Input On/Off` | `External Input Enabled` |
| `whitePinkSwitch` | `White / Pink` | `Noise Color` |
| `filterModSwitch` | `Filter Mod Switch` | `Filter Modulation Enabled` |
| `keyboardCtrlSwitch1` | `Keyboard Control Switch 1` | `Keyboard Control Switch 1` |
| `keyboardCtrlSwitch2` | `Keyboard Control Switch 1` | `Keyboard Control Switch 2` |
| `decaySwitch` | `Decay Switch` | `Decay Enabled` |
| `glideSwitch` | `Glide Switch` | `Glide Enabled` |

Only `tune` changes a legacy default: physical choice index 0 becomes index 5,
whose unchanged ordered choice text is `Zero`. Every other legacy kind, range,
normalization, choice list/order, default, automatable flag, ID, and version hint
is preserved.

Every v2 descriptor now has an explicit semantic unit, mapping, smoothing
class, and `apvtsState` persistence. Semantic units are `choice`, `semitones`,
`panelIndex`, `normalized`, or `boolean`. Mapping remains only
`indexedChoice`, `linear`, or `boolean`. Smoothing is assigned only from the
approved classes `none`, `gainControl`, `control`, `dedicatedPitch`,
`dedicatedCutoff`, `dedicatedGlide`, and `contourStage`.

No hardware-derived taper value was invented. All normalized floats remain
linear, and all ordered panel controls retain indexed-choice host storage.

## Manual-source basis

The bundled `Minimoog_Model_D_Manual.pdf` supports the semantic distinctions:

- pp. 28-31 distinguish Filter Cutoff, Filter Amount of Contour, Filter
  Contour attack/decay/sustain, and Loudness Contour attack/decay/sustain.
- pp. 32-33 distinguish Main Output Volume, Main Output switching, and the
  independently controlled Phones output.
- p. 47 defines Low/High/Last note priority, explicitly calls Low the default,
  and distinguishes Multi-Trigger On from Multi-Trigger Off (Legato Mode).
  The approved `Single` default names the latter single-trigger behavior.
- p. 80 confirms Low/High/Last priority, distinct contour specifications,
  external-input and output-level specifications, and Phones output data.

## Canonical fixtures

| Fixture | SHA-256 |
| --- | --- |
| `Tests/fixtures/parameters/parameter-registry-v2.json` | `2d7d6339fffb3875541aa60547f6e2f2f7b6fb8d288da109bf653291a6b7d284` |
| `Tests/fixtures/parameters/legacy-parameter-inventory.json` | `7ade5c456c54e0822e41082558aed0c94860b6b46f9368713fc3ac103b5bc21d` |
| `Tests/fixtures/state/legacy-default-state.xml` | `07d2069f7c3f274b83e31beab503165064d3fcffd346967281eb2e0157844b21` |
| `Tests/fixtures/state/legacy-representative-state.xml` | `e0d769001dd411425c6dfea6c572b0f9358fdf6cf27b36731eccc3f6526ff0fa` |

The v2 fixture is generated from the production descriptor table by:

```sh
./ModelDTests capture-registry-v2 > parameter-registry-v2.json
```

The Task 1 files remain immutable v0 migration inputs. Tests now validate their
hashes, schemas/roots, structure, exact 44-ID membership/order, and unversioned
state role instead of incorrectly requiring a v2 live instance to equal a v0
default state.

## TDD and verification

RED command:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 4 && \
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^ModelDParameterRegistry$' --output-on-failure
```

The test executable built successfully; `ModelDParameterRegistry` then failed
with 143 assertions identifying the 44-entry registry, legacy metadata/defaults
and policies, absent appended IDs/defaults, and absent v2 fixture. Result: 0/1
passed, as expected before production changes.

Final focused GREEN command:

```sh
cmake --build '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' --config Release --target ModelDTests -j 4 && \
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release -R '^(ModelDParameterRegistry|ModelDLegacyParameterFixtures)$' --output-on-failure
```

Observed result: 2/2 passed, 0 failed.

The complete Release tree built successfully. Full verification command:

```sh
ctest --test-dir '/private/tmp/model-d-agent07-ws03-baseline.oPxqQt/Release build with spaces' -C Release --output-on-failure
```

Observed result: 12/12 passed, 0 failed. The label summary retained all seven
required labels: `artifact`, `dsp`, `host`, `midi`, `realtime`, `state`, and
`unit`.
