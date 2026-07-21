# Agent 01 Kickoff — Workstream 02 Build, Packaging, and Host Validation

This file is the copy-ready prompt for the first implementation agent. It starts phase F0 and only Workstream 02. The earlier documentation-only restriction applied to creation of the planning suite; this agent is authorized to modify implementation, build, validation, and documentation files required by Workstream 02, within the boundaries below.

---

You are Agent 01 for the JUCE Model D synthesizer remediation.

## Mission

Implement **Workstream 02 — Build, Packaging, and Host Validation** and provide verified evidence for requirements BLD-001 through BLD-012. Establish the reproducible build and validation foundation required by every later workstream. Do not begin Workstream 03 or any DSP, parameter-semantic, preset, recreation, or photo-import remediation.

Repository root:

`/Users/agentt/.openclaw/workspace/Developer/synth`

Planning and handoff documents:

1. Master roadmap: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/00-master-remediation-roadmap.md`
2. Traceability matrix: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/01-traceability-matrix.md`
3. Your authoritative plan: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/02-build-packaging-host-validation.md`
4. Next-workstream context only: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/03-parameter-automation-state-contract.md`
5. Test-harness dependency context only: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/12-hardware-reference-regression-system.md`
6. Canonical handoff ledger, which you must maintain: `/Users/agentt/.openclaw/workspace/Developer/synth/docs/remediation/17-implementation-handoff.md`

The roadmap’s fixed product decisions and phase gates are binding. The Workstream 02 plan is the decision-complete implementation authority for your scope. The matrix supplies the exact finding, dependency, behavior, acceptance criterion, and verification mapping for each BLD requirement. If documents genuinely conflict, stop the affected change, record the conflict in the handoff, and request a product decision; do not silently choose new behavior.

## Mandatory preflight

Before editing:

1. Change to the repository root and read any applicable repository instructions such as `AGENTS.md` found between the filesystem root and this repository.
2. Run and record `git status --short`, `git branch --show-current`, and `git rev-parse HEAD` in the handoff. Preserve all pre-existing changes; never discard or overwrite work you did not create.
3. Read the roadmap, the entire traceability introduction and BLD-001–BLD-012 rows, the entire Workstream 02 plan, and the current handoff. Read Workstreams 03 and 12 only far enough to preserve their declared build/test seams and dependencies.
4. Reinspect the current repository by path and symbol. Confirm each Workstream 02 finding still applies without relying on historical line numbers. Record changed or disproven evidence in the plan and matrix before implementation proceeds.
5. Determine from repository history, release records, artifacts, and product-owner evidence whether any plug-in binary has shipped and whether any product/manufacturer identifiers must remain compatible. Do not infer distribution history from code alone.
6. Set the Workstream 02 roadmap row and the handoff snapshot to `in-progress`. Update the Workstream 02 progress table as soon as a requirement begins.

## Scope and fixed decisions

You own only the files and seams necessary for BLD-001–BLD-012:

- top-level CMake and `cmake/` dependency/product-identity support;
- authoritative target definitions and removal of machine-specific build dependencies;
- JUCE 8.0.10 pinning at commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`;
- CI, CTest discovery, validators, artifact staging, and build manifests;
- wrapper identity, instrument flags/categories, processor bus declarations/layout negotiation, and standalone lifecycle;
- JUCE 8 font API migration and project-owned warning cleanup;
- README build and validation commands after those commands pass.

The product is one MIDI instrument target with required Main Output, optional External Input, and optional Phones/Cue output. Do not create a separate effect target. Do not change parameter IDs, normalization, defaults, DSP algorithms, note behavior, presets, UI geometry, or audio semantics except where the Workstream 02 bus/wrapper/standalone/font contracts explicitly require it.

The exact legal manufacturer name, manufacturer code, product code, and reverse-DNS domain are external product inputs. Investigate and request them when absent; never invent them. Implement the release-blocking identity contract defined by Workstream 02. A placeholder or unreviewed development identity cannot satisfy BLD-007 and cannot produce a passing distribution manifest. Record that state as `blocked`, not `pass`.

Generated project files are not authoritative. Do not casually mass-delete tracked generated files: first remove them from supported compilation, prove the CMake replacement, then make any repository cleanup explicit and reviewable.

## Required implementation order

Follow this order unless current-code evidence proves a dependency requires a narrower reorder. Record any reorder and reason in the handoff.

1. **Baseline and identity investigation:** capture the current configure failure, machine-specific JUCE paths, generated wrapper identity, distribution evidence, and legal-identity availability.
2. **Reproducible build:** add CMake 3.24+/C++20, exact JUCE pin plus verified local override, and stable `ModelDCore`, `ModelDPlugin`, `ModelDTests`, and `ModelDOfflineRenderer` targets. First compile current behavior without opportunistic remediation.
3. **JUCE 8 and warning baseline:** replace the scoped removed/deprecated font APIs and eliminate project-owned Debug/Release warnings without blanket suppression.
4. **Instrument wrapper and buses:** implement the fixed synth/MIDI/category contract and the exact Main/External Input/Phones layout table. Preserve internal DSP behavior beyond the wrapper seam.
5. **Tests and CI:** add CTest labels, prove test execution with a temporary sentinel failure, then remove the sentinel and record both failing and passing evidence. Cover supported platform/configuration combinations defined in the plan.
6. **Artifact and host validation:** add deterministic staging/manifests, pluginval strictness 10, auval on macOS, scan/instantiate/editor/MIDI/state/bus smoke coverage, and the designated host matrix. An unavailable tool or host is `not-run`/`blocked`, never `pass`.
7. **Standalone lifecycle:** test normal, invalid, and absent audio/MIDI device states; capture repeatable visible-window evidence.
8. **Documentation:** update README only with clean-clone commands and artifact paths exercised by CI, then reconcile every BLD status and completion checklist item.

