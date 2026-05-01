#include "stellar/import/gltf/Loader.hpp"
#include "stellar/server/WorldSession.hpp"
#include "stellar/scripting/ScriptRegistry.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"
#include "stellar/world/CollisionValidation.hpp"
#include "stellar/world/ObjectCollider.hpp"
#include "stellar/world/RuntimeWorld.hpp"
#include "stellar/world/WorldMetadataValidation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

struct MeshRange {
    std::size_t position_view = 0;
    std::size_t index_view = 0;
    std::size_t position_accessor = 0;
    std::size_t index_accessor = 0;
    std::size_t vertex_count = 0;
    std::size_t index_count = 0;
    Vec3 min{};
    Vec3 max{};
};

struct BufferView {
    std::size_t offset = 0;
    std::size_t length = 0;
    std::optional<std::uint32_t> target;
};

struct FixtureBuffer {
    std::vector<std::uint8_t> bytes;
    std::vector<BufferView> views;
    std::vector<MeshRange> ranges;
};

void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_f32_le(std::vector<std::uint8_t>& out, float value) {
    append_u32_le(out, std::bit_cast<std::uint32_t>(value));
}

void append_vec3(std::vector<std::uint8_t>& out, Vec3 value) {
    append_f32_le(out, value[0]);
    append_f32_le(out, value[1]);
    append_f32_le(out, value[2]);
}

void pad4(std::vector<std::uint8_t>& out) {
    while (out.size() % 4U != 0U) {
        out.push_back(0);
    }
}

std::string base64_encode(std::span<const std::uint8_t> input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2U) / 3U) * 4U);
    std::uint32_t accumulator = 0;
    int bits = -6;
    for (const std::uint8_t byte : input) {
        accumulator = (accumulator << 8U) | byte;
        bits += 8;
        while (bits >= 0) {
            output.push_back(kAlphabet[(accumulator >> bits) & 0x3FU]);
            bits -= 6;
        }
    }
    if (bits > -6) {
        output.push_back(kAlphabet[((accumulator << 8U) >> (bits + 8)) & 0x3FU]);
    }
    while (output.size() % 4U != 0U) {
        output.push_back('=');
    }
    return output;
}

Vec3 min_vec(std::span<const Vec3> positions) {
    Vec3 out = positions.front();
    for (const Vec3& position : positions) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            out[axis] = std::min(out[axis], position[axis]);
        }
    }
    return out;
}

Vec3 max_vec(std::span<const Vec3> positions) {
    Vec3 out = positions.front();
    for (const Vec3& position : positions) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            out[axis] = std::max(out[axis], position[axis]);
        }
    }
    return out;
}

void add_mesh(FixtureBuffer& buffer,
              std::span<const Vec3> positions,
              std::span<const std::uint16_t> indices) {
    MeshRange range;
    range.position_view = buffer.views.size();
    range.position_accessor = buffer.ranges.size() * 2U;
    range.index_accessor = range.position_accessor + 1U;
    range.vertex_count = positions.size();
    range.index_count = indices.size();
    range.min = min_vec(positions);
    range.max = max_vec(positions);

    pad4(buffer.bytes);
    const std::size_t position_offset = buffer.bytes.size();
    for (const Vec3& position : positions) {
        append_vec3(buffer.bytes, position);
    }
    buffer.views.push_back({.offset = position_offset,
                            .length = buffer.bytes.size() - position_offset,
                            .target = 34962});

    pad4(buffer.bytes);
    range.index_view = buffer.views.size();
    const std::size_t index_offset = buffer.bytes.size();
    for (const std::uint16_t index : indices) {
        append_u16_le(buffer.bytes, index);
    }
    buffer.views.push_back({.offset = index_offset,
                            .length = buffer.bytes.size() - index_offset,
                            .target = 34963});
    buffer.ranges.push_back(range);
}

