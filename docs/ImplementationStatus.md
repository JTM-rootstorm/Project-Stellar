# Project Stellar: Next Implementation Plans

Branch target: `collision-movement`

Prepared for: an implementation AI agent such as Codex/Kilo/other code-writing agent.

## Context snapshot

The branch already contains completion notes for the earlier Phase 6 world-authoring work:

- Phase 6A: static glTF collision extraction.
- Phase 6B: collision queries and minimal movement resolution.
- Phase 6C: billboard sprite rendering.
- Phase 6D: world metadata extraction.

Do **not** restart those phases from scratch. Treat them as implemented first passes and focus the next round on making the features coherent, usable at runtime, and harder to break.

## Recommended next-round order

1. **Phase 7A — Status alignment, collision render filtering, and regression hardening**
   - Lowest-risk cleanup.
   - Fix stale docs/status so future agents do not implement already-finished work.
   - Decide and implement whether collision-only glTF nodes should be excluded from render scene generation.
   - Expand regression tests around collision/metadata/sprite coexistence.

2. **Phase 7B — Robust static character movement over imported collision**
   - Highest gameplay value.
   - Upgrade the minimal `CollisionWorld::move_sphere` helper into a small, testable character-movement module.
   - Add support for start-overlap recovery, stable grounding, slope limits, step-up behavior, and better edge/corner contacts.

3. **Phase 7C — Runtime world assembly from imported glTF scenes**
   - Connect imported `SceneAsset` data to an actual runtime/playable world container.
   - Build a single place where render scene, collision world, spawns, trigger metadata, and sprite markers are assembled.

4. **Phase 7D — Runtime trigger volumes and metadata-driven gameplay hooks**
   - Make Phase 6D metadata usable.
   - Implement simple trigger overlap queries and event-style state changes without adding a full ECS or physics engine.

5. **Phase 7E — Static collision broadphase and diagnostics**
   - Add broadphase only after the correctness-focused movement layer is stable.
   - Include debug/stat inspection so performance improvements are measurable.

## Strong recommendation

Implement only one phase per pull request/branch slice. Keep every phase display-free by default, with optional graphics context tests remaining opt-in.

The best next single step is **Phase 7A** because it prevents future agent confusion and gives a reliable baseline before the larger character-controller work.

## Shared implementation rules

- Prefer small public APIs with Doxygen `@brief` comments.
- Keep collision and metadata backend-neutral.
- Do not add a third-party physics engine yet.
- Keep default CTest display-free.
- Preserve OpenGL/Vulkan parity assumptions where rendering is touched.
- Do not expand glTF extension support unless directly required.
- Add completion notes to every plan file after implementation.
- Update `docs/ImplementationStatus.md` after each phase.

## Shared validation baseline

Run this at the end of each phase unless the phase plan says otherwise:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

When graphics context code is touched, keep context tests opt-in and run the relevant optional path separately.
