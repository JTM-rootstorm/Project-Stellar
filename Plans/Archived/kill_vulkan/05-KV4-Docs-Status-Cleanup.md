---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-4 — Active Documentation and Status Cleanup

## Goal

Make active documentation match the new renderer reality:

- OpenGL is the current supported renderer.
- Vulkan is intentionally removed from active support.
- The architecture remains friendly to future DirectX/Metal backends when explicitly scoped.

## Files To Inspect/Edit

Primary:

- `README.md`
- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/NEXT.md`

Also inspect active docs found by grep:

```bash
git grep -n -i 'OpenGL/Vulkan\|Vulkan\|renderer parity\|graphics backend' -- docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Do not broadly rewrite archived historical plans under `Plans/Archived/**`. They are allowed to preserve history.

## Required Wording Changes

### 1. README

Current README points users to implementation status “especially BSP/OpenGL/Vulkan support.”

Change to current wording such as:

```markdown
For current branch implementation status, especially BSP/OpenGL rendering support, see `docs/ImplementationStatus.md`.
```

### 2. `docs/Design.md`

Update high-level technical stack and graphics subsystem sections:

Replace current-support wording like:

```markdown
Rendering: OpenGL 4.5+ and Vulkan 1.2+, selected at runtime.
Backend parity: OpenGL and Vulkan remain runtime-selectable...
```

with current truthful wording:

```markdown
Rendering: OpenGL 4.5+ is the current supported renderer.
Graphics remain behind a backend-neutral abstraction so future native platform backends, such as DirectX/Direct3D on Windows or Metal on macOS, can be added when explicitly scoped.
```

In graphics subsystem:

- Remove statements that Vulkan is currently runtime-selectable.
- Remove “OpenGL/Vulkan parity” as an active requirement.
- Preserve “display-free default validation.”
- Preserve “backend-neutral resource contracts.”
- Add a short “Future native backend direction” paragraph:
  - DirectX/Metal are future candidates.
  - Do not add enum values or CLI aliases until implementation exists.
  - New backends must use the existing `GraphicsDevice` abstraction and display-free tests.

### 3. `docs/ImplementationStatus.md`

Update completed-scope summaries that currently say Vulkan consumes material contracts or OpenGL/Vulkan context smoke tests are opt-in.

Recommended wording:

```markdown
OpenGL consumes the current BSP material contract. Vulkan support was intentionally removed on the kill-vulkan branch to reduce active maintenance burden. Future DirectX/Metal support remains a separate, explicitly scoped backend addition through the existing graphics abstraction.
```

For historical completed phases that mention Vulkan material parity:

- Keep historical notes if needed, but add a clear current-status sentence near the top.
- Do not pretend SNT-7 never happened; it is simply no longer active support.

### 4. `Plans/NEXT.md`

If it mentions Vulkan as current or near-term parity, rewrite it to:

- OpenGL renderer hardening/polish.
- Future native backend exploration for Windows/macOS as a separate planning item.
- No active Vulkan work.

### 5. Active docs grep gate

After edits:

```bash
git grep -n -i 'vulkan' -- docs README.md Plans/NEXT.md ':!Plans/Archived/**'
```

Expected:

- Either no active matches, or only explicit current-status notes saying Vulkan was removed.
- No active docs should instruct users to run `--renderer vulkan`.
- No active docs should list Vulkan SDK as a requirement.
- No active docs should describe OpenGL/Vulkan parity as the current target.

## Acceptance Criteria

- Active docs/status/readme no longer advertise Vulkan as supported.
- Docs preserve future DirectX/Metal possibility without promising implementation.
- Archived historical plans remain untouched unless an active index required a tiny clarification.
- Build/test instructions no longer mention Vulkan context smoke tests.
