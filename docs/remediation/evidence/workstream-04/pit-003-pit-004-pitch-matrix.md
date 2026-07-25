# Workstream 04 PIT-003/PIT-004 Pitch Matrix Evidence

Date: 2026-07-24

Status: PIT-003's five-musical-range software matrix is complete, but PIT-003
remains `awaiting-approved-reference` because LO and hardware calibration are
absent. PIT-004 passes its derived and published software gates. Workstream 04
and the product remain incomplete and non-release-ready.

## Claim boundary

This evidence proves:

- exact baseline software calibration for the five musical ranges;
- musical-range, note, tune, Oscillator 2/3 offset, and boundary pitch across
  44.1, 48, and 96 kHz;
- a centered, linear, symmetric Pitch Wheel in the shared semitone domain;
- published `-7` and `+7` semitone wheel endpoints from the bundled manual;
- all 54 governed pitch gates within `0.01` semitone; and
- reciprocal fixture, source, acceptance, report, and release-replay
  authority.

It does not prove or infer the LO base frequency, an LO calibration profile,
hardware calibration, analog drift, PIT-005 through PIT-008, designated-host
behavior, listening results, distribution readiness, or release readiness.
PIT-003 is therefore not `pass`. The global acceptance manifest remains
`draft`.

## Exact implementation identity and chronology

| Purpose | Commit |
|---|---|
| Approved/base merge base | `ab88a2197d7b0809d9554709594d62cabaaca88b` |
| Approved design | `b970d6992293fbae44f79865926951e4f39dea24` |
| Approved implementation plan | `6dbf7b08551ca19c9683002da1400b39e96da521` |
| Typed wheel and baseline calibration | `eac4ce3c7d2fc2be6c8bbfcbfa819cac051ee775` |
| Processor composition and display | `fd2e19b45e129bafffaa6613f63eb761f769ed95` |
| Processor review correction | `89ec602bf86d33956f20d3849d1c4c9839f05d80` |
| Versioned pitch analyzer | `014c20f7849ec39d3e41c95574b339daee3eb22d` |
| Three cross-rate fixtures | `b24539eefadf22d9bb37d6d528210c4978edc899` |
| Fixture review correction | `edf42530a8686d68903e2a7efbf0d76f0ce3193f` |
| Published/hash acceptance authority | `7027a7ba384fb60d02404e235c3a2e8ec1ec7caf` |
| Final implementation and honest requirement reduction | `71ec81ac6e6ae1b9d2431df3113882587e242362` |

The documentation-closeout commit contains this file and cannot name itself
without recursion. Its exact commit/tree/content identity, rebuilt candidate,
release replay, final no-Git build, and final full-suite result are retained in
the ignored Task 7 execution report.

## RED and GREEN chronology

Every task first demonstrated the missing or incorrect behavior.

| Task | RED evidence | GREEN evidence |
|---|---|---|
| 1 | Strict build exited `2` with 13 missing typed-domain API errors. | `ModelDPitchDomainContract` passed 1/1 in `0.55` seconds. |
| 2 | Two focused tests were 0/2: six production pitch assertions and one wheel-display assertion failed. Review RED was `empty-MIDI keyboard-controlled Oscillator 3 must diagnose invalid pitch once`. | Five focused production/regression tests passed 5/5 before and after the review correction. |
| 3 | Analyzer RED contained 16 calibration/diagnostic failures and passed 0/1 because v2 was absent. | Renderer/analyzer/pitch passed 3/3 in `4.07` seconds; the v2 synthetic grid met its limit. |
| 4 | Fixture RED saw five rather than eight fixtures, missing IDs, and seven contract failures. Integration RED exposed 44.1 kHz range-2 at `41.136...` and ambiguous 48/96 kHz C4. The 96 kHz F0 refinement reduced errors of `0.016227474630` then `0.013324391527` before the accepted `0.007773584344`. Review RED exposed 183 failures caused by two missing resets. | The four fixture/reference slices passed 4/4; review pitch passed in `14.24` seconds and the final immutable hashes matched. |
| 5 | Manifest RED saw 13 derived and zero published gates instead of 61/6; 29 focused subtests and requirement reciprocity rejected the missing policy. | Manifest/pitch/requirement passed 3/3 in `642.75` seconds; analyzer passed in `83.34` and requirement in `591.64`. |
| 6 | The valid clean RED failed only the expected requirement projection after `3728.73` seconds. An earlier dirty-source execution and a test-only null dereference were excluded from RED evidence. | Requirement passed in `3504.71` seconds; an independent parent replay also passed after the READY checkpoint. |

