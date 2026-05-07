---
phase: AAS-0
title: Evidence Audit and Command Selection
depends_on: []
---

# AAS-0 — Evidence Audit and Command Selection

## Goal

Confirm audible audio smoke is the only remaining full-parity blocker.

## Tasks

```bash
git branch --show-current
git status --short
git grep -n 'audible\|audio smoke\|full macOS' -- docs Plans README.md
ctest --test-dir build-macos-metal -R '^(audio_event_router|miniaudio_sink|generated_footstep_assets|footstep_audio_pipeline)$' --output-on-failure
```

Choose one implementation:

- Preferred: standalone `stellar-audio-smoke`.
- Alternative: `stellar-client --audio-smoke`.

## Acceptance criteria

- Remaining blocker is audible playback only.
- No renderer/runtime parity gaps are reopened.
