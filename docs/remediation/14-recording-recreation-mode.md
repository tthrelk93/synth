# Workstream 14 — Recording Recreation Mode

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Presets](13-versioned-presets-official-library.md) · Next: [Photo import](15-photographed-patch-sheet-import.md)

## Goal and user-visible outcome

Users can start from an immutable, sourced Authentic Panel preset and understand what else was required to approach a recording: reference unit, calibration/drift, performance, feedback, pedals, amplification, tape, microphones, EQ, compression, mixing, and multitracking. Claims are evidence-qualified and comparisons describe perceptual similarity, never certainty from panel positions alone.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| REC-001 | Store recreation as a separate layer linked by content hash to an authentic preset; never merge metadata/processing into panel values. |
| REC-002 | Store recording/source identity, citations, confidence, reference instrument/version/unit, calibration/drift, and performance instructions. |
| REC-003 | Store feedback setup, ordered pedals/effects, amplifier/cabinet/microphone, tape, EQ/compression, mix, and multitracking notes with known/unknown status. |
| REC-004 | Require a URL/publication per historical claim and confidence labels; reject definitive wording for speculative data. |
| REC-005 | Enable/bypass recreation processing/instructions without changing the authentic panel snapshot byte-for-byte. |
| REC-006 | Define supported/unsupported processor declarations and exact external-routing/export behavior rather than silently approximating unimplemented gear. |
| REC-007 | Produce level/time-aligned comparison renders and quantitative reports against licensed/user-supplied references. |
| REC-008 | Use a blinded listening protocol and success language based on perceptual similarity, with numerical gates remaining independent. |

## Current-code evidence

There is no recreation schema, reference-recording model, source/confidence field, calibration/performance/feedback/effect/recording-chain metadata, comparison renderer, or listening protocol. `PresetManager` stores only raw APVTS XML, so authentic settings and any future processing would be indistinguishable.

## Prerequisites, ownership, and merge conflicts

Requires functional signal path/routing (09), Authentic Panel/utility UI (10), comparison/reference system (12), and preset schema/content hashes (13). It must not begin publishing famous-sound content before those gates pass.

Owns recreation document/schema/UI, claim/source/confidence validation, linked authentic hash, processing-declaration registry, export/comparison workflow, and recreation listening reports. It does not alter Authentic parameters, license reference recordings, or implement arbitrary third-party hardware without a separate measured processor plan.

Conflict hotspots: preset browser/details, state recreation link/enable flag, output routing/export, and offline renderer. Keep recreation documents separate from preset documents and use content hashes rather than mutable names.

## In scope

Recreation schema/store, sources/confidence, instrument/performance/chain metadata, explicit supported/external processing stages, non-destructive enable/bypass, dry/stem export, reference alignment/metrics, UI disclosure, and listening protocol.

## Out of scope

Claiming exact record identity, distributing copyrighted recordings, guessing missing settings, treating official patch sheets as song evidence, or implementing all named vintage effects/amps/tape in this workstream.

## Recreation schema and public types

```json
{
  "documentType": "modelDRecreation",
  "schemaVersion": 1,
  "recreationId": "uuid",
  "title": "...",
  "recording": { "artist": "...", "work": "...", "release": "...", "track": "...", "date": "...", "version": "..." },
  "authenticPreset": { "presetId": "...", "contentHash": "sha256:..." },
  "claims": [],
  "referenceInstrument": { "modelVersion": "...", "unitProfileId": "...", "calibrationProfileId": "...", "drift": [] },
  "performance": { "notes": [], "range": "...", "articulation": "...", "priority": "...", "trigger": "...", "velocity": "...", "overdubCount": null },
  "feedbackRouting": {},
  "processingChain": [],
  "mixNotes": [],
  "comparison": {},
  "confidence": "confirmed|strong|plausible|speculative"
}
```

Every claim is atomic: statement summary, subject field(s), source citation, quoted evidence omitted or kept within licensing limits, source type, access/publication date, confidence, rationale, contradictions, and reviewer. Unknown is a first-class value; absence never means “none.” Overall confidence is no higher than the weakest material claim.

`ProcessingStage` fields: order, category (`feedback`, `pedal`, `eq`, `compressor`, `tape`, `amplifier`, `cabinet`, `microphone`, `mix`, `multitrack`, `other`), manufacturer/model/version if known, settings with units/source/confidence, routing, wet/dry, executable processor ID/version or `externalOnly`, and bypass state.

## Proposed architecture and data flow

```text
immutable AuthenticPreset(contentHash)
  -> Model D engine dry render
  -> RecreationProcessingPlan
       executable registered stage -> separately versioned/measured processor
       externalOnly stage -> documented stem/export/return requirement
  -> recreation output + dry/stems + comparison report
```

This workstream does **not** substitute generic effects for named equipment. A stage is executable only when `RecreationProcessorRegistry` contains the exact processor ID/version and that processor has its own reference/real-time/latency tests. Otherwise it is `externalOnly`; UI lists connection/order/settings/instructions, export produces the required dry/stem file, and enabling Recreation never pretends the stage was heard.

The initial executable set consists only of the authentic internal feedback/output routing already delivered by Workstream 09 and measurement/level/time-alignment utilities from Workstream 12. Pedal/tape/amp/cab/mic/EQ/compressor stages default `externalOnly` until separately implemented and measured. This boundary is deliberate and user-visible.

