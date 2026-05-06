#include "stellar/audio/MiniaudioSink.hpp"

#include <miniaudio.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace stellar::audio {
namespace {

#ifndef STELLAR_PROJECT_SOURCE_DIR
#define STELLAR_PROJECT_SOURCE_DIR "."
#endif

constexpr std::array<std::string_view, 8> kFootstepSurfaces{
    "generic", "concrete", "metal", "wood", "stone", "dirt", "grass", "water"};
constexpr int kGeneratedFootstepVariantCount = 2;

[[nodiscard]] AudioPresentationDiagnostic diagnostic(std::string code,
                                                     std::string message,
                                                     std::string sound_id = {}) {
    return AudioPresentationDiagnostic{.code = std::move(code),
                                       .message = std::move(message),
                                       .sound_id = std::move(sound_id)};
}

[[nodiscard]] std::string miniaudio_result_message(std::string_view prefix, ma_result result) {
    std::string message(prefix);
    message += ": ";
    message += ma_result_description(result);
    return message;
}

[[nodiscard]] bool explicit_audio_enabled_value(std::string_view value) noexcept {
    return value == "1" || value == "true" || value == "TRUE" || value == "on" ||
           value == "ON" || value == "yes" || value == "YES";
}

[[nodiscard]] bool file_exists(const std::filesystem::path& path) noexcept {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

[[nodiscard]] MiniaudioRequestSinkConfig normalized_config(MiniaudioRequestSinkConfig config) {
    if (config.generated_footstep_root.empty()) {
        config.generated_footstep_root = default_generated_footstep_audio_root();
    }
    if (config.sound_entries.empty()) {
        config.sound_entries = generated_footstep_sound_entries(config.generated_footstep_root);
    }
    return config;
}

} // namespace

struct MiniaudioRequestSink::Impl {
    ma_engine engine{};
    bool initialized = false;
    std::unordered_map<std::string, std::filesystem::path> registry;

    void configure_registry(MiniaudioRequestSinkConfig config) {
        config = normalized_config(std::move(config));
        registry.clear();
        for (MiniaudioSoundRegistryEntry& entry : config.sound_entries) {
            if (!entry.sound_id.empty()) {
                registry.insert_or_assign(std::move(entry.sound_id), std::move(entry.path));
            }
        }
    }
};

std::filesystem::path default_generated_footstep_audio_root() {
    return std::filesystem::path(STELLAR_PROJECT_SOURCE_DIR) / "assets" / "audio" / "footsteps" /
           "generated";
}

std::vector<MiniaudioSoundRegistryEntry> generated_footstep_sound_entries(
    const std::filesystem::path& generated_footstep_root) {
    std::vector<MiniaudioSoundRegistryEntry> entries;
    entries.reserve(kFootstepSurfaces.size() * kGeneratedFootstepVariantCount);

    for (std::string_view surface : kFootstepSurfaces) {
        for (int variant = 0; variant < kGeneratedFootstepVariantCount; ++variant) {
            std::string sound_id = "footstep_";
            sound_id += surface;
            sound_id += "_";
            sound_id += std::to_string(variant);

            entries.push_back(MiniaudioSoundRegistryEntry{
                .sound_id = sound_id,
                .path = generated_footstep_root / (sound_id + ".wav"),
            });
        }
    }

    return entries;
}

bool audio_enabled_from_environment() {
    const char* value = std::getenv("STELLAR_ENABLE_AUDIO");
    return value != nullptr && explicit_audio_enabled_value(value);
}

MiniaudioRequestSink::MiniaudioRequestSink() : MiniaudioRequestSink(MiniaudioRequestSinkConfig{}) {}

MiniaudioRequestSink::MiniaudioRequestSink(MiniaudioRequestSinkConfig config)
    : impl_(std::make_unique<Impl>()) {
    impl_->configure_registry(std::move(config));
}

MiniaudioRequestSink::~MiniaudioRequestSink() {
    shutdown();
}

MiniaudioRequestSink::MiniaudioRequestSink(MiniaudioRequestSink&& other) noexcept = default;

MiniaudioRequestSink& MiniaudioRequestSink::operator=(MiniaudioRequestSink&& other) noexcept {
    if (this != &other) {
        shutdown();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

std::expected<void, stellar::platform::Error> MiniaudioRequestSink::initialize(
    MiniaudioRequestSinkConfig config) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }

    shutdown();
    impl_->configure_registry(std::move(config));

    const ma_result result = ma_engine_init(nullptr, &impl_->engine);
    if (result != MA_SUCCESS) {
        return std::unexpected(stellar::platform::Error(
            miniaudio_result_message("miniaudio engine initialization failed", result)));
    }

    impl_->initialized = true;
    return {};
}

AudioRequestResult MiniaudioRequestSink::play_one_shot(const AudioPlaybackRequest& request) {
    if (impl_ == nullptr) {
        return AudioRequestResult{
            .accepted = false,
            .diagnostics = {diagnostic("audio_sink_unavailable",
                                       "Miniaudio request sink has no implementation",
                                       request.sound_id)}};
    }

    const auto sound = impl_->registry.find(request.sound_id);
    if (sound == impl_->registry.end()) {
        return AudioRequestResult{
            .accepted = false,
            .diagnostics = {diagnostic("unknown_sound_id",
                                       "Presentation sound id is not registered",
                                       request.sound_id)}};
    }
    if (!file_exists(sound->second)) {
        return AudioRequestResult{
            .accepted = false,
            .diagnostics = {diagnostic("missing_sound_asset",
                                       "Registered presentation sound asset is missing",
                                       request.sound_id)}};
    }
    if (!impl_->initialized) {
        return AudioRequestResult{
            .accepted = false,
            .diagnostics = {diagnostic("audio_sink_uninitialized",
                                       "Miniaudio request sink is not initialized",
                                       request.sound_id)}};
    }

    const std::string path = sound->second.string();
    const ma_result result = ma_engine_play_sound(&impl_->engine, path.c_str(), nullptr);
    if (result != MA_SUCCESS) {
        return AudioRequestResult{
            .accepted = false,
            .diagnostics = {diagnostic("miniaudio_playback_failed",
                                       miniaudio_result_message(
                                           "miniaudio failed to play presentation sound", result),
                                       request.sound_id)}};
    }

    return AudioRequestResult{.accepted = true, .diagnostics = {}};
}

void MiniaudioRequestSink::shutdown() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        ma_engine_uninit(&impl_->engine);
        impl_->engine = ma_engine{};
        impl_->initialized = false;
    }
}

RuntimeAudioRequestSink make_runtime_audio_request_sink(MiniaudioRequestSinkConfig config) {
    RuntimeAudioRequestSink result{
        .sink = std::make_unique<NoOpAudioRequestSink>(),
        .diagnostics = {},
        .audio_requested = audio_enabled_from_environment(),
        .miniaudio_active = false,
    };

    if (!result.audio_requested) {
        return result;
    }

    auto sink = std::make_unique<MiniaudioRequestSink>();
    if (auto initialized = sink->initialize(std::move(config)); !initialized) {
        result.diagnostics.push_back(diagnostic(
            "miniaudio_initialization_failed",
            initialized.error().message));
        return result;
    }

    result.miniaudio_active = true;
    result.sink = std::move(sink);
    return result;
}

} // namespace stellar::audio
