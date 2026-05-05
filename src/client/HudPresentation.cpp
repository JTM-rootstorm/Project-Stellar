#include "stellar/client/HudPresentation.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace stellar::client {
namespace {

constexpr std::size_t kMaxRecentMessages = 8;
constexpr std::size_t kMaxProcessedEvents = 64;

[[nodiscard]] HudEventIdentity make_identity(const stellar::network::GameplayEvent& event) {
    return HudEventIdentity{.kind = event.kind,
                            .tick = event.tick,
                            .entity_id = event.entity_id,
                            .player_id = event.player_id,
                            .code = event.code,
                            .message = event.message};
}

[[nodiscard]] bool same_identity(const HudEventIdentity& lhs, const HudEventIdentity& rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.tick == rhs.tick && lhs.entity_id == rhs.entity_id &&
           lhs.player_id == rhs.player_id && lhs.code == rhs.code && lhs.message == rhs.message;
}

[[nodiscard]] bool was_processed(const HudPresentationState& state,
                                 const HudEventIdentity& identity) noexcept {
    return std::any_of(state.processed_events.begin(), state.processed_events.end(),
                       [&identity](const HudEventIdentity& processed) {
                           return same_identity(processed, identity);
                       });
}

void remember_processed_event(HudPresentationState& state, HudEventIdentity identity) {
    state.processed_events.push_back(std::move(identity));
    if (state.processed_events.size() > kMaxProcessedEvents) {
        state.processed_events.erase(state.processed_events.begin(),
                                     state.processed_events.begin() +
                                         static_cast<std::ptrdiff_t>(state.processed_events.size() -
                                                                    kMaxProcessedEvents));
    }
}

void push_recent_message(HudPresentationState& state, std::string message) {
    if (message.empty()) {
        return;
    }
    state.recent_messages.push_back(std::move(message));
    if (state.recent_messages.size() > kMaxRecentMessages) {
        state.recent_messages.erase(state.recent_messages.begin(),
                                    state.recent_messages.begin() +
                                        static_cast<std::ptrdiff_t>(state.recent_messages.size() -
                                                                   kMaxRecentMessages));
    }
}

[[nodiscard]] std::string pickup_message(const stellar::network::GameplayEvent& event) {
    if (!event.message.empty()) {
        return event.message;
    }
    if (!event.code.empty()) {
        return "Pickup collected: " + event.code;
    }
    if (event.entity_id != 0) {
        return "Pickup collected: entity " + std::to_string(event.entity_id);
    }
    return "Pickup collected";
}

[[nodiscard]] std::string door_message(const stellar::network::GameplayEvent& event) {
    if (!event.message.empty()) {
        return event.message;
    }
    if (!event.code.empty()) {
        return "Door/gate state changed: " + event.code;
    }
    if (event.entity_id != 0) {
        return "Door/gate state changed: entity " + std::to_string(event.entity_id);
    }
    return "Door/gate state changed";
}

} // namespace

void reset_hud_presentation(HudPresentationState& state) noexcept {
    state.collected_pickup_count = 0;
    state.recent_messages.clear();
    state.processed_events.clear();
}

void apply_gameplay_event(HudPresentationState& state,
                          const stellar::network::GameplayEvent& event) {
    const HudEventIdentity identity = make_identity(event);
    if (was_processed(state, identity)) {
        return;
    }
    remember_processed_event(state, identity);

    switch (event.kind) {
    case stellar::network::GameplayEventKind::kPickupCollected:
        ++state.collected_pickup_count;
        push_recent_message(state, pickup_message(event));
        break;
    case stellar::network::GameplayEventKind::kDoorStateChanged:
        push_recent_message(state, door_message(event));
        break;
    case stellar::network::GameplayEventKind::kScriptCommandApplied:
    case stellar::network::GameplayEventKind::kScriptError:
        push_recent_message(state, event.message);
        break;
    case stellar::network::GameplayEventKind::kFootstep:
        break;
    }
}

void apply_gameplay_events(HudPresentationState& state,
                           const std::vector<stellar::network::GameplayEvent>& events) {
    for (const stellar::network::GameplayEvent& event : events) {
        apply_gameplay_event(state, event);
    }
}

} // namespace stellar::client
