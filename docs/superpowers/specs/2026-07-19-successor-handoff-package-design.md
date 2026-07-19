# Successor Handoff Package Design

Date: 2026-07-19

Repository: `/Users/agentt/.openclaw/workspace/Developer/synth`

Active branch: `codex/workstream-02-build`

## Purpose

Every implementation agent must finish its session with one self-contained ZIP
that the user can attach to the next agent. The package must preserve the same
planning and primary-reference context supplied to Agent 01, add the current
canonical handoff and implementation evidence, and give the successor an
unambiguous first action.

The first successor artifact under this protocol is:

`Agent-02-Workstream-02-Continuation-Context.zip`

The ZIP is a ready-to-send handoff artifact and remains untracked. Planning and
protocol changes are committed; the archive itself is not added to Git.

## Package layout

The archive has one top-level directory named
`Agent-02-Workstream-02-Continuation-Context/` containing:

```text
START-HERE.md
HANDOFF.md
ARCHIVE-CONTENTS.md
MANIFEST.sha256
planning-suite/
  00-master-remediation-roadmap.md
  01-traceability-matrix.md
  02-build-packaging-host-validation.md through
    16-agent-01-workstream-02-kickoff.md
  17-implementation-handoff.md
  evidence/workstream-02/*.md
primary-reference/
  Minimoog_Model_D_Manual.pdf
original-audit/
  approved-planning-suite.txt
repository-context/
  GIT-STATE.md
  VERIFICATION-SUMMARY.md
  README.md
  CMakeLists.txt
  ci.yml
  ProductIdentity.cmake
  required-host-matrix.json
```

`planning-suite/` preserves the current `docs/remediation/` relative structure,
including all durable Workstream 02 evidence. `HANDOFF.md` is a convenience copy
of the canonical `17-implementation-handoff.md`; the canonical copy remains in
`planning-suite/` as well.

The primary manual and approved audit are copied from the supplied Agent 01
context, preserving the self-contained reference baseline. The selected
repository contracts are orientation snapshots only; `GIT-STATE.md` identifies
the authoritative branch and commit, and `START-HERE.md` requires the successor
to inspect the live repository before editing.

## Successor kickoff contract

`START-HERE.md` is a copy-ready prompt for the next agent. It must:

1. Identify Workstream 02 / F0 as still active.
2. Record the exact branch and handoff commit used to build the package.
3. Direct the agent to read the roadmap, BLD traceability rows, Workstream 02
   plan, canonical handoff, and linked evidence before editing.
4. Preserve the frozen target, bus, status-token, artifact, and evidence-path
   contracts.
5. Start at BLD-001 by obtaining the checked-in eight-job hosted-CI evidence.
6. List the external identity, JUCE licensing, AU registration, VST3 SDK
   validator, and commercial-host inputs still required.
7. Forbid promotion to Workstream 03 until every BLD row and Workstream 02 exit
   gate passes.
8. Require the successor to update the canonical planning/handoff documents and
   produce a new verified successor ZIP before ending its own session.

## Permanent protocol changes

The successor-package obligation is added to three durable authorities:

- `00-master-remediation-roadmap.md`: a repository-wide handoff-package policy
  applying to every implementation agent and workstream.
- `16-agent-01-workstream-02-kickoff.md`: the current kickoff's handoff duties,
  so the original prompt also documents the rule.
- `17-implementation-handoff.md`: exact package contents, safety/verification
  rules, and a new required template field for the successor archive.

Future kickoff prompts inherit the canonical handoff rule. Each agent must
update planning and handoff truthfully, commit its tracked changes, build the
untracked archive from that committed state, verify it, and report its absolute
path and SHA-256 to the user.

An archive cannot contain its own final outer SHA-256 without a recursive
self-reference. Therefore `MANIFEST.sha256` verifies every file inside the ZIP,
while the assistant's final response reports the ZIP file's outer SHA-256.

## Repository-state context

`GIT-STATE.md` records:

- repository root, branch, exact HEAD, and baseline commit;
- short status with user-supplied/extracted context explicitly identified;
- recent commit history and Workstream 02 implementation range;
- tracked files changed since the baseline;
- the rule that the live repository, not copied snapshots, is authoritative.

`VERIFICATION-SUMMARY.md` records the latest fresh Debug/Release 9/9 CTest
results, clean-clone README result, wrapper/pluginval/standalone repeat counts,
manifest hashes, blocked/unrun validators and hosts, recovery paths, requirement
statuses, and the exact resumption point. It links readers back to durable
planning evidence instead of replacing it.

## Archive construction and safety

Construction uses a new temporary directory. Only the files enumerated by this
design are copied into it. The process excludes `.git`, build trees, validator
downloads, prior ZIPs, extracted prior handoff directories, user settings,
recovery files, credentials, and unrelated workspace content.

The temporary package tree is removed only after the ZIP and an independent
test extraction both verify. The final ZIP remains at the repository root as an
untracked artifact. Existing user-supplied archives and extracted context are
not modified.

## Verification gates

The package is ready only if all gates pass:

1. Tracked protocol/document changes pass `git diff --check` and local Markdown
   link validation, then are committed.
2. Every intended file exists and every unintended file class is absent.
3. `MANIFEST.sha256` covers every packaged regular file except itself and
   verifies successfully from a fresh extraction.
4. `unzip -t` reports no errors.
5. Archive entries contain no absolute paths, `..` traversal, or symbolic
   links.
6. Fresh extraction has exactly one expected top-level directory.
7. The packaged planning-suite and primary-reference files hash-match their
   selected sources.
8. `START-HERE.md`, `HANDOFF.md`, `GIT-STATE.md`, and
   `VERIFICATION-SUMMARY.md` contain the exact branch/commit, status, blockers,
   first unmet gate, and successor-package obligation.
9. The final repository status contains only intentionally untracked handoff
   inputs/artifacts and no staged changes.

## Completion result

The user receives a clickable absolute path to
`Agent-02-Workstream-02-Continuation-Context.zip`, its size and SHA-256, the
archive verification result, the protocol-change commit, and a concise reminder
that the next agent remains in Workstream 02 at BLD-001.
