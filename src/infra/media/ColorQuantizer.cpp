#include "infra/media/ColorQuantizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace infra::media {
namespace {

struct Entry {
    std::array<int, 3> rgb;
    std::uint64_t count;
};

struct Box {
    std::size_t begin;
    std::size_t end;
};

int channelRange(const std::vector<Entry>& entries, const Box& box, int channel) {
    int lo = 255, hi = 0;
    for (std::size_t i = box.begin; i < box.end; ++i) {
        lo = std::min(lo, entries[i].rgb[channel]);
        hi = std::max(hi, entries[i].rgb[channel]);
    }
    return hi - lo;
}

std::uint64_t population(const std::vector<Entry>& entries, const Box& box) {
    std::uint64_t total = 0;
    for (std::size_t i = box.begin; i < box.end; ++i) total += entries[i].count;
    return total;
}

}  // namespace

std::vector<std::uint32_t> MedianCutQuantizer::buildPalette(std::size_t maxColors) const {
    std::vector<Entry> entries;
    entries.reserve(histogram_.size());
    for (const auto& [rgb, count] : histogram_) {
        entries.push_back({{static_cast<int>((rgb >> 16) & 0xFF), static_cast<int>((rgb >> 8) & 0xFF),
                            static_cast<int>(rgb & 0xFF)},
                           count});
    }

    std::vector<Box> boxes{{0, entries.size()}};
    while (boxes.size() < maxColors) {
        // 「色の広がり × 画素数の平方根」が最大の箱を分ける（広く、よく使われる色域ほど細かく分ける）
        double bestScore = 0.0;
        std::size_t best = boxes.size();
        int bestChannel = 0;
        for (std::size_t b = 0; b < boxes.size(); ++b) {
            if (boxes[b].end - boxes[b].begin < 2) continue;
            for (int c = 0; c < 3; ++c) {
                const double score = channelRange(entries, boxes[b], c) * std::sqrt(static_cast<double>(population(entries, boxes[b])));
                if (score > bestScore) {
                    bestScore = score;
                    best = b;
                    bestChannel = c;
                }
            }
        }
        if (best == boxes.size()) break;  // これ以上分けられない（色数が maxColors 未満）

        Box box = boxes[best];
        std::sort(entries.begin() + static_cast<std::ptrdiff_t>(box.begin), entries.begin() + static_cast<std::ptrdiff_t>(box.end),
                  [c = bestChannel](const Entry& a, const Entry& b) { return a.rgb[c] < b.rgb[c]; });
        // 画素数で重み付けした中央値で分ける。どちらの箱も空にしない
        const std::uint64_t half = population(entries, box) / 2;
        std::uint64_t cumulative = 0;
        std::size_t split = box.begin + 1;
        for (std::size_t i = box.begin; i < box.end - 1; ++i) {
            cumulative += entries[i].count;
            split = i + 1;
            if (cumulative >= half) break;
        }
        boxes[best] = {box.begin, split};
        boxes.push_back({split, box.end});
    }

    std::vector<std::uint32_t> palette;
    for (const Box& box : boxes) {
        std::array<double, 3> sum{};
        double total = 0.0;
        for (std::size_t i = box.begin; i < box.end; ++i) {
            for (int c = 0; c < 3; ++c) sum[c] += entries[i].rgb[c] * static_cast<double>(entries[i].count);
            total += static_cast<double>(entries[i].count);
        }
        if (total == 0.0) continue;
        const auto channel = [&](int c) { return static_cast<std::uint32_t>(std::lround(sum[c] / total)); };
        palette.push_back((channel(0) << 16) | (channel(1) << 8) | channel(2));
    }
    return palette;
}

std::uint8_t NearestColorMapper::indexOf(std::uint32_t rgb) {
    rgb &= 0xFFFFFF;
    if (const auto it = cache_.find(rgb); it != cache_.end()) return it->second;

    const int r = static_cast<int>((rgb >> 16) & 0xFF), g = static_cast<int>((rgb >> 8) & 0xFF), b = static_cast<int>(rgb & 0xFF);
    int bestDistance = std::numeric_limits<int>::max();
    std::uint8_t best = 0;
    for (std::size_t i = 0; i < palette_.size(); ++i) {
        const int dr = r - static_cast<int>((palette_[i] >> 16) & 0xFF);
        const int dg = g - static_cast<int>((palette_[i] >> 8) & 0xFF);
        const int db = b - static_cast<int>(palette_[i] & 0xFF);
        const int distance = 2 * dr * dr + 4 * dg * dg + 3 * db * db;  // 目の感度に合わせた重み
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<std::uint8_t>(i);
        }
    }
    cache_.emplace(rgb, best);
    return best;
}

}  // namespace infra::media
