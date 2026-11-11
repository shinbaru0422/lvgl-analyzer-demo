#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace infra::media {

// GIF89a アニメーションを組み立てる（グローバルパレット 256 色・LZW 圧縮・無限ループ）。
class GifEncoder {
public:
    // palette: 最大 256 色（0x00RRGGBB）。transparentIndex: 透明として使う色番号（使わないなら -1）
    GifEncoder(int width, int height, const std::vector<std::uint32_t>& palette, int transparentIndex);

    // 画面の (x, y) から w×h の範囲の色番号を 1 フレームとして追加する。delayCs は表示時間（1/100 秒）。
    // transparent が true なら transparentIndex の画素は前のフレームのまま残る（差分だけのフレーム）
    void addFrame(int x, int y, int w, int h, const std::vector<std::uint8_t>& indices, std::uint16_t delayCs,
                  bool transparent);
    bool save(const std::filesystem::path& path);

private:
    void writeLzw(const std::vector<std::uint8_t>& indices);
    void put8(int value) { out_.push_back(static_cast<std::uint8_t>(value)); }
    void put16(int value) {
        put8(value & 0xFF);
        put8((value >> 8) & 0xFF);
    }

    int transparentIndex_;
    std::vector<std::uint8_t> out_;
};

}  // namespace infra::media
