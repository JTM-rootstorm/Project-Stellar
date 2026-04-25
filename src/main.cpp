#include "stellar/graphics/window.hpp"

int main() {
    try {
        stellar::graphics::Window window(800, 600, "Stellar Engine - Spinning Cube");
        window.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}