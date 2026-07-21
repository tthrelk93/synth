# Product Identity and Temporary External-Gate Deferral Design

Date: 2026-07-21
Owner approval: approved in the Agent 06 continuation conversation on 2026-07-21

## Purpose

Replace the unapproved development identity before parameter/state work freezes
host-facing contracts, record that the product has never been distributed, and
allow Workstream 03 to begin while unavailable manual host checks remain honest
pre-release gates.

This decision does not manufacture test evidence, approve a public release, or
authorize an agent to accept third-party legal terms or purchase software.

## Approved product identity

| Field | Approved value |
|---|---|
| Manufacturer/company name | `TTH Audio` |
| Product name | `TTH Model One` |
| Manufacturer code | `TTHA` |
| Product code | `TM01` |
| Reverse-DNS manufacturer domain | `io.github.tthrelk93` |
| Bundle identifier | `io.github.tthrelk93.TTHModelOne` |
| Manufacturer website | `https://github.com/tthrelk93` |

The reverse-DNS namespace is based on the repository's existing
`github.com/tthrelk93` ownership. The four-character codes are distinct,
printable ASCII identifiers. Because there was no prior distribution, no
shipped bundle, AU subtype/manufacturer pair, preset library, or DAW project
identity must be preserved.

The public product name changes from the development name `MiniMoog` to
`TTH Model One`. Internal source target names such as `ModelDCore` and stable
legacy parameter IDs do not need cosmetic renaming. This keeps the product
rename bounded and avoids unrelated source churn before Workstream 03.

This is an owner-approved technical product identity, not a trademark
registration or a claim that `TTH Audio` is an incorporated entity.

## Requirement status policy

- **BLD-007:** may move to `pass` only after the approved values are committed,
  distribution-identity validation accepts them, the old placeholders are
  absent from authoritative build metadata, and the no-prior-distribution
  decision is recorded in durable evidence.
- **BLD-006:** remains `in-progress`. Existing generated VST3
  `Instrument|Synth`, AU `aumu`, bus, actual-wrapper, pluginval, and ephemeral
  hosted-auval evidence remains valid. Required commercial-host category and
  menu-placement checks are explicitly deferred; they are not recorded as run
  or passed.
- **BLD-011:** changes from externally `blocked` to `in-progress/deferred` for
  roadmap scheduling. Automated evidence remains valid, while designated-user
  AU registration, the Steinberg SDK validator, and commercial-host smoke tests
  remain required before release.
- **BLD-012:** remains `in-progress` until the distribution aggregate includes
  the final external validation evidence. Development artifact manifests remain
  usable by later workstreams.

## Temporary phase-gate exception

The product owner authorizes Workstream 03 to begin before BLD-006, BLD-011,
and BLD-012 reach `pass`. This is a bounded scheduling exception, not a waiver
of their acceptance criteria.

The successor must:

1. Verify the frozen Workstream 02 build, wrapper, bus, and test seams before
   changing parameter/state contracts.
2. Begin Workstream 03 in roadmap order and preserve all existing passing build
   and validation checks.
3. Keep each deferred check visibly non-passing in the roadmap, traceability
   matrix, handoff, and generated reports.
4. Avoid release, public distribution, notarization, store submission, or a
   claim that Workstream 02 is fully complete until the deferred gates pass.
5. Accept owner-supplied external evidence later without reopening unrelated
   Workstream 03 design decisions.

## Dedicated validation account

The owner authorizes creation and use of a dedicated standard macOS validation
account. Account creation requires a local administrator authentication prompt,
which is unavailable while the owner is away. The owner will create the account
or enter the administrator credential locally; no password belongs in source,
logs, evidence, chat, or a continuation archive.

The missing account does not block Workstream 03 under the temporary exception.
Once available, it is used only to capture AU registration/`auval` and approved
host evidence for BLD-011.

## Implementation and verification

Implementation updates the authoritative CMake identity, the README, the four
synchronized remediation ledgers, and new owner-decision evidence. Generated
artifact names and validation commands must derive from the new product name and
four-character codes.

Verification must include:

- placeholder and old authoritative product-identity scans;
- distribution-identity configure validation;
- a strict supported build and complete CTest run;
- synchronized BLD statuses and local Markdown links;
- confirmation that Workstream 03 is permitted while release remains gated;
- a rebuilt, freshly extracted successor archive whose internal manifest,
  source equality, safe paths, scope, and kickoff instructions all pass.

## Successor scope

The replacement continuation archive hands off to Workstream 03 — Parameter,
Automation, and State Contract. Its first implementation action is to inventory
and freeze the current parameter IDs and capture legacy state fixtures before
changing parameter metadata or serialization. Deferred Workstream 02 checks are
carried as explicit pre-release obligations, not as its primary work queue.
