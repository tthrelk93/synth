# Successor Handoff Package Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Commit a permanent successor-ZIP handoff protocol and create one verified, self-contained, untracked `Agent-02-Workstream-02-Continuation-Context.zip` for the next agent.

**Architecture:** The roadmap, current kickoff, and canonical handoff define the durable protocol. A temporary package tree mirrors the complete remediation planning/evidence suite, primary references, and selected repository contracts; four package-facing context files orient the successor. An internal SHA-256 manifest verifies extracted contents, while the final response reports the outer ZIP hash.

**Tech Stack:** Markdown, Git, POSIX shell utilities, `zip`/`unzip`, `shasum`, Ruby validation, and repository-local planning/evidence files.

## Global Constraints

- The final archive name is exactly `Agent-02-Workstream-02-Continuation-Context.zip` at the repository root.
- The archive remains untracked; only protocol/planning changes are committed.
- Preserve `Agent-01-Workstream-02-Context.zip` and `Agent-01-Workstream-02-Context/` without modification.
- Include the complete `docs/remediation/` tree, original approved audit, and Model D manual.
- Exclude `.git`, build trees, validator downloads, recovery files, credentials, prior archives, and unrelated workspace files.
- Keep Workstream 02 / F0 active; BLD-001 is the first unmet gate.
- Status tokens remain exactly `not-started`, `in-progress`, `blocked`, `fail`, and `pass`; validator-only metadata may also use `not-run` where its schema permits.
- Every implementation agent must update the canonical handoff, commit tracked changes, create a verified untracked successor ZIP, and report its absolute path and outer SHA-256 before ending a session.
- Use `apply_patch` for authored file changes. Mechanical copying, manifest generation, and ZIP construction may use shell tools.

---

### Task 1: Permanent Successor-Package Protocol

**Files:**
- Modify: `docs/remediation/00-master-remediation-roadmap.md`
- Modify: `docs/remediation/16-agent-01-workstream-02-kickoff.md`
- Modify: `docs/remediation/17-implementation-handoff.md`

**Interfaces:**
- Consumes: the approved design in `docs/superpowers/specs/2026-07-19-successor-handoff-package-design.md`.
- Produces: one repository-wide policy, one kickoff obligation, and one canonical handoff protocol/template field used by every future agent.

- [ ] **Step 1: Run the protocol RED checks**

Run:

```bash
rg -n "successor ZIP|Successor handoff package|Successor archive" \
  docs/remediation/00-master-remediation-roadmap.md \
  docs/remediation/16-agent-01-workstream-02-kickoff.md \
  docs/remediation/17-implementation-handoff.md
```

Expected: no matching permanent protocol in at least the roadmap and required handoff template.

- [ ] **Step 2: Add the roadmap-wide policy**

Use `apply_patch` to add a `Successor handoff package policy` section to
`00-master-remediation-roadmap.md`. It must state that every implementation
agent, whether complete or blocked, must leave a verified self-contained ZIP;
the tracked planning/handoff state is committed first; the ZIP remains
untracked; the final response reports the absolute path and SHA-256; and the
archive must include `START-HERE.md`, `HANDOFF.md`, complete planning/evidence,
primary references, repository state, verification summary, and an internal
manifest.

- [ ] **Step 3: Add the current kickoff obligation**

Use `apply_patch` to add this requirement under the kickoff's handoff duties:

```markdown
Before ending the session, create and verify a self-contained successor ZIP
from the final committed planning/handoff state. Leave the ZIP untracked and
report its absolute path and SHA-256. The successor's `START-HERE.md` must repeat
this obligation so the chain continues for every agent.
```

- [ ] **Step 4: Add the canonical package protocol and template field**

Use `apply_patch` in `17-implementation-handoff.md` to define:

- required package contents and single-top-level-directory layout;
- tracked-before-untracked construction order;
- exclusion of Git/build/tool/user/recovery/credential data;
- `unzip -t`, path/symlink checks, fresh extraction, internal manifest, and
  source-hash comparison gates;
- final response requirements for absolute path, size, outer SHA-256, and first
  next action;
- a required handoff-history field named `Successor package` whose value records
  the filename, untracked status, verification result, and that the outer
  SHA-256 is reported after final archive construction.

