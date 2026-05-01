# Phase PN-6 — Final Documentation, Archive Hardening, and Validation

Optimized for Kilo Code intake.

## Owner

- Primary: `@director`
- Specialist review:
  - `@carmack` for runtime/networking claims
  - `@miyamoto` for graphics/presentation claims
  - `@suzuki` for audio claims if Phase PN-5 audio was implemented
  - `@kojima` for gameplay/HUD semantics

## Goal

Finalize the implementation run with accurate documentation, validation history, archive hygiene, and a clean next-scope handoff.

## Required reading

- All phase completion notes
- `Plans/NEXT.md`
- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `Plans/BspPresentationNetworkingPolish-AgentPlan.md`
- `Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md`
- `Plans/Archived/bsp_gameplay_loop/`

## Required documentation updates

### 1. `docs/ImplementationStatus.md`

For each completed PN phase, add notes:

```markdown
## BSP Presentation and Networking Polish — Active/Complete

- Phase PN-0 — Plan archival and follow-up handoff: complete as of YYYY-MM-DD.
- Phase PN-1 — Live scripted authoritative runtime integration: complete/incomplete.
- Phase PN-2 — Authoritative gameplay snapshot presentation: complete/incomplete.
- Phase PN-3 — Snapshot/delta/event transport contracts: complete/incomplete.
- Phase PN-4 — Local/remote transport bridge: complete/incomplete.
- Phase PN-5 — HUD/audio/toolchain polish: complete/incomplete or partially complete.
- Phase PN-6 — Final docs, archive hardening, and validation: complete.

Validation run:
...
Known deferred:
...
```

Include exact commands actually run and exact pass/fail results.

### 2. `Plans/NEXT.md`

If PN work is complete, update next scope. Recommended next options:

- Remote socket transport implementation.
- Client interpolation/prediction/reconciliation.
- Sprite texture atlas/animation.
- Rich HUD/UI rendering.
- Deeper audio/miniaudio integration.
- BSP toolchain/editor polish.
- Moving brush simulation only if explicitly requested.

If PN work is incomplete, keep `Plans/NEXT.md` pointing to the active PN plan and mark remaining phase.

### 3. `docs/Design.md`

Update current branch direction and relevant sections:

- Live client can now use scripted authoritative local runtime for script-bound BSP maps.
- Gameplay snapshot presentation is client-side and non-authoritative.
- Snapshot/delta/event contracts exist and are the basis for remote transport.
- Transport bridge status: in-memory/local or remote, depending on implementation.
- Known deferrals remain explicit.

Do not overclaim remote multiplayer if only the transport seam exists.

### 4. `docs/BspAuthoring.md`

Update author-facing details:

- Where script files are resolved from.
- What happens if a script is missing.
- How active pickups/door states present.
- Any new FGD/toolchain files.
- Validation commands for scripted maps.

### 5. Plans archive policy

If the PN plan package is complete and a new scope is chosen, archive it:

```text
Plans/Archived/bsp_presentation_networking_polish/
```

Move:

```text
Plans/BspPresentationNetworkingPolish-AgentPlan.md
Plans/ProjectStellar-BSP-PresentationNetworkingPolish-AgentPlan.md
```

Only archive these active PN plans after `docs/ImplementationStatus.md` records the PN scope as complete and `Plans/NEXT.md` points to a new active scope.

If PN is not complete, keep them in root `Plans/`.

## Required validation

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run focused suite:

```bash
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_|player_presentation|gameplay_presentation|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|snapshot_|loopback_transport|client_world_receiver|world_metadata_validation|collision_validation|character_controller|hud_presentation|audio_event_router)' --output-on-failure
```

Run audits:

```bash
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Expected:

- No active retired-importer implementation references.
- No new client-side gameplay scripting authority.
- Old gameplay-loop plan names are only historical/archive references.
- Default tests pass display-free.

## Final acceptance criteria

- Documentation matches implemented reality.
- Old completed plans are archived.
- Active plans are either completed and archived or clearly remain active in root `Plans/`.
- `Plans/NEXT.md` points to the correct next active scope.
- Validation commands and results are recorded.
- No overclaiming: if remote sockets, audio, HUD rendering, prediction, or interpolation are not implemented, docs explicitly mark them deferred.
