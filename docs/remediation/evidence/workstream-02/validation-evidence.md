# Workstream 02 Validator and Linked-Manifest Evidence

Implementation commits: `8e8dd3a`, `fdd941c`, `e1fdc86`, `2164ff0`, and
`a42ec79` on `codex/workstream-02-build`.

Independent review approved the final validator path-safety and evidence
contracts with no Critical, Important, or Minor findings. Hosted and commercial
host execution remains unavailable.

## Local macOS arm64 Release result

`ModelDVerifyValidationEvidence` produced and deep-verified one linked build and
validation evidence set:

- Project-owned actual VST3 wrapper host: 3/3 fresh instances passed scan,
  identity, buses, processing/MIDI, editor, state, teardown, and isolation.
- pluginval 1.0.4: 3/3 separate processes passed strictness 10 with GUI tests,
  a fixed seed, 60-second timeout, and a scrubbed validation environment.
- Standalone lifecycle: 9/9 fresh processes passed across normal, invalid, and
  no-device modes; see [Standalone lifecycle](standalone-lifecycle.md).
- `auval` 1.10.0: `blocked`. The exact command was
  `auval -v aumu Via9 Manu`; the AU was not registered in the local account and
  the non-mutating helper intentionally did not install it.
- Steinberg VST3 SDK validator: `not-run` because no executable was configured.
- Commercial host rows for Logic Pro, Ableton Live, REAPER, and Cubase Pro are
  `blocked`; all individual checks remain `not-run`. The broader standalone
  host-matrix rows remain `not-run`.

The release aggregate therefore remains `blocked`. A green verifier means the
manifest faithfully records this mixed result; it does not convert blocked or
unrun gates to pass.

## Provenance and deterministic hashes

The final local evidence binds every report to Darwin arm64 Release, project
version 1.0.0, the staged VST3/Standalone/AU aggregate hashes, the source commit,
the exact pluginval executable, the fixed host-matrix definition, and the
required/resolved JUCE commit.

Two consecutive finalization/verification runs produced byte-identical files:

| File | SHA-256 |
|---|---|
| `build-manifest.json` | `93943197b44972c67d886d5a14b76dfa677a5486f085f55010c8bf393d983589` |
| `validation-manifest.json` | `7edf740f2888083a218faa8228ae33af16c7d24199db19d3b01830061026f287` |
| `standalone-lifecycle-report.json` | `fabcdf09cef6ba36939cf080fe82c56fcd39da7d7169bf9696156143c8c31214` |

The build manifest explicitly records `identity_approved: false`,
`distribution_eligible: false`, and source `dirty: true` because the supplied
planning documents were intentionally untracked during implementation. These
hashes describe this exact local evidence set, not future builds.

## Safety and negative contracts

Provisioning is pinned by platform asset URL, official archive SHA-256, exact
version output, executable hash, and a scrubbed environment. The executable
override used in the final local run was independently version/hash recorded;
the archive path remained `not-run` for that run.

Manifest verification requires exact evidence set equality, re-hashes every
report/product, deep-verifies actual-wrapper and standalone contents, binds
OS/architecture/configuration, rejects unsafe/symlinked paths before mutation,
and rejects tampered hashes, status fields, repeat counts, seeds, identity,
tool versions, host rows, or undeclared evidence. Debug and Release
`ModelDValidatorPathSafety` tests passed, including simulated Windows and Linux
branches; Windows symbolic-link cases capability-skip if link creation is not
permitted.

BLD-011 is `blocked` on AU registration, the VST3 SDK validator, commercial
hosts, and supported hosted execution. BLD-012 remains `in-progress` because
the linked manifests are locally deterministic but the release aggregate is
blocked by BLD-007/BLD-011 and hosted evidence.