Also update the current snapshot to name
`Agent-02-Workstream-02-Continuation-Context.zip` as the planned successor
package and state that its outer hash will be reported after construction.

- [ ] **Step 5: Validate protocol documentation**

Run:

```bash
git diff --check
ruby -E UTF-8:UTF-8 -e '
missing=[]
Dir.glob("docs/remediation/**/*.md").each do |file|
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
rg -n "successor ZIP|Successor handoff package|Successor archive" \
  docs/remediation/00-master-remediation-roadmap.md \
  docs/remediation/16-agent-01-workstream-02-kickoff.md \
  docs/remediation/17-implementation-handoff.md
```

Expected: whitespace and links pass; all three authorities contain the protocol.

- [ ] **Step 6: Commit the protocol**

```bash
git add docs/remediation/00-master-remediation-roadmap.md \
  docs/remediation/16-agent-01-workstream-02-kickoff.md \
  docs/remediation/17-implementation-handoff.md
git diff --cached --check
git commit -m "docs: require verified successor handoff archives"
```

Expected: one documentation commit; original Agent 01 inputs remain untracked.

---

### Task 2: Assemble the Self-Contained Context Tree

**Files:**
- Create temporarily: `${package_root}/START-HERE.md`
- Create temporarily: `${package_root}/HANDOFF.md`
- Create temporarily: `${package_root}/ARCHIVE-CONTENTS.md`
- Create temporarily: `${package_root}/repository-context/GIT-STATE.md`
- Create temporarily: `${package_root}/repository-context/VERIFICATION-SUMMARY.md`
- Copy temporarily: `docs/remediation/`, `Minimoog_Model_D_Manual.pdf`, the approved audit, and selected repository contracts listed below.

**Interfaces:**
- Consumes: clean tracked protocol commit from Task 1, current canonical planning/evidence, user-supplied primary reference/audit, and read-only Git state.
- Produces: one package tree with exact context, no generated manifest yet.

- [ ] **Step 1: Capture immutable construction inputs**

Run and retain the outputs for authored context files:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git log --oneline --decorate -20
git diff --name-status c30038d1ee39e7e06f4fcf64605defd51b3cdae2..HEAD
shasum -a 256 Minimoog_Model_D_Manual.pdf \
  Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt
```

Expected: branch `codex/workstream-02-build`; only the original ZIP/extracted
context are untracked before package creation; HEAD is the Task 1 protocol
commit.

- [ ] **Step 2: Create a bounded temporary package root**

Run:

```bash
package_work_root=$(mktemp -d '/private/tmp/model-d-agent-02-context.XXXXXX')
package_root="$package_work_root/Agent-02-Workstream-02-Continuation-Context"
mkdir -p "$package_root/planning-suite" "$package_root/primary-reference" \
  "$package_root/original-audit" "$package_root/repository-context"