## Non-destructive enable/bypass contract

`authenticPreset.contentHash` is verified before load. Recreation state holds an immutable copy/reference; UI panel edits create a new authentic preset revision and mark comparison stale rather than mutating the referenced snapshot. Toggling Recreation enabled/bypassed changes only the downstream processing plan/instruction context. A test serializes the authentic panel before/after repeated toggles and requires byte identity.

Unsupported/missing stages do not partially render without disclosure. User chooses: authentic-only audition, supported-stage partial audition visibly labeled, or external export. Default for a published recreation with a missing material stage is authentic-only plus instructions.

## Comparison renders and success language

Reference audio must be user-supplied/licensed with source/segment rights and hash. Store segment timestamps/version/channel, extraction method, sample rate, and known mastering/mix context. Renderer exports dry authentic, supported recreation, stage stems, and a manifest. Alignment estimates time offset, tuning offset, and loudness; it records transformations and keeps unmodified files.

Report pitch/harmonic envelope, temporal envelope, spectral distance, dynamics, and other approved Workstream 12 metrics with uncertainty. Never optimize directly against a final mastered mix without recording that limitation. Allowed wording: “close under the documented comparison,” “perceptually similar for the tested phrase/listeners,” or “unverified/speculative.” Prohibited: “exact,” “the definitive settings,” or “identical” without impossible-to-meet evidence.

## Backward compatibility and migration

Recreation documents are separate from preset schema v2. Host state stores only enabled flag and linked recreation ID/hash; missing document loads Authentic state and warns. Recreation schema future-major rejection is atomic. Updating sources/settings creates a new document revision/content hash and preserves history. Official factory patches remain general presets unless a separate recreation document supplies reliable recording evidence.

## Real-time audio constraints

Document/source parsing, citation validation, alignment, metric analysis, and export run off audio thread/offline. Executable stages must independently satisfy the roadmap RT contract, declare latency/tail/reset, preallocate, and be prepared before activation. Switching plans is committed at a safe boundary with stage-specific smoothing; no network/source lookup occurs during audio.

## Edge cases and failure modes

Missing/stale authentic hash, missing source URL, contradictory sources, confidence downgrade, unknown chain item, unavailable processor/plugin, copyrighted reference absent, wrong recording/master/version, multitrack unknown, external round-trip sample-rate mismatch, stale comparison after edit, and partially supported chain. UI never collapses unknown into zero/off and never labels partial render as full recreation.

## Implementation sequence

1. Define schema/claim/processor registry and validation rules with content/source reviewers.
2. Link immutable authentic presets and implement revision/stale-comparison behavior.
3. Build metadata/editor UI with explicit Unknown, confidence, contradictions, and unsupported-stage disclosures.
4. Implement internal-feedback executable stage plus dry/stem/external export workflow.
5. Integrate reference alignment/metrics and report generation.
6. Run a pilot recreation with licensed/user-supplied audio; conduct blinded listening and language review.

## Automated tests and measurable gates

- Schema rejects a recording claim without citation/confidence, invalid enum/order, missing authentic hash, definitive wording on speculative claims, and silent executable substitution.
- Repeated Recreation enable/bypass/load/save leaves canonical authentic panel bytes/content hash unchanged.
- Editing panel creates a new authentic revision and invalidates/stales prior comparison deterministically.
- Processor registry executes only exact available versions; missing stage produces declared authentic-only/partial/export outcomes and conspicuous status.
- Export/stem manifests and hashes reproduce; latency/time/tuning/loudness alignment metrics pass Workstream 12 synthetic tests.
- General official factory preset cannot acquire recording title/artist without a qualifying recreation document/source.
- Listening report remains supplemental and cannot change a failing numerical/metadata status to pass.

## Manual, listening, and source validation

Content reviewers trace each material field to a source, check recording/master/version, contradictions, confidence, and wording. Musicians follow performance/routing/external-chain instructions from a clean setup. Listening is randomized, blinded, level-matched, phrase-specific, and reports participants/monitoring/repeats/statistics; dry, partial, and full-external versions are never mislabeled.

## Definition of done

- [ ] REC-001 through REC-008 pass.
- [ ] Authentic panel is immutable under Recreation operations.
- [ ] Every material claim has source/confidence or explicit Unknown.
- [ ] Unsupported processing is disclosed/exportable, never silently approximated.
- [ ] Comparison and listening reports use approved similarity language.

## Completion-report evidence

Include schemas/examples; authentic hash invariance and revision tests; source/claim/confidence validation report; processor-registry/unsupported-stage matrix; dry/stem/export manifests; alignment/metric report; pilot source audit; UI disclosures/screenshots; blinded listening protocol/results; and wording review.

## Primary technical references

- Moog [Model D manual](../../Minimoog_Model_D_Manual.pdf), signal flow and patch sheets, pp. 52–79.
- ITU-R [BS.1116](https://www.itu.int/rec/R-REC-BS.1116) and [BS.1534](https://www.itu.int/rec/R-REC-BS.1534).
- IETF [RFC 8785 JSON canonicalization](https://www.rfc-editor.org/rfc/rfc8785).
