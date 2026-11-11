#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace infra::media {

// 色の出現数（ヒストグラム）から、メディアンカット法で最大 N 色のパレットを作る。色は 0x00RRGGBB。
class MedianCutQuantizer {
public:
    void add(std::uint32_t rgb) { ++histogram_[rgb & 0xFFFFFF]; }
    std::vector<std::uint32_t> buildPalette(std::size_t maxColors) const;

private:
    std::unordered_map<std::uint32_t, std::uint64_t> histogram_;
};

// パレットの中でいちばん近い色の番号を返す。同じ色は 2 回目から表引きで済ませる。
class NearestColorMapper {
public:
    explicit NearestColorMapper(std::vector<std::uint32_t> palette) : palette_(std::move(palette)) {}
    std::uint8_t indexOf(std::uint32_t rgb);

private:
    std::vector<std::uint32_t> palette_;
    std::unordered_map<std::uint32_t, std::uint8_t> cache_;
};

}  // namespace infra::media