## Production wheel and calibration contract

The production equation is exactly:

```text
pitchWheelSemitones = 14 * (normalizedPitchWheel - 0.5)
```

Its five anchors are:

| Normalized position | Semitones |
|---:|---:|
| `0` | `-7` |
| `0.25` | `-3.5` |
| `0.5` | `0` |
| `0.75` | `+3.5` |
| `1` | `+7` |

The 1001-point production grid is finite, linear, monotonic, and symmetric.
The processor composes the wheel contribution once in the shared semitone
domain and the editor displays the same `-7...+7` contract.

Baseline calibration returns exact zero semitones for 32′, 16′, 8′, 4′, and
2′. It rejects LO, unknown ranges, and non-finite inputs. The preserved LO path
bypasses musical baseline calibration and applies only the typed wheel
contribution through `exp2`; it does not invent a calibrated LO base frequency.

## Versioned analyzer contract

`audio.pitch.v1`, analyzer version 1, retains these exact settings and
behavior:

```text
algorithm=normalized-autocorrelation-parabolic-v1
ambiguity-separation=0.02
confidence-threshold=0.80
lag-range=20Hz..5000Hz
minimum-periods=4
peak-tie-tolerance=0.00001
periodic-multiple-tolerance=0.05
```

`audio.pitch.v2`, analyzer version 2, uses:

```text
algorithm=normalized-autocorrelation-parabolic-v2
ambiguity-separation=0.02
confidence-threshold=0.80
lag-range=4Hz..5000Hz
minimum-periods=4
peak-tie-tolerance=0.00001
periodic-multiple-tolerance=0.05
```

V2 applies boxcar decimation only when the safely computed original
correlation work exceeds `64000000`, using
`floor(sampleRate / 12000)`. A decimated result receives bounded
original-rate refinement. V2 gives configured-boundary diagnostics
precedence. None of those changes alter the v1 path.

The v2 synthetic grid's maximum absolute error is `0.001699433` semitone at
44.1 kHz / MIDI 84, below its `0.005` synthetic-calibration limit.

## Immutable inputs and published source

| Artifact | SHA-256 |
|---|---|
| Bundled `Minimoog_Model_D_Manual.pdf` | `c7e6f1abd54999cad7aa232d782df397f974ff210db1e6a429a863af6e850b1f` |
| `Tests/reference/fixtures/pit-003-pit-004-pitch-matrix-44100-v1.json` | `7f5f62bfc7799a1a16b89bb4bfb98b684b505b25080c1cc4fd764bf664ed133e` |
| `Tests/reference/fixtures/pit-003-pit-004-pitch-matrix-48000-v1.json` | `49863b9875d4b7aec6a541e27a60e942ef495c39ca36348ecd9eb4e38f781394` |
| `Tests/reference/fixtures/pit-003-pit-004-pitch-matrix-96000-v1.json` | `11e60f4810f7563e6f139420c2635e9bdbadb62f9182d02e49950174c7ee4e79` |
| `Tests/reference/fixture-index-v1.json` | `e9e30173ad5aed6dbbfc277cbf5604293691c5fe2cd6b0ab01da0fcdddc58c52` |

The unchanged PIT-002 fixture remains
`63a2a95b9caee4799d814abe647e4c2433b82eb59fddebf502f38f68864e12ee`.
The three new index entries own PIT-003/PIT-004 in ascending sample-rate order
and bind the exact fixture hashes above. Manual source, version, page `80`,
and source SHA are mandatory for the two published endpoint gates.

## All 54 governed pitch errors

Every row uses `audio.pitch.v2`, analyzer version `2`, metric
`midi-semitones`, unit `semitones`, and allowance `0.01`.

### 44.1 kHz

