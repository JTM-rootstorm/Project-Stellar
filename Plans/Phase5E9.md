# Phase 5E.9 Vulkan Resize, Recreate, And Resource Lifetime Hardening

## Goal

Make the Vulkan renderer robust enough for normal client use by hardening resize/recreate behavior, in-flight GPU resource lifetime, descriptor invalidation, and Vulkan error recovery while preserving the Phase 5E parity work already completed for static, textured, alpha, double-sided, and skinned glTF rendering.

Phase 5E.9 should remain a focused hardening slice. Do not expand it into Phase 5E.10 parity-matrix work or Phase 5F renderer architecture changes.

## Current State

- `VulkanGraphicsDevice` initializes a Vulkan instance, SDL surface, physical device, logical device, queues, command resources, sync resources, descriptor resources, skin draw resources, default material textures, pipeline layout, and swapchain resources.
- Swapchain-dependent resources currently include the swapchain, swapchain image views, render pass, graphics pipeline variants, depth image/view/memory, framebuffers, and per-image fence tracking.
- `begin_frame`, `draw_mesh`, and `end_frame` are active Vulkan frame functions and must remain `noexcept`.
- `begin_frame` already reacts to window size changes, `VK_ERROR_OUT_OF_DATE_KHR`, and `VK_SUBOPTIMAL_KHR`, but Phase 5E.9 must audit failure-state consistency and recovery behavior.
- `end_frame` already submits and presents work, marking the swapchain for rebuild on out-of-date/suboptimal present, but Phase 5E.9 must harden the lifetime and state transitions around present failures.
- Meshes, textures, materials, material descriptor sets, texture samplers, and skin draw uniform descriptors are real Vulkan resources.
- Default tests must remain display-free. Vulkan context/smoke coverage must stay opt-in under `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.
- The existing design requires backend parity through the common graphics abstraction. Do not introduce Vulkan-only behavior into backend-neutral client code unless a hard blocker is found.

## Constraints

- Follow `AGENTS.md`: graphics work belongs to `@miyamoto`; do not modify `AGENTS.md`.
- Keep C++23 and CMake 3.20+ compatibility.
- Keep public API documentation in Doxygen `@brief` style when public APIs change.
- Use `std::expected<T, stellar::platform::Error>` for fallible setup, upload, and initialization paths.
- Do not throw exceptions.
- Keep `begin_frame`, `draw_mesh`, `end_frame`, and destroy paths `noexcept` where they are currently `noexcept`.
- Keep default CTest free of GPU, display, and Vulkan-driver requirements.
- Prefer internal Vulkan implementation changes over backend-neutral API changes.
- Preserve OpenGL/Vulkan behavior parity for existing Phase 5E material, alpha, double-sided, and skinning behavior.
- Do not mark Phase 5E.10 complete from this task.

## Suggested Agent Routing

- `@miyamoto`: primary owner for all Vulkan resize, swapchain, synchronization, descriptors, deletion queues, smoke tests, and documentation updates.
- `@carmack`: consult only if a backend-neutral `GraphicsDevice` resource ownership API change becomes unavoidable.
- `@director`: only for coordination/failure reporting if `@miyamoto` cannot complete after the allowed attempts.

## Files To Inspect First

- `AGENTS.md`
- `docs/Design.md`
- `.kilo/plans/1777257747393-clever-tiger.md`
- `include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`
- `src/graphics/vulkan/VulkanGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceCommon.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceSwapchain.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp`
- `tests/graphics/VulkanContextSmoke.cpp`
- `CMakeLists.txt`

## Tasks

### 1. Audit and harden swapchain recreation

Goal: make resize, minimized-window, suboptimal, and out-of-date paths recoverable without stale frame state.

Tasks:

1. Recreate swapchain-dependent resources when any of these occur:
   - SDL window dimensions change.
   - `vkAcquireNextImageKHR` returns `VK_ERROR_OUT_OF_DATE_KHR`.
   - `vkAcquireNextImageKHR` returns `VK_SUBOPTIMAL_KHR`.
   - `vkQueuePresentKHR` returns `VK_ERROR_OUT_OF_DATE_KHR`.
   - `vkQueuePresentKHR` returns `VK_SUBOPTIMAL_KHR`.
2. Keep `vkDeviceWaitIdle` limited to teardown, unavoidable resize/recreate paths, or the simple fallback resource-destruction path described below.
3. Ensure zero-sized or minimized windows do not crash or poison renderer state:
   - Mark swapchain rebuild pending.
   - Return safely from `begin_frame`.
   - Retry once dimensions become non-zero.
4. Ensure failed acquire, command-buffer reset/begin, submit, present, or recreate paths cannot leave:
   - `frame_in_progress_` incorrectly true.
   - `current_frame_index_` advanced incorrectly.
   - `current_swapchain_image_index_` pointing outside the new swapchain image set.
   - old framebuffers, render passes, or pipelines referenced after failed recreation.
5. Ensure `swapchain_image_fences_` is cleared before swapchain destruction and resized to the new image count after every successful recreation.
6. Add private helpers if they reduce state bugs. Suggested helper names:
   - `mark_swapchain_rebuild_pending() noexcept`
   - `reset_frame_state_after_failed_recording() noexcept`
   - `current_window_extent_or_pending_rebuild(...) noexcept`
   - `recreate_swapchain_from_window_extent() noexcept`

Validation:

- Existing default tests still pass.
- Optional Vulkan smoke test renders at least one frame before and after a resize/recreate trigger when supported by the hidden-window environment.
- Manual Vulkan client resize does not crash or permanently blank after restoring a non-zero size.

### 2. Add a deferred deletion queue for GPU resource lifetime

Goal: avoid freeing resources still referenced by submitted command buffers, without stalling every destroy call unnecessarily.

Preferred implementation:

1. Add an internal deferred deletion mechanism owned by `VulkanGraphicsDevice`.
2. Track the frame index or monotonically increasing retire value associated with each submitted frame.
3. When destroying mesh, texture, material, sampler, descriptor, buffer, image, image view, or memory resources that might be referenced by in-flight GPU work, enqueue the Vulkan handles for later destruction instead of destroying them immediately.
4. Drain deferred deletions only after the relevant in-flight fence has completed.
5. Drain all pending deferred deletions during device teardown after `vkDeviceWaitIdle` succeeds or after the device is known idle enough to destroy resources safely.
6. Keep the queue implementation private to the Vulkan backend. Do not expose it through `GraphicsDevice`.
7. Keep queued deletion records simple and type-safe enough to avoid double-free bugs. Acceptable approaches include:
   - A small struct with nullable Vulkan handles grouped by resource kind.
   - Separate vectors for buffers, images, image views, samplers, descriptor sets, and material records.
   - A vector of private deletion callbacks only if the project style and `noexcept` requirements are respected.

Required deferred-deletion behavior:

- Mesh vertex/index buffers and memory must remain alive until any submitted draw using the mesh is complete.
- Texture image, image view, memory, and sampler handles must remain alive until any submitted draw or descriptor reference using the texture is complete.
- Material descriptor sets and material-owned samplers must remain alive until any submitted draw using the material is complete.
- Skin draw per-frame uniform resources must not be destroyed until frames using them are complete.
- Swapchain-dependent resources may still use `vkDeviceWaitIdle` during recreation; deferred deletion is mainly for user/resource destroy paths and teardown cleanup.

Fallback implementation if deferred deletion becomes too risky for this slice:

1. Add a private `wait_for_in_flight_work() noexcept` helper.
2. Use it before immediate destruction in `destroy_mesh`, `destroy_texture`, and `destroy_material`.
3. The helper should return safely when `device_ == VK_NULL_HANDLE`, log `VkResult` failures, and never throw.
4. Document in the Phase 5E.9 completion notes why the fallback was chosen and what would be needed to replace it with true deferred deletion later.

Acceptance preference:

- Prefer the deferred deletion queue if it can be implemented cleanly and validated.
- Accept the wait-before-destroy fallback only if it is safer within this phase and completion notes clearly call out the tradeoff.

Validation:

- Destroy mesh, texture, and material resources after at least one submitted Vulkan frame.
- Submit additional frames or safely no-op after destruction without use-after-free, stale descriptor use, or validation-layer complaints when validation layers are available manually.

### 3. Harden descriptor/resource invalidation edge cases

Goal: destroying a resource must not leave future draws with invalid descriptors or invalid dependent handles.

Tasks:

1. Audit material descriptor sets and material-owned sampler lifetime.
2. When `destroy_texture` is called for a texture used by live materials, choose one robust behavior:
   - Preferred: wait/defer the old texture resource lifetime, then rewrite affected live material descriptor sets to default white/normal textures as appropriate.
   - Acceptable: mark affected materials invalid and make future `draw_mesh` calls skip them safely.
3. Do not allow a live material descriptor set to keep referencing a destroyed texture image view or sampler after the destruction becomes visible to future frames.
4. If rewriting descriptors fails in a `noexcept` destroy path, log the error and leave the renderer in a safe no-draw/no-crash state.
5. Ensure default white and default normal textures remain valid until teardown and are not accidentally rewritten to missing handles.
6. Ensure default material descriptor set cleanup still occurs before descriptor pool destruction.

Validation:

- Create material using a texture, draw it, destroy the texture, then draw again with the material or skip safely.
- Create and destroy material-owned samplers without double-freeing texture-owned default samplers.

### 4. Improve Vulkan error reporting and recovery consistency

Goal: all checked Vulkan failures should produce useful diagnostics and leave the renderer in a known state.

Tasks:

1. Audit checked `VkResult` paths in initialization, swapchain creation, swapchain recreation, uploads, descriptors, frame recording, submit, and present.
2. Ensure setup/upload paths return `std::unexpected(vulkan_error("operation", result))` with the operation name.
3. Ensure frame paths log operation-specific messages and either:
   - request swapchain rebuild for recoverable swapchain errors, or
   - mark the backend uninitialized only for unrecoverable errors.
4. Ensure partial failures clean up resources already created in that helper.
5. Focus cleanup audit on:
   - swapchain creation failures after `vkCreateSwapchainKHR` succeeds but image query fails.
   - swapchain image view creation failures after some image views were created.
   - render pass creation failure.
   - graphics pipeline variant creation failure.
   - depth image/image memory/image view failures.
   - framebuffer creation failures after some framebuffers were created.
   - descriptor pool, descriptor set layout, descriptor allocation, and descriptor update setup failures.
   - `vkMapMemory`, `vkBindBufferMemory`, `vkBindImageMemory`, `vkQueueSubmit`, and `vkQueuePresentKHR` paths.
6. Keep validation layers and debug utils optional; do not make them required for CI.

Validation:

- Default tests pass.
- Optional Vulkan smoke test still passes on a Vulkan-capable system.
- Manual logs contain operation names for injected or naturally observed Vulkan failures.

### 5. Strengthen teardown ordering

Goal: make backend destruction deterministic and safe after partial initialization, failed recreation, or normal client shutdown.

Required teardown order:

1. Stop frame progress and mark backend no longer initialized.
2. If device exists, wait for device idle during full teardown.
3. Drain any deferred deletion queue, or destroy any resources made safe by the device-idle wait.
4. Destroy mesh GPU resources and clear mesh records.
5. Destroy material records and material-owned sampler/descriptor resources before destroying descriptor pools.
6. Destroy non-default texture resources, then default textures, while avoiding descriptor references after descriptor pool destruction.
7. Destroy swapchain-dependent resources.
8. Destroy pipeline layout.
9. Destroy descriptor pools and descriptor set layouts.
10. Destroy sync objects and command resources.
11. Destroy logical device, surface, and instance.
12. Reset handles and counters to safe defaults.

Tasks:

1. Audit `destroy_vulkan_objects()` against the required order.
2. Ensure partial initialization failure can call `destroy_vulkan_objects()` repeatedly without double-freeing handles.
3. Ensure `textures_.clear()` and `materials_.clear()` are not redundantly called outside safe teardown ordering in a way that bypasses record destruction.
4. Ensure deferred deletion queues do not attempt to destroy handles after the logical device has been destroyed.

Validation:

- Repeated initialize/fail/destroy cycles do not crash.
- Optional Vulkan smoke test can initialize and destroy the device cleanly.

### 6. Expand optional Vulkan smoke coverage

Goal: add focused opt-in coverage for the hardening work without making default tests require Vulkan.

Tasks:

1. Update `tests/graphics/VulkanContextSmoke.cpp` only under the existing `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS` option.
2. Keep the smoke test display/GPU opt-in only.
3. Extend the test to cover:
   - initialization.
   - at least one submitted frame.
   - resize/recreate if feasible in the hidden SDL Vulkan window environment.
   - mesh/material/texture destruction after a submitted frame.
   - another frame after destruction, or a safe no-op if the environment cannot present.
4. If hidden SDL Vulkan windows cannot reliably trigger real swapchain recreation, add a clear comment explaining the limitation and keep manual resize validation in the completion notes.
5. Avoid brittle image-readback expectations for this phase.

Validation:

```bash
cmake -S . -B build-vulkan-tests -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

