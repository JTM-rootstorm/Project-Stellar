# Project Stellar — Doxygen Sanitization Cleanup Plan for Codex

Target branch: `doxygen-gen`  
Repository: `JTM-rootstorm/Project-Stellar`  
Purpose: close the remaining documentation sanitization gaps before merging `doxygen-gen`.

## Current Assessment

The Doxygen generation branch is structurally correct and mostly complete:

- `doxygen-gen` is ahead of `main` and contains the expected docs-generation implementation.
- `CMakeLists.txt` uses `project(StellarEngine VERSION 0.3.2 ...)`.
- `docs/Design.md` also reports version `0.3.2`.
- `docs/Doxyfile.in` uses `@PROJECT_VERSION@`, so generated Doxygen docs should inherit the CMake project version instead of hardcoding a separate version.
- `cmake/StellarDocs.cmake` defines the optional `stellar_docs_doxygen` target.
- Doxygen output is configured under `build/docs/doxygen/html`.
- Doxygen warning logs are configured under `build/docs/doxygen-warnings.log`.
- `tests/CMakeLists.txt` conditionally registers `docs_doxygen_generate` only when `stellar_docs_doxygen` exists.
- Active Markdown files remain the narrative documentation layer.
- Generated Doxygen docs include selected Markdown pages and exclude archived/runtime/agent coordination content.
- Generated HTML does not appear to be committed.

The branch is not fully sanitized yet because `docs/ImplementationStatus.md` records **53 remaining Doxygen warning-log lines**, including:

- Markdown anchor/reference warnings in `docs/Design.md`.
- Markdown anchor/reference warnings in `docs/TrenchBroom.md`.
- A small set of undocumented public-header members in graphics texture-upload and physics detail types.

## Non-Goals

Do not:

- Rewrite the Doxygen integration architecture.
- Move active Markdown docs into C++ comments.
- Remove active Markdown docs from Doxygen input unless a specific warning requires a narrow link fix.
- Make Doxygen mandatory for normal builds.
- Commit generated Doxygen HTML or warning logs.
- Change public API behavior, signatures, target links, namespaces, or build boundaries.
- Reopen completed historical work.
- Edit archived plans except to reference them from active status docs if needed.

---

# Phase DS-0 — Reproduce Current Doxygen Warning Surface

## Objective

Generate the current warning log on `doxygen-gen` and classify every remaining warning before editing.

## Codex Prompt

```text
You are on branch doxygen-gen in JTM-rootstorm/Project-Stellar.

Do not commit or push.

Reproduce the current Doxygen warning surface. Configure, build docs, inspect build/docs/doxygen-warnings.log, and classify warnings into:
1. actual missing public API docs;
2. Markdown anchor/reference warnings;
3. false positives or intentionally ignored internals.

Do not edit files in this phase. Report the warning count and exact files/symbols involved.
```

## Commands

```bash
git status --short
git branch --show-current

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen

test -f build/docs/doxygen/html/index.html
test -f build/docs/doxygen-warnings.log
wc -l build/docs/doxygen-warnings.log
cat build/docs/doxygen-warnings.log
```

## Acceptance Criteria

- Warning count is known.
- Each warning is assigned to a cleanup category.
- No files are changed in DS-0.

---

# Phase DS-1 — Fix Undocumented Graphics Texture Upload Members

## Objective

Close the obvious undocumented public-member warning in `TextureUpload`.

## Target File

- `include/stellar/graphics/GraphicsDevice.hpp`

## Known Finding

`TextureUpload` has a struct-level `@brief`, but its public fields lack member-level comments:

```cpp
struct TextureUpload {
    stellar::assets::ImageAsset image;
    stellar::assets::TextureColorSpace color_space = stellar::assets::TextureColorSpace::kLinear;
};
```

## Required Fix

Add concise Doxygen comments to both public fields:

```cpp
/**
 * @brief Backend-neutral image upload payload with color-space metadata.
 */
struct TextureUpload {
    /** @brief CPU-side image payload to upload into a backend texture. */
    stellar::assets::ImageAsset image;

    /** @brief Color-space interpretation used by the backend texture upload. */
    stellar::assets::TextureColorSpace color_space = stellar::assets::TextureColorSpace::kLinear;
};
```

## Guardrails

- Do not change field order.
- Do not change types or default values.
- Do not change graphics behavior.
- Do not reformat unrelated sections.

## Validation

```bash
cmake --build build --target stellar_docs_doxygen
grep -n "TextureUpload" build/docs/doxygen-warnings.log || true
```

## Acceptance Criteria

- No Doxygen warning remains for `TextureUpload::image`.
- No Doxygen warning remains for `TextureUpload::color_space`.
- No behavioral source changes are introduced.

---

# Phase DS-2 — Hide or Document Public-Header Physics Detail Types

## Objective

Resolve warnings for implementation-detail physics broadphase structs exposed in a public header.

## Target File

- `include/stellar/physics/CollisionWorld.hpp`

## Known Finding

The public header exposes internal implementation storage under `stellar::physics::detail`:

