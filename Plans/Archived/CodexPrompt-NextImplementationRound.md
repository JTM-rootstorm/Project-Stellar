# Codex Prompt: Next Implementation Round for `collision-movement`

You are working in `JTM-rootstorm/Project-Stellar` on the `collision-movement` branch.

## Do not restart completed work

The branch already has first-pass completion notes for:

- Phase 6A static level collision extraction.
- Phase 6B collision queries and minimal movement.
- Phase 6C billboard sprite rendering.
- Phase 6D world metadata extraction.

Your job is to implement the next round using the Phase 7 plans in this package.

## Best first task

Start with:

`Phase7A-StatusAlignment-RenderFiltering-Hardening.md`

Why:

- It fixes stale docs that currently point future agents at already-completed work.
- It resolves collision-only render filtering, a deferred Phase 6A item.
- It hardens regression tests before larger movement/runtime work.

## After Phase 7A

Proceed in this order:

1. `Phase7B-RobustStaticCharacterMovement.md`
2. `Phase7C-RuntimeWorldAssembly.md`
3. `Phase7D-RuntimeTriggersAndMetadataHooks.md`
4. `Phase7E-CollisionBroadphaseAndDiagnostics.md`

## Implementation principles

- One phase per PR/commit slice.
- Keep default CTest display-free.
- No third-party physics engine.
- Preserve existing tests.
- Add completion notes to the phase plan after implementation.
- Update `docs/ImplementationStatus.md` after each completed phase.
- Prefer small APIs with Doxygen `@brief`.
- Use deterministic tests over visual/manual testing.

## Baseline validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