| Gate | Expected | Measured | Absolute error |
|---|---:|---:|---:|
| `pit003.sr44100.boundary.f0-range32` | -7.000000000000000 | -6.999442442901543 | 0.000557557098457 |
| `pit003.sr44100.boundary.c4-range2` | 84.000000000000000 | 84.000288698720041 | 0.000288698720041 |
| `pit003.sr44100.range32` | 21.000000000000000 | 20.999175753617525 | 0.000824246382475 |
| `pit003.sr44100.range16` | 33.000000000000000 | 32.999678307701622 | 0.000321692298378 |
| `pit003.sr44100.range8` | 45.000000000000000 | 45.000793454341249 | 0.000793454341249 |
| `pit003.sr44100.range4` | 57.000000000000000 | 56.999980635257309 | 0.000019364742691 |
| `pit003.sr44100.range2` | 69.000000000000000 | 69.000047478331680 | 0.000047478331680 |
| `pit003.sr44100.tune.minus2p5` | 42.500000000000000 | 42.499947567800383 | 0.000052432199617 |
| `pit003.sr44100.tune.plus2p5` | 47.500000000000000 | 47.499981656933144 | 0.000018343066856 |
| `pit003.sr44100.osc2.minus8` | 37.000000000000000 | 36.999819214983901 | 0.000180785016099 |
| `pit003.sr44100.osc2.plus8` | 53.000000000000000 | 53.000304176929319 | 0.000304176929319 |
| `pit003.sr44100.osc3.minus8` | 37.000000000000000 | 36.999819214983901 | 0.000180785016099 |
| `pit003.sr44100.osc3.plus8` | 53.000000000000000 | 53.000304176929319 | 0.000304176929319 |
| `pit004.sr44100.bend.minus7` | 38.000000000000000 | 37.999785848229635 | 0.000214151770365 |
| `pit004.sr44100.bend.minus3p5` | 41.500000000000000 | 41.499890490475231 | 0.000109509524769 |
| `pit004.sr44100.bend.center` | 45.000000000000000 | 45.000793454341249 | 0.000793454341249 |
| `pit004.sr44100.bend.plus3p5` | 48.500000000000000 | 48.499805260152819 | 0.000194739847181 |
| `pit004.sr44100.bend.plus7` | 52.000000000000000 | 51.999979788575274 | 0.000020211424726 |

The maximum 44.1 kHz error is `0.000824246382475` semitone at
`pit003.sr44100.range32`.

### 48 kHz

| Gate | Expected | Measured | Absolute error |
|---|---:|---:|---:|
| `pit003.sr48000.boundary.f0-range32` | -7.000000000000000 | -7.000583748560825 | 0.000583748560825 |
| `pit003.sr48000.boundary.c4-range2` | 84.000000000000000 | 83.999794739507280 | 0.000205260492720 |
| `pit003.sr48000.range32` | 21.000000000000000 | 20.999385393037343 | 0.000614606962657 |
| `pit003.sr48000.range16` | 33.000000000000000 | 33.001977988989594 | 0.001977988989594 |
| `pit003.sr48000.range8` | 45.000000000000000 | 45.000009758146923 | 0.000009758146923 |
| `pit003.sr48000.range4` | 57.000000000000000 | 57.000215544791928 | 0.000215544791928 |
| `pit003.sr48000.range2` | 69.000000000000000 | 69.000034249233622 | 0.000034249233622 |
| `pit003.sr48000.tune.minus2p5` | 42.500000000000000 | 42.500049555568282 | 0.000049555568282 |
| `pit003.sr48000.tune.plus2p5` | 47.500000000000000 | 47.500116529781010 | 0.000116529781010 |
| `pit003.sr48000.osc2.minus8` | 37.000000000000000 | 36.999502827868781 | 0.000497172131219 |
| `pit003.sr48000.osc2.plus8` | 53.000000000000000 | 52.999987049288080 | 0.000012950711920 |
| `pit003.sr48000.osc3.minus8` | 37.000000000000000 | 36.999502827868781 | 0.000497172131219 |
| `pit003.sr48000.osc3.plus8` | 53.000000000000000 | 52.999987049288080 | 0.000012950711920 |
| `pit004.sr48000.bend.minus7` | 38.000000000000000 | 38.000818889978461 | 0.000818889978461 |
| `pit004.sr48000.bend.minus3p5` | 41.500000000000000 | 41.500213497537032 | 0.000213497537032 |
| `pit004.sr48000.bend.center` | 45.000000000000000 | 45.000009758146923 | 0.000009758146923 |
| `pit004.sr48000.bend.plus3p5` | 48.500000000000000 | 48.499898705962757 | 0.000101294037243 |
| `pit004.sr48000.bend.plus7` | 52.000000000000000 | 52.000037843744344 | 0.000037843744344 |

