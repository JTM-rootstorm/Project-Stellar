---
project: Project Stellar
branch: macos-compat
plan_family: final_audible_audio_smoke
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-06
---

# Final Audible Audio Smoke Plan

## Goal

Prove that the presentation-only miniaudio sink can initialize a real macOS audio output device and audibly play generated footstep WAV assets without changing gameplay/server authority.

## Current evidence to assume

`docs/ImplementationStatus.md` currently records:

- Linux default preset passes 103/103 on a Linux host.
- macOS default, Metal, and Metal-only builds pass.
- display-attached Metal validation passed.
- Metal readback/reporting path passed.
- no-device miniaudio tests pass.
- framework-link validation passes.
- `stellar-server` does not link miniaudio/audio frameworks.
- remaining blocker: optional audible audio smoke.

## Definition of done

1. A macOS host with a real/default audio output device runs an audible smoke command.
2. The command initializes the miniaudio sink, requests at least two generated footstep sounds, keeps the process alive long enough to hear them, and reports success.
3. Operator confirms sound was heard using `STELLAR_AUDIO_SMOKE_CONFIRM=heard`.
4. Default CTest remains audio-device-free.
5. Server/headless binaries remain free of miniaudio/audio-framework dependencies.
6. Status docs and acceptance matrix are updated from active/incomplete to complete.

## Non-goals

- Do not add microphone capture.
- Do not make audio gameplay-authoritative.
- Do not require audio hardware in default CTest.
- Do not modify server, authority, protocol, collision, or scripting targets.
