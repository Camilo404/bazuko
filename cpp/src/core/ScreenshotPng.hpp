#pragma once
// Captura del framebuffer a PNG sin compresion (solo para verificacion visual: BAZUKO_SHOT=ruta.png).
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Bazuko {

inline bool WritePngRgb(const std::string& path, int width, int height, const std::vector<uint8_t>& rgbTopDown) {
    static const std::array<uint32_t, 256> crcTable = [] {
        std::array<uint32_t, 256> table{};
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        return table;
    }();
    const auto crc = [&](const uint8_t* data, size_t size, uint32_t seed) {
        uint32_t c = seed;
        for (size_t i = 0; i < size; ++i) c = crcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        return c;
    };
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(width * 3 + 1) * height);
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = &rgbTopDown[static_cast<size_t>(y) * width * 3];
        raw.insert(raw.end(), row, row + width * 3);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t pos = 0; pos < raw.size();) {
        const size_t len = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + len >= raw.size() ? 1 : 0);
        z.push_back(len & 0xFF); z.push_back(len >> 8);
        z.push_back(~len & 0xFF); z.push_back((~len >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    }
    const uint32_t adler = (b << 16) | a;
    for (int shift = 24; shift >= 0; shift -= 8) z.push_back((adler >> shift) & 0xFF);

    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    const auto be32 = [](uint32_t v) { return std::array<uint8_t, 4>{uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; };
    const auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        const auto length = be32(static_cast<uint32_t>(data.size()));
        std::fwrite(length.data(), 1, 4, file);
        std::fwrite(type, 1, 4, file);
        if (!data.empty()) std::fwrite(data.data(), 1, data.size(), file);
        uint32_t c = crc(reinterpret_cast<const uint8_t*>(type), 4, 0xFFFFFFFFu);
        c = crc(data.data(), data.size(), c) ^ 0xFFFFFFFFu;
        const auto tail = be32(c);
        std::fwrite(tail.data(), 1, 4, file);
    };
    const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::fwrite(signature, 1, 8, file);
    std::vector<uint8_t> header;
    for (uint32_t v : {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}) { const auto be = be32(v); header.insert(header.end(), be.begin(), be.end()); }
    header.insert(header.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", header);
    chunk("IDAT", z);
    chunk("IEND", {});
    std::fclose(file);
    return true;
}

} // namespace Bazuko
