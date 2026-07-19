# Workstream 13 — Versioned Presets and Official Patch Library

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Reference system](12-hardware-reference-regression-system.md) · Next: [Recording recreation](14-recording-recreation-mode.md)

## Goal and user-visible outcome

Users can save, exchange, recover, migrate, and understand presets without losing panel meaning or provenance. A bundled factory library is transcribed only from legally usable official Model D patch sheets and is clearly separated from recording recreations.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| PRE-001 | Define deterministic preset schema v2 with stable authentic panel semantics and migration history. |
| PRE-002 | Detect and migrate legacy unversioned APVTS XML through Workstream 03 without silent contour/automation reinterpretation. |
| PRE-003 | Store provenance, instrument/version, calibration profile, photographed positions, confidence, performance notes, feedback routing, and external-processing declarations. |
| PRE-004 | Build factory content only from licensed/redistributable official patch-sheet sources with URL/publication/page and access/version data. |
| PRE-005 | Label official sheets as general factory presets unless reliable independent evidence ties one to a recording. |
| PRE-006 | Define stable preset IDs/names, duplicate handling, factory/user separation, search/tags, and immutable factory records. |
| PRE-007 | Save atomically, validate/read back, recover from corrupt/interrupted writes, and never overwrite silently. |
| PRE-008 | Pass deterministic schema/state/panel round trips, migration fixtures, content hashes, and cross-platform path tests. |
| PRE-009 | Gate factory publication on corrected parameter/DSP/UI/reference contracts and license/source review. |
| PRE-010 | Require source URLs/publications and confidence labels for any historical/recording claim; never present speculation as definitive. |

## Current-code evidence

- `PresetManager` writes `apvts.copyState().createXml()` directly to a user file and replaces APVTS state directly when loading. There is no schema/version/migration/provenance or atomic temporary/read-back/rename protocol.
- `getPresetNames` scans XML and de-duplicates display names, which can make distinct files indistinguishable. Legal filename conversion can also collide.
- `last_preset.txt` is replaced directly; corrupt-file/backup recovery and concurrent-instance behavior are undefined.
- `SynthPreset` contains a name and an inaccessible/empty float map; `PresetLibrary` declares three default objects but publishes no usable content.
- No official patch sheet source/license ledger or distinction between factory patches and song recreations exists.

## Prerequisites, ownership, and merge conflicts

Requires state/parameter v2 (03), Authentic Panel geometry/bindings (10), approved regression/reference profiles (12), and all DSP publication gates (04–09). Factory publication waits for F4 even if schema work begins earlier.

Owns preset document/schema/storage/indexing/migration UI, factory content ingestion, license/source ledger, duplicate/corrupt recovery, and round-trip tests. Workstream 14 owns recreation documents linked to presets; 15 owns photo inference but writes only through this schema.

Conflict hotspots: `PresetManager`, `SynthPreset`, `PresetLibrary`, editor preset controls, state migrator calls, and resources/build packaging. Preserve a single migration implementation from Workstream 03.

## In scope

Preset JSON schema v2, legacy XML import, user/factory stores, content-addressed assets/references, atomic storage/recovery/index, official sheet transcription/review, provenance/confidence, and publication gate.

## Out of scope

Claiming famous-song associations from official sheets alone, distributing copyrighted sheets/images without permission, recording-chain DSP, cloud sync/marketplace, or photo recognition.

## Preset schema and public types

Canonical JSON (UTF-8, normalized key ordering for hashing):

```json
{
  "documentType": "modelDPreset",
  "schemaVersion": 2,
  "presetId": "uuid",
  "name": "...",
  "kind": "user|factory|imported",
  "authenticPanel": {
    "parameterContract": "canonicalContours|legacyCrossedContours",
    "values": { "stableParameterId": 0.0 },
    "panelPositions": { "semanticControlId": { "position": 0.0, "source": "entered|transcribed|photo", "confidence": null } }
  },
  "instrument": { "model": "Minimoog Model D", "version": "...", "calibrationProfileId": "..." },
  "routing": { "externalSource": "autoDetect|auxiliaryBus|internalFeedback", "notes": "..." },
  "performanceNotes": [],
  "externalProcessing": [],
  "provenance": { "sources": [], "confidence": "confirmed|strong|plausible|speculative" },
  "assets": [],
  "migrationHistory": [],
  "extensions": {}
}
```

Normalized parameter values are stored by stable host ID for exact restoration; semantic panel positions are stored separately so source transcription/photo estimates are not confused with DSP-calibrated physical values. Switches/selectors store canonical enum tokens in panel positions in addition to normalized values. Save validation proves the two representations agree under the named parameter/calibration contract.

`SourceCitation`: source type, title/author/publisher, URL or publication identifier, page/figure/patch name, publication/version date if known, access date, archived URL/hash where permitted, quote-free claim summary, rights/license status, and reviewer. Confidence applies per claim/control where needed; document confidence is the conservative aggregate.

## Proposed architecture and data flow

```text
UI/state -> PresetBuilder -> schema validation -> canonical serialization/hash
  -> temp file in destination directory -> flush/fsync -> read-back parse/hash
  -> atomic rename/replace -> index transaction -> optional prior-version backup

load -> detect JSON v2 / legacy XML -> parse temporary -> migrate/validate
  -> resolve calibration/assets -> preview warnings -> atomic apply to live state
```

