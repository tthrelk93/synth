# Agent 07 — Workstream 03 Continuation Kickoff

## Closeout status

This kickoff's implementation action has been completed. The automated
parameter/state contract freeze landed in commits `3206baa` through `f79faa1`;
the consolidated result is the [Workstream 03 verification summary](evidence/workstream-03/workstream-03-verification-summary.md).

PAR-001/003/005/008/009 are `pass`. PAR-002/004/007 remain `in-progress`
because their designated-host exercises are `not-run`, and PAR-006 remains
`in-progress` until Workstream 12/later DSP work supplies the approved-manifest
click/step evidence. BLD-006/011/012 also remain `in-progress` external
pre-release gates. The successor's first engineering action is Workstream 12
harness/manifest coordination against the frozen 48-parameter registry and v2
state contract; Workstream 04 follows in roadmap order.

The instructions below are preserved as the historical execution contract.

You are Agent 07. Continue the synthesizer remediation on branch
`codex/workstream-02-build`, but take primary ownership of **Workstream 03 —
Parameter, Automation, and State Contract**. Workstream 02 remains open only for
the explicitly deferred pre-release evidence described below.

## Mandatory preflight — completed

Before editing:

1. Verify the supplied archive with `unzip -t` and
   `shasum -a 256 -c MANIFEST.sha256` from its extracted root.
2. Read, in order:
   - `planning-suite/00-master-remediation-roadmap.md`
   - `planning-suite/01-traceability-matrix.md`
   - `planning-suite/03-parameter-automation-state-contract.md`
   - `planning-suite/12-hardware-reference-regression-system.md`
   - `HANDOFF.md`
   - `planning-suite/evidence/workstream-02/owner-identity-and-deferral-decision.md`
3. Fetch remote state and compare local HEAD, the upstream branch, and draft PR
   #1. If they differ, reconcile before implementation; do not discard user
   changes or predecessor archives.
4. Run a proportionate local build/CTest baseline before changing parameter or
   state behavior.

## First implementation action — completed

Begin Workstream 03 at its first dependency-ordered action:

1. Inventory every existing host parameter ID, type, range, default, display
   name, unit, automation flag, and processor/editor use site.
2. Capture a machine-readable host enumeration fixture and representative
   unversioned APVTS XML/state fixtures before changing metadata or
   serialization.
3. Add failing registry/fixture tests before implementing the centralized
   descriptor registry or versioned state migration.

Do not begin with oscillator, filter, envelope, MIDI-lifecycle, or UI redesign.
Workstream 03 owns the parameter/state seam that those later workstreams must
consume.

## Frozen inputs

Preserve these established contracts unless the roadmap, owner plan, matrix,
fixtures, and migration record are changed together with explicit evidence:

- JUCE 8.0.10 at commit
  `3af3ce009f6a02f6fa651008fffb5b41743a9fab`;
- existing CMake target names and one-instrument/no-effect wrapper topology;
- the required stereo main output, optional stereo External Input, and optional
  stereo Phones/Cue output bus contract;
- approved identity `TTH Audio` / `TTH Model One`, `TTHA` / `TM01`,
  `io.github.tthrelk93`, bundle `io.github.tthrelk93.TTHModelOne`;
- every existing host parameter ID and the explicit legacy crossed-contour
  compatibility policy;
- the seven required CTest labels and all currently passing build, lifecycle,
  wrapper, manifest, validator, product-identity, and dynamic AU workflow
  contracts.

## Deferred Workstream 02 obligations

The owner authorized development to proceed, but did not authorize fabricated
passes:

- BLD-006 remains `in-progress` pending the fixed commercial-host category and
  menu-placement matrix.
- BLD-007 is `pass`; no prior distribution exists and the approved identity
  guard passes.
- BLD-011 remains `in-progress` pending designated-account AU registration and
  `auval`, the Steinberg VST3 SDK validator, and required commercial-host smoke
  checks.
- BLD-012 remains `in-progress` until distribution aggregation includes the
  final external evidence.

These rows are pre-release gates. They do not block Workstream 03 under the
owner-approved scheduling exception, but they still block release,
distribution, notarization, store submission, and a claim that Workstream 02 is
complete.

The owner authorized a dedicated standard macOS validation account, but its
creation requires local administrator authentication. Never request, retain, or
package a password. Do not purchase hosts or accept third-party legal terms on
the owner's behalf.

## Execution and handoff discipline

- Follow Workstream 03 in roadmap order using test-first implementation.
- Keep the roadmap, traceability matrix, Workstream 03 plan, and canonical
  handoff synchronized after every material milestone.
- Preserve unrelated user changes and all predecessor archives.
- Do not weaken validation to make a gate pass and do not describe unrun manual
  checks as successful.
- Before ending, commit all tracked work, verify it, and create one untracked
  successor ZIP from that exact commit. The ZIP must use one top-level root,
  include `START-HERE.md`, `HANDOFF.md`, the complete planning/evidence suite,
  required primary references, repository state, a verification summary, and
  `MANIFEST.sha256`; it must exclude Git data, builds, tools, credentials,
  settings, recovery files, prior archives, and unrelated workspace content.
- Verify `unzip -t`, safe paths, no symlinks, a fresh extraction, every internal
  hash, and source equality. Report the clickable absolute ZIP path, size,
  outer SHA-256, verification result, and the next exact action.
