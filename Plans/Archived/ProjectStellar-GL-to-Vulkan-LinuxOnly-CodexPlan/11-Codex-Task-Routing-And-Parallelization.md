---
title: "Codex Task Routing And Parallelization"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
scope: "Linux Vulkan renderer replacement; macOS Metal preserved"
status: "ready"
---

# Codex Task Routing And Parallelization

## Role Defaults

Use `@director` for:

- phase coordination
- docs/status truth
- architecture guardrails
- final integration review

Use `@carmack` for:

- CMake
- presets
- target boundaries
- dependency gates
- platform build rules

Use `@miyamoto` for:

- Vulkan backend
- shaders
- render path
- material parity
- frame readback
- graphics tests

## Recommended Work Packets

| Packet | Phase | Role | Parallel? | Notes |
|---|---|---|---:|---|
| Baseline audit | VK-0 | `@director` | No | Must happen first. |
| Linux CMake gates | VK-1 | `@carmack` | After VK-0 | Can run with docs draft. |
| Preset updates | VK-1 | `@carmack` | With CMake gates | Keep macOS Vulkan-free. |
| Backend enum/parser/factory | VK-2 | `@miyamoto` | After VK-1 | Coordinate with tests. |
| CLI/window routing | VK-2 | `@miyamoto` | With parser | Keep Metal routing intact. |
| Vulkan clear/present scaffold | VK-3 | `@miyamoto` | No | Core bring-up. |
| SPIR-V CMake/shaders | VK-4 | `@miyamoto` | Late VK-3 | Agree uniform ABI first. |
| Mesh/texture uploads | VK-5 | `@miyamoto` | After VK-3 | Can split mesh vs texture internally. |
| Descriptor/material upload | VK-5 | `@miyamoto` | With upload | Needs shader ABI. |
| Draw/material parity | VK-6 | `@miyamoto` | After VK-5 | Hardest graphics slice. |
| Readback | VK-7 | `@miyamoto` | After VK-6 | Depends on frame lifecycle. |
| Tests matrix | VK-8 | `@director` + `@miyamoto` | Incremental | Add tests as features land. |
| Docs/final handoff | VK-9 | `@director` | Last | Must match implementation. |
| OpenGL retirement | VK-9 | `@carmack` + `@miyamoto` | Last | Only after Vulkan parity. |

## Parallelization Rules

Can run in parallel:

- VK-1 CMake presets and VK-0 documentation inventory after baseline commands finish.
- VK-4 shader file drafting while VK-3 scaffold is underway, once descriptor/uniform layout is agreed.
- VK-5 mesh upload and texture upload if both use shared memory helper decisions.
- VK-8 test registration can grow as each feature lands.
- Documentation updates can be drafted early, but final wording must wait for validation.

Do not run in parallel:

- OpenGL retirement with Vulkan bring-up.
- Factory/parser changes with CMake gates unless compile definitions are agreed.
- Readback before stable frame lifecycle.
- Final docs before final validation.
- macOS renderer changes with Linux Vulkan work unless fixing a regression.

## Codex Prompt Template

```text
You are working in JTM-rootstorm/Project-Stellar on branch GL-to-vulkan.
Read Plans/GLToVulkanLinuxOnly-CodexPlan/00-MASTER-GLToVulkanLinuxOnly-CodexPlan.md and the current phase file.
Implement only the current phase.
Vulkan is Linux-only. macOS remains Metal. Do not add MoltenVK or macOS Vulkan support.
Do not commit unless explicitly asked.
Run the narrowest relevant validation and report results.
```

## Phase Completion Report Template

```markdown
## Phase <ID> Completion

Branch: GL-to-vulkan
Commit: <sha or "not committed">

Changed files:
- ...

Summary:
- ...

Validation:
- `<command>`: <pass/fail/not run with reason>

Guardrail checks:
- Linux Vulkan-only scope preserved: yes/no
- macOS Metal preserved: yes/no
- No MoltenVK/macOS Vulkan added: yes/no
- Server/gameplay/audio boundaries preserved: yes/no

Risks:
- ...

Next recommended phase:
- ...
```