### 7. Keep default tests display-free

Goal: preserve default CI behavior.

Tasks:

1. Do not register Vulkan smoke tests outside `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.
2. Do not add environment-variable requirements to default CTest.
3. Do not add validation-layer or shader-compiler runtime requirements to default tests.
4. Run default validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Validation

Required default validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Required opt-in Vulkan validation when a Vulkan-capable environment is available:

```bash
cmake -S . -B build-vulkan-tests -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Manual validation when a display and Vulkan device are available:

```bash
./build/stellar-client --renderer vulkan
```

Manual checklist:

- Window resize does not crash.
- Minimize/restore or zero-size equivalent does not permanently poison rendering.
- Static cube or generated mesh still renders.
- Representative static textured glTF still renders.
- Alpha mask/blend behavior is unchanged from Phase 5E.7.
- Double-sided material behavior is unchanged from Phase 5E.7.
- Skinned glTF behavior is unchanged from Phase 5E.8.
- Destroying mesh, texture, and material resources after submitted frames does not crash or use freed handles.
- Destroying a texture referenced by a live material either rebinds defaults or skips the affected material safely.

## Completion Notes Requirements

After implementation and validation, update both:

1. This `Phase5E9.md` draft plan.
2. `.kilo/plans/1777257747393-clever-tiger.md` under `### Phase 5E.9: Resize, Recreate, And Resource Lifetime Hardening`.

