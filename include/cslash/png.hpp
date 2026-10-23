#pragma once

#include <cstdint>
#include <vector>

namespace cslash {

// Builds a valid solid-colour RGB PNG (dependency-free, stored deflate blocks).
// Used by #byte_generate for .png targets. width/height must be 1..64.
std::vector<std::uint8_t> make_png(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b);

}  // namespace cslash
