---
title: Codex Task Card — Final Audible Audio Smoke
branch: macos-compat
phase: AAS
---

# Codex Task Card — Final Audible Audio Smoke

## Mission

Close the final full macOS compatibility gap by adding/running an opt-in audible miniaudio smoke harness and recording evidence.

## Deliverables

1. `stellar-audio-smoke` executable or equivalent CLI.
2. No-device audio CTest still passing.
3. Audible smoke transcript with exit code 0.
4. Optional framework-link audit.
5. Updated status docs and acceptance matrix.

## Stop conditions

- No audio output device: skip/record, do not mark complete.
- `ma_engine_init` fails: record error, do not mark complete.
- `play_one_shot` returns diagnostics: fix assets/registry/sink first.
- Server links audio frameworks unexpectedly: stop and fix target graph.