Use this completion-note format in both files:

```md
Completion notes (YYYY-MM-DD):

- Hardened Vulkan swapchain recreation on resize, minimized/zero-size windows, suboptimal acquire/present, and out-of-date acquire/present paths.
- Ensured frame state is reset safely after failed acquire, command recording, submit, present, and recreate paths while keeping `begin_frame`, `draw_mesh`, and `end_frame` `noexcept`.
- Implemented `<deferred deletion queue / wait-before-destroy fallback>` for in-flight mesh, texture, material, descriptor, and sampler lifetime safety.
- `<If deferred deletion queue was implemented: describe retire/fence behavior and teardown drain behavior.>`
- `<If fallback was used: explain why deferred deletion was deferred and document the future upgrade path.>`
- Rewrote or safely invalidated material descriptors when dependent textures are destroyed, falling back to default white/normal textures or skipping affected materials safely.
- Audited Vulkan initialization, upload, recreate, and frame error paths so checked `VkResult` failures report operation-specific `std::expected` errors or frame-path log messages.
- Hardened teardown ordering for partial initialization, failed recreation, normal shutdown, and pending deferred deletions.
- Extended the opt-in Vulkan smoke test to cover resize/recreate and resource-destroy-after-frame behavior where supported by the hidden SDL Vulkan window environment.
- Validated default display-free coverage with: `<commands and result>`.
- Validated opt-in Vulkan coverage with: `<commands and result, or clear reason if unavailable>`.
- Manual validation: `<static cube/glTF/resize/skinned result, or not run with reason>`.
```

