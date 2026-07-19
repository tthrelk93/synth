# Workstream 02 Dependency and License Review

Captured: 2026-07-19 on `codex/workstream-02-build`.

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

BLD-003 remains `in-progress` until supported hosted configurations reproduce
the dependency result. This report closes the previously missing local licence
review record; it does not grant or certify distribution rights.
