#include "infra/lvgl/WebDisplayBackend.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

// ---- JavaScript 側の処理（emscripten の EM_JS で C から呼べる関数として定義する）

// canvas のページ上の位置と表示サイズ（CSS ピクセル）を out[0..3] に書く
EM_JS(void, js_canvas_rect, (const char* selector, double* out), {
    const r = document.querySelector(UTF8ToString(selector)).getBoundingClientRect();
    HEAPF64[(out >> 3) + 0] = r.left;
    HEAPF64[(out >> 3) + 1] = r.top;
    HEAPF64[(out >> 3) + 2] = r.width;
    HEAPF64[(out >> 3) + 3] = r.height;
});

// rgba（width×height の RGBA）のうち、(x, y, w, h) の範囲を canvas に描く
EM_JS(void, js_blit, (const char* selector, const std::uint8_t* rgba, int width, int height, int x, int y, int w, int h), {
    if (!Module.lvglContext) {
        Module.lvglContext = document.querySelector(UTF8ToString(selector)).getContext('2d');
    }
    const pixels = new Uint8ClampedArray(HEAPU8.buffer, rgba, width * height * 4);
    Module.lvglContext.putImageData(new ImageData(pixels, width, height), 0, 0, x, y, w, h);
});

namespace infra::lvgl {
namespace {

// ---- ブラウザの入力イベント → WebDisplayBackend
// 戻り値 true でブラウザ既定の動作（スクロール・ズーム・タッチから作られるマウスイベント）を止める

bool onMouseDown(int, const EmscriptenMouseEvent* e, void* user) {
    if (e->button != 0) return false;
    static_cast<WebDisplayBackend*>(user)->onPointer(e->clientX, e->clientY, true);
    return true;
}

bool onMouseMove(int, const EmscriptenMouseEvent* e, void* user) {
    auto* self = static_cast<WebDisplayBackend*>(user);
    self->onPointer(e->clientX, e->clientY, self->pointer().pressed);
    return false;
}

bool onMouseUp(int, const EmscriptenMouseEvent*, void* user) {
    static_cast<WebDisplayBackend*>(user)->onRelease();
    return false;
}

bool onTouch(int eventType, const EmscriptenTouchEvent* e, void* user) {
    auto* self = static_cast<WebDisplayBackend*>(user);
    if (eventType == EMSCRIPTEN_EVENT_TOUCHEND || eventType == EMSCRIPTEN_EVENT_TOUCHCANCEL) {
        if (e->numTouches <= 1) self->onRelease();  // 最後の指が離れた
        return true;
    }
    if (e->numTouches > 0) self->onPointer(e->touches[0].clientX, e->touches[0].clientY, true);
    return true;
}

}  // namespace

WebDisplayBackend::WebDisplayBackend(int width, int height, std::string canvasSelector)
    : width_(width),
      height_(height),
      selector_(std::move(canvasSelector)),
      frame_(static_cast<std::size_t>(width) * height, 0),
      rgba_(static_cast<std::size_t>(width) * height * 4, 255),
      dirtyX1_(INT_MAX),
      dirtyY1_(INT_MAX),
      dirtyX2_(-1),
      dirtyY2_(-1) {
    const char* target = selector_.c_str();
    emscripten_set_mousedown_callback(target, this, true, &onMouseDown);
    // 押したまま canvas の外へ出て離した場合も拾えるよう、移動と離す操作はページ全体で受ける
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, this, true, &onMouseMove);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, this, true, &onMouseUp);
    emscripten_set_touchstart_callback(target, this, true, &onTouch);
    emscripten_set_touchmove_callback(target, this, true, &onTouch);
    emscripten_set_touchend_callback(target, this, true, &onTouch);
    emscripten_set_touchcancel_callback(target, this, true, &onTouch);
}

void WebDisplayBackend::writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) {
    const int areaWidth = x2 - x1 + 1;
    for (int y = y1; y <= y2; ++y, pixels += areaWidth) {
        if (y < 0 || y >= height_) continue;
        for (int x = x1; x <= x2; ++x) {
            if (x < 0 || x >= width_) continue;
            frame_[static_cast<std::size_t>(y) * width_ + x] = pixels[x - x1];
        }
    }
    dirtyX1_ = std::min(dirtyX1_, std::max(x1, 0));
    dirtyY1_ = std::min(dirtyY1_, std::max(y1, 0));
    dirtyX2_ = std::max(dirtyX2_, std::min(x2, width_ - 1));
    dirtyY2_ = std::max(dirtyY2_, std::min(y2, height_ - 1));
}

void WebDisplayBackend::present() {
    if (dirtyX2_ < dirtyX1_ || dirtyY2_ < dirtyY1_) return;
    for (int y = dirtyY1_; y <= dirtyY2_; ++y) {
        for (int x = dirtyX1_; x <= dirtyX2_; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * width_ + x;
            const std::uint32_t p = frame_[i];
            rgba_[i * 4 + 0] = static_cast<std::uint8_t>((p >> 16) & 0xFF);  // R
            rgba_[i * 4 + 1] = static_cast<std::uint8_t>((p >> 8) & 0xFF);   // G
            rgba_[i * 4 + 2] = static_cast<std::uint8_t>(p & 0xFF);          // B
            rgba_[i * 4 + 3] = 255;
        }
    }
    js_blit(selector_.c_str(), rgba_.data(), width_, height_, dirtyX1_, dirtyY1_, dirtyX2_ - dirtyX1_ + 1,
            dirtyY2_ - dirtyY1_ + 1);
    dirtyX1_ = dirtyY1_ = INT_MAX;
    dirtyX2_ = dirtyY2_ = -1;
}

void WebDisplayBackend::onPointer(double clientX, double clientY, bool pressed) {
    double rect[4];
    js_canvas_rect(selector_.c_str(), rect);
    if (rect[2] <= 0 || rect[3] <= 0) return;
    const double x = (clientX - rect[0]) * width_ / rect[2];
    const double y = (clientY - rect[1]) * height_ / rect[3];
    pointer_.x = std::clamp(static_cast<int>(std::lround(x)), 0, width_ - 1);
    pointer_.y = std::clamp(static_cast<int>(std::lround(y)), 0, height_ - 1);
    pointer_.pressed = pressed;
}

}  // namespace infra::lvgl
