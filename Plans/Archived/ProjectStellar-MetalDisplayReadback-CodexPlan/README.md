# Project Stellar — macOS Metal Display Readback / Pixel Parity Codex Plan

Generated: 2026-05-06

## Purpose

This agent plan is optimized for Codex to execute **display-attached Metal validation** and **readback/pixel parity checks**.

It assumes:
- `macos-compat` branch.
- Metal backend is built and Apple-gated.
- OpenGL material parity exists conceptually.

## Outputs

- Pixel/sample histograms.
- Pass/fail report per material slot.
- Debug logs for projection, depth, viewport, and drawable.
- Optional tolerance thresholds for minor color differences.
