# Product Identity and Workstream 03 Handoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install the owner-approved TTH Audio product identity, record a truthful temporary exception for unavailable external validation, and deliver a verified Agent 07 archive whose successor begins Workstream 03.

**Architecture:** `cmake/ProductIdentity.cmake` remains the single authoritative host/build identity seam. One durable owner-decision evidence record drives synchronized roadmap, traceability, Workstream 02, and handoff updates; a new kickoff document makes the temporary scheduling exception explicit without converting unrun checks into passes. The final archive exports the exact committed planning state and selected repository contracts under one safe root with an internal SHA-256 manifest.

**Tech Stack:** CMake, JUCE 8.0.10, C++20, CTest, Markdown, Git, POSIX shell utilities, Ruby, `zip`/`unzip`, and `shasum`.

## Global Constraints

- Approved identity values are exactly: manufacturer `TTH Audio`, product `TTH Model One`, manufacturer code `TTHA`, product code `TM01`, manufacturer domain `io.github.tthrelk93`, bundle ID `io.github.tthrelk93.TTHModelOne`, and website `https://github.com/tthrelk93`.
- Record the owner's factual decision that the product has never been distributed.
- Preserve internal CMake/library target names and every existing parameter ID; only host-facing product identity changes.
- BLD-007 may pass only after distribution-identity configuration succeeds with the committed values.
- BLD-006 remains `in-progress`; required commercial-host category/menu evidence is deferred and unrun.
- BLD-011 becomes `in-progress`, not `pass`; designated-account AU, Steinberg validator, and commercial-host checks remain deferred and required before release.
- BLD-012 remains `in-progress` until final distribution aggregation includes the deferred evidence.
- Workstream 03 may begin under the approved scheduling exception, but public distribution, notarization, store submission, and claims that Workstream 02 fully passes remain prohibited.
- Creating the dedicated macOS user still requires the owner to authenticate locally; never record or request a password.
- The replacement archive is `Agent-07-Workstream-03-Continuation-Context.zip`; the older Workstream 02 archive remains untouched but is superseded.
- Use `apply_patch` for authored source/document changes. Mechanical export, copying, hashing, and archive construction may use shell utilities.
- Preserve all unrelated untracked predecessor archives and extracted directories.

---

### Task 1: Install and Prove the Approved Product Identity

**Files:**
- Modify: `cmake/ProductIdentity.cmake`
- Modify: `README.md`

**Interfaces:**
- Consumes: the approved identity in `docs/superpowers/specs/2026-07-21-product-identity-and-gate-deferral-design.md`.
- Produces: authoritative CMake variables consumed by `juce_add_plugin`, artifact staging, AU validation, and build/validation manifests.

- [ ] **Step 1: Run the existing distribution guard as a RED check**

Run in a fresh bounded directory:

```bash
identity_red_root=$(mktemp -d '/private/tmp/tth-model-one-identity-red.XXXXXX')
cmake -S . -B "$identity_red_root/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON \
  -DSYNTH_BUILD_TESTS=OFF
```

Expected: configure fails with `Distribution identity validation failed (BLD-007)` and reports the placeholder manufacturer/domain and unapproved identity.

- [ ] **Step 2: Replace the authoritative identity atomically**

Use `apply_patch` so `cmake/ProductIdentity.cmake` contains exactly:

```cmake
# Owner-approved host and distribution identity. The product owner confirmed on
# 2026-07-21 that no prior distribution identity or compatibility history exists.
set(SYNTH_PRODUCT_NAME "TTH Model One")
set(SYNTH_MANUFACTURER_NAME "TTH Audio")
set(SYNTH_MANUFACTURER_WEBSITE "https://github.com/tthrelk93")
set(SYNTH_MANUFACTURER_EMAIL "")
set(SYNTH_MANUFACTURER_DOMAIN "io.github.tthrelk93")
set(SYNTH_MANUFACTURER_CODE "TTHA")
set(SYNTH_PRODUCT_CODE "TM01")
set(SYNTH_BUNDLE_ID "io.github.tthrelk93.TTHModelOne")
set(SYNTH_IDENTITY_APPROVED ON)
```

