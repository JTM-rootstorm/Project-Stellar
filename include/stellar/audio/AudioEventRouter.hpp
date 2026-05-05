#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stellar/network/Messages.hpp"

namespace stellar::audio {

/** @brief Presentation-only one-shot audio playback request. */
struct AudioPlaybackRequest {
    /** @brief Stable presentation sound identifier requested by the event router. */
    std::string sound_id;

    /** @brief Server tick that approved the gameplay event, when available. */
    std::uint64_t source_tick = 0;

    /** @brief Authoritative entity/collider id related to the source event, when known. */
    std::uint32_t entity_id = 0;

    /** @brief Authoritative player id related to the source event, when known. */
    std::uint32_t player_id = 0;
};

/** @brief Presentation-only diagnostic emitted by audio routing or a playback sink. */
struct AudioPresentationDiagnostic {
    /** @brief Stable machine-readable diagnostic code. */
    std::string code;

    /** @brief Human-readable deterministic diagnostic message. */
    std::string message;

    /** @brief Sound id related to the diagnostic, when applicable. */
    std::string sound_id;
};

/** @brief Result returned by a presentation audio request sink. */
struct AudioRequestResult {
    /** @brief True when the sink accepted or intentionally no-oped the request. */
    bool accepted = true;

    /** @brief Presentation-only diagnostics such as missing local sound ids. */
    std::vector<AudioPresentationDiagnostic> diagnostics;
};

/** @brief Sink interface for presentation-only audio requests. */
class AudioRequestSink {
public:
    /** @brief Release presentation audio sink resources. */
    virtual ~AudioRequestSink() = default;

    /** @brief Submit a one-shot presentation audio request without affecting gameplay state. */
    [[nodiscard]] virtual AudioRequestResult play_one_shot(
        const AudioPlaybackRequest& request) = 0;
};

/** @brief Explicit no-op sink for headless, unavailable, or intentionally disabled audio. */
class NoOpAudioRequestSink final : public AudioRequestSink {
public:
    /** @brief Accept and discard a one-shot request as an explicit presentation no-op. */
    [[nodiscard]] AudioRequestResult play_one_shot(const AudioPlaybackRequest& request) override;
};

/** @brief Mapping from an authoritative footstep surface id to a presentation sound prefix. */
struct FootstepSurfaceSound {
    /** @brief Server-approved footstep surface id such as wood, metal, or generic. */
    std::string surface_id;

    /** @brief Presentation sound id prefix before the deterministic variant suffix. */
    std::string sound_prefix;
};

/** @brief Router configuration for server-approved gameplay event audio mapping. */
struct AudioEventRouterConfig {
    /** @brief Sound id for pickup collection one-shots. */
    std::string pickup_sound_id = "pickup";

    /** @brief Sound id for door/gate opening one-shots. */
    std::string door_open_sound_id = "door_open";

    /** @brief Sound id for door/gate closing one-shots. */
    std::string door_close_sound_id = "door_close";

    /** @brief Optional debug sound id for script errors; empty disables script-error audio. */
    std::string script_error_sound_id;

    /** @brief Number of deterministic variants per footstep surface; zero is treated as one. */
    std::uint32_t footstep_variant_count = 2;

    /** @brief Fallback footstep sound prefix for empty or unknown surface ids. */
    std::string footstep_generic_prefix = "footstep_generic";

    /** @brief Stable surface-to-prefix mappings for server-approved footstep events. */
    std::vector<FootstepSurfaceSound> footstep_surface_sounds = {
        {"generic", "footstep_generic"}, {"concrete", "footstep_concrete"},
        {"metal", "footstep_metal"},     {"wood", "footstep_wood"},
        {"stone", "footstep_stone"},     {"dirt", "footstep_dirt"},
        {"grass", "footstep_grass"},     {"water", "footstep_water"}};
};

/** @brief Result of routing server-approved events into presentation audio requests. */
struct AudioEventRouteResult {
    /** @brief Count of one-shot requests submitted to the sink. */
    std::uint32_t submitted_requests = 0;

    /** @brief Presentation-only diagnostics collected while routing or submitting requests. */
    std::vector<AudioPresentationDiagnostic> diagnostics;
};

/** @brief Presentation-only router from server-approved gameplay events to audio requests. */
class AudioEventRouter {
public:
    /** @brief Construct an audio event router with deterministic sound-id mapping. */
    explicit AudioEventRouter(AudioEventRouterConfig config = {});

    /** @brief Route one server-approved gameplay event to a presentation audio sink. */
    [[nodiscard]] AudioEventRouteResult route_event(
        const stellar::network::GameplayEvent& event,
        AudioRequestSink& sink) const;

    /** @brief Route multiple server-approved gameplay events to a presentation audio sink. */
    [[nodiscard]] AudioEventRouteResult route_events(
        const std::vector<stellar::network::GameplayEvent>& events,
        AudioRequestSink& sink) const;

private:
    AudioEventRouterConfig config_;
};

} // namespace stellar::audio