The maximum 48 kHz error is `0.001977988989594` semitone at
`pit003.sr48000.range16`.

### 96 kHz

| Gate | Expected | Measured | Absolute error |
|---|---:|---:|---:|
| `pit003.sr96000.boundary.f0-range32` | -7.000000000000000 | -6.992226415655960 | 0.007773584344040 |
| `pit003.sr96000.boundary.c4-range2` | 84.000000000000000 | 83.999977507379839 | 0.000022492620161 |
| `pit003.sr96000.range32` | 21.000000000000000 | 21.005411181877115 | 0.005411181877115 |
| `pit003.sr96000.range16` | 33.000000000000000 | 33.001946175994426 | 0.001946175994426 |
| `pit003.sr96000.range8` | 45.000000000000000 | 45.000251807087380 | 0.000251807087380 |
| `pit003.sr96000.range4` | 57.000000000000000 | 57.000141503342192 | 0.000141503342192 |
| `pit003.sr96000.range2` | 69.000000000000000 | 69.000082171549735 | 0.000082171549735 |
| `pit003.sr96000.tune.minus2p5` | 42.500000000000000 | 42.499980903332784 | 0.000019096667216 |
| `pit003.sr96000.tune.plus2p5` | 47.500000000000000 | 47.499077708625023 | 0.000922291374977 |
| `pit003.sr96000.osc2.minus8` | 37.000000000000000 | 36.999893585813183 | 0.000106414186817 |
| `pit003.sr96000.osc2.plus8` | 53.000000000000000 | 52.999690790849463 | 0.000309209150537 |
| `pit003.sr96000.osc3.minus8` | 37.000000000000000 | 36.999893585813183 | 0.000106414186817 |
| `pit003.sr96000.osc3.plus8` | 53.000000000000000 | 52.999690790849463 | 0.000309209150537 |
| `pit004.sr96000.bend.minus7` | 38.000000000000000 | 38.000374900076764 | 0.000374900076764 |
| `pit004.sr96000.bend.minus3p5` | 41.500000000000000 | 41.499492389091699 | 0.000507610908301 |
| `pit004.sr96000.bend.center` | 45.000000000000000 | 45.000251807087380 | 0.000251807087380 |
| `pit004.sr96000.bend.plus3p5` | 48.500000000000000 | 48.500123945507049 | 0.000123945507049 |
| `pit004.sr96000.bend.plus7` | 52.000000000000000 | 51.999783343802179 | 0.000216656197821 |

The maximum 96 kHz and overall error is `0.007773584344040` semitone at
`pit003.sr96000.boundary.f0-range32`. All 54 errors are below `0.01`.

## Authoritative candidates and report truth

The clean final implementation commit produced:

```text
/private/tmp/model-d-pitch-candidates.GUJovf/candidate-a
/private/tmp/model-d-pitch-candidates.GUJovf/candidate-b
```

Each contains exactly 50 regular files and `diff -qr` is silent. Their common
file-content tree hash is
`67cf6bc79d7cad6e4a54e068e9715e74c64a9cea914fc8600a86fa301d2517c1`;
both `requirements-report.json` files hash to
`5ae7986de4d53e828e807ad4123b59e5106ee388e774c8e61ae19a60a1ccff1a`.

The exact inventory is two root files, `metrics.json` and
`requirements-report.json`, plus these six files under each render:
`control-trace.json`, `event-trace.json`, `main.wav`, `metrics.json`,
`phones.wav`, and `render.json`. The eight render directories are:

```text
legacy-contour-foundation
migrated-v2-foundation
native-v2-foundation
pit-001-oscillator-offset-sweep-v1
pit-002-composed-pitch-v1
pit-003-pit-004-pitch-matrix-44100-v1
pit-003-pit-004-pitch-matrix-48000-v1
pit-003-pit-004-pitch-matrix-96000-v1
```

The candidate records source commit `71ec81ac6e6ae1b9d2431df3113882587e242362`,
source tree `c64b5599ef0f74ce69afc69454a155fc8c9b6673`, content identity
`ad1138aaab7dc02bf349e3c300b8a84e2415be76d57fb28d12042e517074b478`,
clean source, Darwin arm64 Release, AppleClang `14.0.3.14030022`, and JUCE
`3af3ce009f6a02f6fa651008fffb5b41743a9fab`.

| Governed render metric artifact | SHA-256 |
|---|---|
| 44.1 kHz `metrics.json` | `ec772d7564805e12b9d30d79034ed051e16b81302aa87764a69a03b56fb854ee` |
| 48 kHz `metrics.json` | `f58443a5a1bf50fed0f80b6d8dec11832c737736ce9c3af29bb6bfe8b5e2e8ec` |
| 96 kHz `metrics.json` | `34df8f7e91fc42f6e7bd62b23d40af95943cd422a96491c2fd12ece7d9028170` |

The report has exactly 127 requirements and 164 gates:

```text
pass=17
not-run=91
awaiting-approved-reference=19
fail=0
releaseReady=false
```

PIT-003 is `awaiting-approved-reference` with reason
`acceptance.metric-pass`: its software gates pass, but LO/hardware evidence is
missing. PIT-004 is `pass` with reason `requirement.gates-pass`.

Release verification exits `3`, writes zero stdout bytes, and writes exactly:

```text
release-not-ready: BLD-006 not-run requirement.not-run
```

## Clean verification matrix

The authoritative path-with-spaces Release build used tests on, validators on,
warnings-as-errors on, and exact JUCE
`3af3ce009f6a02f6fa651008fffb5b41743a9fab`.

- strict all-target build passed in `206.41` seconds;
- full serial CTest passed 23/23 in `4424.73` seconds;
- artifact passed 5/5 in `3093.34` seconds;
- dsp passed 5/5 in `58.72` seconds;
- host passed 2/2 in `20.50` seconds;
- midi passed 1/1 in `0.02` seconds;
- realtime passed 2/2 in `0.10` seconds;
- state passed 10/10 in `4179.14` seconds;
- unit passed 6/6 in `102.62` seconds; and
- the exact focused Task 7 regex passed all eight registered selections in
  `2994.08` seconds.

CTest/CMake `4.3.1` does not emit `Labels:` under plain `ctest -N`, so the
brief's exact label pipeline produced an empty file. `ctest -N -V` and
`ctest --show-only=json-v1` independently exposed the same exact seven-label
inventory used above. The focused regex names seven standalone guards that are
not registered in the current 23-test inventory; no pass is invented for those
absent names. Full CTest covers the live aggregate authority contracts and the
registered legacy/state/contour tests.

The ten frozen fixture/parameter/state/contour paths are byte-identical to
`ab88a2197d7b0809d9554709594d62cabaaca88b`; the frozen diff is empty. The
manual and immutable fixture/index hashes match the table above.

At clean implementation commit `71ec81a`, `git archive HEAD` extracted to
`/private/tmp/model-d-pitch-extract.dUEkZZ` without `.git`. Release configure
passed in `42.97` seconds with tests off, validators off, warnings-as-errors
on, and exact JUCE; the all-target production build passed in `162.11`
seconds. Exact documentation-head verification is retained in the ignored
Task 7 report because recording that identity here would be self-referential.

## Explicit retained work

The next Workstream 04 engineering requirement is PIT-005. PIT-006 through
PIT-008 also remain open. PIT-003 remains the first unmet pitch-evidence gate
until an approved LO/hardware calibration campaign supplies its missing
reference inputs. Parallel BLD, PAR, and TST obligations remain non-pass as
recorded in their owner ledgers.

Nothing in this evidence claims hardware authenticity, designated-host
validation, listening approval, distribution approval, or release readiness.