- [ ] **Step 3: Reconcile user-facing build documentation**

Use `apply_patch` in `README.md` to make these exact semantic changes:

```markdown
# TTH Model One

A JUCE 8 Model D-inspired MIDI instrument by TTH Audio.
```

Replace the development-only identity warning with a statement that the owner-approved build identity lives in `cmake/ProductIdentity.cmake`, no prior distribution exists, and release still requires deferred external validation. Replace each staged product path basename `MiniMoog` with `TTH Model One` while retaining VST3, Standalone, and AU layout descriptions.

- [ ] **Step 4: Run the GREEN identity configuration and metadata checks**

Run:

```bash
identity_green_root=$(mktemp -d '/private/tmp/tth-model-one-identity-green.XXXXXX')
cmake -S . -B "$identity_green_root/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON \
  -DSYNTH_BUILD_TESTS=OFF
rg -n 'TTH Model One|TTH Audio|TTHA|TM01|io\.github\.tthrelk93' \
  cmake/ProductIdentity.cmake README.md
if rg -n 'yourcompany|com\.yourcompany|"Manu"|"Via9"' \
  cmake/ProductIdentity.cmake README.md; then
  exit 1
fi
```

Expected: configure exits 0; every approved value is present; no authoritative placeholder is found.

- [ ] **Step 5: Commit the identity seam**

Run:

```bash
git add cmake/ProductIdentity.cmake README.md
git diff --cached --check
git commit -m "build: approve TTH Model One product identity"
```

Expected: one focused build/documentation commit.

---

### Task 2: Record the Owner Decision and Open Workstream 03

**Files:**
- Create: `docs/remediation/evidence/workstream-02/owner-identity-and-deferral-decision.md`
- Create: `docs/remediation/18-agent-07-workstream-03-kickoff.md`
- Modify: `docs/remediation/00-master-remediation-roadmap.md`
- Modify: `docs/remediation/01-traceability-matrix.md`
- Modify: `docs/remediation/02-build-packaging-host-validation.md`
- Modify: `docs/remediation/17-implementation-handoff.md`

**Interfaces:**
- Consumes: the passing identity seam from Task 1 and the approved design/spec.
- Produces: one factual evidence authority, synchronized BLD status narratives, and a copy-ready Workstream 03 kickoff.

- [ ] **Step 1: Add durable owner-decision evidence**

Use `apply_patch` to create the evidence record with these complete facts:

```markdown
# Owner identity and temporary external-gate deferral

On 2026-07-21 the product owner confirmed that no version of this product had
previously been distributed and approved the identity defined in
`cmake/ProductIdentity.cmake`: TTH Audio / TTH Model One, `TTHA` / `TM01`,
`io.github.tthrelk93`, and `io.github.tthrelk93.TTHModelOne`.

The owner also authorized Workstream 03 to begin while commercial-host checks,
designated-account AU registration/auval, and the Steinberg SDK validator are
temporarily unavailable. These checks remain unrun pre-release requirements;
they are not waived or treated as passing. The dedicated standard macOS account
is authorized but must be created through a local administrator prompt without
exposing its password.
```

Add sections mapping the decision to BLD-006=`in-progress`, BLD-007=`pass` after the Task 1 configure proof, BLD-011=`in-progress`, and BLD-012=`in-progress`, plus an explicit prohibition on release/distribution claims.

- [ ] **Step 2: Synchronize the roadmap and active workstream**

Use `apply_patch` in `00-master-remediation-roadmap.md` to:

- change the header to Workstream 03 / Agent 07;
- keep Workstream 02 `in-progress` for BLD-006/011/012 pre-release evidence;
- set Workstream 03 to `In progress — Agent 07` under the approved scheduling exception;
- change the Workstream 03 entry gate from `Workstream 02 passes` to `Workstream 02 automated build/test seams pass; owner-approved temporary exception carries BLD-006/011/012 as pre-release gates`;
- add the approved identity and no-prior-distribution decision to the fixed product decisions table;
- link the new owner-decision evidence and kickoff.

