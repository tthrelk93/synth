# Ultimate-Review Authority Remediation Design

**Base:** `37226cf2c5f047058ac242ccdf09cc0839f085c4`

**Goal:** Close the ultimate-review event-local metric, build/source identity,
frozen-registry, and documentation synchronization findings without changing
production DSP, registry/state/contour definitions, approvals, or requirement
statuses.

## Constraints

- Work only in the Workstream 12 F0 remediation worktree.
- Make no changes under `Source/`.
- Preserve the global draft manifest, all open acceptance statuses, and all
  prior containment, typed-request, authoritative-replay, governance, compiler,
  and no-`.git` behavior.
- Drive every behavior change with a focused RED/GREEN contract.
- Keep tests-off configuration independent of Git and reference-harness source
  identity targets.
- Finish with a strict validators-on 21-test build, all seven labels, CLI and
  mutation checks, exact-head package verification, and an append-only report.

## Event-local control-step authority

`control.step.v1` retains absolute render-sample coordinates. First change,
settling, monotonicity, and overshoot continue to begin at the declared event.
Maximum per-sample movement also begins at the event boundary, with the event
sample compared to the immediately preceding sample so the transition itself
is included.

Dense request construction uses the already-derived effective pre-event start
as the value before the event. It ignores same-key trace points before the
requested origin and applies only origin and later points in trace order. This
prevents historical automation from being replayed onto a buffer whose initial
value is already the final pre-event state, preserves final same-sample target
selection, and keeps all reported sample values in the existing absolute v1
domain.

A regression trace contains at least two distinct prior same-key changes and
asserts exact first-change, settled-sample, monotonic, overshoot, and maximum
movement results for the requested transition.

## Build-generated source identity

The reference harness receives a generated header produced by an always-run,
test-only CMake target. On every build, the generator inspects the source Git
repository and writes the header only when its canonical values change:

- exact `HEAD` commit;
- exact `HEAD^{tree}` identity;
- SHA-256 content identity over the committed tree identity plus the tracked
  working-tree diff from `HEAD`;
- working-tree dirty flag; and
- exact Git executable used for inspection.

The common reference type header consumes the generated header, so a changed
identity invalidates the relevant reference-harness compilation without a
manual CMake reconfigure. All of this remains inside `SYNTH_BUILD_TESTS=ON`;
tests-off archive configuration neither discovers Git nor creates source
identity or harness targets.

`ReproducibilityInfo` records commit, tree, content identity, and dirty state.
Render manifests, render metrics, registry metrics, requirement reports,
candidate comparisons, and authoritative replay serialize and compare all four
fields.

At runtime, the authoritative `run` and `verify-release` commands inspect the
current repository before creating output or replaying evidence. They reject:

1. a binary whose embedded build identity is dirty;
2. a currently dirty repository;
3. a current commit, tree, or tracked-content identity different from the
   embedded build identity; or
4. an unreadable or malformed Git identity.

Ordinary compilation and non-authoritative validation remain possible while
dirty. Candidate/report generation and release verification require a clean
rebuild bound to the exact current source.

Isolated temporary Git repositories exercise a clean match, a binary identity
captured while dirty and checked after revert, a clean binary checked against a
dirty current tree, and a stale binary checked after a new commit.

## Complete frozen registry comparison

The semantic validator for
`Tests/fixtures/parameters/parameter-registry-v2.json` compares every entry, in
order, to `ParameterRegistry::descriptors()`. The exact contract includes:

- array position and stored index;
- ID, semantic key, version hint, display name, short label, and unit;
- kind, float range start/end/interval/skew/default, and symmetric skew;
- ordered choice values;
- mapping, automatable flag, smoothing class, and persistence scope.

JSON numerics are converted to the descriptor's float representation before an
exact comparison. Enum values use explicit exhaustive name mappings. Missing,
mistyped, reordered, or changed fields return `semantic.registry` during index
loading. Therefore even a coordinated fixture-byte and fixture-index SHA update
fails before gate construction and before `hard.registry.count` can support
PAR-001.

Mutation contracts update the fixture SHA in a copied index while changing
each descriptor field family, including non-ID metadata, ordered choices,
flags, mapping, smoothing, and persistence.

## Documentation and exact-head closure

TST-001 documentation states the precise inventory: three render fixtures
produce six files each, for 18 per-render artifacts; the root registry metric
and requirement report bring each candidate to 20 total files. Existing status
text remains `in-progress`.

The prior closure plan's verification, package, and report checkboxes are
checked only after those actions have fresh evidence. Because checking them
creates a post-package documentation commit, the final package is rebuilt and
reverified from that exact post-checklist commit. The ignored remediation
report appends the RED/GREEN chronology, commits, commands, hashes, counts, and
remaining open obligations.

## Verification boundary

Focused tests cover event-local metrics, generated and runtime source identity,
all frozen descriptor field families, provenance serialization, and forged or
missing identity rejection. Final verification uses a fresh path-with-spaces
Release build with tests, validators, warnings-as-errors, and distribution
identity checks enabled; exact local JUCE; 21/21 serial CTest; all seven labels;
renderer smoke, validation, repeat candidate generation and byte equality;
release exit 3; tamper and source-identity probes; registry mutations; source,
status, matrix, link, hash, diff, and history guards; exact-head ZIP manifest
and equality checks; and extracted no-`.git`, tests-off configuration.
