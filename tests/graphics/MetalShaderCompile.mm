#include "stellar/graphics/metal/MetalGraphicsDevice.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdio>

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::puts("Skipping Metal shader compile test: no default MTLDevice");
            return 77;
        }

        NSError* error = nil;
        NSString* source = [NSString
            stringWithUTF8String:stellar::graphics::metal::metal_shader_source()];
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (library == nil) {
            if (error != nil && error.localizedDescription != nil) {
                std::fprintf(stderr, "Metal shader compile failed: %s\n",
                             [error.localizedDescription UTF8String]);
            } else {
                std::fprintf(stderr, "Metal shader compile failed\n");
            }
            return 1;
        }

        id<MTLFunction> vertex = [library newFunctionWithName:@"stellar_vertex"];
        id<MTLFunction> fragment = [library newFunctionWithName:@"stellar_fragment"];
        if (vertex == nil || fragment == nil) {
            std::fprintf(stderr, "Metal shader functions are missing\n");
            return 1;
        }
    }
    return 0;
}
