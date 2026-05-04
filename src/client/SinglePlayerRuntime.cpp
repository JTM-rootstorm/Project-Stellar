#include "stellar/client/SinglePlayerRuntime.hpp"

#include <array>
#include <cmath>
#include <iterator>
#include <utility>
#include <variant>

#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/server/MovementSimulation.hpp"
#include "stellar/scripting/ScriptedWorldSession.hpp"

namespace stellar::client {
namespace {

[[nodiscard]] float fixed_dt_or_default(float configured) noexcept {
    const float fixed_dt = std::isfinite(configured) && configured > 0.0F
                               ? configured
                               : stellar::server::MovementSimulationConfig{}.fixed_dt;
    return std::isfinite(fixed_dt) && fixed_dt > 0.0F ? fixed_dt : 1.0F / 60.0F;
}

[[nodiscard]] int sanitized_max_ticks(int value) noexcept {
    return value > 0 ? value : 0;
}

[[nodiscard]] stellar::server::WorldSnapshot snapshot_for(
    const std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession>&
        session) {
    return std::visit([](const auto& active_session) { return active_session.snapshot(); }, session);
}

} // namespace

SinglePlayerRuntime::SinglePlayerRuntime(stellar::authority::PreparedAuthority authority,
                                         SinglePlayerRuntimeConfig config)
    : authority_(std::move(authority)), config_(config),
      latest_snapshot_(stellar::authority::make_network_snapshot(snapshot_for(authority_.session))) {}

ClientRuntimeFrame SinglePlayerRuntime::update(const stellar::platform::Input& input,
                                               float delta_seconds) noexcept {
    ClientRuntimeFrame frame;
    view_state_ = update_client_view_state(input, delta_seconds, view_state_, config_.look_mapper);

    if (std::isfinite(delta_seconds) && delta_seconds > 0.0F) {
        accumulated_seconds_ += delta_seconds;
    }

    const float fixed_dt = fixed_dt_or_default(config_.fixed_dt_seconds);
    const int max_ticks = sanitized_max_ticks(config_.max_ticks_per_update);

    std::vector<stellar::scripting::ScriptOutputEvent> script_events;
    std::vector<stellar::scripting::ScriptError> script_errors;
    std::vector<stellar::scripting::ScriptCommandResult> command_results;

    while (accumulated_seconds_ >= fixed_dt && frame.ticks_run < max_ticks) {
        const stellar::network::MovementCommand movement =
            make_movement_command(input, view_state_, config_.input_mapper);
        const stellar::server::PlayerCommand command{
            .player_id = local_player_id(),
            .movement = stellar::authority::make_server_movement_command(movement),
        };
        const std::array<stellar::server::PlayerCommand, 1> commands{command};

        if (auto* scripted = std::get_if<stellar::scripting::ScriptedWorldSession>(
                &authority_.session)) {
            stellar::scripting::ScriptedWorldFrame scripted_frame = scripted->tick(commands);
            script_events.insert(script_events.end(),
                                 std::make_move_iterator(scripted_frame.script_events.begin()),
                                 std::make_move_iterator(scripted_frame.script_events.end()));
            script_errors.insert(script_errors.end(),
                                 std::make_move_iterator(scripted_frame.script_errors.begin()),
                                 std::make_move_iterator(scripted_frame.script_errors.end()));
            command_results.insert(
                command_results.end(),
                std::make_move_iterator(scripted_frame.command_results.begin()),
                std::make_move_iterator(scripted_frame.command_results.end()));
        } else {
            [[maybe_unused]] const stellar::server::WorldSnapshot tick_snapshot =
                std::get<stellar::server::WorldSession>(authority_.session).tick(commands);
        }

        accumulated_seconds_ -= fixed_dt;
        ++frame.ticks_run;
    }

    if (accumulated_seconds_ >= fixed_dt) {
        frame.dropped_excess_time = true;
        accumulated_seconds_ = 0.0F;
    }

    latest_snapshot_ = stellar::authority::make_network_snapshot(snapshot_for(authority_.session));
    frame.events = stellar::authority::make_gameplay_events(latest_snapshot_.tick, script_events,
                                                            command_results, script_errors);
    frame.snapshot = latest_snapshot_;
    frame.local_player_id = local_player_id();
    frame.session_state = stellar::network::SessionState::kConnected;
    return frame;
}

ClientRuntimeMode SinglePlayerRuntime::mode() const noexcept {
    return ClientRuntimeMode::kSinglePlayer;
}

const stellar::network::NetworkWorldSnapshot& SinglePlayerRuntime::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

stellar::network::PlayerId SinglePlayerRuntime::local_player_id() const noexcept {
    if (!latest_snapshot_.players.empty() && latest_snapshot_.players.front().player_id != 0) {
        return latest_snapshot_.players.front().player_id;
    }
    return stellar::server::WorldSessionConfig{}.local_player_id;
}

} // namespace stellar::client
