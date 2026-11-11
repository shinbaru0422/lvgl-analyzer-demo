#pragma once

#include <cstdint>

namespace infra::lvgl {

// LVGL が描いたピクセルの出力先。
// PC: Win32 ウィンドウ / テスト: 画像ファイル / 実機 Linux: /dev/fb0 や DRM。
class IDisplaySink {
public:
    virtual ~IDisplaySink() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    // (x1,y1)-(x2,y2)（両端を含む）の矩形に 32bit ピクセル（メモリ上 B,G,R,A の順）を書き込む
    virtual void writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) = 0;
    virtual void present() = 0;  // 1フレーム分の書き込みが終わった
};

}  // namespace infra::lvgl