- `CollisionAabb`
- `CollisionTriangleRef`
- `CollisionBvhNode`

These have struct-level one-line `@brief`s, but their fields may still generate undocumented-member warnings.

## Preferred Fix

Hide the entire `detail` namespace block from generated public Doxygen output using Doxygen conditional markers:

```cpp
/** @cond INTERNAL */
namespace detail {

// internal broadphase structs...

} // namespace detail
/** @endcond */
```

Place `@cond INTERNAL` immediately before `namespace detail {` and `@endcond` immediately after the matching closing brace/comment.

## Alternate Fix

If hiding `detail` from Doxygen causes Doxygen or formatting problems, document every public field inside the detail structs instead. This is acceptable but less ideal because the generated public API docs should not highlight internal broadphase storage.

## Guardrails

- Do not move the structs.
- Do not rename the structs.
- Do not change field visibility, types, defaults, or namespace.
- Do not alter collision behavior.
- Do not hide non-detail public API from Doxygen.

## Validation

```bash
cmake --build build --target stellar_docs_doxygen
grep -n "CollisionAabb\|CollisionTriangleRef\|CollisionBvhNode" build/docs/doxygen-warnings.log || true
```

## Acceptance Criteria

- No Doxygen warnings remain for the physics `detail` structs or their members.
- Public collision API docs for `CollisionWorld`, `RaycastHit`, `GroundProbeHit`, `MoveResult`, `CollisionQueryFilter`, and public query functions remain visible.
- Collision tests still pass in final validation.

---

# Phase DS-3 — Fix Markdown Anchor and Reference Warnings

## Objective

Resolve Doxygen Markdown warnings without weakening the active documentation model.

## Target Files

- `docs/Design.md`
- `docs/TrenchBroom.md`
- Any other Markdown files named in `build/docs/doxygen-warnings.log`

## Known Finding

`docs/ImplementationStatus.md` records remaining Markdown anchor/reference warnings in `docs/Design.md` and `docs/TrenchBroom.md`.

## Required Approach

Use the exact warning log to identify broken references. Fix only the affected links/anchors.

Common Doxygen Markdown issues to check:

- GitHub-style generated anchors may differ from Doxygen-generated anchors.
- Numbered headings such as `## 9.5 Server-Authoritative Scripting` may produce anchors that differ from manually written table-of-contents links.
- Relative links to Markdown files may need plain file links instead of fragment links.
- Duplicate headings may generate unstable anchors.
- Punctuation, slashes, underscores, and periods in headings may produce unexpected Doxygen anchor IDs.

## Possible Fix Patterns

Prefer simple file links when fragment links are not needed:

```markdown
[Documentation Standards](Design.md)
```

instead of fragile heading fragments.

For stable intra-page links, add explicit anchors near headings:

```markdown
<a id="documentation-standards"></a>
## 15. Documentation Standards
```

Then link to:

```markdown
[Documentation Standards](#documentation-standards)
```

Only add explicit anchors when necessary.

## Guardrails

- Do not rewrite large Markdown sections.
- Do not remove existing GitHub usability without need.
- Do not alter documentation authority or status meanings.
- Do not remove active docs from `docs/Doxyfile.in` just to hide warnings, unless the file is not meant to be in generated docs.
- Do not touch archived docs for this phase unless a generated-doc input accidentally includes them.

## Validation

```bash
cmake --build build --target stellar_docs_doxygen
cat build/docs/doxygen-warnings.log
grep -n "docs/Design.md\|docs/TrenchBroom.md" build/docs/doxygen-warnings.log || true
python3 tools/docs/check_docs_consistency.py
```

## Acceptance Criteria

- No Markdown anchor/reference warnings remain for `docs/Design.md`.
- No Markdown anchor/reference warnings remain for `docs/TrenchBroom.md`.
- `tools/docs/check_docs_consistency.py` still passes.
- Markdown still reads correctly on GitHub.

---

# Phase DS-4 — Add Doxygen Plan Reference to Implementation Status

## Objective

Make `docs/ImplementationStatus.md` consistent with other completed scopes by referencing the archived Doxygen handoff plan.

## Target File

- `docs/ImplementationStatus.md`

## Known Finding

The branch added:

```text
Plans/Archived/DoxygenGen-CodexPlan/
```

The Doxygen section describes completed DG-6 status and validation but does not clearly list the completed handoff plan the way other completed scopes do.

## Required Fix

Under:

```markdown
## Completed Scope — Doxygen Generation
```

after the status line, add:

```markdown
Completed handoff plan:

- `Plans/Archived/DoxygenGen-CodexPlan/00-MASTER-DoxygenGen-CodexPlan.md`
```

## Guardrails

- Keep the Doxygen status section short.
- Do not update the validation result text unless validation is re-run and results differ.
- Do not move the Doxygen section below older scopes.
- Do not rewrite unrelated implementation history.

## Validation

```bash
python3 tools/docs/check_docs_consistency.py
grep -n "DoxygenGen-CodexPlan" docs/ImplementationStatus.md
```

## Acceptance Criteria

