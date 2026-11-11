#include "infra/lvgl/HeadlessDisplayBackend.h"

#include <fstream>

namespace infra::lvgl {
namespace {

void putLe(std::vector<char>& out, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
}

}  // namespace

HeadlessDisplayBackend::HeadlessDisplayBackend(int width, int height)
    : width_(width), height_(height), frame_(static_cast<std::size_t>(width) * height, 0) {}

void HeadlessDisplayBackend::writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) {
    const int areaWidth = x2 - x1 + 1;
    for (int y = y1; y <= y2; ++y, pixels += areaWidth) {
        if (y < 0 || y >= height_) continue;
        for (int x = x1; x <= x2; ++x) {
            if (x < 0 || x >= width_) continue;
            frame_[static_cast<std::size_t>(y) * width_ + x] = pixels[x - x1];
        }
    }
}

std::uint64_t HeadlessDisplayBackend::frameHash() const {
    std::uint64_t hash = 14695981039346656037ull;
    for (const std::uint32_t pixel : frame_) {
        for (int shift = 0; shift < 24; shift += 8) {
            hash ^= (pixel >> shift) & 0xFF;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

// 24bit 非圧縮 BMP（下の行から並べる一般的な形式）で保存する
bool HeadlessDisplayBackend::saveBmp(const std::filesystem::path& path) const {
    const std::uint32_t rowBytes = (static_cast<std::uint32_t>(width_) * 3 + 3) & ~3u;
    const std::uint32_t imageBytes = rowBytes * static_cast<std::uint32_t>(height_);
    constexpr std::uint32_t kHeaderBytes = 14 + 40;

    std::vector<char> out;
    out.reserve(kHeaderBytes + imageBytes);
    // BITMAPFILEHEADER
    out.push_back('B');
    out.push_back('M');
    putLe(out, kHeaderBytes + imageBytes, 4);
    putLe(out, 0, 4);
    putLe(out, kHeaderBytes, 4);
    // BITMAPINFOHEADER
    putLe(out, 40, 4);
    putLe(out, static_cast<std::uint32_t>(width_), 4);
    putLe(out, static_cast<std::uint32_t>(height_), 4);
    putLe(out, 1, 2);   // planes
    putLe(out, 24, 2);  // bits per pixel
    putLe(out, 0, 4);   // BI_RGB
    putLe(out, imageBytes, 4);
    putLe(out, 2835, 4);  // 72 dpi
    putLe(out, 2835, 4);
    putLe(out, 0, 4);
    putLe(out, 0, 4);

    for (int y = height_ - 1; y >= 0; --y) {
        const std::uint32_t* row = &frame_[static_cast<std::size_t>(y) * width_];
        for (int x = 0; x < width_; ++x) {
            putLe(out, row[x] & 0xFFFFFF, 3);  // B, G, R
        }
        for (std::uint32_t pad = static_cast<std::uint32_t>(width_) * 3; pad < rowBytes; ++pad) out.push_back(0);
    }

    std::ofstream file(path, std::ios::binary);
    file.write(out.data(), static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(file);
}

}  // namespace infra::lvgl
