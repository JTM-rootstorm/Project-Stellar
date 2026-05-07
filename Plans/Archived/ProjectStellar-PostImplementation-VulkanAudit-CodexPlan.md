# Project Stellar — Post-Implementation Vulkan Audit Codex Plan


---

# File: README.md

# Project Stellar — Post-Implementation Vulkan Audit Codex Plan

This plan pack captures the post-implementation audit findings for `JTM-rootstorm/Project-Stellar`
on branch `GL-to-vulkan`, including the user-provided note that manual Linux Vulkan validation had
already passed before the branch was pushed.

## Purpose

Use this pack as a small Codex-ready cleanup handoff. The implementation itself appears structurally
complete for the requested renderer matrix:

- Linux: Vulkan.
- macOS: Metal.
- No macOS Vulkan or MoltenVK lane.
- OpenGL retired from active code/build/runtime support.

The remaining work is documentation and handoff hygiene, not renderer implementation.

## Recommended execution order

1. Read `00-MASTER-PostImplementationAudit-CodexPlan.md`.
2. Apply `01-Record-Linux-Vulkan-Manual-Validation.md`.
3. Apply `02-Refresh-Codex-Agent-Guidance.md`.
4. Apply `03-README-Fallback-Command-Polish.md`.
5. Use `04-Final-Acceptance-Checklist.md` to verify the branch.

## Important user validation note

The user confirmed after the audit that manual Linux Vulkan validation had passed before the push.
The original audit saw only repository-recorded skip notes for display-attached Vulkan runs, so the
documentation should be corrected to reflect the actual manual validation evidence.

Exact command logs were not provided in this chat. Do not fabricate output. Either quote the user's
confirmation as operator-provided evidence, or, if local shell history/logs are available, paste the
exact command/results into `docs/ImplementationStatus.md`.


---

# File: 00-MASTER-PostImplementationAudit-CodexPlan.md

---
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
created_for: "Codex implementation follow-up"
status: "ready-for-small-cleanup"
scope: "post-implementation audit findings, documentation repair, validation handoff"
---

# MASTER — Post-Implementation Audit Cleanup For Linux Vulkan / macOS Metal

## 0. Mission

Clean up the final documentation and Codex guidance after the GL-to-Vulkan implementation.

The implementation audit found that the main architecture targets are in place:

- Vulkan is Linux-only.
- macOS remains Metal.
- macOS Vulkan and MoltenVK are not part of the renderer matrix.
- OpenGL has been retired from active implementation/build/runtime support.
- Vulkan backend, shaders, tests, presets, docs, and phased plan files were added.
- Metal remains available and validated on macOS.

The user later clarified that manual Linux Vulkan validation passed before the push, but the docs were
not updated beforehand. This cleanup should record that validation accurately.

## 1. Inputs From Audit

Observed implementation status:

- `CMakeLists.txt` defaults `STELLAR_ENABLE_VULKAN_BACKEND` to `ON` only on Linux and hard-errors if
  Vulkan is enabled on non-Linux platforms.
- `CMakePresets.json` defines Linux Vulkan presets and macOS Metal presets.
- `GraphicsBackend` exposes Vulkan and Metal only when compiled.
- Parser accepts `vulkan`, `vk`, and `Vulkan` only when Vulkan is compiled, rejects OpenGL aliases,
  and reports compiled backend diagnostics.
- `GraphicsDeviceFactory` constructs `vulkan::VulkanGraphicsDevice` and `metal::MetalGraphicsDevice`.
- Client window routing uses `SDL_WINDOW_VULKAN` for Vulkan and `SDL_WINDOW_METAL` for Metal.
- Vulkan shaders are built through `glslc` into generated SPIR-V files.
- `VulkanGraphicsDevice` implements `GraphicsDevice` and `FrameReadbackDevice`.
- Vulkan projection convention is `vulkan_ndc_z_zero_to_one`.
- Tests include `vulkan_shader_compile`, `vulkan_context_smoke`, and `vulkan_render_readback`, with
  display/GPU tests opt-in and skip-safe.
