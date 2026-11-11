#pragma once

#include "infra/media/ColorQuantizer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace infra::media {

// 画面のフレームを受け取り、最後にアニメーション GIF にする。
// 前のフレームから変化した矩形だけを覚え、矩形内でも変化していない画素は透明にする（ファイルが小さくなる）。
// 色は全フレームのヒストグラムから 255 色に減らす（残りの 1 色は透明用）。
class AnimationRecorder {
public:
    AnimationRecorder(int width, int height);

    // frame: width×height の画素（下位 24bit が RGB）。delayCs: このフレームを見せる時間（1/100 秒）。
    // 前と同じ画面なら新しいフレームは作らず、前のフレームの表示時間を延ばす
    void capture(const std::uint32_t* frame, std::uint16_t delayCs);
    bool saveGif(const std::filesystem::path& path) const;
    std::size_t frameCount() const { return frames_.size(); }

private:
    static constexpr std::uint32_t kUnchanged = 0xFFFFFFFF;

    struct Frame {
        int x = 0, y = 0, w = 0, h = 0;
        std::vector<std::uint32_t> pixels;  // 変化していない画素は kUnchanged
        std::uint32_t delayCs = 0;
    };

    int width_;
    int height_;
    std::vector<std::uint32_t> previous_;
    std::vector<Frame> frames_;
    MedianCutQuantizer quantizer_;
};

}  // namespace infra::media