## Verification and completion rules

Treat every automated and manual gate in Workstream 02 as mandatory. At minimum, record:

- clean configure/build commands from a path containing spaces;
- exact JUCE commit verification and offline override behavior;
- Debug and Release warning reports;
- CTest discovery, per-label execution, and sentinel-failure proof;
- wrapper capability and exhaustive accepted/rejected bus-layout results;
- finite/non-silent MIDI smoke render and deterministic no-MIDI behavior under the defined patch;
- pluginval, auval, artifact scanning, repeated instance/editor/state tests, and exact tool versions;
- standalone-window and commercial-host results with OS, architecture, and host versions;
- artifact hashes plus the generated build/validation manifest;
- `git diff --check` and a final inventory of changed files.

Do not mark a requirement `pass` because implementation exists. `pass` requires the stated acceptance criterion and linked verification evidence. Do not weaken a threshold, suppress a validator, or change a requirement merely to obtain a pass. Subjective listening cannot replace numerical, host, state, or build evidence.

Run the most focused tests after each stage and the complete applicable Workstream 02 suite before handoff. If an external dependency prevents full completion, finish all independent in-scope work, leave the tree coherent, and identify the exact blocked requirement, failed command, required external input, and safe resumption point.

## Continuous planning-document maintenance

Maintaining the planning suite is part of your implementation responsibility, not an end-of-task cleanup.

After every material milestone and before every pause or handoff:

1. Update the **Implementation control** table in `00-master-remediation-roadmap.md` with the active phase, owner, truthful status, blocker, and next gate.
2. Update the **Implementation progress** table and Definition of Done checkboxes in `02-build-packaging-host-validation.md`. Use exactly `not-started`, `in-progress`, `blocked`, `fail`, or `pass`; link evidence for every `pass`.
3. Update the relevant BLD row in `01-traceability-matrix.md` if current evidence, dependency, behavior, acceptance, verification, or ownership has changed. Do not alter the matrix merely to report progress and never reassign primary ownership without updating the roadmap and owner plan in the same change.
4. Update `17-implementation-handoff.md` continuously using its required schema. The handoff must always be usable if your session ends unexpectedly.
5. Cross-link any new durable evidence report from the workstream progress table and handoff. Do not use transient console output as the only evidence for a pass.

Planning changes must describe reality. Do not check a box early, erase unresolved deficits, or rewrite historical findings without retaining the evidence and reason.

## Handoff obligations

The canonical handoff is `docs/remediation/17-implementation-handoff.md`. Update its mutable current snapshot throughout the task and append a dated work entry before stopping for any reason. Preserve previous entries.

Your handoff must state exactly:

- branch, starting and ending commit, dirty state, and pre-existing changes;
- every file and important symbol you changed, with the reason;
- every BLD requirement’s status and durable evidence location;
- exact commands run, meaningful results, failures, and tools/hosts not run;
- build artifacts and manifests produced, including paths and hashes where applicable;
- product-identity/distribution evidence and any unresolved approval;
- decisions, assumptions, deviations, and planning-document changes;
- known defects, partial work, temporary scaffolding, and cleanup still required;
- the first unmet gate and the exact files/commands where the next agent should resume;
- the next agent’s workstream and roadmap phase.

Before ending the session, create and verify a self-contained successor ZIP
from the final committed planning/handoff state. Leave the ZIP untracked and
report its absolute path, size, outer SHA-256, and verification result. The
archive must follow the canonical package protocol in
`17-implementation-handoff.md`, and the successor's `START-HERE.md` must repeat
this obligation so the chain continues for every agent.

The next-owner rule is deterministic:

- If any BLD-001–BLD-012 requirement is not `pass`, the next agent remains in **Workstream 02 / F0** and starts at the first unmet dependency-ordered BLD requirement. Do not advance the roadmap.
- Only if all BLD requirements pass and all Workstream 02 completion evidence is present may the next agent begin **Workstream 03 / F0**. That agent starts by verifying the frozen CMake target/bus/test seams, then reads `03-parameter-automation-state-contract.md` and coordinates the Workstream 12 harness contract. Do not start F2.
- Workstream 12’s harness skeleton may be separately assigned after Workstream 02’s test/offline targets are stable, but it does not replace the Workstream 03 prerequisite and must not redefine parameter semantics.

## Completion response

When stopping, report the outcome first, then:

1. BLD requirements passed, failed, blocked, or not run;
2. implementation and planning files changed;
3. verification commands and results;
4. external inputs or host access still required;
5. handoff path and the exact next-agent starting point.

Do not claim Workstream 02 or F0 complete unless the roadmap, owner plan, traceability evidence, tests, and handoff all agree.

---

Related planning documents: [Roadmap](00-master-remediation-roadmap.md) · [Traceability matrix](01-traceability-matrix.md) · [Workstream 02](02-build-packaging-host-validation.md) · [Canonical handoff](17-implementation-handoff.md)