Do not mark Phase 5E.10 complete.

## Acceptance Criteria

Phase 5E.9 is complete only when:

- Default build and CTest pass without requiring a GPU or display.
- Optional Vulkan smoke test still builds only when `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`.
- `begin_frame`, `draw_mesh`, and `end_frame` remain `noexcept`.
- Resize, minimized-window, out-of-date, and suboptimal swapchain paths do not crash and leave renderer state recoverable.
- Mesh, texture, material, descriptor, sampler, buffer, image, and memory destruction cannot race in-flight GPU work.
- A deferred deletion queue is implemented, or the wait-before-destroy fallback is implemented and documented with the reason deferred deletion was postponed.
- Destroying textures used by live materials cannot lead to stale invalid descriptor use on later draws.
- Initialization, upload, and recreate failures return clear `std::expected` errors with operation names.
- Runtime frame failures log clear messages and safely no-op, request rebuild, or mark the backend uninitialized according to recoverability.
- Teardown works after normal initialization, partial initialization failure, failed swapchain recreation, and repeated calls.
- `.kilo/plans/1777257747393-clever-tiger.md` and `Phase5E9.md` both include completion notes.

## Recommended Implementation Order

1. Audit swapchain/frame state transitions and add small private helpers.
2. Implement deferred deletion queue skeleton and teardown drain path.
3. Route mesh, texture, material, sampler, descriptor, buffer, image, image view, and memory destruction through the queue where needed.
4. Add descriptor rewrite or safe invalidation for materials referencing destroyed textures.
5. Audit Vulkan error reporting and partial-failure cleanup.
6. Harden teardown ordering.
7. Expand opt-in Vulkan smoke coverage.
8. Run default validation and optional Vulkan validation.
9. Update completion notes in both plan files.

## Key Decisions To Keep Explicit

- Deferred deletion strategy: prefer fence-retired deletion queues; use wait-before-destroy fallback only if queue complexity becomes unsafe for this phase.
- Resize behavior: swapchain recreation can still use `vkDeviceWaitIdle`; normal frame rendering should not stall the whole device.
- Descriptor invalidation: prefer rebinding affected material descriptors to default textures over destroying or invalidating unrelated materials.
- Testing: keep Vulkan context coverage opt-in and never make GPU/display tests part of default CTest.
- Scope: do not pull in Phase 5E.10 parity-matrix completion or Phase 5F larger renderer architecture changes.
