---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
status: active
source_bundle: Plans/audio_footsteps_plan/00-MASTER-AudioFootsteps-AgentPlan.md
---

# Audio Footsteps Active Plan

`audio-impl` is currently focused on texture/material-dependent footstep audio. The active handoff
bundle is:

- `Plans/audio_footsteps_plan/00-MASTER-AudioFootsteps-AgentPlan.md`

The implementation path is:

1. AF-0 - Branch/docs guardrails.
2. AF-1 - Collision surface identity and footstep surface resolver.
3. AF-2 - Authoritative footstep cadence and `GameplayEventKind::kFootstep`.
4. AF-3 - Presentation audio routing and generated retro footstep sounds.
5. AF-4 - End-to-end display-free hardening and documentation.

Footsteps are server-approved presentation events. BSP source texture names feed a small
authoritative surface-audio resolver; `.stellar_material` sidecars remain presentation-only render
data and are not gameplay truth.

Non-goals for this slice:

- No full material gameplay system.
- No client-side footstep authority.
- No prediction/reconciliation.
- No broad spatial audio engine.
- No dynamic terrain/material decals.