```

Record both absolute paths and do not use `/tmp`, the repository root, `$HOME`,
or a glob as a cleanup target.

- [ ] **Step 3: Author `START-HERE.md` with `apply_patch`**

The complete prompt must identify Agent 02 as a Workstream 02 continuation,
embed the captured branch/HEAD, enumerate mandatory reading order, list every
BLD status, begin at hosted CI for BLD-001, list identity/licensing/AU/SDK/host
blockers, preserve frozen seams, forbid Workstream 03, require continuous
planning updates, and repeat the verified-successor-ZIP obligation.

- [ ] **Step 4: Author the package inventory and repository summaries with `apply_patch`**

Create `ARCHIVE-CONTENTS.md`, `repository-context/GIT-STATE.md`, and
`repository-context/VERIFICATION-SUMMARY.md` with the exact content required by
the approved design. Include these current facts:

```text
BLD-004=pass
BLD-007=blocked
BLD-011=blocked
BLD-001/002/003/005/006/008/009/010/012=in-progress
Debug CTest=9/9 pass
Release CTest=9/9 pass
clean-clone README commands=pass
actual wrapper=3/3 pass
pluginval 1.0.4 strictness 10=3/3 pass
standalone lifecycle=9/9 pass per configuration
auval=blocked because the AU is not registered
VST3 SDK validator=not-run
commercial hosts/hosted CI=blocked or not-run
build manifest SHA-256=93943197b44972c67d886d5a14b76dfa677a5486f085f55010c8bf393d983589
validation manifest SHA-256=7edf740f2888083a218faa8228ae33af16c7d24199db19d3b01830061026f287
standalone report SHA-256=fabcdf09cef6ba36939cf080fe82c56fcd39da7d7169bf9696156143c8c31214
```

Record the two recovery paths from the canonical handoff, but do not package
their files.

- [ ] **Step 5: Copy the exact source set mechanically**

Export the committed planning suite with `git archive` so ignored files such as
`.DS_Store` cannot enter the package, then create directories and copy:

```text
tracked files under HEAD:docs/remediation/ -> planning-suite/
docs/remediation/17-implementation-handoff.md -> HANDOFF.md
Minimoog_Model_D_Manual.pdf -> primary-reference/Minimoog_Model_D_Manual.pdf
Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt -> original-audit/approved-planning-suite.txt
README.md -> repository-context/README.md
CMakeLists.txt -> repository-context/CMakeLists.txt
.github/workflows/ci.yml -> repository-context/ci.yml
cmake/ProductIdentity.cmake -> repository-context/ProductIdentity.cmake
validation/required-host-matrix.json -> repository-context/required-host-matrix.json
```

Use:

```bash
tracked_export="$package_work_root/tracked-export"
mkdir "$tracked_export"
git archive --format=tar HEAD docs/remediation | tar -xf - -C "$tracked_export"
cp -R "$tracked_export/docs/remediation/." "$package_root/planning-suite/"
```

Use `cp` for the remaining exact files; do not transform copied content.

- [ ] **Step 6: Run the pre-manifest content audit**

Run:

```bash
find "$package_root" -type f -print | LC_ALL=C sort
test "$(find "$package_root" -type f | wc -l | tr -d ' ')" = "39"
test -z "$(find "$package_root" \
  \( -name .git -o -name .DS_Store -o -name CMakeCache.txt \
     -o -name validation-tools -o -name '*.settings' -o -name '*.log' \
     -o -name '*.zip' -o -iname '*recovery*' -o -iname '*secret*' \) \
  -print -quit)"
test "$(find "$package_work_root" -mindepth 1 -maxdepth 1 -type d \
  -name 'Agent-02-Workstream-02-Continuation-Context' | wc -l | tr -d ' ')" = "1"
```

Expected: 39 regular files before the manifest, no excluded entry, and exactly
one package directory (the separate `tracked-export` helper is not packaged).

---

### Task 3: Generate, Verify, and Deliver the ZIP

**Files:**
- Create temporarily: `${package_root}/MANIFEST.sha256`
- Create untracked: `Agent-02-Workstream-02-Continuation-Context.zip`
- Create temporarily: one independent extraction directory.

**Interfaces:**
- Consumes: audited package tree from Task 2.
- Produces: one ready-to-send untracked ZIP with verified internal contents and a reported outer SHA-256.

- [ ] **Step 1: Generate the internal content manifest**

From the package root, mechanically generate `MANIFEST.sha256` over every
regular file except `MANIFEST.sha256` itself:

```bash
LC_ALL=C find . -type f ! -name MANIFEST.sha256 -print0 \
  | LC_ALL=C sort -z \
  | xargs -0 shasum -a 256 > MANIFEST.sha256
```

Verify manifest coverage equals `regular-file-count - 1`, with no duplicate or
absolute manifest paths.

- [ ] **Step 2: Create the deterministic-metadata ZIP**

From the temporary directory that contains the one package root, run:

```bash
/usr/bin/zip -X -q -r \
  /Users/agentt/.openclaw/workspace/Developer/synth/Agent-02-Workstream-02-Continuation-Context.zip \
  Agent-02-Workstream-02-Continuation-Context
