#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-$(pwd)}"

python3 - "$ROOT" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve()
cmake = root / "CMakeLists.txt"
text = cmake.read_text(encoding="utf-8")

link_pattern = re.compile(r"target_link_libraries\s*\(\s*([A-Za-z0-9_\-]+)(.*?)\)", re.S)
links: dict[str, set[str]] = {}
for match in link_pattern.finditer(text):
    target = match.group(1)
    body = re.sub(r"#.*", " ", match.group(2))
    tokens = re.findall(r"[^\s()]+", body)
    scoped = {"PUBLIC", "PRIVATE", "INTERFACE", "debug", "optimized", "general"}
    links.setdefault(target, set()).update(token for token in tokens if token not in scoped)

violations: list[str] = []

def forbid(target: str, forbidden: set[str]) -> None:
    direct = links.get(target, set())
    for dep in sorted(direct & forbidden):
        violations.append(f"{target} directly links forbidden dependency {dep}")

forbid("stellar_protocol", {
    "stellar_authority", "stellar_server_core", "stellar_server_runtime", "stellar_dedicated_server",
    "stellar_scripting", "stellar_client_runtime", "stellar_client_net", "stellar_single_player",
    "stellar_listen_server", "stellar_client_presentation", "stellar_client_app", "stellar_graphics",
    "stellar_audio", "stellar_audio_miniaudio", "miniaudio", "stellar_platform",
})
forbid("stellar_transport", {
    "stellar_authority", "stellar_server_core", "stellar_server_runtime", "stellar_dedicated_server",
    "stellar_scripting", "stellar_client_runtime", "stellar_client_presentation", "stellar_client_app",
})
forbid("stellar_client_net", {
    "stellar_authority", "stellar_server_core", "stellar_server_runtime", "stellar_dedicated_server",
    "stellar_scripting",
})
forbid("stellar_dedicated_server", {
    "stellar_client_runtime", "stellar_client_net", "stellar_single_player", "stellar_listen_server",
    "stellar_client_presentation", "stellar_client_app", "stellar_graphics", "stellar_audio",
    "stellar_audio_miniaudio", "miniaudio",
})
forbid("stellar_server_runtime", {
    "stellar_client_runtime", "stellar_client_net", "stellar_single_player", "stellar_listen_server",
    "stellar_client_presentation", "stellar_client_app", "stellar_graphics", "stellar_audio",
    "stellar_audio_miniaudio", "miniaudio",
})

source_checks = [
    ("protocol/transport sources", ["include/stellar/network", "src/network"],
     ["stellar/server/", "stellar/scripting/", "stellar/client/", "stellar/graphics/", "stellar/audio/"]),
    ("client_net sources", [
        "include/stellar/client/ClientRuntime.hpp",
        "include/stellar/client/ClientWorldReceiver.hpp",
        "include/stellar/client/MovementInputMapper.hpp",
        "include/stellar/client/RemoteClientRuntime.hpp",
        "src/client/ClientWorldReceiver.cpp",
        "src/client/MovementInputMapper.cpp",
        "src/client/RemoteClientRuntime.cpp",
    ], ["stellar/server/", "stellar/scripting/", "stellar/authority/"]),
    ("dedicated/server_runtime sources", [
        "include/stellar/server/DedicatedServer.hpp",
        "include/stellar/server/ServerRuntime.hpp",
        "src/server/DedicatedServer.cpp",
        "src/server/ServerRuntime.cpp",
    ], ["stellar/client/", "stellar/graphics/", "stellar/audio/"]),
]

for label, paths, needles in source_checks:
    for rel in paths:
        path = root / rel
        files = path.rglob("*") if path.is_dir() else [path]
        for file_path in files:
            if not file_path.is_file() or file_path.suffix not in {".hpp", ".cpp", ".h", ".cxx"}:
                continue
            for number, line in enumerate(file_path.read_text(encoding="utf-8").splitlines(), 1):
                for needle in needles:
                    if needle in line:
                        violations.append(f"{label}: {file_path.relative_to(root)}:{number} contains {needle}")

if violations:
    print("Target boundary check failed:", file=sys.stderr)
    for violation in violations:
        print(f"  - {violation}", file=sys.stderr)
    sys.exit(1)

print("Target boundary check passed")
PY
