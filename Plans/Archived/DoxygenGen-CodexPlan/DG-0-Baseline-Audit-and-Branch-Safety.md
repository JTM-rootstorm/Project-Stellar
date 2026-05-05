# Phase DG-0 — Baseline Audit and Branch Safety

## Objective

Establish the `doxygen-gen` baseline and prevent the implementation from accidentally becoming a Markdown migration or broad documentation rewrite.

## Codex Prompt

```text
You are working in JTM-rootstorm/Project-Stellar on branch doxygen-gen.

Do not commit or push.

Perform a baseline documentation audit for Doxygen integration only. Confirm:
- branch name and working tree status;
- whether Doxygen config files already exist;
- whether CMake already has a docs target or find_package(Doxygen);
- which active Markdown files should remain narrative docs;
- which public header directories should be the initial Doxygen API input.

Do not modify files in this phase unless explicitly asked. Produce a concise audit summary and recommended file touch list for DG-1.
```

## Commands

```bash
git status --short
git branch --show-current
find . -maxdepth 3 \( -iname 'Doxyfile*' -o -iname '*doxygen*' \) -print
grep -RIn "find_package *(Doxygen\|doxygen_add_docs\|Doxyfile\|GENERATE_HTML\|USE_MDFILE_AS_MAINPAGE" CMakeLists.txt cmake docs tests 2>/dev/null || true
find include/stellar -type f \( -name '*.hpp' -o -name '*.h' \) | sort
find docs -maxdepth 2 -type f -name '*.md' | sort
```

## Expected Findings

- No existing Doxygen config or target, unless branch-local work was added after this plan was written.
- Public headers under `include/stellar/**` are the primary API reference source.
- `README.md`, `docs/Design.md`, `docs/ImplementationStatus.md`, `docs/BspAuthoring.md`, `docs/TrenchBroom.md`, and similar narrative docs remain Markdown.
- Archived plans are excluded from generated docs unless specifically requested later.

## Acceptance Criteria

- Baseline is recorded in the response.
- No source files are changed.
- DG-1 file touch list is identified.

---
