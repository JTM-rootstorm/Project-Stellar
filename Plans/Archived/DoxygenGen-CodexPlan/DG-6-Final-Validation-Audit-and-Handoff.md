# Phase DG-6 — Final Validation, Audit, and Handoff

## Objective

Close the `doxygen-gen` branch work with clear validation evidence and no hidden docs artifacts in the source tree.

## Commands

```bash
git status --short

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

cmake --build build --target stellar_docs_doxygen
test -f build/docs/doxygen/html/index.html

python3 tools/docs/check_docs_consistency.py

find . -path './build' -prune -o \
    \( -path './docs/html' -o -path './html' -o -name 'doxygen-warnings.log' \) -print
```

Optional focused checks:

```bash
ctest --test-dir build -R '^(docs_consistency|docs_doxygen_generate|target_boundary)$' --output-on-failure
tools/dev/check_target_boundaries.sh
```

## Acceptance Criteria

- Full build passes.
- Full CTest passes.
- Doxygen generation passes.
- Generated HTML exists only under the build tree.
- No generated Doxygen output is accidentally staged.
- Markdown docs consistency still passes.
- Target boundary check still passes.
- Handoff summary states:
  - files changed;
  - docs target name;
  - where generated output lands;
  - whether Doxygen warnings remain;
  - which Markdown pages are included;
  - which archives are excluded;
  - validation commands and results.

## Final Codex Handoff Template

```markdown
## Doxygen Generation Handoff

Branch: doxygen-gen

### Summary
- Added Doxygen config template at:
- Added CMake target:
- Generated output path:
- Markdown pages included:
- Excluded stale/archive paths:

### Validation
- cmake configure:
- full build:
- full ctest:
- docs target:
- docs consistency:
- target boundary:

### Remaining Warnings
- Doxygen warning log:
- Warning count:
- Main warning categories:
- Recommended next cleanup:

### Notes
- Markdown remains the architecture/status/authoring layer.
- Doxygen is the generated public API reference layer.
- No generated docs are committed.
```

---

# Recommended Phase Order

1. DG-0 — Baseline audit.
2. DG-1 — Add Doxyfile/CMake generation.
3. DG-2 — Add landing page and Markdown inclusion policy.
4. DG-3 — Audit/fill public header `@brief` comments.
5. DG-4 — Add optional docs validation gate.
6. DG-5 — Clarify policy in active docs.
7. DG-6 — Final validation and handoff.

## Safe Parallel Work

After DG-1 is complete, DG-2 and DG-3 can overlap if they avoid the same files. DG-3 can be split by `include/stellar` subdirectory. DG-4 should wait until DG-1 is stable. DG-5 should wait until the target name and output path are finalized.

## Most Important Recommendation

Do not treat Markdown as a bug. The fix is to add Doxygen generation and API-reference hygiene while preserving Markdown as the project’s narrative documentation layer.
