# Project Stellar GL-to-Vulkan Linux-Only Codex Plan

Repository: `JTM-rootstorm/Project-Stellar`  
Target branch: `GL-to-vulkan`  
Created: `2026-05-06`  
Package intent: Codex-ready phased implementation plan files.

## Direction Change

Vulkan is now scoped to **Linux only**.

macOS remains on **Metal**, and this plan explicitly rejects MoltenVK/macOS Vulkan work for this migration. The renderer direction is:

- Linux: move from OpenGL to Vulkan.
- macOS: keep Metal as the supported native renderer.
- OpenGL: keep only as temporary migration fallback, then retire when Vulkan-on-Linux and Metal-on-macOS parity is validated.
- Server, gameplay, Lua, collision, networking, audio, and BSP import remain backend-neutral.

## Suggested Codex Use

Give Codex the master plan first:

```text
Read Plans/GLToVulkanLinuxOnly-CodexPlan/00-MASTER-GLToVulkanLinuxOnly-CodexPlan.md.
Implement the next incomplete phase only. Do not commit unless explicitly asked.
```

Then feed one phase file at a time, starting at `01-Phase-VK0-Baseline-Branch-Truth.md`.

## File Index

1. `00-MASTER-GLToVulkanLinuxOnly-CodexPlan.md`
2. `01-Phase-VK0-Baseline-Branch-Truth.md`
3. `02-Phase-VK1-Linux-Vulkan-Build-Dependency-Presets.md`
4. `03-Phase-VK2-Backend-Selection-Platform-Routing.md`
5. `04-Phase-VK3-Linux-Vulkan-Device-Scaffold.md`
6. `05-Phase-VK4-SPIRV-Shader-Pipeline.md`
7. `06-Phase-VK5-Resource-Upload-Descriptors.md`
8. `07-Phase-VK6-Draw-Path-Material-Parity.md`
9. `08-Phase-VK7-Frame-Readback.md`
10. `09-Phase-VK8-Tests-Validation-Matrix.md`
11. `10-Phase-VK9-Docs-Handoff-OpenGL-Retirement.md`
12. `11-Codex-Task-Routing-And-Parallelization.md`

## Non-Goals

- No Vulkan on macOS.
- No MoltenVK path.
- No Vulkan SDK requirement for macOS builds.
- No server-side renderer dependencies.
- No client-side gameplay authority changes.
- No full PBR expansion.
- No third-party physics, Source/VBSP support, or renderer-driven gameplay state.
