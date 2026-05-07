---
title: Codex Task Card — Metal Display Readback
branch: macos-compat
phase: FMP-5.1
---

## Scope

- Execute display-attached Metal render
- Read back frame buffer per material slot
- Compute per-channel histograms
- Compare against reference/OpenGL outputs
- Report pass/fail + debug logs

## Deliverables

- MetalRenderReadback.cpp (test harness)
- JSON/CSV histogram outputs
- Summary report for each fixture
- Debug logs for viewport, projection, depth

## Stop Conditions

- No attached display -> skip with code 77
- No Metal device -> skip with code 77
- Crash/hang in render -> record and halt
