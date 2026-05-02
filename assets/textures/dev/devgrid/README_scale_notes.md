# GoldSrc-Scale Developer Texture

Texture: `dev_orange_16_32_128_1024.png`

Scale contract:
- 1 gameplay/world unit = 1 inch
- 1 full texture tile = 128 x 128 units = 128 x 128 inches = 10 ft 8 in square
- Texture resolution = 1024 x 1024 px
- 8 px = 1 world unit / inch
- Minor grid = 16 units = 16 inches = 1 ft 4 in
- Medium grid = 32 units = 32 inches = 2 ft 8 in
- Half-tile line = 64 units = 64 inches = 5 ft 4 in
- Major tile = 128 units = 128 inches = 10 ft 8 in

Suggested use:
- Set material/texture wrap to repeat.
- Map one full texture tile to 128 world units.
- With UVs: one 128-unit brush face should use UV scale 1.0 x 1.0.
- For a 256-unit wall, tile 2x across that axis.
- For a 72-inch default player capsule height, the height lands halfway between 64 and 80 units.
