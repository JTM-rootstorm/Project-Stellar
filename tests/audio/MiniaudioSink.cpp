#include "stellar/audio/MiniaudioSink.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifndef STELLAR_PROJECT_SOURCE_DIR
#error "STELLAR_PROJECT_SOURCE_DIR must be defined for miniaudio sink tests"
#endif

namespace {

void clear_audio_environment() {
#if defined(_WIN32)
    _putenv_s("STELLAR_ENABLE_AUDIO", "0");
#else
    unsetenv("STELLAR_ENABLE_AUDIO");
#endif
}

void generated_footstep_registry_maps_expected_assets() {
    const std::filesystem::path root = std::filesystem::path(STELLAR_PROJECT_SOURCE_DIR) /
                                      "assets" / "audio" / "footsteps" / "generated";
    const std::vector<stellar::audio::MiniaudioSoundRegistryEntry> entries =
        stellar::audio::generated_footstep_sound_entries(root);

    assert(entries.size() == 16);
    assert(entries.front().sound_id == "footstep_generic_0");
    assert(entries.front().path == root / "footstep_generic_0.wav");
    assert(entries.back().sound_id == "footstep_water_1");
    assert(entries.back().path == root / "footstep_water_1.wav");
}

void environment_factory_defaults_to_no_op_without_device() {
    clear_audio_environment();

    stellar::audio::RuntimeAudioRequestSink runtime_sink =
        stellar::audio::make_runtime_audio_request_sink();

    assert(runtime_sink.sink != nullptr);
    assert(!runtime_sink.audio_requested);
    assert(!runtime_sink.miniaudio_active);
    assert(runtime_sink.diagnostics.empty());

    const stellar::audio::AudioRequestResult result =
        runtime_sink.sink->play_one_shot(stellar::audio::AudioPlaybackRequest{
            .sound_id = "footstep_generic_0",
            .source_tick = 1,
            .entity_id = 2,
            .player_id = 3,
        });
    assert(result.accepted);
    assert(result.diagnostics.empty());
}

void uninitialized_sink_reports_presentation_diagnostics_without_device() {
    const std::filesystem::path missing_root = std::filesystem::path(STELLAR_PROJECT_SOURCE_DIR) /
                                              "build" / "definitely_missing_audio_assets";
    stellar::audio::MiniaudioRequestSink sink(
        stellar::audio::MiniaudioRequestSinkConfig{.generated_footstep_root = missing_root});

    const stellar::audio::AudioRequestResult missing = sink.play_one_shot(
        stellar::audio::AudioPlaybackRequest{.sound_id = "footstep_wood_0"});
    assert(!missing.accepted);
    assert(missing.diagnostics.size() == 1);
    assert(missing.diagnostics[0].code == "missing_sound_asset");
    assert(missing.diagnostics[0].sound_id == "footstep_wood_0");

    const stellar::audio::AudioRequestResult unknown = sink.play_one_shot(
        stellar::audio::AudioPlaybackRequest{.sound_id = "not_registered"});
    assert(!unknown.accepted);
    assert(unknown.diagnostics.size() == 1);
    assert(unknown.diagnostics[0].code == "unknown_sound_id");
    assert(unknown.diagnostics[0].sound_id == "not_registered");
}

} // namespace

int main() {
    generated_footstep_registry_maps_expected_assets();
    environment_factory_defaults_to_no_op_without_device();
    uninitialized_sink_reports_presentation_diagnostics_without_device();
    return 0;
}
