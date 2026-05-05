# Phase DG-2 — Doxygen Site Structure and Markdown Inclusion

## Objective

Make the generated docs usable without turning Doxygen into the source of truth for design/status docs.

## Recommended Files to Add/Modify

Optional add:

- `docs/DoxygenMainPage.md`

Modify:

- `docs/Doxyfile.in`
- Possibly `CMakeLists.txt` if using a new main page path.

## Implementation Requirements

Codex should choose one of these two approaches:

### Option A — README as Main Page

Use `README.md` as `USE_MDFILE_AS_MAINPAGE`.

Pros:

- Minimal new files.
- Root project intro remains the generated docs landing page.

Cons:

- README is intentionally short and may not orient API readers enough.

### Option B — Add `docs/DoxygenMainPage.md`

Create a focused landing page for generated docs:

```markdown
# Stellar Engine API Reference

This generated site documents public C++ APIs under `include/stellar`.

For current implementation status, see `docs/ImplementationStatus.md`.

For broad architecture and long-term direction, see `docs/Design.md`.

For BSP and TrenchBroom authoring workflows, see:
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`

Historical plans under `Plans/Archived/` are intentionally excluded from generated API docs.
```

Then set:

```ini
USE_MDFILE_AS_MAINPAGE = @STELLAR_DOXYGEN_MAINPAGE@
```

with `STELLAR_DOXYGEN_MAINPAGE` pointing to `docs/DoxygenMainPage.md`.

Preferred choice: **Option B**, because it cleanly explains the Doxygen/Markdown split.

## Markdown Inclusion Policy

Include these narrative Markdown files in Doxygen:

- `README.md`
- `docs/DoxygenMainPage.md`, if added
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`

Exclude by default:

- `Plans/Archived/**`
- `.kilo/**`
- `.codex/**`
- `AGENTS.md`
- agent role profiles
- old branch handoff docs unless explicitly reactivated

Do not duplicate Markdown content into source comments.

## Validation Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
grep -R "Plans/Archived" build/docs/doxygen/html 2>/dev/null && exit 1 || true
test -f build/docs/doxygen/html/index.html
```

## Acceptance Criteria

- Generated site has a clear landing page.
- API docs are discoverable.
- Active narrative docs are linked or included.
- Archived plans are not prominent in generated API output.
- No Markdown content is rewritten into headers just to satisfy Doxygen.

## Parallelization

Can run in parallel with DG-3 after DG-1 exists, but avoid simultaneous edits to `docs/Doxyfile.in`.

---
