# FMP Render Fixture Matrix

This matrix names the display-free fixture coverage used for Metal/OpenGL
material parity and the remaining opt-in GPU/readback coverage.

| Feature | Display-free coverage | Optional GPU/readback coverage |
|---|---|---|
| Base color texture | `render_level_upload`, `render_level_inspection`, `bsp_materials` | Not covered yet |
| Vertex color | `render_level_upload` draw path and primitive metadata | Not covered yet |
| Lightmap texture / secondary UV | `render_level_upload`, `render_level_inspection`, `bsp_lightmaps` | Not covered yet |
| Normal texture / tangent basis | `render_level_upload` sidecar material fixture with tangents | Not covered yet |
| Specular texture/factor/power | `render_level_upload` sidecar material fixture | Not covered yet |
| Metallic/roughness texture/factors | `render_level_upload` sidecar material fixture | Not covered yet |
| Occlusion texture/strength | `render_level_upload` sidecar material fixture | Not covered yet |
| Emissive texture/factor | `render_level_upload` sidecar material fixture | Not covered yet |
| Texture transforms | `render_level_upload` sidecar material fixture | Not covered yet |
| Texcoord set selection | `render_level_upload` sidecar material fixture | Not covered yet |
| Alpha mask/blend | `render_level_upload` material ordering and sidecar alpha fixture data | Not covered yet |
| Double-sided material | `render_level_upload` sidecar material fixture | Not covered yet |
| Unlit material | `render_level_upload`, `render_level_inspection` fullbright paths | Not covered yet |
| Metal shader syntax/functions | `metal_shader_compile` | Not applicable |

The display-free layer is intended to catch missing material-slot upload and
shader-source compile regressions. FMP-5 still needs a future display/GPU-backed
Metal readback test that renders deterministic samples or histograms and compares
them against stable thresholds.
