# Workstream 15 — Photographed Patch-Sheet Import (Gated Future Feature)

[Roadmap](00-master-remediation-roadmap.md) · [Traceability](01-traceability-matrix.md) · Previous: [Recording recreation](14-recording-recreation-mode.md)

## Goal and user-visible outcome

After the foundational product is stable, a user may import a photograph/scan of a supported Model D patch sheet, review per-control estimates and confidence, correct every value, and explicitly commit a versioned preset. The feature never silently applies uncertain settings or discards the source/provenance.

## Requirements and original deficits covered

| ID | Required result |
|---|---|
| IMG-001 | Enforce a go/no-go gate: Authentic geometry v1, preset schema v2, provenance, validation dataset, and rights approval must be stable first. |
| IMG-002 | Detect supported sheet edition/geometry and correct rotation, crop, perspective, and scale before estimating controls. |
| IMG-003 | Estimate every supported knob/switch/control with calibrated per-control confidence and explicit Unknown/Not Visible states. |
| IMG-004 | Present a review overlay and require manual confirmation/correction before applying any value. |
| IMG-005 | Preserve source image hash/asset, edition, transforms, model/version, tool/model versions, estimates, corrections, citations, rights, and confidence through preset v2. |
| IMG-006 | Handle lighting, blur, skew, perspective, shadows, handwriting, partial occlusion/crop, different editions, and unsupported layouts safely. |
| IMG-007 | Approve model choice/training/validation data, privacy, security, and licensing before implementation/publishing. |
| IMG-008 | Measure detection/calibration performance on a held-out approved dataset and block release on missing/biased/unproven gates. |

## Current-code evidence

No image-import code, panel geometry export, image/provenance preset fields, confidence UI, supported-edition registry, dataset, model, license review, or validation harness exists. Workstream 10 and 13 deliberately provide the required geometry/schema foundations; this absence is expected until the gate opens.

## Gate, prerequisites, ownership, and merge conflicts

Implementation must not start until a signed gate record confirms:

- Workstream 10 `authentic-panel-geometry.json` v1 is frozen and maps every semantic control once.
- Workstream 13 preset schema v2/source-asset/confidence/correction fields are released and backward-compatible.
- Workstream 14 claim/provenance wording is available for any recording association.
- Supported sheet editions have legally usable training/validation images and layout specifications.
- Privacy/security review approves local/cloud processing and image retention behavior.
- A held-out validation set, metric definitions, and acceptance thresholds are approved before model evaluation.

This workstream owns edition registry, image preprocessing, control estimation, confidence calibration, review UI, dataset/model governance, and import evidence. It consumes but never forks panel geometry/preset schema.

Conflict hotspots: preset import UI/assets, geometry exporter, and source-citation controls. Coordinate changes require a new geometry version and retraining/validation decision, not silent reinterpretation.

## In scope

Local supported-edition recognition, geometric rectification, knob/switch/mark estimation, calibrated confidence, review/correction, provenance/asset retention, validation metrics, and safe failure.

## Out of scope

Arbitrary synthesizer photos, inferring hidden/occluded controls, identifying famous songs from images, applying settings without confirmation, scraping/licensing images implicitly, or training on private user images without explicit consent.

## Chosen architecture and data flow

Use a deterministic classical-vision-first pipeline for known sheet editions; add learned components only if the gate’s validation comparison proves they are necessary and rights-approved.

```text
user image (decode limits + metadata strip policy)
 -> edition classifier / fiducial-template match
 -> orientation + document quadrilateral + homography
 -> rectified sheet in edition coordinate system
 -> control ROI extraction from Authentic geometry/edition mapping
 -> per-control estimator (knob line/mark, rocker state, handwritten mark)
 -> confidence calibrator + Unknown/NotVisible
 -> immutable ImportDraft
 -> review overlay/manual corrections
 -> explicit Commit -> PresetBuilder v2
```

OpenCV homography/perspective operations and auditable feature/template methods are the baseline. A learned classifier/regressor must be versioned, run locally by default, include a model card/data sheet, and pass the same held-out gates. Cloud inference is a separate product/privacy decision and is off by default.

## Public interfaces and types

```cpp
enum class EstimateState { estimated, unknown, notVisible, unsupported };
struct ControlEstimate { SemanticControlId id; EstimateState state; PanelValue value; float calibratedConfidence; ImageRegion evidence; };
struct ImportDraft { AssetHash source; EditionId edition; Transform transform; ModelVersion model; std::vector<ControlEstimate> estimates; std::vector<Correction> corrections; };
```

`calibratedConfidence` is an empirical probability-like score only after calibration testing; UI also shows plain labels and the evidence crop. Every control begins “unconfirmed,” regardless of score. Commit is disabled until the user has confirmed or explicitly left Unknown for every required control. Unknown values do not overwrite the current panel.

## Provenance, assets, and privacy

