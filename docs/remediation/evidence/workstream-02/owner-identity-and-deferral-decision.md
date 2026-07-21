# Owner identity and temporary external-gate deferral

Date: 2026-07-21
Decision source: direct product-owner approval in the Agent 06 continuation
conversation

## Factual distribution-history decision

The product owner confirmed that no version of this product has previously been
distributed. There is therefore no shipped bundle identifier, AU
manufacturer/subtype pair, preset library, or DAW project identity that must be
preserved while replacing the development placeholders.

## Approved technical product identity

The owner delegated selection of the identity values and approved this exact
contract:

| Field | Approved value |
|---|---|
| Manufacturer/company name | `TTH Audio` |
| Product name | `TTH Model One` |
| Manufacturer code | `TTHA` |
| Product code | `TM01` |
| Reverse-DNS manufacturer domain | `io.github.tthrelk93` |
| Bundle identifier | `io.github.tthrelk93.TTHModelOne` |
| Manufacturer website | `https://github.com/tthrelk93` |

The repository's `github.com/tthrelk93` ownership supplies the stable namespace
basis. The four-character codes are distinct printable ASCII values. This is an
owner-approved technical plug-in identity; it is not a representation that TTH
Audio is incorporated or that a trademark has been registered.

The authoritative values are committed in `cmake/ProductIdentity.cmake`. A
fresh macOS arm64 Release configure with
`SYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON` accepted them after the same guard was
observed failing for the former placeholders.

## Fresh post-change verification

On 2026-07-21 at 13:35 PDT, commit
`87b5418e24b6e47b48c7029714c7995e0f642858` was verified on macOS 13.0.1
arm64 with AppleClang 14.0.3, CMake 4.3.1, and a space-bearing build path.

The Release configure enabled distribution-identity validation, warnings as
errors, tests, and validators. It resolved JUCE 8.0.10 at exact commit
`3af3ce009f6a02f6fa651008fffb5b41743a9fab`; the strict all-target build passed;
and CTest passed 9/9. Each required label contained tests: `unit=1`, `state=1`,
`dsp=1`, `midi=1`, `realtime=1`, `host=2`, and `artifact=2`.

The build produced `TTH Model One.vst3`, `TTH Model One.component`, and
`TTH Model One.app`. Generated JUCE definitions and metadata record manufacturer
`TTH Audio`, codes `TTHA` / `TM01`, bundle
`io.github.tthrelk93.TTHModelOne`, VST3 category `Instrument|Synth`, and AU type
`aumu`.

No commercial DAW, designated-user AU registration, local designated-account
`auval`, or Steinberg SDK validator was run in this baseline. Their statuses
remain deferred and non-passing.

## Independent-review correction and final local verification

An independent code review found two handoff defects after the initial identity
baseline: hosted macOS CI still named `MiniMoog.component` literally, and the
BLD-007 guard validated only approval, manufacturer name, and domain. Commit
`a18444e8336626ecf5c627558454264f55d823ea` fixes both without changing DSP,
parameters, state, buses, or wrapper topology.

The CI workflow now discovers exactly one staged `AU/*.component`, derives its
basename, refuses an overwrite, and permits cleanup only for a direct
`*.component` child of the expected account-local Components directory. Its
static lifecycle contract rejects the former literal name and requires the
dynamic install/cleanup safety checks. `actionlint` 1.7.12 reports zero errors,
and a bounded local simulation resolves and removes `TTH Model One.component`.

Identity validation is now shared by distribution configure, staged-artifact
verification, validation-manifest generation, and linked-manifest verification.
The new negative configure suite proves rejection of empty/placeholder product
and bundle values, a malformed domain, short or non-ASCII-alphanumeric codes,
duplicate codes, reserved codes, and an unapproved identity.

At 14:08 PDT, a fresh space-path macOS arm64 Release configure at the exact
implementation commit above resolved the pinned JUCE revision and passed a
warnings-as-errors all-target build. CTest passed 10/10 with label counts
`unit=1`, `state=1`, `dsp=1`, `midi=1`, `realtime=1`, `host=2`, and
`artifact=3`. Actual-wrapper and pluginval each passed 3/3; the linked manifest
deep-verifier passed with identity aggregate `pass` and release aggregate
`blocked`. Non-mutating local `auval` remained `blocked`, and the Steinberg SDK
validator and commercial-host rows remained unrun.

The resulting build-manifest SHA-256 is
`722666e6c9faa46f9a29707d4a27218927f90cabeb9bf89286e93764c379cf2d`;
the validation-manifest SHA-256 is
`d18db83780b4fbd94e2d59dedafe42e99aff6aa7c4c00e15198a692791166ad0`;
and the standalone-lifecycle report SHA-256 is
`eff02934322455bc9020c75ad60ebdafdba44bf93f50a1333adbdba12c258e7b`.
These reports are build-tree evidence; this document retains their reproducible
commands, exact commit, outcomes, and hashes.

## Temporary external-validation deferral

The owner authorized Workstream 03 to begin while commercial-host checks,
designated-account AU registration/`auval`, and the Steinberg SDK validator are
temporarily unavailable. These checks remain unrun pre-release requirements;
they are not waived or treated as passing.

The requirement effect is:

| Requirement | Current status | Decision effect |
|---|---|---|
| BLD-006 | `in-progress` | Generated VST3 `Instrument|Synth`, AU `aumu`, bus, wrapper, pluginval, and ephemeral hosted-auval evidence remains valid; commercial-host category/menu evidence is deferred. |
| BLD-007 | `pass` | No prior distribution is confirmed, the exact identity is owner-approved and committed, and the distribution configure guard accepts it. |
| BLD-011 | `in-progress` | Automated evidence remains valid; designated-account AU, Steinberg validator, and commercial-host smoke checks are deferred. |
| BLD-012 | `in-progress` | Development manifests remain usable; final distribution aggregation waits for BLD-006/011 external evidence. |

This is a bounded scheduling exception, not a Workstream 02 completion claim.
Public release, distribution, notarization, store submission, and any statement
that all Workstream 02 acceptance criteria pass remain prohibited until the
deferred evidence is captured and reconciled.

## Dedicated validation account

The owner authorized creation and use of a dedicated standard macOS validation
account. The current administrator cannot create it noninteractively without a
local authentication prompt. The owner will create it or enter the
administrator credential locally. No password may appear in source, logs,
evidence, chat, or a continuation archive.

The missing account does not block Workstream 03 under the scheduling
exception. Once available, it is used to capture designated-user AU
registration/`auval` and approved host evidence for BLD-011.
