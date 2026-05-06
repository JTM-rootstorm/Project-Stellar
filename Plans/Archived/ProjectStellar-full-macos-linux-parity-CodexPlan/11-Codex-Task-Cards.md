---
title: Codex Task Cards
branch: macos-compat
plan_family: full_macos_linux_parity
---

# Codex Task Cards

## Card A — Build Matrix Owner

Phases: FMP-0, FMP-1.

Deliver:
- Acceptance matrix.
- CMake/CI/preset changes.
- Linux + macOS build transcripts.

Stop if:
- macOS Metal-only cannot build due shared OpenGL dependency; record exact target causing it.

## Card B — Backend Selection Owner

Phase: FMP-2.

Deliver:
- Deterministic backend defaults.
- Parser/factory tests.
- CLI validation tests.

## Card C — Metal Frame Contract Owner

Phase: FMP-3.

Deliver:
- Correct projection/depth/drawable behavior.
- Retina/HiDPI handling.
- Debug diagnostics.

## Card D — Metal Material Owner

Phase: FMP-4.

Deliver:
- Full MSL material parity.
- Texture slot contract.
- Shader/material tests.

Stop if:
- Backend-neutral material data is insufficient. Document exactly which field is missing before editing asset contracts.

## Card E — Render Parity Test Owner

Phase: FMP-5.

Deliver:
- Fixture matrix.
- Display-free material coverage.
- Optional Metal readback/smoke coverage.

## Card F — Runtime Owner

Phase: FMP-6.

Deliver:
- macOS client/server smoke scripts.
- Runtime mode validation.
- Socket/input/asset-path fixes.

## Card G — Audio Owner

Phase: FMP-7.

Deliver:
- macOS miniaudio parity.
- No-device tests.
- Optional audible smoke.

## Card H — Tooling Owner

Phase: FMP-8.

Deliver:
- Portable shell/Python tooling.
- TrenchBroom/BSP macOS notes.
- Optional external tool skip behavior.

## Card I — Handoff Owner

Phase: FMP-9.

Deliver:
- Docs/status/NEXT update.
- Final validation transcript.
- No stale follow-up language for completed parity.
