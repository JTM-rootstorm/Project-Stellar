# Phase DG-3 — Public Header Documentation Audit and Minimal Gap Fill

## Objective

Bring public API comments into better alignment with the existing project rule: public APIs require Doxygen `@brief` documentation.

## Scope

Only audit and minimally improve public API comments under:

```text
include/stellar/**
```

Do not broadly rewrite private `src/**` implementation docs in this phase.

## Codex Prompt

```text
Audit public headers under include/stellar for exported classes, structs, enums, public methods, and public free functions that lack useful Doxygen @brief documentation.

Make minimal documentation-only edits. Do not change behavior, names, signatures, includes, target boundaries, or tests.

Prefer concise @brief comments that describe ownership, authority boundaries, serialization expectations, or presentation/runtime role where relevant.
```

## Suggested Audit Commands

```bash
find include/stellar -type f \( -name '*.hpp' -o -name '*.h' \) | sort

grep -RIn "class \|struct \|enum class \|^[[:space:]]*[A-Za-z_][A-Za-z0-9_:<>]* .*(" include/stellar \
    | grep -v "@brief" \
    | head -200
```

The grep is intentionally rough. Codex must inspect files manually before editing.

## Documentation Style

Use concise comments:

```cpp
/**
 * @brief Backend-neutral static level payload used by runtime and presentation systems.
 */
struct LevelAsset {
    // ...
};
```

For methods:

```cpp
/**
 * @brief Advances authoritative movement for one fixed simulation step.
 */
MovementResult step_movement(const MovementInput& input, float dt);
```

For enums:

```cpp
/**
 * @brief Runtime mode selected by client application startup configuration.
 */
enum class ClientRuntimeMode {
    // ...
};
```

## Important Boundaries

Mention server/client authority where useful:

- Server-owned systems should say server-owned or authoritative.
- Client presentation systems should say presentation-only.
- Protocol DTOs should say serializable/backend-neutral when applicable.
- Graphics APIs should say backend-neutral or OpenGL-specific as appropriate.
- Audio APIs should not imply gameplay authority.

## Validation Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
cmake --build build --target stellar_docs_doxygen
```

## Acceptance Criteria

- Public headers touched only for comments.
- No ABI/API behavior changes.
- Doxygen generation succeeds.
- Warning count for undocumented public symbols is reduced from DG-1 baseline.
- Existing tests still pass.

## Parallelization

This phase can be split by directory after DG-1:

- Agent A: `include/stellar/assets`, `include/stellar/import`
- Agent B: `include/stellar/world`, `include/stellar/server`, `include/stellar/scripting`
- Agent C: `include/stellar/client`, `include/stellar/network`, `include/stellar/protocol`
- Agent D: `include/stellar/graphics`, `include/stellar/audio`, `include/stellar/platform`, `include/stellar/core`

Only run parallel agents if the user explicitly asks for subagents or parallel work.

---