- [ ] **Step 3: Synchronize requirement evidence and status ledgers**

Use `apply_patch` so both `02-build-packaging-host-validation.md` and the current ledger in `17-implementation-handoff.md` contain these exact statuses:

```text
BLD-001 pass
BLD-002 pass
BLD-003 pass
BLD-004 pass
BLD-005 pass
BLD-006 in-progress
BLD-007 pass
BLD-008 pass
BLD-009 pass
BLD-010 pass
BLD-011 in-progress
BLD-012 in-progress
```

Link the owner-decision evidence from BLD-006, BLD-007, BLD-011, and BLD-012. State that BLD-007 passed through the owner-approved identity and successful distribution configure; state that the other three remain required before release. Replace every current instruction forbidding Workstream 03 with the bounded exception, while retaining historical handoff snapshots unchanged as history.

- [ ] **Step 4: Reconcile traceability findings**

Use `apply_patch` in `01-traceability-matrix.md` to update:

- BLD-006 evidence with the explicit commercial-host deferral and non-pass status;
- BLD-007 evidence with the approved identity, no-prior-distribution record, and successful guard;
- BLD-011 evidence with the owner-authorized account and remaining local-authentication/tool/host work;
- BLD-012 evidence with the reason development manifests remain usable but distribution aggregation is incomplete.

Do not change requirement ownership, severity, dependencies, or acceptance criteria.

- [ ] **Step 5: Create the Agent 07 Workstream 03 kickoff**

Use `apply_patch` to create `18-agent-07-workstream-03-kickoff.md`. It must command the successor to:

1. verify package `MANIFEST.sha256`, compare packaged HEAD with local/upstream/PR state, and read roadmap → traceability → Workstream 03 → Workstream 12 → handoff → owner-decision evidence;
2. begin Workstream 03 by inventorying every existing parameter ID and capturing host enumeration and unversioned APVTS state fixtures before changing metadata;
3. preserve CMake targets, JUCE pin, wrapper/bus topology, approved TTH identity, legacy parameter IDs, and passing tests;
4. treat BLD-006/011/012 as deferred pre-release obligations, never as passes;
5. avoid purchases, legal-term acceptance, account passwords, releases, or public distribution without new owner action;
6. update roadmap/matrix/Workstream 03/handoff continuously and produce the next verified successor ZIP.

- [ ] **Step 6: Validate documentation consistency**

Run:

```bash
git diff --check
ruby -E UTF-8:UTF-8 -e '
missing=[]
Dir.glob("docs/**/*.md").each do |file|
  File.read(file, encoding: "UTF-8").scan(/\[[^\]]*\]\(([^)]+)\)/).flatten.each do |target|
    next if target =~ %r{\A(?:https?://|mailto:|#)}
    path=target.split("#",2).first
    next if path.empty?
    missing << "#{file}: #{target}" unless File.exist?(File.expand_path(path, File.dirname(file)))
  end
end
abort("Missing local links:\n"+missing.join("\n")) unless missing.empty?
puts "Local Markdown links: pass"
'
ruby -e '
files=%w[docs/remediation/02-build-packaging-host-validation.md docs/remediation/17-implementation-handoff.md]
ids=(1..12).map { |n| format("BLD-%03d", n) }
files.each do |file|
  text=File.read(file)
  statuses={}
  text.scan(/^\| (BLD-\d{3}) \| (not-started|in-progress|blocked|fail|pass) \|/).each { |id,s| statuses[id] ||= s }
  expected={"BLD-006"=>"in-progress","BLD-007"=>"pass","BLD-011"=>"in-progress","BLD-012"=>"in-progress"}
  (ids-expected.keys).each { |id| expected[id]="pass" }
  abort("status mismatch in #{file}: #{statuses}") unless expected.all? { |id,s| statuses[id] == s }
end
puts "BLD status ledgers: pass"
'
test "$(rg -o '^\| [A-Z]+-[0-9]{3} \|' docs/remediation/01-traceability-matrix.md | sort -u | wc -l | tr -d ' ')" = "127"
rg -n 'Workstream 03|BLD-006|BLD-007|BLD-011|BLD-012|pre-release' \
  docs/remediation/00-master-remediation-roadmap.md \
  docs/remediation/18-agent-07-workstream-03-kickoff.md \
  docs/remediation/17-implementation-handoff.md
```