- `docs/ImplementationStatus.md`, `docs/Design.md`, and `README.md` largely reflect Linux Vulkan /
  macOS Metal.

Two issues remained from the audit:

1. Repository documentation recorded Linux display-attached Vulkan validation as skipped due missing
   display access. User now confirms manual Linux validation passed before the push.
2. Codex-facing guidance files still contain stale OpenGL-era phrasing.

## 2. User Validation Note To Preserve

The user stated:

> Manual Linux validation had passed prior to the push; the documentation was not updated beforehand.

Codex should update documentation to reflect this, without inventing exact logs.

Preferred documentation wording if exact logs are unavailable:

```markdown
Manual Linux Vulkan display validation was operator-confirmed before push. The follow-up audit initially
saw only the earlier headless-session skip notes; this section records the user-confirmed manual pass.
Exact terminal logs were not attached to this handoff.
```

If local logs or shell history are available, replace that sentence with the exact command list and
result summary.

## 3. Cleanup Phases

| Phase | Name | Purpose |
|---|---|---|
| PVA-1 | Record Linux manual validation | Correct `ImplementationStatus` to include user-confirmed Linux Vulkan manual pass. |
| PVA-2 | Refresh Codex guidance | Remove stale OpenGL parity language from `AGENTS.md` and `.codex/agents/miyamoto.toml`. |
| PVA-3 | Polish README fallback commands | Avoid ambiguous platform fallback build instructions. |
| PVA-4 | Final acceptance check | Run focused grep/build/doc checks and report final status. |

## 4. Non-Goals

Do not rewrite the Vulkan backend.

Do not reintroduce OpenGL.

Do not add macOS Vulkan or MoltenVK support.

Do not modify `.codex/agents/*.toml` beyond stale renderer matrix wording.

Do not fabricate validation logs. Record the user-provided manual validation note honestly.

## 5. Final Expected State

- `docs/ImplementationStatus.md` says Linux Vulkan manual display validation passed, based on
  user/operator confirmation or exact logs if available.
- `AGENTS.md` no longer says OpenGL/Vulkan parity is an active rule.
- `.codex/agents/miyamoto.toml` no longer lists `src/graphics/opengl/` as active or instructs agents
  to preserve OpenGL/Vulkan parity.
- README fallback commands are platform-specific.
- Active code/build docs remain Linux Vulkan plus macOS Metal.


---

# File: 01-Record-Linux-Vulkan-Manual-Validation.md

---
phase: "PVA-1"
title: "Record Linux Vulkan Manual Validation"
owner: "Codex / @director"
risk: "low"
---

# PVA-1 — Record Linux Vulkan Manual Validation

## Goal

Update the branch-facing status documentation so it no longer implies Linux Vulkan display-attached
validation is still unproven.

The user confirmed that manual Linux validation passed before the push. The earlier audit only saw
headless-session skip notes in the repository docs, so the status file needs a correction.

## Files

Primary:

- `docs/ImplementationStatus.md`

Optional:

- `Plans/NEXT.md`
- `Plans/ProjectStellar-GL-to-Vulkan-LinuxOnly-CodexPlan/README.md`

## Required Change

Find the VK-7, VK-8, or VK-9 validation sections that mention:

- `STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 ... skipped`
- `x11 not available`
- display access unavailable
- Linux Vulkan display validation not proven

Keep those earlier skip notes if they are historically useful, but add a later correction block.

## Suggested Wording

```markdown
### Linux Vulkan Manual Display Validation Correction

Status: operator-confirmed passed before push.

After the automated/headless validation notes above were written, manual Linux Vulkan validation was
run in a display-capable Linux session and passed before the branch was pushed. The post-implementation
audit initially saw only the earlier headless-session skip notes; this entry records the corrected
manual validation state.

Validated target:
- Linux Vulkan display/context path.
- Linux Vulkan readback/display validation path.
- Linux Vulkan default renderer behavior.

Evidence source:
- User/operator confirmation in post-implementation audit follow-up.
- Exact terminal logs were not attached to this handoff. If local logs are available, replace this
  paragraph with exact commands and results.
```

