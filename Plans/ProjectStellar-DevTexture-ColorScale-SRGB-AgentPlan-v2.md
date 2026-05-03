# Project Stellar — Developer Texture Color/Scale and sRGB Render Parity Plan

Target branch: `fixes`

## Purpose

Fix the remaining developer-texture mismatch where the in-engine floor/ceiling `dev/grid_64` major lines appear orange/darker than the actual texture preview, and ensure runtime developer materials match TrenchBroom-visible PNG/WAD previews.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

## Current user-visible symptom

In-engine:
- floor/ceiling `dev/grid_64` shows orange major lines;
- texture appears darker/more saturated than the source texture preview;
- wall texture is closer than before, but runtime/editor parity should still be verified.

User has manually updated TrenchBroom configuration profiles. Do not spend time on `CompilationProfiles.cfg` in this slice unless required for tests.

## Branch evidence

Important files:
- `tools/bsp/create_stellar_dev_wad.py`
- `src/import/bsp/DeveloperTextures.cpp`
- `src/graphics/RenderLevel.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `tools/trenchbroom/Stellar/materials/dev/grid_64.png`
- `tools/trenchbroom/Stellar/materials/stellar_dev_grid_64.png`

Current code facts:
- `grid_64` special-case major lines use palette index `19`.
- Palette index `19` is `(255, 176, 48)`, which is orange.
- Palette index `20` is `(255, 224, 128)`, which is the more yellowish color.
- `RenderLevel::material_asset_for_level_surface()` sets level material `base_color_factor` to `0.75, 0.75, 0.75, 1.0`.
- `OpenGLGraphicsDevice::texture_format_from_image()` maps sRGB textures to `GL_SRGB8` / `GL_SRGB8_ALPHA8`.
- `OpenGLGraphicsDevice` now has a scoped unpack alignment fix, which should remain.
- No confirmed framebuffer sRGB encode path was observed in the OpenGL file; verify and implement if absent.

## Root-cause hypotheses

### Hypothesis A — The generated texture is actually orange

The current generator uses palette index `19` for `grid_64` major lines:
- index `19`: orange `(255, 176, 48)`
- index `20`: yellow `(255, 224, 128)`

If the actual texture file should show yellowish major lines, the generator and fallback are using the wrong palette entry.

Required fix:
- Change `grid_64` major grid line index from `19` to `20` in both:
  - `tools/bsp/create_stellar_dev_wad.py`
  - `src/import/bsp/DeveloperTextures.cpp`

Candidate Python patch:
```python
# create_stellar_dev_wad.py
if x % 64 <= 1 or y % 64 <= 1:
    pixels.append(20)  # yellow major line: 255,224,128
```

Candidate C++ patch:
```cpp
// DeveloperTextures.cpp
if (x % 64U <= 1U || y % 64U <= 1U) {
    return 20U; // yellow major line: 255,224,128
}
```

Acceptance:
- regenerated `dev/grid_64.png`, `stellar_dev_grid_64.png`, WAD, and runtime fallback all use the same yellow major-line color.

### Hypothesis B — Runtime tints textured BSP materials by 0.75

`RenderLevel.cpp` uses a `0.75` base color factor for level surfaces. That darkens every texture, including developer materials. Even if source texture lines are yellow, multiplying by `0.75` will make them visibly darker and more orange/brown.

Required fix:
- Preserve texture color for textured materials by using white base color when a real texture exists.
- Keep a neutral fallback tint only for untextured/missing-texture materials if desired.

Candidate patch:
```cpp
stellar::assets::MaterialAsset material_asset_for_level_surface(
    const stellar::assets::LevelSurfaceMaterial& surface_material) {
  stellar::assets::MaterialAsset material;
  ...
  if (surface_material.texture_index.has_value()) {
      material.base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
  } else {
      material.base_color_factor = {0.75F, 0.75F, 0.75F, 1.0F};
  }
  ...
}
```

If the function currently does not have access to `texture_index`, it already takes `LevelSurfaceMaterial`, so use `surface_material.texture_index`.

Acceptance:
- With `STELLAR_DISABLE_LIGHTMAPS=1`, source texture colors render without material tinting.
- Untextured fallback surfaces remain visually distinct.

### Hypothesis C — sRGB textures are decoded but final framebuffer is not encoded

The OpenGL upload path uses sRGB internal formats for sRGB images. Sampling from `GL_SRGB8` decodes textures to linear in the shader. If the final framebuffer is not encoded back to sRGB, colors appear too dark/saturated when shown on a normal display.

Required audit:
1. Search for:
   - `SDL_GL_FRAMEBUFFER_SRGB_CAPABLE`
   - `GL_FRAMEBUFFER_SRGB`
   - `glEnable(GL_FRAMEBUFFER_SRGB)`
2. If absent, implement an sRGB framebuffer path.

Candidate patch:
```cpp
// before SDL_GL_CreateContext
SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

// after context creation / GLEW init
if (!env_flag_enabled("STELLAR_DISABLE_FRAMEBUFFER_SRGB")) {
    glEnable(GL_FRAMEBUFFER_SRGB);
}
```

Add diagnostic:
```cpp
if (env_flag_enabled("STELLAR_DEBUG_RENDER")) {
    GLint srgb_enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    ...
}
```

Caveats:
- Default framebuffer sRGB support can vary by platform/window system.
- Keep `STELLAR_DISABLE_FRAMEBUFFER_SRGB=1` for debugging.

Acceptance:
- With lightmaps disabled, runtime texture colors match PNG/WAD previews more closely.
- `STELLAR_DISABLE_FRAMEBUFFER_SRGB=1` visibly changes output only as expected.
- Tests document intended color-space behavior.

### Hypothesis D — PNG, WAD, and C++ fallback can still drift

The Python generator now writes PNG previews, WAD, and deterministic output. C++ fallback duplicates the same pattern logic manually. That duplication can drift again.

Required hardening:
1. Add a sample-color manifest generated by `create_stellar_dev_wad.py`.
2. Include sample coordinates for every developer texture:
   - `(0, 0)`, `(1, 1)`, `(16, 16)`, `(32, 32)`, `(64, 64)`, `(96, 96)`, `(127, 127)`
   - plus texture-specific samples such as major grid line `(64, 10)` and wall ruler `(8, 48)`.
3. Add a C++ test that creates fallback textures and compares those sample colors to the manifest.
4. Add a Python test or script check that PNG previews and WAD generation share the same sample colors.

Acceptance:
- CI catches generator/fallback color drift.
- The yellow/orange `grid_64` issue cannot regress silently.

## Implementation sequence

### Phase 1 — Make the desired color explicit

1. Decide final intended `grid_64` major-line color.
   - Recommended: use palette index `20` `(255,224,128)` if the actual texture file is the desired yellow.
2. Update both Python and C++ generator logic to use the chosen color.
3. Regenerate:
   ```bash
   python3 tools/bsp/create_stellar_dev_wad.py      --out tools/trenchbroom/Stellar/materials/stellar_dev.wad      --png-out tools/trenchbroom/Stellar/materials
   ```
4. Verify:
   ```bash
   python3 tools/bsp/create_stellar_dev_wad.py      --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad      --verify-png tools/trenchbroom/Stellar/materials
   ```

### Phase 2 — Remove unintended material tint

1. Update `material_asset_for_level_surface()` so textured BSP materials use:
   ```cpp
   {1.0F, 1.0F, 1.0F, 1.0F}
   ```
2. Keep fallback/untextured material tint if desired.
3. Update `tests/graphics/RenderSceneInspection.cpp` or `RenderSceneUpload.cpp`:
   - textured material upload has white base color factor;
   - untextured default material can remain `0.75`.

### Phase 3 — Correct or document sRGB output

1. Add OpenGL framebuffer sRGB support if missing.
2. Add `STELLAR_DISABLE_FRAMEBUFFER_SRGB=1`.
3. Add `STELLAR_DEBUG_RENDER=1` output for:
   - texture color space;
   - framebuffer sRGB enabled state;
   - base color factor for material uploads if feasible.
4. Add optional OpenGL color readback test:
   - upload a known sRGB texture;
   - render a simple quad with framebuffer sRGB enabled;
   - assert output is closer to source-space expectation.
5. If reliable framebuffer sRGB testing is too platform-sensitive, keep a display-free material/color-space test and a manual checklist.

### Phase 4 — Add texture debug and parity tests

1. Add/finish `STELLAR_DEBUG_TEXTURES=1`:
   - texture name;
   - source URI;
   - image dimensions;
   - texture source: embedded, WAD, fallback;
   - sample RGBA colors;
   - material base color factor;
   - uv0 min/max per surface.
2. Add manifest/sample tests:
   - WAD sample colors;
   - PNG sample colors;
   - C++ fallback sample colors.
3. Add a regression test specifically for:
   - `dev/grid_64` major line sample must equal intended yellow;
   - `dev/wall_96` ruler sample must match intended wall-reference color.

### Phase 5 — Manual visual verification

1. Reinstall or link TrenchBroom package if necessary:
   ```bash
   tools/trenchbroom/install_stellar_game_package.sh      --dest "$HOME/.TrenchBroom/games"      --link
   ```
2. Restart TrenchBroom or reload materials.
3. Recompile a fresh BSP. Do not use old compiled BSPs:
   ```bash
   tools/bsp/compile_trenchbroom_bsp30.sh      --map maps/src/test_room.map      --out maps/compiled/test_room_texture_check.bsp      --profile full      --toolchain vhlt
   ```
4. Launch without lightmaps first:
   ```bash
   STELLAR_DISABLE_LIGHTMAPS=1    STELLAR_DEBUG_TEXTURES=1    build/stellar-client --renderer opengl      --map maps/compiled/test_room_texture_check.bsp
   ```
5. Launch with lighting:
   ```bash
   build/stellar-client --renderer opengl      --map maps/compiled/test_room_texture_check.bsp
   ```

## Acceptance criteria

Automated:
- `create_stellar_dev_wad.py --verify` passes.
- `create_stellar_dev_wad.py --verify-png` passes.
- C++ fallback sample-color tests pass.
- Textured level material base color factor is white.
- Optional OpenGL sRGB/readback test passes when enabled.

Manual:
- `dev/grid_64` major floor/ceiling lines appear yellowish, matching the texture preview.
- `dev/grid_64` base/minor grid colors match TrenchBroom preview.
- `dev/wall_96` still displays the intended wall-height reference.
- Lighting does not reintroduce orange tint when `STELLAR_DISABLE_LIGHTMAPS=0`.
- With `STELLAR_DISABLE_LIGHTMAPS=1`, color parity is clearly testable.

## Do not do in this slice

- Do not alter TrenchBroom compilation profiles; user says they were fixed manually.
- Do not hide color mismatch by changing light colors.
- Do not disable lightmaps permanently.
- Do not keep independently drifting PNG/WAD/C++ texture definitions.
- Do not use stale compiled BSPs for validation.