Expected: whitespace, local links, both 12-row ledgers, 127 unique traceability IDs, and phase-gate language pass.

- [ ] **Step 7: Commit the owner decision and phase transition**

Run:

```bash
git add docs/remediation
git diff --cached --check
git commit -m "docs: open Workstream 03 with deferred release gates"
```

Expected: one focused remediation/handoff commit.

---

### Task 3: Run the Fresh Supported Baseline and Reconcile Evidence

**Files:**
- Modify: `docs/remediation/evidence/workstream-02/owner-identity-and-deferral-decision.md`
- Modify: `docs/remediation/17-implementation-handoff.md`

**Interfaces:**
- Consumes: Task 1 identity and Task 2 status policy.
- Produces: exact local verification evidence at the final tracked source state used by the successor archive.

- [ ] **Step 1: Configure the strict Release baseline in a space-bearing path**

Run:

```bash
baseline_root=$(mktemp -d '/private/tmp/tth-model-one baseline.XXXXXX')
cmake -S . -B "$baseline_root/Release build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON \
  -DSYNTH_WARNINGS_AS_ERRORS=ON \
  -DSYNTH_BUILD_TESTS=ON \
  -DSYNTH_BUILD_VALIDATORS=ON
```

Expected: configure exits 0, resolves exact JUCE commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`, and accepts the distribution identity.

- [ ] **Step 2: Build all supported local targets and run CTest**

Run:

```bash
cmake --build "$baseline_root/Release build" --config Release --parallel 2
ctest --test-dir "$baseline_root/Release build" -C Release --output-on-failure
for label in unit state dsp midi realtime host artifact; do
  ctest --test-dir "$baseline_root/Release build" -C Release \
    --show-only=json-v1 -L "$label" > "$baseline_root/$label.json"
  ruby -rjson -e 'j=JSON.parse(File.read(ARGV[0])); abort("empty label") if j.fetch("tests").empty?' \
    "$baseline_root/$label.json"
done
```

Expected: all-target build exits 0; CTest reports 9/9 passed; every required label contains at least one test.

- [ ] **Step 3: Capture generated host identity/category evidence**

Run `rg` beneath the generated build for `TTH Model One`, `TTHA`, `TM01`, `Instrument|Synth`, and `aumu`. Confirm that authoritative generated metadata uses the new identity and retains the established wrapper categories. Do not record any commercial host as run.

- [ ] **Step 4: Append exact verification facts**

Use `apply_patch` to add the date, platform, architecture, commands, 9/9 result, required-label coverage, JUCE commit, approved generated identity, and retained generated category evidence to the owner-decision evidence and current handoff evidence table. Explicitly repeat that no designated-account or commercial-host test ran.

- [ ] **Step 5: Re-run documentation and diff gates, then commit**

Run:

```bash
git diff --check
git diff --name-only | ruby -e '
allowed=[%r{\Adocs/remediation/evidence/workstream-02/owner-identity-and-deferral-decision\.md\z},%r{\Adocs/remediation/17-implementation-handoff\.md\z}]
bad=STDIN.read.lines.map(&:chomp).reject { |p| allowed.any? { |r| r.match?(p) } }
abort("unexpected files: #{bad.join(", ")}") unless bad.empty?
'
git add docs/remediation/evidence/workstream-02/owner-identity-and-deferral-decision.md \
  docs/remediation/17-implementation-handoff.md