If exact commands/results are available from local history, prefer:

```markdown
Validation results:
- `<exact command>`: passed.
- `<exact command>`: passed.
- `<exact command>`: passed.
```

## Acceptance Criteria

- `docs/ImplementationStatus.md` no longer leaves Linux display-attached Vulkan validation as merely
  skipped/unproven.
- The note is honest about evidence provenance.
- The note does not fabricate terminal output.
- VK-9 final status still says no remaining GL-to-Vulkan follow-up.


---

# File: 02-Refresh-Codex-Agent-Guidance.md

---
phase: "PVA-2"
title: "Refresh Codex Agent Guidance"
owner: "Codex / @director"
risk: "low"
---

# PVA-2 — Refresh Codex Agent Guidance

## Goal

Remove stale OpenGL-era instructions from Codex-facing guidance so future Codex tasks do not preserve
a renderer parity target that no longer exists.

## Files

- `AGENTS.md`
- `.codex/agents/miyamoto.toml`

## Findings To Fix

The audit found stale guidance such as:

- `OpenGL 4.5+ / Vulkan 1.2+ through a shared graphics abstraction`
- `OpenGL and Vulkan behavior should remain aligned through backend-neutral abstractions`
- `src/graphics/opengl/`
- `OpenGL/Vulkan rendering`
- `Do not introduce Vulkan-only or OpenGL-only material semantics without documenting parity impact`

These no longer match the branch state because OpenGL is retired.

## Replacement Direction

Use this renderer matrix consistently:

```text
Rendering matrix: Vulkan on Linux, Metal on macOS. OpenGL is retired from active support.
macOS Vulkan and MoltenVK are out of scope.
```

## Suggested `AGENTS.md` Updates

Replace tech stack bullet:

```markdown
- Vulkan on Linux and Metal on macOS through a shared graphics abstraction
```

Replace non-negotiable rendering rule:

```markdown
- Vulkan and Metal behavior should remain aligned through backend-neutral graphics contracts where
  both backends implement the same presentation feature.
```

Replace validation bullet:

```markdown
- Default validation should remain display-free; GPU/display Vulkan and Metal context tests must be
  opt-in.
```

Update routing text:

```markdown
| Vulkan/Metal rendering | `@miyamoto` | `@director` | Preserve shared graphics abstraction |
```

## Suggested `.codex/agents/miyamoto.toml` Updates

Change description:

```toml
description = "Graphics subsystem specialist for rendering abstraction, Linux Vulkan, macOS Metal, BSP rendering, sprites, shaders, textures, and materials."
```

Primary areas should remove OpenGL:

```toml
- `src/graphics/vulkan/`
- `src/graphics/metal/`
- `assets/shaders/`
- `tests/graphics/`
```

Replace parity responsibility:

```text
- Preserve Vulkan/Metal behavior through shared public graphics contracts where both backends support
  the feature.
```

Replace material warning:

```text
- Do not introduce backend-only material semantics without documenting parity impact and fallback
  behavior.
```

## Acceptance Criteria

- No active Codex guidance presents OpenGL as an active backend.
- No active Codex guidance lists `src/graphics/opengl/` as an active work area.
- Future Codex tasks are steered toward Linux Vulkan and macOS Metal.


---

# File: 03-README-Fallback-Command-Polish.md

---
phase: "PVA-3"
title: "README Fallback Command Polish"
owner: "Codex / @director"
risk: "low"
---

# PVA-3 — README Fallback Command Polish

## Goal

Make README fallback build commands unambiguous for Linux and macOS.

## Finding

The README now correctly recommends:

- Linux default: `linux-vulkan-only`
- macOS default: `macos-metal-only`

However, its generic fallback manual build command may be ambiguous because default options are
platform-dependent and macOS requires Metal to be enabled explicitly when bypassing presets.

