# TrenchBroom Manual QA Checklist

Use this report template for human TrenchBroom validation. Automated CI remains display-free and compiler-independent; this checklist covers editor, GUI, external compiler, and launch behavior.

## Environment report

- Date:
- OS / distro:
- TrenchBroom version:
- Stellar commit/branch:
- Package mode: repo-local / copied package
- Compiler/toolchain: generic BSP30 / VHLT fast / VHLT full
- Graphics backend checked: OpenGL / Vulkan / not launched
- Notes:

## Checklist

1. Install the repo-local package from `tools/trenchbroom/Stellar/` and confirm the **Stellar** game appears.
2. Copy the package to the TrenchBroom games directory, set `STELLAR_REPO_ROOT` or `.stellar_repo_root`, and confirm package-local compile/validate shims run.
3. Create a new Stellar map using BSP30/Valve 220 style source.
4. Confirm coordinate/grid policy: X/Y is the floor plane, Z is height, floor `z = 0`, and player origin `0 0 36`.
5. Confirm developer materials and thumbnails appear, especially `dev/grid_32`, `dev/grid_64`, and `dev/wall_96`.
6. Confirm WAD workflow: package `materials/stellar_dev.wad` is visible/usable and source maps do not receive absolute WAD paths.
7. Place `light` and `light_spot`, compile with the Full/VHLT profile, and verify validation reports BSP30 output with lighting data.
8. Place `trigger_multiple_point` / `trigger_once_point` and verify `_stellar_extents` smart properties are preserved.
9. Place `stellar_object_collider_point` with pickup/item archetype and `_stellar_extents`.
10. Place a brush `trigger_stellar` / `trigger_multiple` and verify script/table or target properties are preserved.
11. Place `stellar_sprite` or `env_sprite` and verify sprite id, texture, size, and alpha properties.
12. Place `func_wall`, `func_illusionary`, `func_door`, and `func_button`; wire the button `target` to the door `targetname`.
13. Compile through Fast and Full profiles from TrenchBroom.
14. Validate through the Stellar wrapper: `tools/bsp/validate_trenchbroom_bsp30.sh --map <compiled.bsp>`.
15. Launch local client with `build/stellar-client --map <compiled.bsp>` and verify materials, lightmaps, collision, trigger activation, pickup collection, door/button movement, and sprite presentation.
16. Optionally launch local socket mode with `build/stellar-server --map <compiled.bsp> --listen 127.0.0.1:7777` and `build/stellar-client --connect 127.0.0.1:7777`.

## Fixture mapping

- Minimal room: `tests/fixtures/trenchbroom/src/minimal_zup_room.map`
- Entity palette: `entity_matrix_zup.map`
- Scripted trigger/pickup: `scripted_interaction_zup.map`
- Lit room: `lit_zup_room.map`
- WAD material workflow: `material_wad_zup.map`
- Moving door/button: `moving_door_button_zup.map`
- Point volumes: `point_volume_zup.map`
- Static vs illusionary brush: `illusionary_static_zup.map`

## Result

- Pass / fail:
- Blocking issues:
- Non-blocking notes:
- Screenshots/log paths:
