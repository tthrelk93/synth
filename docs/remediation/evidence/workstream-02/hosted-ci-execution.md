# Workstream 02 Hosted CI Execution

Captured: 2026-07-19 18:51 PDT.

## Publication

The product owner explicitly authorized remote publication and any other
previously blocked in-scope actions. Agent 02 pushed
`codex/workstream-02-build` to `origin` at
`0bb28fe7ca1824f9d812c9e37fe7d05ec2bb901b` and confirmed the remote ref
resolved to that exact commit. The authenticated repository Actions policy was
enabled with all actions allowed.

Agent 02 also opened draft pull request
[#1](https://github.com/tthrelk93/synth/pull/1) from the published branch to
`main`. It remains a draft; no merge was attempted.

## Initial hosted triggers

Four hosted triggers were accepted by GitHub, but each ended in
`startup_failure` before GitHub created any matrix jobs:

| Event | Run | Head commit | Result |
|---|---|---|---|
| push | [29709977979](https://github.com/tthrelk93/synth/actions/runs/29709977979) | `0bb28fe7ca1824f9d812c9e37fe7d05ec2bb901b` | `startup_failure`; no jobs executed |
| pull request | [29710001882](https://github.com/tthrelk93/synth/actions/runs/29710001882) | `0bb28fe7ca1824f9d812c9e37fe7d05ec2bb901b` | `startup_failure`; no jobs executed |
| push | [29711364250](https://github.com/tthrelk93/synth/actions/runs/29711364250) | `0e1a69505ba0fe3df9aadb42dc6bfad458274299` | `startup_failure`; no jobs executed |
| pull request | [29711378051](https://github.com/tthrelk93/synth/actions/runs/29711378051) | `0e1a69505ba0fe3df9aadb42dc6bfad458274299` | `startup_failure`; no jobs executed |

At the same time, GitHub Status reported a critical
[Incident with GitHub Actions](https://stspg.io/w8d77c7t94zf), with the Actions
component in `partial_outage` and the incident under investigation. GitHub did
not allow either startup-failed run to be rerun. These records are hosted
platform availability evidence only: they are neither a project test failure
nor satisfying evidence for any supported-CI acceptance criterion.

## Status effect and next action

No BLD status is promoted from these triggers. The branch-publication
blocker is resolved, but the eight Linux, Windows, macOS arm64, and macOS
x86_64 Debug/Release jobs still need to start and finish. Push this evidence
commit to create a fresh trigger, monitor GitHub Actions recovery, then retain
every job result and `model-d-validation-*` artifact before reconciling the
hosted-dependent requirements.

The evidence commit was pushed as
`0e1a69505ba0fe3df9aadb42dc6bfad458274299`. Its push and pull-request
synchronize records appeared only after delayed API visibility; both had the
same jobless `startup_failure` result. Through 2026-07-20 01:50 UTC, GitHub
still reported Actions `partial_outage`. Four trigger records exist, but none
contains a job or artifact to retain.
