#include "stellar/core/WorldAxes.hpp"

#include <cassert>

int main() {
    using namespace stellar::core;

    static_assert(kWorldRight[0] == 1.0F);
    static_assert(kWorldRight[1] == 0.0F);
    static_assert(kWorldRight[2] == 0.0F);

    static_assert(kWorldForward[0] == 0.0F);
    static_assert(kWorldForward[1] == 1.0F);
    static_assert(kWorldForward[2] == 0.0F);

    static_assert(kWorldUp[0] == 0.0F);
    static_assert(kWorldUp[1] == 0.0F);
    static_assert(kWorldUp[2] == 1.0F);

    static_assert(kWorldDown[0] == 0.0F);
    static_assert(kWorldDown[1] == 0.0F);
    static_assert(kWorldDown[2] == -1.0F);

    static_assert(kWorldRightIndex == 0);
    static_assert(kWorldForwardIndex == 1);
    static_assert(kWorldUpIndex == 2);
    static_assert(kHorizontalPlaneXIndex == 0);
    static_assert(kHorizontalPlaneYIndex == 1);
    static_assert(kDefaultPlayerSpawnHeightInches == 36.0F);

    assert(kWorldUp[kWorldUpIndex] == 1.0F);
    assert(kWorldDown[kWorldUpIndex] == -1.0F);
    assert(kDefaultPlayerSpawnHeightInches == 36.0F);

    return 0;
}
