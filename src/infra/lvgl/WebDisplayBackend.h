#pragma once

#include "infra/lvgl/IDisplaySink.h"
#include "infra/lvgl/IPointerSource.h"

#include <cstdint>
#include <string>
#include <vector>

namespace infra::lvgl {

// ブラウザ（WebAssembly）用: HTML の <canvas> を LCD、マウスや指のタッチをタッチパネルに見立てる。
// canvas は CSS で画面幅に合わせて拡大・縮小されるので、入力の座標は 800x480 の座標に換算する。
class WebDisplayBackend final : public IDisplaySink, public IPointerSource {
public:
    WebDisplayBackend(int width, int height, std::string canvasSelector);
    WebDisplayBackend(const WebDisplayBackend&) = delete;
    WebDisplayBackend& operator=(const WebDisplayBackend&) = delete;

    int width() const override { return width_; }
    int height() const override { return height_; }
    void writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) override;
    void present() override;  // 変化した範囲だけを canvas に転送する
    PointerState pointer() const override { return pointer_; }

    // ブラウザの入力イベントから呼ぶ（座標はページ上の CSS ピクセル）
    void onPointer(double clientX, double clientY, bool pressed);
    void onRelease() { pointer_.pressed = false; }

private:
    int width_;
    int height_;
    std::string selector_;
    std::vector<std::uint32_t> frame_;  // LVGL の画素（メモリ上 B,G,R,A）
    std::vector<std::uint8_t> rgba_;    // canvas 用（R,G,B,A）
    int dirtyX1_, dirtyY1_, dirtyX2_, dirtyY2_;
    PointerState pointer_;
};

}  // namespace infra::lvgl