FixtureBuffer build_fixture_buffer() {
    FixtureBuffer buffer;
    const std::array<Vec3, 4> visible_floor{{
        {-3.0F, 0.0F, -2.0F},
        {3.0F, 0.0F, -2.0F},
        {3.0F, 0.0F, 2.0F},
        {-3.0F, 0.0F, 2.0F},
    }};
    const std::array<Vec3, 4> collision_floor{{
        {-3.0F, 0.0F, -2.0F},
        {3.0F, 0.0F, -2.0F},
        {3.0F, 0.0F, 2.0F},
        {-3.0F, 0.0F, 2.0F},
    }};
    const std::array<std::uint16_t, 6> indices{{0, 2, 1, 0, 3, 2}};
    add_mesh(buffer, visible_floor, indices);
    add_mesh(buffer, collision_floor, indices);
    return buffer;
}

std::string vec3_json(Vec3 value) {
    return "[" + std::to_string(value[0]) + "," + std::to_string(value[1]) + "," +
           std::to_string(value[2]) + "]";
}

std::string build_gltf_json(const FixtureBuffer& buffer) {
    std::string json;
    json.reserve(8192);
    json += "{\n";
    json += "\"asset\":{\"version\":\"2.0\",\"generator\":\"Phase12EObjectColliderSmoke\"},\n";
    json += "\"scene\":0,\n";
    json += "\"buffers\":[{\"byteLength\":" + std::to_string(buffer.bytes.size()) +
            ",\"uri\":\"data:application/octet-stream;base64," + base64_encode(buffer.bytes) +
            "\"}],\n";

    json += "\"bufferViews\":[";
    for (std::size_t i = 0; i < buffer.views.size(); ++i) {
        const BufferView& view = buffer.views[i];
        if (i != 0) {
            json += ",";
        }
        json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(view.offset) +
                ",\"byteLength\":" + std::to_string(view.length);
        if (view.target.has_value()) {
            json += ",\"target\":" + std::to_string(*view.target);
        }
        json += "}";
    }
    json += "],\n";

    json += "\"accessors\":[";
    for (std::size_t i = 0; i < buffer.ranges.size(); ++i) {
        const MeshRange& range = buffer.ranges[i];
        if (i != 0) {
            json += ",";
        }
        json += "{\"bufferView\":" + std::to_string(range.position_view) +
                ",\"componentType\":5126,\"count\":" + std::to_string(range.vertex_count) +
                ",\"type\":\"VEC3\",\"min\":" + vec3_json(range.min) +
                ",\"max\":" + vec3_json(range.max) + "},";
        json += "{\"bufferView\":" + std::to_string(range.index_view) +
                ",\"componentType\":5123,\"count\":" + std::to_string(range.index_count) +
                ",\"type\":\"SCALAR\"}";
    }
    json += "],\n";

    json += "\"materials\":[{\"name\":\"VisibleMaterial\"}],\n";
    json += "\"meshes\":[";
    const std::array<std::string_view, 2> names{{"VisibleFloor", "CollisionFloorMesh"}};
    for (std::size_t i = 0; i < buffer.ranges.size(); ++i) {
        const MeshRange& range = buffer.ranges[i];
        if (i != 0) {
            json += ",";
        }
        json += "{\"name\":\"" + std::string(names[i]) + "\",\"primitives\":[{";
        json += "\"attributes\":{\"POSITION\":" + std::to_string(range.position_accessor) +
                "},";
        json += "\"indices\":" + std::to_string(range.index_accessor) + ",\"material\":0}]}";
    }
    json += "],\n";

    json += "\"nodes\":[";
    json += "{\"name\":\"VisibleFloor\",\"mesh\":0},";
    json += "{\"name\":\"COL_Floor\",\"mesh\":1},";
    json += "{\"name\":\"MetadataRoot\",\"children\":[3,4]},";
    json += "{\"name\":\"SPAWN_Player\",\"translation\":[-1.5,0.5,0]},";
    json += "{\"name\":\"COLLIDER_PickupGem\",\"translation\":[-0.5,0.5,0],";
    json += "\"scale\":[0.2,0.5,0.5],";
    json += "\"extras\":{\"stellar\":{\"script\":\"scripts/pickup.lua\",\"table\":\"PickupGem\"}}}";
    json += "],\n";
    json += "\"scenes\":[{\"name\":\"ScriptedObjectColliderSmoke\",\"nodes\":[0,1,2]}]\n";
    json += "}\n";
    return json;
}

