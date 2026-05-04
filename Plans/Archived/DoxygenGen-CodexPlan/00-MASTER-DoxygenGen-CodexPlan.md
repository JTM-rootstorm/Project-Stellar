# Project Stellar — Doxygen Generation Integration Plan for Codex

Target branch: `doxygen-gen`  
Repository: `JTM-rootstorm/Project-Stellar`  
Plan purpose: implement Doxygen generation without replacing the active Markdown documentation model.

## Baseline Findings

The current documentation model is split across two intended layers:

1. Public API documentation belongs in Doxygen-style comments near code.
2. Architecture, implementation status, authoring workflows, validation runbooks, and historical plans belong in Markdown.

This split should be preserved. The problem is not that Markdown exists. The problem is that the repository currently states that public APIs require Doxygen `@brief` documentation, but the `doxygen-gen` branch does not yet have an actual Doxygen build/configuration path.

Observed branch state:

- `doxygen-gen` exists.
- Root `CMakeLists.txt` does not currently find Doxygen or define a Doxygen docs target.
- No root `Doxyfile` was found.
- No `docs/Doxyfile.in` was found.
- Active Markdown docs are part of the project workflow and should not be migrated wholesale into C++ comments.
- `tools/docs/check_docs_consistency.py` and CTest `docs_consistency` already protect some active Markdown content.
- Public headers already contain some Doxygen-style `@brief` comments, so this work should wire generation and fill gaps, not rewrite the entire documentation system.

## Non-Goals

Do not:

- Delete or migrate active Markdown docs wholesale into Doxygen comments.
- Move `docs/Design.md`, `docs/ImplementationStatus.md`, `docs/BspAuthoring.md`, or `docs/TrenchBroom.md` into headers.
- Include `Plans/Archived/**`, `.kilo/**`, or stale historical handoff packages in generated Doxygen output by default.
- Make Doxygen a hard build dependency for normal compile/test workflows.
- Add broad TODO comments to source.
- Reopen completed historical implementation phases.
- Commit, push, create a PR, or alter branch state unless explicitly instructed.

## Desired End State

The `doxygen-gen` branch should end with:

- A Doxygen configuration template committed under `docs/`.
- A CMake docs target that can generate HTML API docs when Doxygen is installed.
- Generated docs output written only under the build tree, not the source tree.
- Selected Markdown pages included in the Doxygen site as narrative pages.
- Archived plans and agent/runtime prompt files excluded from generated API docs.
- Public API headers under `include/stellar/**` treated as the primary Doxygen source.
- Active Markdown consistency checks preserved.
- Implementation docs updated to clarify that Markdown remains the architecture/status/authoring layer while Doxygen is the API-reference layer.
- Display-free default validation preserved.

---

---

# Master Index

This folder splits the `doxygen-gen` implementation plan into separate Codex-ready phase files.

## Phase Files

- [DG-0 — Baseline Audit and Branch Safety](DG-0-Baseline-Audit-and-Branch-Safety.md)
- [DG-1 — Add Doxygen Configuration and CMake Docs Target](DG-1-Add-Doxygen-Configuration-and-CMake-Docs-Target.md)
- [DG-2 — Doxygen Site Structure and Markdown Inclusion](DG-2-Doxygen-Site-Structure-and-Markdown-Inclusion.md)
- [DG-3 — Public Header Documentation Audit and Minimal Gap Fill](DG-3-Public-Header-Documentation-Audit-and-Minimal-Gap-Fill.md)
- [DG-4 — Documentation Validation and Warning Policy](DG-4-Documentation-Validation-and-Warning-Policy.md)
- [DG-5 — Clarify Documentation Policy in Active Docs](DG-5-Clarify-Documentation-Policy-in-Active-Docs.md)
- [DG-6 — Final Validation, Audit, and Handoff](DG-6-Final-Validation-Audit-and-Handoff.md)

## Recommended Execution Order

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
