#pragma once

#include "infra/lvgl/IDisplaySink.h"
#include "infra/lvgl/IPointerSource.h"

#include <windows.h>

#include <cstdint>
#include <vector>

namespace infra::lvgl {

// PC シミュレータ用: Win32 ウィンドウを LCD、マウスをタッチパネルに見立てる。
// 高DPI画面では表示倍率に近い整数倍に拡大する（1ピクセルをそのまま大きく表示し、実機 LCD の見え方を保つ）。
class Win32DisplayBackend final : public IDisplaySink, public IPointerSource {
public:
    Win32DisplayBackend(int width, int height, const wchar_t* title);
    ~Win32DisplayBackend() override;
    Win32DisplayBackend(const Win32DisplayBackend&) = delete;
    Win32DisplayBackend& operator=(const Win32DisplayBackend&) = delete;

    int width() const override { return width_; }
    int height() const override { return height_; }
    void writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) override;
    void present() override;
    PointerState pointer() const override { return pointer_; }

    // ウィンドウメッセージを処理する。ウィンドウが閉じられたら false
    bool processEvents();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void updatePointer(LPARAM lParam);
    void paint();

    int width_;
    int height_;
    int scale_ = 1;
    std::vector<std::uint32_t> frame_;
    HWND hwnd_ = nullptr;
    PointerState pointer_;
    bool quit_ = false;
};

}  // namespace infra::lvgl
