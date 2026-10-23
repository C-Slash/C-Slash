#include "cslash/png.hpp"

#include <array>
#include <stdexcept>
#include <string>

namespace cslash {

namespace {

using Bytes = std::vector<std::uint8_t>;

std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t crc = 0) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[n] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (std::size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const Bytes& data) {
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t byte : data) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void put32(Bytes& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void chunk(Bytes& out, const char type[5], const Bytes& body) {
    put32(out, static_cast<std::uint32_t>(body.size()));
    Bytes block(type, type + 4);
    block.insert(block.end(), body.begin(), body.end());
    out.insert(out.end(), block.begin(), block.end());
    put32(out, crc32(block.data(), block.size()));
}

}  // namespace

Bytes make_png(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (width < 1 || width > 64 || height < 1 || height > 64) {
        throw std::invalid_argument("png size must be 1..64");
    }

    // Raw scanlines: filter byte 0 + RGB pixels.
    Bytes raw;
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < width; ++x) {
            raw.push_back(r);
            raw.push_back(g);
            raw.push_back(b);
        }
    }

    // zlib stream with one stored (uncompressed) deflate block (<= 65535 bytes).
    Bytes z = {0x78, 0x01, 0x01};
    const auto len = static_cast<std::uint16_t>(raw.size());
    z.push_back(static_cast<std::uint8_t>(len & 0xFF));
    z.push_back(static_cast<std::uint8_t>(len >> 8));
    z.push_back(static_cast<std::uint8_t>(~len & 0xFF));
    z.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFF));
    z.insert(z.end(), raw.begin(), raw.end());
    put32(z, adler32(raw));

    Bytes out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    Bytes ihdr;
    put32(ihdr, static_cast<std::uint32_t>(width));
    put32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // colour type: RGB
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace

    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

}  // namespace cslash