std::filesystem::path write_fixture() {
    const FixtureBuffer buffer = build_fixture_buffer();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "stellar_scripted_object_collider_smoke.gltf";
    std::ofstream file(path, std::ios::binary);
    const std::string json = build_gltf_json(buffer);
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    return path;
}

stellar::server::WorldSessionConfig session_config() {
    stellar::server::WorldSessionConfig config;
    config.local_player_id = 1;
    config.movement.max_speed = 3.0F;
    config.movement.acceleration = 100.0F;
    config.movement.gravity = 0.0F;
    config.movement.terminal_fall_speed = 50.0F;
    config.movement.fixed_dt = 0.1F;
    config.movement.character.radius = 0.25F;
    config.movement.character.height = 1.0F;
    config.movement.character.skin_width = 0.0F;
    config.movement.character.ground_snap_distance = 0.0F;
    config.movement.character.max_slide_iterations = 4;
    return config;
}

stellar::scripting::ScriptRegistry pickup_registry() {
    stellar::scripting::ScriptRegistry registry;
    registry.set_script("scripts/pickup.lua",
                        "PickupGem = {}\n"
                        "function PickupGem.on_object_collider_enter(event)\n"
                        "  stellar.emit_event('object_collider.set_enabled', "
                        "{id = event.collider_id, enabled = false})\n"
                        "  stellar.emit_event('gameplay.pickup_collected', "
                        "{name = event.collider_name})\n"
                        "end\n"
                        "function PickupGem.on_object_collider_stay(event)\n"
                        "  stellar.emit_event('unexpected_stay', {id = event.collider_id})\n"
                        "end\n"
                        "function PickupGem.on_object_collider_exit(event)\n"
                        "  stellar.emit_event('unexpected_exit_hook', {id = event.collider_id})\n"
                        "end\n");
    return registry;
}

