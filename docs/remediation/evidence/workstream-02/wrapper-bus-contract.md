# Workstream 02 Instrument Wrapper and Bus-Contract Evidence

Implementation commits: `1d5be04`, `6b8896e`, and `405a704` on
`codex/workstream-02-build`.

Independent review: final review approved specification compliance and task
quality with no Critical, Important, or Minor findings. Earlier review feedback
led to test-only External Input sensitivity and false-positive hardening.

## Implemented contract

- The sole `ModelDPlugin` product is generated with `IS_SYNTH TRUE`, MIDI input
  enabled, MIDI output disabled, and MIDI-effect mode disabled. No effect target
  is declared.
- Generated JUCE 8 definitions report VST3 category `Instrument|Synth` and AU
  main type `aumu`. The supported CMake build does not use the historical
  tracked `JucePluginDefines.h`; effective evidence comes from JUCE-generated
  `Defs.txt`, AU `Info.plist`, and VST3 `moduleinfo.json`.
- Processor buses are exactly one optional stereo-by-default `External Input`,
  one required stereo-by-default `Main Output`, and one optional
  stereo-by-default `Phones/Cue`. VST3 marks input bus 0 as auxiliary.
- Layout negotiation accepts exactly Main mono/stereo crossed with External
  disabled/mono/stereo and Phones disabled/mono/stereo: 18 combinations.
  Missing/extra buses, disabled/surround/discrete Main, and surround/discrete
  optional buses are rejected without assertions.
- Processing uses JUCE named bus views. The current mono engine renders Main as
  mono or dual mono and mirrors enabled Phones/Cue at unity without new
  parameter semantics.

## Test-first and focused evidence

The initial processor-contract RED run linked the real plug-in target and
failed eight historical wrapper/topology expectations: synth and MIDI flags,
VST3/AU types, runtime MIDI capability, output-bus count, and VST3 auxiliary
extension. The implementation GREEN run reported all 18 required layouts,
representative invalid-layout rejection, deterministic no-MIDI silence,
finite A440 routing for mono/stereo Main and Phones, zero-sample handling,
reset, and reprepare.

Enabled External Input coverage uses an explicit external-input-only existing
patch. Zero mono and stereo controls are silent; nonzero inputs must materially
change output. Mono input matches duplicated stereo, distinguishable stereo
matches its mono average, output-only process channels are poisoned, and Main
plus both Phones channels must remain sample-identical where required. An
ignored-input mutation triggered both sensitivity assertions; a forced
left-channel-only mutation triggered all three stereo-downmix assertions.

Restored focused output included:

```text
EXTERNAL_INPUT case=zero-control mono_abs_sum=0 stereo_abs_sum=0 result=true
EXTERNAL_INPUT case=mono-duplication difference_from_zero=132.505 result=true
EXTERNAL_INPUT case=stereo-downmix difference_from_zero=126.728 result=true
Model D processor contract passed
```

## Local verification

Fresh Debug and Release paths contained spaces, used JUCE 8.0.10 commit
`3af3ce009f6a02f6fa651008fffb5b41743a9fab`, and configured
`SYNTH_WARNINGS_AS_ERRORS=ON`. All four required targets built, CTest and the
offline renderer passed, generated AU/VST3 metadata matched the contract, and
`git diff --check` passed.

BLD-005 and BLD-006 remain `in-progress`: supported-CI results, staged artifact
validation, and validator/host category scans are not yet complete.