```

Expected: exit 0; the final path exists and remains untracked.

- [ ] **Step 3: Verify ZIP integrity and entry safety**

Run:

```bash
unzip -t Agent-02-Workstream-02-Continuation-Context.zip
zipinfo -1 Agent-02-Workstream-02-Continuation-Context.zip
zipinfo -l Agent-02-Workstream-02-Continuation-Context.zip
```

Use Ruby to reject empty names, absolute paths, drive/UNC forms, any `..` path
component, more than one top-level directory, and Unix symbolic-link modes.
Expected: all checks pass and the one top-level name is exact.

Run the path check as:

```bash
ruby -E UTF-8:UTF-8 -e '
zip=ARGV.fetch(0)
entries=IO.popen(["zipinfo", "-1", zip], &:read).lines.map(&:chomp)
abort("empty archive entry") if entries.any?(&:empty?)
unsafe=entries.select do |entry|
  entry.start_with?("/", "\\") || entry.match?(/\A[A-Za-z]:/) ||
    entry.split(/[\\\/]+/).include?("..")
end
abort("unsafe archive entries: #{unsafe.join(", ")}") unless unsafe.empty?
tops=entries.map { |entry| entry.split("/", 2).first }.uniq
expected="Agent-02-Workstream-02-Continuation-Context"
abort("unexpected top-level entries: #{tops.join(", ")}") unless tops == [expected]
puts "Archive entry paths: pass"
' Agent-02-Workstream-02-Continuation-Context.zip
if zipinfo -l Agent-02-Workstream-02-Continuation-Context.zip | awk '$1 ~ /^l/ { found=1 } END { exit found ? 0 : 1 }'; then
  echo "Archive contains a symbolic link" >&2
  exit 1
fi
```

- [ ] **Step 4: Verify a fresh extraction and internal manifest**

Create a new bounded extraction directory, extract the ZIP, and identify the
single expected extracted package root:

```bash
package_extract_root=$(mktemp -d '/private/tmp/model-d-agent-02-extract.XXXXXX')
unzip -q Agent-02-Workstream-02-Continuation-Context.zip -d "$package_extract_root"
extracted_package_root="$package_extract_root/Agent-02-Workstream-02-Continuation-Context"
cd "$extracted_package_root"
```

Then run:

```bash
shasum -a 256 -c MANIFEST.sha256
```

Expected: every entry reports `OK`. Confirm manifest coverage again after
extraction.

- [ ] **Step 5: Compare packaged sources to their authorities**

Run exact recursive/file comparisons:

```bash
cd /Users/agentt/.openclaw/workspace/Developer/synth
diff -qr docs/remediation "$extracted_package_root/planning-suite"
cmp Minimoog_Model_D_Manual.pdf "$extracted_package_root/primary-reference/Minimoog_Model_D_Manual.pdf"
cmp Agent-01-Workstream-02-Context/original-audit/approved-planning-suite.txt \
  "$extracted_package_root/original-audit/approved-planning-suite.txt"
cmp docs/remediation/17-implementation-handoff.md "$extracted_package_root/HANDOFF.md"
```

Expected: all commands exit 0. Also verify the selected repository-context
snapshots with `cmp` against their tracked sources.

- [ ] **Step 6: Verify kickoff/handoff semantic requirements**

Use `rg` against the extracted `START-HERE.md`, `HANDOFF.md`, `GIT-STATE.md`, and
`VERIFICATION-SUMMARY.md` for the exact branch, captured HEAD, BLD-001,
Workstream 02, all external blockers, successor ZIP obligation, and prohibition
on starting Workstream 03. Expected: every required concept has a match.

- [ ] **Step 7: Compute final artifact metadata and repository status**

Run:

```bash
ls -lh Agent-02-Workstream-02-Continuation-Context.zip
shasum -a 256 Agent-02-Workstream-02-Continuation-Context.zip
git status --short
git diff --check
git diff --cached --check
git log -3 --oneline
```

Expected: archive path/size/hash are available; no staged or modified tracked
files; only original handoff inputs plus the new successor ZIP are untracked.

- [ ] **Step 8: Remove only owned temporary directories**

After every verification gate passes, remove the two exact `mktemp` directories
created in Tasks 2 and 3. Recheck that the final ZIP still exists and its outer
SHA-256 is unchanged. Do not remove the original Agent 01 ZIP/directory or either
recovery path.

- [ ] **Step 9: Final handoff response**

Report the ready-to-send ZIP first, with a clickable absolute path, size,
SHA-256, and verification summary. Report the protocol commit and that the ZIP
is intentionally untracked. State that Agent 02 remains in Workstream 02 / F0
at BLD-001 and name the external blockers. Do not claim Workstream 02 complete.
