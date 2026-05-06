---
phase: AAS-4
title: Final Completion Docs
depends_on: [AAS-2, AAS-3]
---

# AAS-4 — Final Completion Docs

## Goal

Mark full macOS/Linux parity complete only after audible smoke evidence exists.

## Files

```text
docs/ImplementationStatus.md
Plans/NEXT.md
Plans/ProjectStellar-full-macos-linux-parity-CodexPlan/12-FMP-Acceptance-Matrix.md
README.md
```

## Tasks

1. Mark FMP-7 audible smoke complete.
2. Add exact command/result evidence.
3. Change full macOS/Linux parity scope from active to complete.
4. Update acceptance matrix audible smoke row from `NOT_COVERED` to `PASS` for the executed build(s).
5. Run stale blocker audit:

```bash
git grep -n 'remaining active blockers\|audible audio smoke remains\|NOT_COVERED\|full parity.*active' -- docs Plans README.md
git diff --check
tools/dev/check_target_boundaries.sh .
```

## Acceptance criteria

- No stale docs claim audible smoke is untested.
- No stale docs claim full parity is active.
- Acceptance matrix has no required `NOT_COVERED` rows.
