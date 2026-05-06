---
title: Codex Agent Task Cards
branch: macos-compat
---

# Codex Agent Task Cards

## Card A — Build Gardener

Scope:
- MC-1 only.
- Fix CMake typo and Apple gating.
- Add `STELLAR_ENABLE_METAL` option but no Metal enum yet.

Output:
- Patch.
- Configure/build/test transcript.
- Note any Homebrew/pkg-config issues.

Stop if:
- Linux/default configure breaks.
- Metal enum is required to proceed.

## Card B — Socket Portability

Scope:
- MC-2 only.
- Replace Linux-only `MSG_NOSIGNAL` assumption.

Output:
- Patch.
- Focused transport/socket test transcript.
- Boundary check transcript.

Stop if:
- Transport starts linking non-protocol/client/platform targets.

## Card C — Audio Playback Lane

Scope:
- MC-3 only.
- Optional `MiniaudioRequestSink`.
- Keep default no-op path.

Output:
- Patch.
- Audio tests transcript.
- Manual smoke instructions.

Stop if:
- Audio becomes a server/authority dependency.
- Default tests require an audio device.

## Card D — Metal Scaffold

Scope:
- MC-5 only.
- Add backend enum/factory/window flags and clear-only Metal device initialization.

Output:
- Patch.
- `graphics_backend_selection` result.
- `metal_context_smoke` result or skip reason.
- `--validate-display --renderer metal` result or skip reason.

Stop if:
- No macOS machine/display is available for smoke; produce code + skip report, do not fake success.

## Card E — Metal Renderer

Scope:
- MC-6 and MC-7.
- Implement resources, frame lifecycle, shaders, materials.

Output:
- Patch series.
- Render/upload/inspection tests.
- Manual screenshot/visual notes if locally possible.

Stop if:
- Projection/depth mismatch is found. Record it and patch backend-specific projection before material work continues.

## Card F — Final Handoff

Scope:
- MC-8.
- Docs, status, runbooks, audits.

Output:
- Patch.
- Final validation transcript.
- Explicit unsupported/experimental state list.
