#include "infra/media/AnimationRecorder.h"

#include "infra/media/GifEncoder.h"

#include <algorithm>
#include <utility>

namespace infra::media {

AnimationRecorder::AnimationRecorder(int width, int height) : width_(width), height_(height) {}

void AnimationRecorder::capture(const std::uint32_t* frame, std::uint16_t delayCs) {
    const auto pixelAt = [&](int x, int y) { return frame[static_cast<std::size_t>(y) * width_ + x] & 0xFFFFFF; };

    if (previous_.empty()) {  // 最初のフレームは画面全体
        Frame first{0, 0, width_, height_, {}, delayCs};
        first.pixels.reserve(static_cast<std::size_t>(width_) * height_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                first.pixels.push_back(pixelAt(x, y));
                quantizer_.add(pixelAt(x, y));
            }
        }
        previous_ = first.pixels;
        frames_.push_back(std::move(first));
        return;
    }

    // 変化した画素を囲む矩形を求める
    int left = width_, top = height_, right = -1, bottom = -1;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (pixelAt(x, y) != previous_[static_cast<std::size_t>(y) * width_ + x]) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < 0) {  // 変化なし: 前のフレームを長く見せる
        frames_.back().delayCs += delayCs;
        return;
    }

    Frame delta{left, top, right - left + 1, bottom - top + 1, {}, delayCs};
    delta.pixels.reserve(static_cast<std::size_t>(delta.w) * delta.h);
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            std::uint32_t& before = previous_[static_cast<std::size_t>(y) * width_ + x];
            const std::uint32_t now = pixelAt(x, y);
            if (now == before) {
                delta.pixels.push_back(kUnchanged);
            } else {
                delta.pixels.push_back(now);
                quantizer_.add(now);
                before = now;
            }
        }
    }
    frames_.push_back(std::move(delta));
}

bool AnimationRecorder::saveGif(const std::filesystem::path& path) const {
    constexpr int kTransparent = 255;
    const std::vector<std::uint32_t> palette = quantizer_.buildPalette(255);
    NearestColorMapper mapper(palette);
    GifEncoder encoder(width_, height_, palette, kTransparent);

    for (std::size_t i = 0; i < frames_.size(); ++i) {
        const Frame& frame = frames_[i];
        std::vector<std::uint8_t> indices;
        indices.reserve(frame.pixels.size());
        for (const std::uint32_t pixel : frame.pixels) {
            indices.push_back(pixel == kUnchanged ? static_cast<std::uint8_t>(kTransparent) : mapper.indexOf(pixel));
        }
        const auto delay = static_cast<std::uint16_t>(std::min<std::uint32_t>(frame.delayCs, 65535));
        encoder.addFrame(frame.x, frame.y, frame.w, frame.h, indices, delay, i > 0);
    }
    return encoder.save(path);
}

}  // namespace infra::media