- Implementation status references the archived Doxygen plan package.
- Existing docs consistency checks still pass.

---

# Phase DS-5 — Reconcile Warning Count and Status Text

## Objective

Make status text match the final warning state after cleanup.

## Target File

- `docs/ImplementationStatus.md`

## Current Text to Review

The Doxygen section currently says:

```markdown
Remaining Doxygen warnings: 53 lines in `build/docs/doxygen-warnings.log`. The remaining categories
are Markdown anchor/reference warnings in `docs/Design.md` and `docs/TrenchBroom.md`, plus a small
set of undocumented graphics texture-upload and physics detail members in public headers.
```

## Required Fix

After DS-1 through DS-3, update this paragraph to match the actual final state.

### If Warning Log Is Empty

Use:

```markdown
Remaining Doxygen warnings: none.
```

### If Only Accepted Non-Actionable Warnings Remain

Use:

```markdown
Remaining Doxygen warnings: N lines in `build/docs/doxygen-warnings.log`.

Remaining warnings are limited to [specific category]. They are accepted for now because [reason].
```

Do not leave stale counts or stale categories.

## Guardrails

- Do not claim zero warnings unless `build/docs/doxygen-warnings.log` is empty or absent after successful Doxygen generation.
- If warnings remain, list exact categories and why they are accepted.
- Prefer fixing warnings over accepting them.

## Validation

```bash
cmake --build build --target stellar_docs_doxygen
if [ -f build/docs/doxygen-warnings.log ]; then wc -l build/docs/doxygen-warnings.log; cat build/docs/doxygen-warnings.log; fi
python3 tools/docs/check_docs_consistency.py
```

## Acceptance Criteria

- `docs/ImplementationStatus.md` warning count matches the generated warning log.
- Warning categories in status text match the generated warning log.
- No stale “53 lines” claim remains if the count changes.

---

# Phase DS-6 — Final Branch Sanitization Validation

## Objective

Confirm the branch is merge-ready from a documentation and build/test perspective.

## Commands

```bash
git status --short

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

cmake --build build --target stellar_docs_doxygen
test -f build/docs/doxygen/html/index.html

python3 tools/docs/check_docs_consistency.py
tools/dev/check_target_boundaries.sh

ctest --test-dir build -R '^(docs_consistency|docs_doxygen_generate|target_boundary)$' --output-on-failure

if [ -f build/docs/doxygen-warnings.log ]; then
    wc -l build/docs/doxygen-warnings.log
    cat build/docs/doxygen-warnings.log
fi

find . -path './build' -prune -o \
    \( -path './docs/html' -o -path './html' -o -name 'doxygen-warnings.log' \) -print
```

## Acceptance Criteria

- Working tree contains only intended source/doc changes.
- Configure passes.
- Full build passes.
- Full CTest passes.
- Doxygen generation passes.
- `build/docs/doxygen/html/index.html` exists.
- Generated docs remain under the build tree only.
- `tools/docs/check_docs_consistency.py` passes.
- `tools/dev/check_target_boundaries.sh` passes.
- `docs_doxygen_generate`, `docs_consistency`, and `target_boundary` pass.
- Doxygen warning log is empty, or any remaining warnings are explicitly documented and accepted in `docs/ImplementationStatus.md`.

## Final Handoff Summary Template

Codex should end with:

```markdown
## Doxygen Sanitization Handoff

Branch: doxygen-gen

### Changes Made
- Fixed TextureUpload member docs:
- Hid or documented physics detail types:
- Fixed Markdown Doxygen references:
- Added archived handoff plan reference:
- Updated warning-count/status text:

### Validation
- cmake configure:
- full build:
- full ctest:
- stellar_docs_doxygen:
- docs_consistency:
- target_boundary:
- focused docs ctest:

### Doxygen Warning Log
- Warning log path:
- Final warning count:
- Remaining categories:
- Accepted remaining warnings, if any:

### Merge Readiness
- Ready / Not ready:
- Remaining blockers:
```

---

# Recommended Execution Order

1. DS-0 — Reproduce warning surface.
2. DS-1 — Fix `TextureUpload` public member docs.
3. DS-2 — Hide or document physics `detail` structs.
4. DS-3 — Fix Markdown anchor/reference warnings.
5. DS-4 — Add archived Doxygen plan reference.
6. DS-5 — Reconcile warning count/status text.
7. DS-6 — Final validation and handoff.

## Parallelization

DS-1 and DS-2 can be done in parallel after DS-0 because they touch different headers.

DS-3 should be done by one agent because Markdown warning fixes depend on the exact generated warning log.

DS-4 and DS-5 should be done after DS-1 through DS-3 so the status text reflects the final warning state.

Only use parallel subagents if the user explicitly requests subagents or parallel execution.

## Most Important Rule

Do not treat active Markdown as a documentation bug. This cleanup is about eliminating stale Doxygen warnings and making status text truthful, while preserving the intended split:

- Doxygen: generated public C++ API reference.
- Markdown: architecture, current status, authoring workflows, validation runbooks, and handoff notes.