On explicit consent, copy the source into the user’s content-addressed preset-asset store; otherwise store hash plus user-managed reference and warn it may become unavailable. Record original filename only if consented, image hash/dimensions/metadata policy, capture/import date, source/citation/rights, edition, transform, algorithm/model/calibrator versions, raw estimates/confidence/evidence regions, user corrections, and final reviewer identity (`user`). Strip location/device EXIF from shared exports by default while retaining a disclosed local provenance option.

Imported official sheets remain general presets unless separate reliable recording evidence is added through Workstream 14. Image recognition must not invent song/artist labels.

## Confidence and validation policy

Metrics are per edition and control type: edition accuracy/rejection, corner/reprojection error, knob angular/position error, switch-state confusion matrix, Unknown/NotVisible precision/recall, confidence calibration error, commit correction rate, and catastrophic silent-overwrite count (required exact zero by design).

Numeric acceptance values come from the preapproved product/validation manifest based on task risk and dataset measurement; implementers do not choose them after seeing test results. Dataset split is by physical sheet/capture session/source owner to prevent near-duplicate leakage. Report performance across lighting, device, angle, handwriting, occlusion, demographic/accessibility-relevant use where applicable, and every supported edition. Unsupported/out-of-distribution inputs must reject rather than produce confident guesses.

## Backward compatibility and preset behavior

Import writes only preset schema v2 through `PresetBuilder`. Import model/geometry/edition versions are immutable provenance. Re-running a newer model creates a new draft/revision and preserves old estimates/corrections. Missing source assets do not invalidate final confirmed panel values but reduce provenance status visibly. Future edition/model formats do not partially apply.

## Real-time constraints

All decoding, rectification, inference, asset I/O, and review run off audio thread and outside DSP. Committing a reviewed preset uses Workstream 13’s safe apply path. Cancel/import failure changes neither live parameters nor saved state.

## Edge cases and failure modes

Rotated/mirrored/upside-down images; severe perspective; crop missing fiducials/controls; glare/shadow/color cast/blur; high-resolution decompression bomb; corrupt/hostile image; handwriting styles; erasures; multiple marks; partial occlusion; unsupported/vintage/modified sheet; mixed editions; printer scaling; privacy-sensitive EXIF; unavailable model; low memory. Decode is size/time bounded and sandboxed where practical. Any ambiguity becomes Unknown/unconfirmed; no partial live apply.

## Implementation sequence

1. Obtain signed gate and freeze edition/geometry/schema/dataset/metrics.
2. Implement bounded secure decode, edition registry, fiducial detection, homography, and synthetic geometry tests.
3. Implement control estimators and Unknown/NotVisible logic using approved data.
4. Calibrate confidence on calibration split; evaluate once on held-out set.
5. Build evidence-overlay review/correction/commit UI and preset provenance integration.
6. Run robustness/privacy/security/accessibility/user correction studies; approve or keep feature gated.

## Automated tests and measurable gates

- Synthetic rotation/scale/perspective transforms recover geometry and control positions within preapproved manifest bounds.
- Held-out edition/control/confidence/Unknown metrics pass all predeclared gates with no post-hoc threshold widening.
- Unsupported/out-of-distribution/corrupt/oversized/partial images reject safely and leave live panel/state byte-for-byte unchanged.
- Every successful draft contains source hash, edition, transform, versions, per-control state/confidence/evidence, and correction history.
- Commit remains impossible until every required control is confirmed or explicitly Unknown; Unknown never overwrites a value.
- Save/load/export preserves final values and import provenance under preset v2; photo import never creates a recording association by itself.
- Security/privacy tests enforce decode limits, path isolation, metadata policy, local-default processing, and no unintended upload.

## Manual and user validation

Test real phone photos/scans across approved editions, devices, angles, lighting, folds/shadows, handwriting, occlusion, crop, accessibility/zoom/keyboard navigation, and correction workflow. Users must understand confidence/Unknown, find evidence crops, correct quickly, and predict that nothing applies before Commit. Record correction burden separately from model accuracy.

## Definition of done

- [ ] IMG-001 through IMG-008 pass.
- [ ] Signed gate, rights/data/privacy/security approvals exist.
- [ ] Held-out metrics pass predeclared thresholds.
- [ ] Zero silent/uncertain application is enforced structurally.
- [ ] Source/provenance/corrections round-trip through preset v2.

## Completion-report evidence

Include signed go/no-go; edition/geometry registry; dataset sheet/model card/license ledger/split hashes; algorithm/model comparison; held-out metrics/confusion/calibration/robustness reports; security/privacy tests; ImportDraft/preset round trips; UI screenshots/usability results; failure/no-state-change hashes; and examples showing Unknown/correction/provenance.

## Primary technical references

- OpenCV [geometric image transformations](https://docs.opencv.org/4.x/da/d54/group__imgproc__transform.html) and [homography tutorial](https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html).
- NIST, [AI Risk Management Framework](https://www.nist.gov/itl/ai-risk-management-framework).
- Timnit Gebru et al., [Datasheets for Datasets](https://doi.org/10.1145/3458723).
- Margaret Mitchell et al., [Model Cards for Model Reporting](https://doi.org/10.1145/3287560.3287596).
