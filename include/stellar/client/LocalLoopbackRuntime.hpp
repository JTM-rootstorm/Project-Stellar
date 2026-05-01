#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "stellar/client/MovementInputMapper.hpp"
#include "stellar/platform/Input.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::client {

/** @brief Config for an in-process authoritative local server runtime. */
struct LocalLoopbackRuntimeConfig {
    /** @brief Authoritative server session configuration. */
    stellar::server::WorldSessionConfig session{};

    /** @brief Input mapper configuration used to convert client input to server intent. */
    MovementInputMapperConfig input_mapper{};

    /** @brief Maximum fixed authoritative ticks allowed during one client frame update. */
    int max_ticks_per_frame = 4;
};

/** @brief Result of advancing the local authoritative runtime for one client frame. */
struct LocalLoopbackFrameResult {
    /** @brief Authoritative snapshot after the frame, with trigger events produced this frame. */
    stellar::server::WorldSnapshot snapshot{};

    /** @brief Number of authoritative fixed ticks run during this client frame. */
    int ticks_run = 0;

    /** @brief True when accumulated time was dropped after hitting max_ticks_per_frame. */
    bool dropped_excess_time = false;
};

/** @brief In-process local authoritative runtime used by single-player/client presentation. */
class LocalLoopbackRuntime {
public:
    /** @brief Construct a runtime over caller-owned backend-neutral runtime world data. */
    explicit LocalLoopbackRuntime(const stellar::world::RuntimeWorld& world,
                                  LocalLoopbackRuntimeConfig config = {});

    /** @brief Return the latest authoritative state snapshot without replaying frame events. */
    [[nodiscard]] const stellar::server::WorldSnapshot& latest_snapshot() const noexcept;

    /** @brief Hard-reset authoritative object colliders and clear all object overlap state. */
    void set_object_colliders(std::span<const stellar::world::ObjectCollider> colliders);

    /** @brief Replace colliders by id and synchronously return any removed/disabled exits. */
    [[nodiscard]] std::vector<stellar::server::ObjectColliderEvent>
    replace_object_colliders_preserving_overlaps(
        std::span<const stellar::world::ObjectCollider> colliders) noexcept;

    /** @brief Enable or disable one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult set_object_collider_enabled(
        std::uint32_t collider_id, bool enabled) noexcept;

    /** @brief Insert or replace one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult upsert_object_collider(
        const stellar::world::ObjectCollider& collider) noexcept;

    /** @brief Remove one collider and synchronously return mutation exits only. */
    [[nodiscard]] stellar::server::ObjectColliderMutationResult remove_object_collider(
        std::uint32_t collider_id) noexcept;

    /** @brief Submit client input and advance the authoritative local server by fixed ticks. */
    [[nodiscard]] LocalLoopbackFrameResult update(const stellar::platform::Input& input,
                                                  float delta_seconds) noexcept;

private:
    LocalLoopbackRuntimeConfig config_{};
    stellar::server::WorldSession session_;
    stellar::server::WorldSnapshot latest_snapshot_{};
    float accumulated_seconds_ = 0.0F;
};

} // namespace stellar::client