const stellar::scripting::ScriptField* find_field(
    const stellar::scripting::ScriptOutputEvent& event,
    std::string_view key) {
    for (const auto& field : event.fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

void assert_same_object_events(const std::vector<stellar::server::ObjectColliderEvent>& lhs,
                               const std::vector<stellar::server::ObjectColliderEvent>& rhs) {
    assert(lhs.size() == rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        assert(lhs[i].player_id == rhs[i].player_id);
        assert(lhs[i].collider_id == rhs[i].collider_id);
        assert(lhs[i].name == rhs[i].name);
        assert(lhs[i].archetype == rhs[i].archetype);
        assert(lhs[i].entered == rhs[i].entered);
        assert(lhs[i].stayed == rhs[i].stayed);
        assert(lhs[i].exited == rhs[i].exited);
    }
}

void assert_same_snapshot(const stellar::server::WorldSnapshot& lhs,
                          const stellar::server::WorldSnapshot& rhs) {
    assert(lhs.tick == rhs.tick);
    assert(lhs.players.size() == rhs.players.size());
    for (std::size_t i = 0; i < lhs.players.size(); ++i) {
        assert(lhs.players[i].player_id == rhs.players[i].player_id);
        assert(lhs.players[i].position == rhs.players[i].position);
        assert(lhs.players[i].velocity == rhs.players[i].velocity);
        assert(lhs.players[i].rotation == rhs.players[i].rotation);
        assert(lhs.players[i].grounded == rhs.players[i].grounded);
    }
    assert(lhs.trigger_events.size() == rhs.trigger_events.size());
    assert_same_object_events(lhs.object_collider_events, rhs.object_collider_events);
}

void assert_same_script_events(const std::vector<stellar::scripting::ScriptOutputEvent>& lhs,
                               const std::vector<stellar::scripting::ScriptOutputEvent>& rhs) {
    assert(lhs.size() == rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        assert(lhs[i].name == rhs[i].name);
        assert(lhs[i].fields.size() == rhs[i].fields.size());
        for (std::size_t field = 0; field < lhs[i].fields.size(); ++field) {
            assert(lhs[i].fields[field].key == rhs[i].fields[field].key);
            assert(lhs[i].fields[field].type == rhs[i].fields[field].type);
            assert(lhs[i].fields[field].string_value == rhs[i].fields[field].string_value);
            assert(lhs[i].fields[field].number_value == rhs[i].fields[field].number_value);
            assert(lhs[i].fields[field].bool_value == rhs[i].fields[field].bool_value);
        }
    }
}

void assert_same_command_results(
    const std::vector<stellar::scripting::ScriptCommandResult>& lhs,
    const std::vector<stellar::scripting::ScriptCommandResult>& rhs) {
    assert(lhs.size() == rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        assert(lhs[i].event_name == rhs[i].event_name);
        assert(lhs[i].applied == rhs[i].applied);
        assert(lhs[i].code == rhs[i].code);
        assert(lhs[i].message == rhs[i].message);
        assert_same_object_events(lhs[i].object_collider_events, rhs[i].object_collider_events);
    }
}

std::vector<stellar::scripting::ScriptedWorldFrame> run_scripted_path(
    const stellar::world::RuntimeWorld& world) {
    auto created = stellar::scripting::ScriptedWorldSession::create(world, session_config(),
                                                                    pickup_registry());
    assert(created.has_value());

    const std::vector<stellar::server::PlayerCommand> move_right{{
        .player_id = 1,
        .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}},
    }};

    std::vector<stellar::scripting::ScriptedWorldFrame> frames;
    frames.reserve(8);
    for (int i = 0; i < 8; ++i) {
        frames.push_back(created->tick(move_right));
    }
    return frames;
}

void assert_imported_collider_marker(const stellar::world::RuntimeWorld& world) {
    const auto markers = stellar::world::find_markers_by_type(
        world, stellar::assets::WorldMarkerType::kObjectCollider);
    assert(markers.size() == 1);
    assert(markers[0]->name == "PickupGem");
    assert(markers[0]->script.has_value());
    assert(markers[0]->script->script_id == "scripts/pickup.lua");
    assert(markers[0]->script->table_name == "PickupGem");

    const auto colliders_a = stellar::world::build_object_colliders(world);
    const auto colliders_b = stellar::world::build_object_colliders(world);
    assert(colliders_a.size() == 1);
    assert(colliders_b.size() == 1);
    assert(colliders_a[0].id == 2);
    assert(colliders_a[0].id == colliders_b[0].id);
    assert(colliders_a[0].name == "PickupGem");
    assert(colliders_a[0].shape.type == stellar::world::ObjectColliderShapeType::kAabb);
    assert(colliders_a[0].shape.center == colliders_b[0].shape.center);
    assert(colliders_a[0].shape.half_extents == colliders_b[0].shape.half_extents);
}

void assert_native_session_emits_object_enter(const stellar::world::RuntimeWorld& world) {
    stellar::server::WorldSession session(world, session_config());
    const std::vector<stellar::server::PlayerCommand> move_right{{
        .player_id = 1,
        .movement = {.wish_direction = {1.0F, 0.0F, 0.0F}},
    }};

    const auto first = session.tick(move_right);
    const auto second = session.tick(move_right);
    assert(first.object_collider_events.empty());
    assert(second.object_collider_events.size() == 1);
    assert(second.object_collider_events[0].collider_id == 2);
    assert(second.object_collider_events[0].name == "PickupGem");
    assert(second.object_collider_events[0].entered);
}