git diff --cached --check
git commit -m "docs: record TTH identity verification baseline [skip ci]"
```

Expected: exact evidence-only commit.

---

### Task 4: Build and Fully Verify the Workstream 03 Successor Archive

**Files:**
- Create temporarily: one package tree and one independent extraction tree under `/private/tmp`.
- Create untracked: `Agent-07-Workstream-03-Continuation-Context.zip`

**Interfaces:**
- Consumes: exact committed HEAD, canonical kickoff/handoff/remediation suite, original approved audit, and Model D manual.
- Produces: one ready-to-attach successor archive with safe paths, exact source snapshots, and internal/outer SHA-256 verification.

- [ ] **Step 1: Capture final immutable state**

Run:

```bash
git status --short --branch
git branch --show-current
git rev-parse HEAD
git rev-parse origin/codex/workstream-02-build
git log --oneline --decorate -10
shasum -a 256 Minimoog_Model_D_Manual.pdf \
  Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt
```

Expected: tracked tree clean, branch `codex/workstream-02-build`; predecessor inputs remain untracked. Push committed changes before packaging so local, upstream, and PR head are identical.

- [ ] **Step 2: Create a bounded package root and export committed sources**

Run:

```bash
package_work_root=$(mktemp -d '/private/tmp/tth-agent-07-ws03.XXXXXX')
package_name='Agent-07-Workstream-03-Continuation-Context'
package_root="$package_work_root/$package_name"
mkdir -p "$package_root/planning-suite" "$package_root/primary-reference" \
  "$package_root/original-audit" "$package_root/repository-context"
tracked_export="$package_work_root/tracked-export"
mkdir "$tracked_export"
git archive --format=tar HEAD docs/remediation | tar -xf - -C "$tracked_export"
cp -R "$tracked_export/docs/remediation/." "$package_root/planning-suite/"
cp docs/remediation/18-agent-07-workstream-03-kickoff.md "$package_root/START-HERE.md"
cp docs/remediation/17-implementation-handoff.md "$package_root/HANDOFF.md"
cp Minimoog_Model_D_Manual.pdf "$package_root/primary-reference/Minimoog_Model_D_Manual.pdf"
cp Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt \
  "$package_root/original-audit/approved-planning-suite.txt"
cp README.md CMakeLists.txt cmake/ProductIdentity.cmake \
  validation/required-host-matrix.json .github/workflows/ci.yml \
  "$package_root/repository-context/"
```

The `cp` command preserves the exact basenames `README.md`, `CMakeLists.txt`,
`ProductIdentity.cmake`, `required-host-matrix.json`, and `ci.yml`; do not
transform their contents.

- [ ] **Step 3: Author package-only state summaries with `apply_patch`**

Create `ARCHIVE-CONTENTS.md`, `repository-context/GIT-STATE.md`, and `repository-context/VERIFICATION-SUMMARY.md`. Record the exact branch, final local/upstream/PR HEAD, recent commits, approved identity, 12-row BLD ledger, fresh build/CTest result, deferred checks, reading order, excluded content, and that Workstream 03 begins with parameter-ID/state-fixture capture. State that the older `Agent-07-Workstream-02-Continuation-Context.zip` is superseded and must not be used.

- [ ] **Step 4: Generate the internal manifest and archive**

Run:

```bash
(cd "$package_root" && \
  LC_ALL=C find . -type f ! -name MANIFEST.sha256 -print0 | \
  LC_ALL=C sort -z | xargs -0 shasum -a 256 > MANIFEST.sha256)
(cd "$package_work_root" && \
  /usr/bin/zip -X -q -r \
  /Users/agentt/.openclaw/workspace/Developer/synth/Agent-07-Workstream-03-Continuation-Context.zip \
  "$package_name")
