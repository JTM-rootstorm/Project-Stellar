#include "stellar/client/GameplayPresentation.hpp"

#include <algorithm>
#include <cmath>

namespace stellar::client {
namespace {

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float finite_positive_or(float value, float fallback) noexcept {
    return std::isfinite(value) && value > 0.0F ? value : fallback;
}

std::array<float, 2> safe_size(const std::array<float, 3>& authored,
                               const std::array<float, 2>& fallback) noexcept {
    return {finite_positive_or(authored[0], fallback[0]),
            finite_positive_or(authored[1], fallback[1])};
}

std::array<float, 3> safe_position(const std::array<float, 3>& position) noexcept {
    return {finite_or(position[0], 0.0F), finite_or(position[1], 0.0F),
            finite_or(position[2], 0.0F)};
}

std::array<float, 4> safe_color(std::array<float, 4> color, float alpha) noexcept {
    for (float& channel : color) {
        channel = std::clamp(finite_or(channel, 1.0F), 0.0F, 1.0F);
    }
    color[3] *= std::clamp(finite_or(alpha, 1.0F), 0.0F, 1.0F);
    return color;
}

template <typename Entity>
stellar::graphics::BillboardSprite make_billboard(
    const Entity& entity,
    std::array<float, 4> color,
    const GameplayPresentationConfig& config) noexcept {
    stellar::graphics::BillboardSprite sprite;
    sprite.position = safe_position(entity.transform.position);
    sprite.size = safe_size(entity.metadata.size, config.default_sprite_size);
    sprite.color = safe_color(color, entity.metadata.alpha);
    sprite.alpha_blend = sprite.color[3] < 1.0F;
    sprite.alpha_mask = false;
    return sprite;
}

} // namespace

GameplayPresentationFrame make_gameplay_presentation_frame(
    const stellar::server::WorldSnapshot& snapshot,
    const GameplayPresentationConfig& config) noexcept {
    GameplayPresentationFrame frame;
    frame.sprites.reserve(snapshot.gameplay_world.entities.size());

    for (const auto& entity : snapshot.gameplay_world.entities) {
        switch (entity.kind) {
        case stellar::server::EntityKind::kSprite:
            if (entity.active) {
                frame.sprites.push_back(
                    make_billboard(entity, config.default_sprite_color, config));
            }
            break;
        case stellar::server::EntityKind::kPickup:
            if (entity.active) {
                frame.sprites.push_back(make_billboard(entity, config.pickup_color, config));
            }
            break;
        case stellar::server::EntityKind::kDoor:
            if (config.show_debug_interaction_markers) {
                const auto color = entity.open ? config.door_open_color : config.door_closed_color;
                frame.sprites.push_back(make_billboard(entity, color, config));
            }
            break;
        case stellar::server::EntityKind::kPlayer:
        case stellar::server::EntityKind::kTrigger:
        case stellar::server::EntityKind::kObjectCollider:
            break;
        }
    }

    return frame;
}

GameplayPresentationFrame make_gameplay_presentation_frame(
    const stellar::network::NetworkWorldSnapshot& snapshot,
    const GameplayPresentationConfig& config) noexcept {
    GameplayPresentationFrame frame;
    frame.sprites.reserve(snapshot.entities.size());

    for (const auto& entity : snapshot.entities) {
        switch (entity.kind) {
        case stellar::server::EntityKind::kSprite:
            if (entity.active) {
                frame.sprites.push_back(
                    make_billboard(entity, config.default_sprite_color, config));
            }
            break;
        case stellar::server::EntityKind::kPickup:
            if (entity.active) {
                frame.sprites.push_back(make_billboard(entity, config.pickup_color, config));
            }
            break;
        case stellar::server::EntityKind::kDoor:
            if (config.show_debug_interaction_markers) {
                const auto color = entity.open ? config.door_open_color : config.door_closed_color;
                frame.sprites.push_back(make_billboard(entity, color, config));
            }
            break;
        case stellar::server::EntityKind::kPlayer:
        case stellar::server::EntityKind::kTrigger:
        case stellar::server::EntityKind::kObjectCollider:
            break;
        }
    }

    return frame;
}

} // namespace stellar::client
