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
observed failing for the former placeholders. Exact strict build and CTest
results are appended after the final post-change baseline.

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
