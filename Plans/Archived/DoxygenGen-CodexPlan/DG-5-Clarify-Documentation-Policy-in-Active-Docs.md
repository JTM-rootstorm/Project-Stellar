# Phase DG-5 — Clarify Documentation Policy in Active Docs

## Objective

Prevent future agents from trying to “fix” Markdown by moving architecture/status/authoring docs into Doxygen comments.

## Recommended Files to Modify

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `README.md`
- Possibly `AGENTS.md`, only if the user explicitly allows edits to agent guidance.

## Implementation Requirements

Add a short policy note:

```markdown
## Documentation Model

Generated Doxygen documentation is the public C++ API reference for headers under `include/stellar/**`.
Active Markdown files remain the source for architecture, current implementation status, authoring
workflows, validation runbooks, and implementation handoff notes.

Do not migrate active Markdown docs into C++ comments. Instead, include selected Markdown pages in the
generated Doxygen site and keep API details documented near public declarations.
```

Keep it short. Avoid large rewrites.

## Placement Suggestions

- In `docs/Design.md`, add this under the existing documentation standards/source-of-truth section.
- In `docs/ImplementationStatus.md`, add a small current-scope note once Doxygen generation is complete.
- In `README.md`, mention the generated docs target once implemented:

```markdown
To generate public API reference docs when Doxygen is installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
```
```

## Validation Commands

```bash
python3 tools/docs/check_docs_consistency.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
ctest --test-dir build -R '^(docs_consistency|docs_doxygen_generate)$' --output-on-failure
```

## Acceptance Criteria

- Active docs clearly state the Markdown/Doxygen split.
- README contains the docs generation command only after the target exists.
- Existing docs consistency checks still pass.
- No historical plans are reactivated.

## Parallelization

Can be done in parallel with DG-3 if agents coordinate to avoid editing the same docs files.

---
