# Workstream 02 Dependency and License Review

Captured: 2026-07-19; owner-decision gate refreshed 2026-07-21 on
`codex/workstream-02-build`.

## Resolved dependency

The supported CMake build has one fetched source dependency: JUCE 8.0.10 at
commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`. Both fetched and
`SYNTH_JUCE_SOURCE_DIR` builds verify the checkout root and exact Git revision;
a mismatched revision fails before JUCE is added.

The inspected checkout reported:

```text
3af3ce009f6a02f6fa651008fffb5b41743a9fab
2025-09-15
JUCE version 8.0.10
```

The supported project does not consume `MiniMoog.jucer`, its historical
machine-specific module path, or tracked `JuceLibraryCode` amalgamations.

## License inventory and decision

The pinned JUCE `LICENSE.md` says JUCE modules are dual-licensed under AGPLv3
and the commercial JUCE licence. It also inventories bundled dependencies,
including the AudioUnit SDK (Apache 2.0), VST3 SDK (Steinberg proprietary or
GPLv3 terms), image/font/codec libraries under permissive licences, and other
SDKs that are not enabled by this project.

This record does not select a licence on the product owner's behalf. Before any
binary distribution, the owner must either document AGPLv3 compliance for the
whole combined work or confirm an applicable commercial JUCE licence, and must
review the notices/terms for the JUCE modules and embedded SDK code actually
linked into each platform artifact. The project currently has no distribution
approval and `SYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON` fails independently on
the placeholder identity.

Agent 04's [continuation preflight](agent-04-continuation-preflight.md) found no
licensing selection or approved notice set in the supplied package, live
repository, remote branch, or draft-PR context. The required owner record must
select `AGPLv3` or `commercial JUCE`, document the applicable compliance or
entitlement basis without exposing credentials, and approve the notices for
the linked JUCE/SDK code in each distributed platform artifact. The decision
remains not supplied.

pluginval 1.0.4 is a validation-only executable. It is provisioned outside the
staged product, cryptographically recorded, and is not packaged into MiniMoog.
CMake, compilers, GitHub Actions, Xvfb, and system libraries are build/runtime
tools rather than project payloads in the development stage.

## Verification

- The local JUCE checkout `LICENSE.md` and its linked dependency inventory were
  inspected at the exact required commit.
- `cmake/Dependencies.cmake` records and rejects revision mismatch for both the
  fetched and local-override paths.
- The build manifest records equal required/resolved JUCE commit fields.
- The development stage contains only declared project products, manifests,
  and hashed reports; pluginval is not a staged product.

Supported hosted configurations reproduce the exact dependency result in run
`29728203657`. BLD-003 remains `in-progress` solely because the reviewed owner
licensing decision and required notice set are absent. This report records the
local licence inventory; it does not grant or certify distribution rights.