User presets live in a versioned application-data directory; factory presets are read-only signed/hashed resources. Index is rebuildable from documents and never the sole source of truth. Filename is the immutable preset ID plus extension; display name does not determine identity. Same content hash prompts alias/replace/cancel, default cancel. Same name with different IDs is allowed and disambiguated by source/kind.

Atomic save uses a same-directory unique temp file, flush plus platform durability call, read-back schema/hash validation, atomic rename, and index update. On failure, original remains untouched and temp is quarantined/reported. Recovery scans valid main/backup/temp candidates, selects only after hash/schema/time policy, and asks before replacing user data.

## Official factory library policy

The bundled manual pp. 54–79 contains official patch sheets. Before transcription/distribution, record written license/terms confirming redistribution of derived parameter data and any names/images. If permission is absent, the factory-library publication gate remains closed; local user transcription may still be supported without bundling source images.

Each factory preset has two-person transcription/review against the exact edition/page, panel geometry version, source image/page hash, ambiguity list, and confidence per uncertain control. “Official Model D patch” is allowed; a song/artist claim requires a separate reliable citation and moves to Workstream 14. Factory data is generated into resources from reviewed source documents, never hand-edited in binary code.

## Backward compatibility and migration

Legacy XML detection is structural, not filename-based. It passes through Workstream 03’s v0 migrator, keeps `legacyCrossedContours`, fills new defaults with a migration log, and previews warnings before apply. Saving after import writes v2 and retains source XML hash plus optional backup; it never overwrites the legacy file by default.

Unknown future major schema is rejected with no state mutation. Safe unknown extensions round-trip. Missing calibration loads baseline with a visible warning but preserves requested profile ID. Legacy parameter IDs remain stable.

## Real-time audio constraints

All filesystem, JSON/XML, hashing, asset resolution, indexing, dialogs, and migration occur off audio thread. Apply a validated preset by preparing an immutable state change then committing at a safe processor boundary through Workstream 03; DSP smoothing/transition policies suppress unsafe discontinuities. No preset autosave occurs in callback.

## Edge cases and failure modes

Illegal/duplicate names, Unicode normalization, path length, concurrent instances, read-only/full disk, interrupted save, stale temp, corrupt JSON/XML, hash mismatch, missing asset/profile, future schema, symlink/path traversal, malicious extension size, factory/user ID collision, source URL rot, and license revocation. User files never execute code or address arbitrary paths; assets are content-addressed and size/type limited.

## Implementation sequence

1. Inventory/capture legacy XML fixtures and user-directory behavior.
2. Define schema/types/validator/canonical serializer and state adapters.
3. Implement atomic user store, recovery, rebuildable index, duplicates, and UI preview/errors.
4. Implement legacy XML import/migration and round-trip suite.
5. Create source/license ledger and two-person official-sheet transcription pipeline.
6. Run DSP/panel/reference gates; publish factory resources only after license/content approval.

## Automated tests and measurable gates

- Schema accepts every valid field/enum and rejects missing required, duplicate ID, mismatched panel/parameter, unsafe path/asset, invalid confidence/source, and future-major documents without live-state mutation.
- Canonical v2 save→load→save is byte-identical; state→preset→state matches all declared values/compatibility/routing/profile fields.
- All legacy XML fixtures migrate deterministically with expected warning/default log and preserve legacy contour routing/render trace.
- Fault injection at each atomic-save step leaves either the complete old file or complete new file, never a corrupt replacement; recovery is deterministic and user-safe.
- Duplicate name/content/ID, Unicode/path, concurrent reader/writer, missing profile/asset, corrupt index, and factory immutability tests pass.
- Every factory entry validates sources, rights, page/edition, transcription reviewers, confidence, parameter contract, calibration, and content hash; no recording claim lacks separate evidence.
- Factory preset render/round-trip regression passes the approved DSP/reference suite.

## Manual, host, and content validation

Create/rename/duplicate/save/load/delete user presets; crash/fail saves; recover backups; move between platforms; load in multiple hosts; inspect generic automation/state; review every factory patch side-by-side with licensed source. Confirm factory/user visual separation and that speculative/ambiguous fields are visible.

## Definition of done

- [ ] PRE-001 through PRE-010 pass.
- [ ] v2 schema/storage/recovery and legacy migration are deterministic.
- [ ] Factory library is license-approved, two-person reviewed, and DSP-gated.
- [ ] General patch sheets are not mislabeled as famous recording settings.
- [ ] Every claim/control uncertainty has source and confidence.

## Completion-report evidence

Include schema and examples; legacy migration table/fixtures/hashes; atomic-save fault matrix and recovery captures; index/duplicate/path/security tests; factory source/license ledger; transcription/reviewer diffs; factory inventory/render report; parameter/panel round-trip hashes; and screenshots of warnings/separation/confidence.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), pp. 54–79 (official patch sheets) and 80–81.
- IETF [RFC 8259 — JSON](https://www.rfc-editor.org/rfc/rfc8259) and [RFC 8785 — JSON Canonicalization Scheme](https://www.rfc-editor.org/rfc/rfc8785).
- JUCE [`File`](https://docs.juce.com/master/classFile.html), [`ValueTree`](https://docs.juce.com/master/classValueTree.html), and [`XmlDocument`](https://docs.juce.com/master/classXmlDocument.html).
