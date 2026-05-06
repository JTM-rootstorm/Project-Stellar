#pragma once

#include "stellar/audio/AudioEventRouter.hpp"
#include "stellar/platform/Error.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace stellar::audio {

/** @brief One sound-id to file-path entry for miniaudio-backed presentation playback. */
struct MiniaudioSoundRegistryEntry {
    /** @brief Stable presentation sound id requested by AudioEventRouter. */
    std::string sound_id;

    /** @brief Local audio asset path to play for the sound id. */
    std::filesystem::path path;
};

/** @brief Runtime configuration for the presentation-only miniaudio request sink. */
struct MiniaudioRequestSinkConfig {
    /** @brief Root directory for generated footstep WAV assets. */
    std::filesystem::path generated_footstep_root;

    /** @brief Explicit sound registry entries; empty uses generated footstep defaults. */
    std::vector<MiniaudioSoundRegistryEntry> sound_entries;
};

/** @brief Device-free decoded metadata for a local audio asset. */
struct MiniaudioDecodedAudioInfo {
    /** @brief Number of decoded output channels. */
    std::uint32_t channels = 0;

    /** @brief Decoded output sample rate in hertz. */
    std::uint32_t sample_rate = 0;

    /** @brief Decoded PCM frame count reported by miniaudio. */
    std::uint64_t pcm_frame_count = 0;
};

/** @brief Result of selecting the runtime audio request sink for the client presentation layer. */
struct RuntimeAudioRequestSink {
    /** @brief Selected presentation audio sink; never null on return. */
    std::unique_ptr<AudioRequestSink> sink;

    /** @brief Presentation-only diagnostics produced while selecting or initializing the sink. */
    std::vector<AudioPresentationDiagnostic> diagnostics;

    /** @brief True when STELLAR_ENABLE_AUDIO requested audible playback. */
    bool audio_requested = false;

    /** @brief True when a miniaudio backend initialized and is the active sink. */
    bool miniaudio_active = false;
};

/** @brief Return the default generated footstep audio asset root. */
[[nodiscard]] std::filesystem::path default_generated_footstep_audio_root();

/** @brief Build sound-id entries for generated footstep one-shot WAV assets. */
[[nodiscard]] std::vector<MiniaudioSoundRegistryEntry> generated_footstep_sound_entries(
    const std::filesystem::path& generated_footstep_root);

/** @brief Return true when an environment value explicitly requests audible audio. */
[[nodiscard]] bool audio_enabled_from_environment();

/** @brief Decode local audio metadata without initializing an audio playback device. */
[[nodiscard]] std::expected<MiniaudioDecodedAudioInfo, stellar::platform::Error>
decode_audio_file_info(const std::filesystem::path& path);

/** @brief Presentation-only miniaudio sink for one-shot audio requests. */
class MiniaudioRequestSink final : public AudioRequestSink {
public:
    /** @brief Construct a sink with the default generated-footstep registry. */
    MiniaudioRequestSink();

    /** @brief Construct a sink with an explicit registry config, without opening a device. */
    explicit MiniaudioRequestSink(MiniaudioRequestSinkConfig config);

    /** @brief Release miniaudio resources. */
    ~MiniaudioRequestSink() override;

    MiniaudioRequestSink(const MiniaudioRequestSink&) = delete;
    MiniaudioRequestSink& operator=(const MiniaudioRequestSink&) = delete;

    /** @brief Move a presentation audio sink and its owned miniaudio resources. */
    MiniaudioRequestSink(MiniaudioRequestSink&& other) noexcept;

    /** @brief Move-assign a presentation audio sink and its owned miniaudio resources. */
    MiniaudioRequestSink& operator=(MiniaudioRequestSink&& other) noexcept;

    /** @brief Initialize miniaudio device resources for presentation playback. */
    [[nodiscard]] std::expected<void, stellar::platform::Error> initialize(
        MiniaudioRequestSinkConfig config = {});

    /** @brief Submit a one-shot presentation audio request through miniaudio. */
    [[nodiscard]] AudioRequestResult play_one_shot(const AudioPlaybackRequest& request) override;

    /** @brief Shut down miniaudio resources and keep the registry available for diagnostics. */
    void shutdown() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/** @brief Select a no-op or miniaudio-backed request sink from STELLAR_ENABLE_AUDIO. */
[[nodiscard]] RuntimeAudioRequestSink make_runtime_audio_request_sink(
    MiniaudioRequestSinkConfig config = {});

} // namespace stellar::audio