void assert_scripted_object_collider_path(const stellar::world::RuntimeWorld& world) {
    const auto first_run = run_scripted_path(world);
    const auto second_run = run_scripted_path(world);
    assert(first_run.size() == second_run.size());

    std::size_t enter_count = 0;
    std::size_t stay_count = 0;
    std::size_t command_exit_count = 0;
    bool saw_pickup_event = false;

    for (std::size_t i = 0; i < first_run.size(); ++i) {
        assert_same_snapshot(first_run[i].snapshot, second_run[i].snapshot);
        assert_same_script_events(first_run[i].script_events, second_run[i].script_events);
        assert_same_command_results(first_run[i].command_results, second_run[i].command_results);
        assert(first_run[i].script_errors.empty());
        assert(second_run[i].script_errors.empty());

        for (const auto& event : first_run[i].snapshot.object_collider_events) {
            assert(event.collider_id == 2);
            assert(event.name == "PickupGem");
            if (event.entered) {
                ++enter_count;
            }
            if (event.stayed) {
                ++stay_count;
            }
            assert(!event.exited);
        }

        if (!first_run[i].snapshot.object_collider_events.empty()) {
            assert(first_run[i].script_events.size() == 2);
            assert(first_run[i].script_events[0].name == "object_collider.set_enabled");
            assert(first_run[i].script_events[1].name == "gameplay.pickup_collected");
            const auto* id = find_field(first_run[i].script_events[0], "id");
            const auto* enabled = find_field(first_run[i].script_events[0], "enabled");
            const auto* name = find_field(first_run[i].script_events[1], "name");
            assert(id != nullptr && id->number_value == 2.0);
            assert(enabled != nullptr && enabled->type == stellar::scripting::ScriptValueType::kBoolean);
            assert(!enabled->bool_value);
            assert(name != nullptr && name->string_value == "PickupGem");
            saw_pickup_event = true;

            assert(first_run[i].command_results.size() == 2);
            assert(first_run[i].command_results[0].event_name == "object_collider.set_enabled");
            assert(first_run[i].command_results[0].applied);
            assert(first_run[i].command_results[0].code == "applied");
            assert(first_run[i].command_results[0].object_collider_events.size() == 1);
            assert(first_run[i].command_results[0].object_collider_events[0].exited);
            assert(first_run[i].command_results[0].object_collider_events[0].collider_id == 2);
            ++command_exit_count;
            assert(first_run[i].command_results[1].event_name == "gameplay.pickup_collected");
            assert(!first_run[i].command_results[1].applied);
            assert(first_run[i].command_results[1].code == "unsupported_event");
        } else {
            assert(first_run[i].script_events.empty());
            assert(first_run[i].command_results.empty());
        }
    }

    assert(enter_count == 1);
    assert(stay_count == 0);
    assert(command_exit_count == 1);
    assert(saw_pickup_event);
}

void assert_smoke_coverage(const stellar::assets::SceneAsset& scene,
                           const stellar::world::RuntimeWorld& world) {
    assert(scene.level_collision.has_value());
    assert(scene.level_collision->meshes.size() == 1);
    assert(scene.level_collision->meshes[0].name == "COL_Floor");
    const auto collision_report = stellar::world::validate_level_collision(*scene.level_collision);
    assert(!collision_report.has_errors);
    assert(collision_report.findings.empty());

    const auto metadata_report = stellar::world::validate_world_metadata(world);
    assert(!metadata_report.has_errors);
    for (const auto& finding : metadata_report.findings) {
        assert(finding.code == "raw_extras_json_unparsed");
    }

    assert(world.diagnostics.has_collision);
    assert(world.diagnostics.marker_count == 2);
    assert(world.diagnostics.has_player_spawn);
    assert_imported_collider_marker(world);
    assert_native_session_emits_object_enter(world);
    assert_scripted_object_collider_path(world);
}

} // namespace

int main() {
    const std::filesystem::path fixture_path = write_fixture();
    const auto imported = stellar::import::gltf::load_scene(fixture_path.string());
    assert(imported.has_value());

    const auto world = stellar::world::build_runtime_world(*imported);
    assert_smoke_coverage(*imported, world);

    std::filesystem::remove(fixture_path);
    return EXIT_SUCCESS;
}
