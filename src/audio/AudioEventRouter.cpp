#include "stellar/audio/AudioEventRouter.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace stellar::audio {
namespace {

enum class DoorAudioState { kUnknown, kOpen, kClosed };

[[nodiscard]] std::string lower_copy(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return result;
}

[[nodiscard]] bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

[[nodiscard]] DoorAudioState door_state_from_event(const stellar::network::GameplayEvent& event) {
    const std::string code = lower_copy(event.code);
    const std::string message = lower_copy(event.message);

    if (code == "door_open" || code == "open" || contains(message, "opened") ||
        contains(message, "open")) {
        return DoorAudioState::kOpen;
    }
    if (code == "door_close" || code == "door_closed" || code == "closed" || code == "close" ||
        contains(message, "closed") || contains(message, "close")) {
        return DoorAudioState::kClosed;
    }
    return DoorAudioState::kUnknown;
}

[[nodiscard]] AudioPresentationDiagnostic diagnostic(std::string code,
                                                     std::string message,
                                                     std::string sound_id = {}) {
    return AudioPresentationDiagnostic{.code = std::move(code),
                                       .message = std::move(message),
                                       .sound_id = std::move(sound_id)};
}

void append_diagnostics(AudioEventRouteResult& route_result, AudioRequestResult request_result) {
    route_result.diagnostics.insert(route_result.diagnostics.end(),
                                    std::make_move_iterator(request_result.diagnostics.begin()),
                                    std::make_move_iterator(request_result.diagnostics.end()));
}

void submit_request(AudioEventRouteResult& result,
                    AudioRequestSink& sink,
                    const stellar::network::GameplayEvent& event,
                    const std::string& sound_id) {
    if (sound_id.empty()) {
        result.diagnostics.push_back(diagnostic(
            "missing_sound_id", "Audio event mapped to an empty presentation sound id"));
        return;
    }

    AudioPlaybackRequest request{.sound_id = sound_id,
                                 .source_tick = event.tick,
                                 .entity_id = event.entity_id,
                                 .player_id = event.player_id};
    AudioRequestResult request_result = sink.play_one_shot(request);
    ++result.submitted_requests;
    append_diagnostics(result, std::move(request_result));
}

} // namespace

AudioRequestResult NoOpAudioRequestSink::play_one_shot(const AudioPlaybackRequest& request) {
    (void)request;
    return AudioRequestResult{.accepted = true, .diagnostics = {}};
}

AudioEventRouter::AudioEventRouter(AudioEventRouterConfig config) : config_(std::move(config)) {}

AudioEventRouteResult AudioEventRouter::route_event(const stellar::network::GameplayEvent& event,
                                                     AudioRequestSink& sink) const {
    AudioEventRouteResult result{};

    switch (event.kind) {
    case stellar::network::GameplayEventKind::kPickupCollected:
        submit_request(result, sink, event, config_.pickup_sound_id);
        break;
    case stellar::network::GameplayEventKind::kDoorStateChanged: {
        const DoorAudioState state = door_state_from_event(event);
        if (state == DoorAudioState::kOpen) {
            submit_request(result, sink, event, config_.door_open_sound_id);
        } else if (state == DoorAudioState::kClosed) {
            submit_request(result, sink, event, config_.door_close_sound_id);
        } else {
            result.diagnostics.push_back(diagnostic(
                "unknown_door_state",
                "Door/gate audio event did not include open or closed presentation state"));
        }
        break;
    }
    case stellar::network::GameplayEventKind::kScriptError:
        if (!config_.script_error_sound_id.empty()) {
            submit_request(result, sink, event, config_.script_error_sound_id);
        } else if (!event.message.empty()) {
            result.diagnostics.push_back(
                diagnostic("script_error", event.message, config_.script_error_sound_id));
        }
        break;
    case stellar::network::GameplayEventKind::kScriptCommandApplied:
        break;
    }

    return result;
}

AudioEventRouteResult AudioEventRouter::route_events(
    const std::vector<stellar::network::GameplayEvent>& events,
    AudioRequestSink& sink) const {
    AudioEventRouteResult result{};
    for (const stellar::network::GameplayEvent& event : events) {
        AudioEventRouteResult event_result = route_event(event, sink);
        result.submitted_requests += event_result.submitted_requests;
        result.diagnostics.insert(result.diagnostics.end(),
                                  std::make_move_iterator(event_result.diagnostics.begin()),
                                  std::make_move_iterator(event_result.diagnostics.end()));
    }
    return result;
}

} // namespace stellar::audio
