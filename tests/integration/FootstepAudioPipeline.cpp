#include "stellar/audio/AudioEventRouter.hpp"
#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/world/RuntimeWorld.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

class RecordingSink final : public stellar::audio::AudioRequestSink {
public:
    [[nodiscard]] stellar::audio::AudioRequestResult play_one_shot(
        const stellar::audio::AudioPlaybackRequest& request) override {
        sound_ids.push_back(request.sound_id);
        return {};
    }

    std::vector<std::string> sound_ids;
};

stellar::assets::CollisionTriangle triangle(Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
    return stellar::assets::CollisionTriangle{.a = a, .b = b, .c = c, .normal = normal};
}

stellar::assets::WorldMarker player_spawn(Vec3 position) {
    stellar::assets::WorldMarker marker{};
    marker.type = stellar::assets::WorldMarkerType::kPlayerSpawn;
    marker.name = "SPAWN_Player";
    marker.position = position;
    return marker;
}

stellar::assets::LevelAsset floor_level(std::string source_name, std::string surface_id) {
    stellar::assets::LevelAsset level{};
    level.world_metadata.markers.push_back(player_spawn({0.0F, 0.0F, 0.55F}));
    auto floor_a = triangle({-16.0F, -16.0F, 0.0F}, {16.0F, -16.0F, 0.0F},
                            {16.0F, 16.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    auto floor_b = triangle({-16.0F, -16.0F, 0.0F}, {16.0F, 16.0F, 0.0F},
                            {-16.0F, 16.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    floor_a.surface.source_material_name = std::move(source_name);
    floor_a.surface.footstep_surface_id = std::move(surface_id);
    floor_b.surface = floor_a.surface;
    stellar::assets::CollisionMesh floor;
    floor.name = "Floor";
    floor.triangles = {floor_a, floor_b};
    level.level_collision = stellar::assets::LevelCollisionAsset{.meshes = {floor}};
    return level;
}

stellar::server::WorldSessionConfig footstep_config() {
    stellar::server::WorldSessionConfig config{};
    config.local_player_id = 7;
    config.movement.max_speed = 10.0F;
    config.movement.acceleration = 100.0F;
    config.movement.gravity = 24.0F;
    config.movement.fixed_dt = 0.1F;
    config.movement.character.radius = 0.25F;
    config.movement.character.height = 1.0F;
    config.movement.character.skin_width = 0.0F;
    config.movement.character.ground_snap_distance = 0.2F;
    config.footsteps.min_horizontal_speed = 0.0F;
    config.footsteps.walk_step_distance = 0.01F;
    config.footsteps.run_step_distance = 0.01F;
    return config;
}

std::vector<stellar::network::GameplayEvent> run_authority_for_surface(
    std::string_view source_name,
    std::string_view surface_id) {
    const auto level = floor_level(std::string(source_name), std::string(surface_id));
    const auto world = stellar::world::build_runtime_world(level);
    const auto config = footstep_config();
    stellar::server::WorldSession session(world, config);
    const std::array commands{stellar::server::PlayerCommand{
        .player_id = config.local_player_id,
        .movement = stellar::server::MovementCommand{.wish_direction = {1.0F, 0.0F, 0.0F}}}};

    std::vector<stellar::server::FootstepEvent> footstep_events;
    for (int tick = 0; tick < 3 && footstep_events.empty(); ++tick) {
        auto snapshot = session.tick(commands);
        footstep_events.insert(
            footstep_events.end(),
            std::make_move_iterator(snapshot.footstep_events.begin()),
            std::make_move_iterator(snapshot.footstep_events.end()));
    }

    assert(!footstep_events.empty());
    return stellar::authority::make_gameplay_events(
        footstep_events.back().tick, {}, {}, {}, footstep_events);
}

void surface_routes_to_expected_audio_prefix(std::string_view source_name,
                                             std::string_view surface_id,
                                             std::string_view sound_prefix) {
    const auto events = run_authority_for_surface(source_name, surface_id);
    assert(!events.empty());
    const auto footstep = std::find_if(events.begin(), events.end(), [](const auto& event) {
        return event.kind == stellar::network::GameplayEventKind::kFootstep;
    });
    assert(footstep != events.end());
    assert(footstep->code == surface_id);
    assert(footstep->message == source_name);

    stellar::audio::AudioEventRouter router;
    RecordingSink sink;
    const auto result = router.route_events(events, sink);

    assert(result.diagnostics.empty());
    assert(result.submitted_requests == 1);
    assert(sink.sound_ids.size() == 1);
    assert(sink.sound_ids[0].starts_with(sound_prefix));
}

} // namespace

int main() {
    surface_routes_to_expected_audio_prefix("dev/grid_32", "concrete", "footstep_concrete_");
    surface_routes_to_expected_audio_prefix("metal/grate01", "metal", "footstep_metal_");
    return 0;
}
