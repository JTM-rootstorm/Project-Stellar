#include "stellar/server/ServerRuntime.hpp"

#include <array>
#include <cmath>
#include <iterator>
#include <utility>

#include "stellar/authority/NetworkConversion.hpp"
#include "stellar/network/SnapshotCodec.hpp"
#include "stellar/network/SnapshotDelta.hpp"

namespace stellar::server {
namespace {

[[nodiscard]] float fixed_dt_or_default(
    const stellar::server::MovementSimulationConfig& movement) noexcept {
    if (std::isfinite(movement.fixed_dt) && movement.fixed_dt > 0.0F) {
        return movement.fixed_dt;
    }
    return stellar::server::MovementSimulationConfig{}.fixed_dt;
}

[[nodiscard]] int sanitized_max_ticks(int max_ticks_per_pump) noexcept {
    return max_ticks_per_pump > 0 ? max_ticks_per_pump : 0;
}

[[nodiscard]] float snapshot_dt_or_zero(int snapshot_rate) noexcept {
    return snapshot_rate > 0 ? 1.0F / static_cast<float>(snapshot_rate) : 0.0F;
}

void record_error(ServerRuntimePumpResult& result, std::string code, std::string message) {
    ++result.rejected_packets;
    result.errors.push_back(ServerRuntimeError{.code = std::move(code),
                                               .message = std::move(message)});
}

[[nodiscard]] std::expected<void, stellar::network::TransportError> send_bytes(
    stellar::network::ServerTransport& transport,
    std::vector<std::uint8_t> bytes) {
    return transport.send_to_client(stellar::network::TransportPacket{
        .channel = stellar::network::TransportChannel::kReliable,
        .payload = stellar::network::to_payload(bytes),
    });
}

[[nodiscard]] stellar::network::ServerWelcome accepted_welcome(
    const ServerRuntimeConfig& config,
    stellar::network::SessionId session_id) {
    return stellar::network::ServerWelcome{
        .accepted = true,
        .protocol_version = stellar::network::kCurrentProtocolVersion,
        .session_id = session_id,
        .assigned_player_id = config.session.local_player_id,
        .map_id = config.map_identity.map_id,
        .rejection_code = {},
        .message = "session accepted",
    };
}

[[nodiscard]] stellar::network::ServerWelcome rejected_welcome(const ServerRuntimeConfig& config,
                                                               std::string code,
                                                               std::string message) {
    return stellar::network::ServerWelcome{
        .accepted = false,
        .protocol_version = stellar::network::kCurrentProtocolVersion,
        .session_id = 0,
        .assigned_player_id = 0,
        .map_id = config.map_identity.map_id,
        .rejection_code = std::move(code),
        .message = std::move(message),
    };
}

void send_welcome_or_record(ServerRuntimePumpResult& result,
                            stellar::network::ServerTransport& transport,
                            const stellar::network::ServerWelcome& welcome) {
    auto encoded = stellar::network::encode_server_welcome(welcome);
    if (!encoded) {
        record_error(result, encoded.error().code, encoded.error().message);
        return;
    }
    if (auto sent = send_bytes(transport, std::move(*encoded)); !sent) {
        record_error(result, sent.error().code, sent.error().message);
    }
}

} // namespace

ServerRuntime::ServerRuntime(const stellar::world::RuntimeWorld& world, ServerRuntimeConfig config)
    : config_(config),
      session_(std::in_place_type<stellar::server::WorldSession>, world, config_.session),
      latest_snapshot_(stellar::authority::make_network_snapshot(
          std::get<stellar::server::WorldSession>(session_).snapshot())) {
    pending_command_.player_id = config_.session.local_player_id;
}

ServerRuntime::ServerRuntime(stellar::scripting::ScriptedWorldSession scripted_session,
                             ServerRuntimeConfig config)
    : config_(config),
      session_(std::in_place_type<stellar::scripting::ScriptedWorldSession>,
               std::move(scripted_session)),
      latest_snapshot_(stellar::authority::make_network_snapshot(
          std::get<stellar::scripting::ScriptedWorldSession>(session_).snapshot())) {
    pending_command_.player_id = config_.session.local_player_id;
}

ServerRuntime::ServerRuntime(
    std::variant<stellar::server::WorldSession, stellar::scripting::ScriptedWorldSession> session,
    ServerRuntimeConfig config)
    : config_(config),
      session_(std::move(session)),
      latest_snapshot_(stellar::authority::make_network_snapshot(
          std::visit([](const auto& active_session) { return active_session.snapshot(); }, session_))) {
    pending_command_.player_id = config_.session.local_player_id;
}

const stellar::network::NetworkWorldSnapshot& ServerRuntime::latest_snapshot() const noexcept {
    return latest_snapshot_;
}

stellar::network::SessionState ServerRuntime::session_state() const noexcept {
    return session_state_;
}

std::uint32_t ServerRuntime::connected_clients() const noexcept {
    return connected_clients_;
}

ServerRuntimePumpResult ServerRuntime::pump(stellar::network::ServerTransport& transport,
                                            float delta_seconds) noexcept {
    ServerRuntimePumpResult result;
    for (const stellar::network::TransportPacket& packet : transport.receive_from_client()) {
        const std::vector<std::uint8_t> bytes = stellar::network::from_payload(packet.payload);
        if (auto hello = stellar::network::decode_client_hello(bytes)) {
            if (session_state_ == stellar::network::SessionState::kConnected && connected_clients_ >= 1) {
                send_welcome_or_record(result, transport,
                                       rejected_welcome(config_, "server_full",
                                                        "Server already has an active client"));
                continue;
            }
            if (hello->protocol_version != stellar::network::kCurrentProtocolVersion) {
                session_state_ = stellar::network::SessionState::kRejected;
                send_welcome_or_record(
                    result, transport,
                    rejected_welcome(config_, "protocol_mismatch",
                                     "Client protocol version is unsupported"));
                continue;
            }
            if (!hello->requested_map_id.empty() &&
                hello->requested_map_id != config_.map_identity.map_id) {
                session_state_ = stellar::network::SessionState::kRejected;
                send_welcome_or_record(result, transport,
                                       rejected_welcome(config_, "map_mismatch",
                                                        "Client requested map does not match server map"));
                continue;
            }
            session_state_ = stellar::network::SessionState::kConnected;
            connected_clients_ = 1;
            send_welcome_or_record(result, transport, accepted_welcome(config_, session_id_));
            continue;
        }

        auto command = stellar::network::decode_player_command(bytes);
        if (!command) {
            record_error(result, command.error().code, command.error().message);
            continue;
        }
        if (session_state_ != stellar::network::SessionState::kConnected) {
            record_error(result, "input_before_welcome",
                         "Input command arrived before an accepted session welcome");
            continue;
        }
        command->player_id = config_.session.local_player_id;
        pending_command_ = *command;
    }

    result.session_state = session_state_;
    if (session_state_ != stellar::network::SessionState::kConnected) {
        return result;
    }

    const float fixed_dt = fixed_dt_or_default(config_.session.movement);
    const int max_ticks = sanitized_max_ticks(config_.max_ticks_per_pump);
    if (std::isfinite(delta_seconds) && delta_seconds > 0.0F) {
        accumulated_seconds_ += delta_seconds;
        snapshot_seconds_ += delta_seconds;
    }

    std::vector<stellar::scripting::ScriptOutputEvent> script_events;
    std::vector<stellar::scripting::ScriptError> script_errors;
    std::vector<stellar::scripting::ScriptCommandResult> command_results;
    std::vector<stellar::server::FootstepEvent> footstep_events;

    while (accumulated_seconds_ >= fixed_dt && result.ticks_run < max_ticks) {
        const stellar::server::PlayerCommand command{.player_id = config_.session.local_player_id,
                                                     .movement = stellar::authority::
                                                         make_server_movement_command(
                                                             pending_command_.movement)};
        const std::array<stellar::server::PlayerCommand, 1> commands{command};
        if (auto* scripted = std::get_if<stellar::scripting::ScriptedWorldSession>(&session_)) {
            stellar::scripting::ScriptedWorldFrame frame = scripted->tick(commands);
            footstep_events.insert(
                footstep_events.end(),
                std::make_move_iterator(frame.snapshot.footstep_events.begin()),
                std::make_move_iterator(frame.snapshot.footstep_events.end()));
            script_events.insert(script_events.end(),
                                 std::make_move_iterator(frame.script_events.begin()),
                                 std::make_move_iterator(frame.script_events.end()));
            script_errors.insert(script_errors.end(),
                                 std::make_move_iterator(frame.script_errors.begin()),
                                 std::make_move_iterator(frame.script_errors.end()));
            command_results.insert(command_results.end(),
                                   std::make_move_iterator(frame.command_results.begin()),
                                   std::make_move_iterator(frame.command_results.end()));
        } else {
            stellar::server::WorldSnapshot tick_snapshot =
                std::get<stellar::server::WorldSession>(session_).tick(commands);
            footstep_events.insert(
                footstep_events.end(),
                std::make_move_iterator(tick_snapshot.footstep_events.begin()),
                std::make_move_iterator(tick_snapshot.footstep_events.end()));
        }
        accumulated_seconds_ -= fixed_dt;
        ++result.ticks_run;
    }

    if (accumulated_seconds_ >= fixed_dt) {
        result.dropped_excess_time = true;
        accumulated_seconds_ = 0.0F;
    }

    const stellar::server::WorldSnapshot snapshot =
        std::visit([](const auto& session) { return session.snapshot(); }, session_);
    const stellar::network::NetworkWorldSnapshot next_snapshot =
        stellar::authority::make_network_snapshot(snapshot);

    const float snapshot_dt = snapshot_dt_or_zero(config_.snapshot_rate);
    const bool snapshot_due = !has_sent_snapshot_ || snapshot_dt <= 0.0F ||
                              snapshot_seconds_ >= snapshot_dt ||
                              next_snapshot.tick != latest_snapshot_.tick;
    if (snapshot_due) {
        std::expected<std::vector<std::uint8_t>, stellar::network::CodecError> encoded_state;
        if (has_sent_snapshot_ && config_.emit_deltas) {
            encoded_state = stellar::network::encode_snapshot_delta(
                stellar::network::make_snapshot_delta(latest_snapshot_, next_snapshot));
        } else {
            encoded_state = stellar::network::encode_snapshot(next_snapshot);
        }
        if (!encoded_state) {
            record_error(result, encoded_state.error().code, encoded_state.error().message);
        } else if (auto sent = send_bytes(transport, std::move(*encoded_state)); !sent) {
            record_error(result, sent.error().code, sent.error().message);
            session_state_ = stellar::network::SessionState::kDisconnected;
            connected_clients_ = 0;
        } else {
            latest_snapshot_ = next_snapshot;
            has_sent_snapshot_ = true;
            snapshot_seconds_ = 0.0F;
        }
    }

    for (const stellar::network::GameplayEvent& event : stellar::authority::make_gameplay_events(
             latest_snapshot_.tick, script_events, command_results, script_errors,
             footstep_events)) {
        auto encoded_event = stellar::network::encode_gameplay_event(event);
        if (!encoded_event) {
            record_error(result, encoded_event.error().code, encoded_event.error().message);
            continue;
        }
        if (auto sent = send_bytes(transport, std::move(*encoded_event)); !sent) {
            record_error(result, sent.error().code, sent.error().message);
            session_state_ = stellar::network::SessionState::kDisconnected;
            connected_clients_ = 0;
        }
    }

    result.session_state = session_state_;
    return result;
}

} // namespace stellar::server