## File

- `README.md`

## Suggested Replacement

Replace the generic fallback section with platform-specific commands:

~~~markdown
Fallback Linux manual build and tests:

```bash
cmake -S . -B build-linux-manual -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_VULKAN_BACKEND=ON
cmake --build build-linux-manual -j$(nproc)
ctest --test-dir build-linux-manual --output-on-failure
```

Fallback macOS manual build and tests:

```bash
cmake -S . -B build-macos-manual -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON -DSTELLAR_ENABLE_VULKAN_BACKEND=OFF
cmake --build build-macos-manual -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-manual --output-on-failure
```
~~~

## Acceptance Criteria

- README has no ambiguous manual fallback command.
- Linux fallback clearly enables Vulkan.
- macOS fallback clearly enables Metal and disables Vulkan.
- No MoltenVK or macOS Vulkan setup is introduced.


---

# File: 04-Final-Acceptance-Checklist.md

---
phase: "PVA-4"
title: "Final Acceptance Checklist"
owner: "Codex / @director"
risk: "low"
---

# PVA-4 — Final Acceptance Checklist

Use this after applying PVA-1 through PVA-3.

## Documentation Greps

```bash
git grep -n -i \
  -e 'OpenGL 4.5' \
  -e 'OpenGL/Vulkan' \
  -e 'OpenGL and Vulkan' \
  -e 'src/graphics/opengl' \
  -- AGENTS.md .codex docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected:

- No active Codex guidance treats OpenGL as active.
- Historical mentions are allowed only when clearly described as retired/completed history.

```bash
git grep -n -i \
  -e 'MoltenVK' \
  -e 'macOS Vulkan' \
  -e 'VK_KHR_portability' \
  -- CMakeLists.txt CMakePresets.json include src tests docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected:

- No implementation path for macOS Vulkan/MoltenVK.
- Documentation may mention macOS Vulkan/MoltenVK only as explicitly out of scope.

## Focused Build/Test Checks

Linux:

```bash
cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only --parallel $(nproc)
ctest --preset linux-vulkan-only --output-on-failure
tools/dev/check_target_boundaries.sh
```

macOS:

```bash
cmake --preset macos-metal-only
cmake --build --preset macos-metal-only --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal-only --output-on-failure
```

Optional display/GPU checks, when environment supports them:

```bash
STELLAR_RUN_VULKAN_CONTEXT_TESTS=1 \
ctest --test-dir build-linux-vulkan -R '^vulkan_(context_smoke|render_readback)$' --output-on-failure

build-linux-vulkan/stellar-client --validate-display --renderer vulkan
```

macOS Metal:

```bash
STELLAR_RUN_METAL_CONTEXT_TESTS=1 \
ctest --test-dir build-macos-metal-only -R '^metal_(context_smoke|render_readback)$' --output-on-failure

build-macos-metal-only/stellar-client --validate-display --renderer metal
```

## Final Report Template

```markdown
## Post-Implementation Audit Cleanup Complete

Branch: GL-to-vulkan
Date: YYYY-MM-DD

Summary:
- Recorded user/operator-confirmed Linux Vulkan manual validation pass.
- Refreshed Codex guidance to reflect Linux Vulkan + macOS Metal.
- Removed stale active OpenGL parity guidance.
- Clarified README fallback build commands by platform.

Validation:
- Documentation grep for stale OpenGL guidance: <pass/fail>
- macOS Vulkan/MoltenVK implementation grep: <pass/fail>
- `cmake --preset linux-vulkan-only`: <pass/fail/not run>
- `cmake --build --preset linux-vulkan-only --parallel $(nproc)`: <pass/fail/not run>
- `ctest --preset linux-vulkan-only --output-on-failure`: <pass/fail/not run>
- `tools/dev/check_target_boundaries.sh`: <pass/fail/not run>
- macOS Metal validation: <pass/fail/not run>

Remaining follow-up:
- None, unless exact Linux manual validation logs are still desired for archival precision.
```
