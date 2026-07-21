# Workstream 02 Dependency and License Review

Captured: 2026-07-19; product-owner decision approved 2026-07-21 on
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

### Product-owner decision

On 2026-07-21, the product owner selected and approved the **commercial JUCE 8
Starter** path rather than AGPLv3. The approved entitlement basis is the
owner's attestation that:

- the owner is an individual product owner;
- JUCE-related revenue or funding during the previous 12 months is below the
  Starter threshold of USD 20,000;
- the project may use the no-fee Starter tier while that basis remains true;
- the owner will upgrade to the appropriate JUCE tier if the threshold is
  exceeded; and
- the applicable JUCE and linked-dependency notices may be retained and
  distributed with the product.

This records the owner's licensing-path decision without storing account
credentials or claiming that the repository itself grants a JUCE licence.
Eligibility must be rechecked before a distribution and whenever the owner's
revenue/funding basis changes.

The controlling references reviewed for this record are the official
[JUCE 8 EULA](https://juce.com/legal/juce-8-licence/) and the exact pinned
[JUCE dependency/licence inventory](https://raw.githubusercontent.com/juce-framework/JUCE/3af3ce009f6a02f6fa651008fffb5b41743a9fab/LICENSE.md),
both accessed 2026-07-21.

### Required notice handling

For every distributed platform artifact, preserve applicable JUCE copyright,
trademark, and other proprietary notices, and include the notices or licence
texts required for third-party code actually linked or redistributed in that
artifact. Determine that platform-specific set from the pinned JUCE
`LICENSE.md` inventory and the final linked payload; do not represent SDKs that
are disabled or validation-only tools as shipped dependencies. Keep the notice
bundle with the corresponding release evidence.

This decision closes the BLD-003 dependency/licensing review. It does not
approve a legal product identity, establish distribution history, or override
the independent BLD-007, BLD-011, and BLD-012 release gates.

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
`29728203657`. With the product-owner decision and notice policy recorded above,
BLD-003 is `pass`. This report records the dependency review and approved path;
it does not independently grant or certify distribution rights.
