#include "stellar/world/FootstepSurface.hpp"

#include <cassert>

namespace {

void resolver_maps_known_surface_tokens() {
    assert(stellar::world::resolve_footstep_surface_id("METAL/GRATE01") == "metal");
    assert(stellar::world::resolve_footstep_surface_id("wood/plank_01") == "wood");
    assert(stellar::world::resolve_footstep_surface_id("castle/brick_floor") == "stone");
    assert(stellar::world::resolve_footstep_surface_id("terrain/mud_01") == "dirt");
    assert(stellar::world::resolve_footstep_surface_id("outdoor/GRASS_02") == "grass");
    assert(stellar::world::resolve_footstep_surface_id("liquids/water_pool") == "water");
}

void resolver_handles_paths_extensions_and_developer_grid() {
    assert(stellar::world::resolve_footstep_surface_id(R"(DEV\GRID_32.wad)") == "concrete");
    assert(stellar::world::resolve_footstep_surface_id("stellar_dev_grid_32") == "concrete");
    assert(stellar::world::resolve_footstep_surface_id("materials/asphalt_floor.png") ==
           "concrete");
}

void resolver_defaults_unknown_and_empty_to_generic() {
    assert(stellar::world::resolve_footstep_surface_id("") == "generic");
    assert(stellar::world::resolve_footstep_surface_id("unknown_weird") == "generic");
}

} // namespace

int main() {
    resolver_maps_known_surface_tokens();
    resolver_handles_paths_extensions_and_developer_grid();
    resolver_defaults_unknown_and_empty_to_generic();
    return 0;
}
