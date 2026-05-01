# Project Stellar — Active BSP Presentation and Networking Polish Plan

Branch target: `bsp-gameplay-loop`

This is the concise active handoff for presentation and networking polish over the completed BSP
gameplay loop. Use it together with `docs/ImplementationStatus.md` and the detailed plan at
`Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md`.

Do not commit directly unless explicitly instructed by the user. For this implementation run, the
user requested a commit after each completed phase. Do not push or pull.

## Completed historical scope

The previous BSP gameplay-loop plan package is complete and archived at
`Plans/Archived/bsp_gameplay_loop/`. Do not restart completed collision, movement, trigger,
object-collider, Lua scripting, BSP canonical migration, BSP rendering, BSP hardening, or BSP
gameplay-loop foundation work.

## Active scope

Presentation and networking polish over the completed BSP gameplay loop while preserving server
authority, mandatory sandboxed Lua scripting, display-free default tests, and runtime-selectable
OpenGL/Vulkan presentation.

## Phase order

```text
Phase PN-0 — Plan archival and follow-up handoff
Phase PN-1 — Live scripted authoritative runtime integration
Phase PN-2 — Authoritative gameplay snapshot presentation
Phase PN-3 — Snapshot/delta/event transport contracts
Phase PN-4 — Local/remote transport bridge
Phase PN-5 — HUD/audio/toolchain polish
Phase PN-6 — Final docs, archive hardening, and validation
```

Parallelization rules:

- Phase PN-0 must finish before code work.
- Phase PN-1 must finish before live-client interaction presentation is considered complete.
- Phase PN-2 and Phase PN-3 may overlap after PN-1 exposes stable snapshot data to the client loop.
- Phase PN-4 depends on PN-3.
- Phase PN-5 can begin after PN-3 event types are stable; visual-only work may start after PN-2.
- Phase PN-6 runs last.

## Non-goals

- Client-side gameplay scripting.
- Renderer-, audio-, or HUD-owned gameplay authority.
- Source/VBSP, moving brush simulation, dynamic rigid bodies, third-party physics, full PBR, or broad
  model/animation systems unless explicitly requested.
- Remote socket transport, prediction, or reconciliation before the transport-neutral contracts and
  local bridge are stable.

## Phase completion requirements

After each completed phase:

1. Record completion notes and validation results in `docs/ImplementationStatus.md`.
2. Update `docs/Design.md`, `docs/BspAuthoring.md`, and `Plans/NEXT.md` when behavior or handoff
   status changes.
3. Run the narrowest useful display-free validation, plus broader validation when practical.
4. Commit the phase to the active branch without pushing or pulling.