```

Expected: manifest covers every regular file except itself and ZIP creation exits 0.

- [ ] **Step 5: Verify archive integrity and entry safety**

Run:

```bash
unzip -t Agent-07-Workstream-03-Continuation-Context.zip
ruby -E UTF-8:UTF-8 -e '
zip=ARGV.fetch(0)
entries=IO.popen(["zipinfo", "-1", zip], &:read).lines.map(&:chomp)
abort("empty archive entry") if entries.any?(&:empty?)
unsafe=entries.select { |e| e.start_with?("/", "\\") || e.match?(/\A[A-Za-z]:/) || e.split(/[\\\/]+/).include?("..") }
abort("unsafe entries: #{unsafe.join(", ")}") unless unsafe.empty?
tops=entries.map { |e| e.split("/",2).first }.uniq
abort("unexpected roots: #{tops.join(", ")}") unless tops == ["Agent-07-Workstream-03-Continuation-Context"]
puts "Archive entry paths: pass"
' Agent-07-Workstream-03-Continuation-Context.zip
if zipinfo -l Agent-07-Workstream-03-Continuation-Context.zip | awk '$1 ~ /^l/ { found=1 } END { exit found ? 0 : 1 }'; then
  echo 'Archive contains a symbolic link' >&2
  exit 1
fi
```

Expected: `unzip -t` and safe-path checks pass; no symlink entry exists; exactly one top-level directory exists.

- [ ] **Step 6: Verify fresh extraction, internal hashes, and source equality**

Run:

```bash
package_extract_root=$(mktemp -d '/private/tmp/tth-agent-07-ws03-extract.XXXXXX')
unzip -q Agent-07-Workstream-03-Continuation-Context.zip -d "$package_extract_root"
extracted_root="$package_extract_root/Agent-07-Workstream-03-Continuation-Context"
(cd "$extracted_root" && shasum -a 256 -c MANIFEST.sha256)
diff -qr docs/remediation "$extracted_root/planning-suite"
cmp docs/remediation/18-agent-07-workstream-03-kickoff.md "$extracted_root/START-HERE.md"
cmp docs/remediation/17-implementation-handoff.md "$extracted_root/HANDOFF.md"
cmp Minimoog_Model_D_Manual.pdf "$extracted_root/primary-reference/Minimoog_Model_D_Manual.pdf"
cmp Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt \
  "$extracted_root/original-audit/approved-planning-suite.txt"
cmp README.md "$extracted_root/repository-context/README.md"
cmp CMakeLists.txt "$extracted_root/repository-context/CMakeLists.txt"
cmp cmake/ProductIdentity.cmake "$extracted_root/repository-context/ProductIdentity.cmake"
cmp validation/required-host-matrix.json "$extracted_root/repository-context/required-host-matrix.json"
cmp .github/workflows/ci.yml "$extracted_root/repository-context/ci.yml"
```

Expected: every manifest entry reports `OK`; every comparison exits 0.

- [ ] **Step 7: Verify successor semantics**

Run `rg` over extracted `START-HERE.md`, `HANDOFF.md`, `GIT-STATE.md`, and `VERIFICATION-SUMMARY.md` for all of these concepts: exact final HEAD; Workstream 03; parameter ID inventory; legacy state fixture; TTH Audio; TTH Model One; BLD-007 pass; BLD-006/011/012 in progress; deferred pre-release tests; no distribution; password prohibition; successor ZIP obligation; and superseded Workstream 02 archive. Expected: every concept has at least one match in the intended authority.

- [ ] **Step 8: Record final artifact metadata and clean only owned temporary roots**

Run:

```bash
ls -lh Agent-07-Workstream-03-Continuation-Context.zip
shasum -a 256 Agent-07-Workstream-03-Continuation-Context.zip
git status --short --branch
git diff --check
git log -5 --oneline --decorate
```

After every verification passes, remove only the exact three bounded temporary roots created by this plan using `find <validated-path> -depth -delete`; verify the ZIP hash is unchanged. Do not remove any predecessor archive or extracted directory.

- [ ] **Step 9: Deliver the successor**

Report the new archive's clickable absolute path, byte/human size, outer SHA-256, final commit, push/PR synchronization, and complete verification result. State that the successor's first action is Workstream 03 parameter-ID and legacy-state fixture capture, while BLD-006/011/012 remain pre-release obligations.
