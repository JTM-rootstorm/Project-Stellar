#include "stellar/audio/AudioEventRouter.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeAudioSink final : public stellar::audio::AudioRequestSink {
public:
    explicit FakeAudioSink(std::vector<std::string> available_sounds)
        : available_sounds_(std::move(available_sounds)) {}

    [[nodiscard]] stellar::audio::AudioRequestResult play_one_shot(
        const stellar::audio::AudioPlaybackRequest& request) override {
        requests.push_back(request);
        if (std::find(available_sounds_.begin(), available_sounds_.end(), request.sound_id) ==
            available_sounds_.end()) {
            return stellar::audio::AudioRequestResult{
                .accepted = false,
                .diagnostics = {stellar::audio::AudioPresentationDiagnostic{
                    .code = "missing_sound",
                    .message = "Presentation sound id is not loaded",
                    .sound_id = request.sound_id}}};
        }
        return stellar::audio::AudioRequestResult{.accepted = true, .diagnostics = {}};
    }

    std::vector<stellar::audio::AudioPlaybackRequest> requests;

private:
    std::vector<std::string> available_sounds_;
};

stellar::network::GameplayEvent event(stellar::network::GameplayEventKind kind,
                                      std::uint64_t tick,
                                      std::uint32_t entity_id,
                                      std::string code,
                                      std::string message) {
    return stellar::network::GameplayEvent{.kind = kind,
                                           .tick = tick,
                                           .entity_id = entity_id,
                                           .player_id = 7,
                                           .code = std::move(code),
                                           .message = std::move(message)};
}

void pickup_event_routes_to_pickup_one_shot() {
    stellar::audio::AudioEventRouter router;
    FakeAudioSink sink({"pickup"});

    const auto result = router.route_event(
        event(stellar::network::GameplayEventKind::kPickupCollected, 10, 42, "gem", "Gem"), sink);

    assert(result.submitted_requests == 1);
    assert(result.diagnostics.empty());
    assert(sink.requests.size() == 1);
    assert(sink.requests[0].sound_id == "pickup");
    assert(sink.requests[0].source_tick == 10);
    assert(sink.requests[0].entity_id == 42);
    assert(sink.requests[0].player_id == 7);
}

void door_events_route_to_open_and_close_one_shots() {
    stellar::audio::AudioEventRouter router;
    FakeAudioSink sink({"door_open", "door_close"});

    const auto opened = router.route_event(
        event(stellar::network::GameplayEventKind::kDoorStateChanged, 11, 0, "DoorBlocker",
              "Door opened"),
        sink);
    const auto closed = router.route_event(
        event(stellar::network::GameplayEventKind::kDoorStateChanged, 12, 0, "DoorBlocker",
              "Door closed"),
        sink);

    assert(opened.submitted_requests == 1);
    assert(closed.submitted_requests == 1);
    assert(sink.requests.size() == 2);
    assert(sink.requests[0].sound_id == "door_open");
    assert(sink.requests[1].sound_id == "door_close");
}

void no_op_sink_accepts_events_without_errors() {
    stellar::audio::AudioEventRouter router;
    stellar::audio::NoOpAudioRequestSink sink;

    const auto result = router.route_events(
        {event(stellar::network::GameplayEventKind::kPickupCollected, 1, 2, "", ""),
         event(stellar::network::GameplayEventKind::kDoorStateChanged, 2, 0, "", "Door opened")},
        sink);

    assert(result.submitted_requests == 2);
    assert(result.diagnostics.empty());
}

void missing_sound_is_presentation_diagnostic_only() {
    stellar::audio::AudioEventRouter router;
    FakeAudioSink sink({});

    const auto result = router.route_event(
        event(stellar::network::GameplayEventKind::kPickupCollected, 3, 4, "", ""), sink);

    assert(result.submitted_requests == 1);
    assert(sink.requests.size() == 1);
    assert(result.diagnostics.size() == 1);
    assert(result.diagnostics[0].code == "missing_sound");
    assert(result.diagnostics[0].sound_id == "pickup");
}

void script_error_is_diagnostic_unless_debug_sound_is_configured() {
    stellar::audio::AudioEventRouter router;
    FakeAudioSink sink({"script_error"});

    const auto diagnostic = router.route_event(
        event(stellar::network::GameplayEventKind::kScriptError, 4, 0, "door:on_enter", "Lua failed"),
        sink);

    assert(diagnostic.submitted_requests == 0);
    assert(diagnostic.diagnostics.size() == 1);
    assert(diagnostic.diagnostics[0].code == "script_error");
    assert(sink.requests.empty());

    stellar::audio::AudioEventRouter debug_router(
        stellar::audio::AudioEventRouterConfig{.script_error_sound_id = "script_error"});
    const auto debug = debug_router.route_event(
        event(stellar::network::GameplayEventKind::kScriptError, 5, 0, "door:on_enter", "Lua failed"),
        sink);

    assert(debug.submitted_requests == 1);
    assert(debug.diagnostics.empty());
    assert(sink.requests.size() == 1);
    assert(sink.requests[0].sound_id == "script_error");
}

void unknown_door_state_reports_diagnostic_without_request() {
    stellar::audio::AudioEventRouter router;
    FakeAudioSink sink({"door_open", "door_close"});

    const auto result = router.route_event(
        event(stellar::network::GameplayEventKind::kDoorStateChanged, 6, 0, "DoorBlocker",
              "Collision mesh state updated"),
        sink);

    assert(result.submitted_requests == 0);
    assert(result.diagnostics.size() == 1);
    assert(result.diagnostics[0].code == "unknown_door_state");
    assert(sink.requests.empty());
}

} // namespace

int main() {
    pickup_event_routes_to_pickup_one_shot();
    door_events_route_to_open_and_close_one_shots();
    no_op_sink_accepts_events_without_errors();
    missing_sound_is_presentation_diagnostic_only();
    script_error_is_diagnostic_unless_debug_sound_is_configured();
    unknown_door_state_reports_diagnostic_without_request();
    return 0;
}
