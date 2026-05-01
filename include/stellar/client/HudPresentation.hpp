#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"

namespace stellar::client {

/** @brief Presentation-only identity used to ignore duplicate server-approved HUD events. */
struct HudEventIdentity {
    /** @brief Server-approved event category. */
    stellar::network::GameplayEventKind kind =
        stellar::network::GameplayEventKind::kScriptCommandApplied;

    /** @brief Authoritative tick associated with the event. */
    std::uint64_t tick = 0;

    /** @brief Related authoritative entity/collider id when known, otherwise zero. */
    std::uint32_t entity_id = 0;

    /** @brief Related authoritative player id when known, otherwise zero. */
    std::uint32_t player_id = 0;

    /** @brief Stable event or result code used by presentation routing. */
    std::string code;

    /** @brief Human-readable deterministic event diagnostic. */
    std::string message;
};

/** @brief Presentation-only HUD cache derived from authoritative server events. */
struct HudPresentationState {
    /** @brief Count of unique pickup-collected events observed by presentation. */
    std::uint32_t collected_pickup_count = 0;

    /** @brief Recent deterministic presentation messages for future text rendering. */
    std::vector<std::string> recent_messages;

    /** @brief Recent processed event identities used only for duplicate presentation suppression. */
    std::vector<HudEventIdentity> processed_events;
};

/** @brief Reset disposable HUD presentation cache for a new map or session. */
void reset_hud_presentation(HudPresentationState& state) noexcept;

/** @brief Apply one server-approved gameplay event to the presentation-only HUD cache. */
void apply_gameplay_event(HudPresentationState& state,
                          const stellar::network::GameplayEvent& event);

/** @brief Apply multiple server-approved gameplay events to the presentation-only HUD cache. */
void apply_gameplay_events(HudPresentationState& state,
                           const std::vector<stellar::network::GameplayEvent>& events);

} // namespace stellar::client
