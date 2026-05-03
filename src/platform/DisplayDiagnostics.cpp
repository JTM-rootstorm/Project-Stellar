#include "stellar/platform/DisplayDiagnostics.hpp"

#include <cstdlib>
#include <sstream>
#include <string>

#if defined(__linux__)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace stellar::platform {

namespace {

std::string env_value(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return "unset";
    }
    return value;
}

void append_env(std::ostringstream& stream, const char* name) {
    stream << name << '=' << env_value(name);
}

} // namespace

std::string display_environment_diagnostics() {
    std::ostringstream stream;
    stream << "Display environment: ";
    append_env(stream, "DISPLAY");
    stream << ", ";
    append_env(stream, "WAYLAND_DISPLAY");
    stream << ", ";
    append_env(stream, "XDG_SESSION_TYPE");
    stream << ", ";
    append_env(stream, "XDG_RUNTIME_DIR");
    stream << ", ";
    append_env(stream, "XAUTHORITY");
    stream << ", ";
    append_env(stream, "SDL_VIDEODRIVER");

#if defined(__linux__)
    const uid_t effective_uid = geteuid();
    stream << ", euid=" << static_cast<unsigned long>(effective_uid);
    if (const passwd* user = getpwuid(effective_uid); user != nullptr && user->pw_name != nullptr) {
        stream << ", user=" << user->pw_name;
    }
    stream << ", running_as_root=" << (effective_uid == 0 ? "yes" : "no");
#else
    stream << ", platform_uid=unavailable";
#endif

    return stream.str();
}

std::string append_display_environment_diagnostics(std::string message) {
    message += '\n';
    message += display_environment_diagnostics();
    message += '\n';
    message += "Hint: run the client as the desktop user with DISPLAY/WAYLAND_DISPLAY and "
               "XAUTHORITY from that session; try SDL_VIDEODRIVER=x11 or wayland when "
               "debugging Linux display startup.";
    return message;
}

} // namespace stellar::platform
