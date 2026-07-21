# Workstream 02 Artifact-Staging and Build-Manifest Evidence

Implementation commits: `835b3d4`, `20dedb1`, `fdd941c`, `e1fdc86`,
`2164ff0`, and `a42ec79` on
`codex/workstream-02-build`.

Independent review: approved after path-containment, exact product/payload,
and required-format enforcement were hardened. Hosted Windows/Linux/macOS
execution remains not-run.

## Implemented contract

- `ModelDStageDevelopmentArtifacts` stages only JUCE's authoritative
  `JUCE_PLUGIN_ARTEFACT_FILE` products: VST3 and Standalone on every platform,
  plus AU on macOS. `COPY_PLUGIN_AFTER_BUILD` remains off.
- The default stage is an isolated configuration/OS/architecture child beneath
  the build tree. Cleanup requires the exact project marker and refuses broad,
  source, build, home, drive-root, non-direct-child, or unmarked targets.
- `build-manifest.json` records project version, runtime Git commit/dirty state,
  exact required/resolved JUCE commit, compiler and stable SDK identifier,
  configuration, OS, architecture, enabled formats, full current identity and
  approval flags, every staged payload SHA-256, deterministic product aggregate
  hashes, and any declared build-tree test-report hashes.
- Placeholder identity is explicit:
  `identity_approved: false` and `distribution_eligible: false`.
- `ModelDVerifyStagedArtifacts` independently parses JSON, requires the exact
  platform format set, enforces each product/payload relationship, resolves
  existing paths beneath trusted real roots, re-hashes every payload/report,
  and rejects missing, changed, duplicate, extra, unsafe, or undeclared files.

## Test-first and negative evidence

The RED target build failed because
`ModelDStageDevelopmentArtifacts` did not exist. After implementation:

- fresh path-with-spaces Debug and Release stage/verify targets passed;
- Debug CTest passed 7/7 and retained every required label;
- two identical post-commit stage runs produced byte-identical manifests;
- payload mutation failed with a hash mismatch;
- an undeclared staged file failed verification;
- missing and duplicate report declarations failed;
- `../../../outside`, `..\\outside`, drive/root/UNC forms, and resolved paths
  outside trusted roots failed;
- removing Standalone or changing a product/payload-root relationship failed;
- a single-file fixture exercised the native Windows/Linux standalone branch;
- re-staging restored every mutation and verification returned green.

## Current local artifact

The initial staging verification used:

```text
/tmp/model d task6a debug/development-stage/Debug-Darwin-arm64/
  VST3/MiniMoog.vst3
  Standalone/MiniMoog.app
  AU/MiniMoog.component
  build-manifest.json
```

Two immediate staging runs at commit `20dedb1` produced manifest SHA-256
`79088c71470e9d7494ecae3515734a1d0ce22c67adfadcd8bc02f3359fc20cf3`.
The hash is evidence for this exact source/artifact state, not a promised hash
for a later commit.

Task 6B subsequently added a cryptographically linked validation manifest and
exact report inventory. Two consecutive Release finalization runs at commit
`a42ec79` produced byte-identical build manifest SHA-256
`93943197b44972c67d886d5a14b76dfa677a5486f085f55010c8bf393d983589`
and validation manifest SHA-256
`7edf740f2888083a218faa8228ae33af16c7d24199db19d3b01830061026f287`.
See [validator and linked-manifest evidence](validation-evidence.md).

## Remaining BLD-012 gates

BLD-012 remains `in-progress`: supported hosted jobs have not executed,
BLD-011 host/validator evidence is blocked, and BLD-007 legal identity is
blocked. No distribution package or passing release claim is produced.
