#include "stellar/audio/MiniaudioSink.hpp"

#include <chrono>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kFailure = 1;
constexpr int kUsageError = 2;
constexpr int kSkip = 77;
constexpr int kDefaultDurationMs = 2500;
constexpr int kDefaultGapMs = 250;

struct Options {
    std::vector<std::string> sound_ids{"footstep_concrete_0", "footstep_metal_1"};
    std::filesystem::path asset_root;
    int duration_ms = kDefaultDurationMs;
    int gap_ms = kDefaultGapMs;
    bool list_defaults = false;
};

void print_usage(std::ostream& output) {
    output << "usage: stellar-audio-smoke [options]\n"
           << "  --sound <sound_id>       repeatable; replaces default sounds\n"
           << "  --duration-ms <ms>       default " << kDefaultDurationMs << "\n"
           << "  --gap-ms <ms>            default " << kDefaultGapMs << "\n"
           << "  --list-defaults          list generated footstep registry entries\n"
           << "  --asset-root <path>      generated footstep WAV root\n";
}

[[nodiscard]] bool parse_non_negative_int(std::string_view text, int& value) {
    int parsed = 0;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, parsed);
    if (error != std::errc{} || position != end || parsed < 0) {
        return false;
    }

    value = parsed;
    return true;
}

[[nodiscard]] stellar::audio::MiniaudioRequestSinkConfig make_config(const Options& options) {
    stellar::audio::MiniaudioRequestSinkConfig config;
    config.generated_footstep_root = options.asset_root;
    return config;
}

[[nodiscard]] std::filesystem::path effective_asset_root(const Options& options) {
    if (!options.asset_root.empty()) {
        return options.asset_root;
    }
    return stellar::audio::default_generated_footstep_audio_root();
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options) {
    bool saw_explicit_sound = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);

        if (argument == "--help" || argument == "-h") {
            print_usage(std::cout);
            std::exit(kSuccess);
        }
        if (argument == "--list-defaults") {
            options.list_defaults = true;
            continue;
        }
        if (argument == "--sound") {
            if (i + 1 >= argc) {
                std::cerr << "stellar-audio-smoke: usage_error missing_value=--sound\n";
                return false;
            }
            if (!saw_explicit_sound) {
                options.sound_ids.clear();
                saw_explicit_sound = true;
            }
            options.sound_ids.emplace_back(argv[++i]);
            continue;
        }
        if (argument == "--duration-ms") {
            if (i + 1 >= argc ||
                !parse_non_negative_int(argv[++i], options.duration_ms)) {
                std::cerr << "stellar-audio-smoke: usage_error invalid_value=--duration-ms\n";
                return false;
            }
            continue;
        }
        if (argument == "--gap-ms") {
            if (i + 1 >= argc || !parse_non_negative_int(argv[++i], options.gap_ms)) {
                std::cerr << "stellar-audio-smoke: usage_error invalid_value=--gap-ms\n";
                return false;
            }
            continue;
        }
        if (argument == "--asset-root") {
            if (i + 1 >= argc) {
                std::cerr << "stellar-audio-smoke: usage_error missing_value=--asset-root\n";
                return false;
            }
            options.asset_root = argv[++i];
            continue;
        }

        std::cerr << "stellar-audio-smoke: usage_error unknown_option=" << argument << "\n";
        return false;
    }

    if (options.sound_ids.empty()) {
        std::cerr << "stellar-audio-smoke: usage_error missing_sound=1\n";
        return false;
    }
    return true;
}

void print_diagnostics(
    const std::vector<stellar::audio::AudioPresentationDiagnostic>& diagnostics) {
    for (const stellar::audio::AudioPresentationDiagnostic& diagnostic : diagnostics) {
        std::cerr << "stellar-audio-smoke: audio diagnostic code=" << diagnostic.code
                  << " sound_id=" << diagnostic.sound_id
                  << " message=\"" << diagnostic.message << "\"\n";
    }
}

[[nodiscard]] bool operator_confirmed_audio() {
    const char* const value = std::getenv("STELLAR_AUDIO_SMOKE_CONFIRM");
    return value != nullptr && std::string_view(value) == "heard";
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(std::cerr);
        return kUsageError;
    }

    const std::filesystem::path asset_root = effective_asset_root(options);
    if (options.list_defaults) {
        const std::vector<stellar::audio::MiniaudioSoundRegistryEntry> entries =
            stellar::audio::generated_footstep_sound_entries(asset_root);
        for (const stellar::audio::MiniaudioSoundRegistryEntry& entry : entries) {
            std::cout << "stellar-audio-smoke: default sound_id=" << entry.sound_id
                      << " path=\"" << entry.path.string() << "\"\n";
        }
        return kSuccess;
    }

    const bool audio_requested = stellar::audio::audio_enabled_from_environment();
    std::cout << "stellar-audio-smoke: audio_requested=" << (audio_requested ? 1 : 0) << "\n";
    if (!audio_requested) {
        std::cout << "stellar-audio-smoke: skip reason=STELLAR_ENABLE_AUDIO_not_enabled\n";
        return kSkip;
    }

    stellar::audio::MiniaudioRequestSink sink;
    const auto initialized = sink.initialize(make_config(options));
    if (!initialized) {
        std::cout << "stellar-audio-smoke: miniaudio_active=0\n";
        std::cout << "stellar-audio-smoke: skip reason=miniaudio_initialization_failed"
                  << " message=\"" << initialized.error().message << "\"\n";
        return kSkip;
    }
    std::cout << "stellar-audio-smoke: miniaudio_active=1\n";
    std::cout << "stellar-audio-smoke: asset_root=\"" << asset_root.string() << "\"\n";

    bool accepted_all = true;
    for (std::size_t index = 0; index < options.sound_ids.size(); ++index) {
        const std::string& sound_id = options.sound_ids[index];
        const stellar::audio::AudioRequestResult result =
            sink.play_one_shot(stellar::audio::AudioPlaybackRequest{.sound_id = sound_id});
        std::cout << "stellar-audio-smoke: play sound_id=" << sound_id
                  << " accepted=" << (result.accepted ? 1 : 0) << "\n";
        print_diagnostics(result.diagnostics);
        accepted_all = accepted_all && result.accepted;

        if (index + 1 < options.sound_ids.size() && options.gap_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(options.gap_ms));
        }
    }

    if (options.duration_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(options.duration_ms));
    }

    if (!accepted_all) {
        return kFailure;
    }

    const bool confirmed = operator_confirmed_audio();
    std::cout << "stellar-audio-smoke: operator_confirmation="
              << (confirmed ? "heard" : "missing") << "\n";
    if (!confirmed) {
        std::cout << "stellar-audio-smoke: skip reason=operator_confirmation_missing\n";
        return kSkip;
    }

    return kSuccess;
}
