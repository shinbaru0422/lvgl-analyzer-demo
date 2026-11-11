#include "infra/lvgl/Win32DisplayBackend.h"

#include <windowsx.h>

#include <algorithm>
#include <stdexcept>

namespace infra::lvgl {
namespace {

constexpr wchar_t kWindowClass[] = L"LvglDemoWindow";

// 表示倍率（150% なら 1.5）に近い整数倍にする。ただし作業領域（タスクバーを除く画面）に収まる倍率まで。
// 整数倍にするのは、1 ピクセルをそのまま拡大して実機 LCD の見え方を保つため（ぼかさない）。
int chooseIntegerScale(int width, int height, DWORD style) {
    HDC screen = GetDC(nullptr);
    const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int scale = std::max(1, (dpi + 48) / 96);  // 四捨五入
    for (; scale > 1; --scale) {
        RECT rect{0, 0, width * scale, height * scale};
        AdjustWindowRect(&rect, style, FALSE);
        if (rect.right - rect.left <= workArea.right - workArea.left &&
            rect.bottom - rect.top <= workArea.bottom - workArea.top) {
            break;
        }
    }
    return scale;
}

}  // namespace

Win32DisplayBackend::Win32DisplayBackend(int width, int height, const wchar_t* title)
    : width_(width), height_(height), frame_(static_cast<std::size_t>(width) * height, 0) {
    SetProcessDPIAware();
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    scale_ = chooseIntegerScale(width_, height_, style);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &Win32DisplayBackend::windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    RECT rect{0, 0, width_ * scale_, height_ * scale_};
    AdjustWindowRect(&rect, style, FALSE);

    hwnd_ = CreateWindowExW(0, kWindowClass, title, style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                            rect.bottom - rect.top, nullptr, nullptr, instance, this);
    if (!hwnd_) throw std::runtime_error("CreateWindowExW failed");
    ShowWindow(hwnd_, SW_SHOW);
}

Win32DisplayBackend::~Win32DisplayBackend() {
    if (hwnd_) DestroyWindow(hwnd_);
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
}

void Win32DisplayBackend::writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) {
    const int areaWidth = x2 - x1 + 1;
    for (int y = y1; y <= y2; ++y, pixels += areaWidth) {
        if (y < 0 || y >= height_) continue;
        for (int x = x1; x <= x2; ++x) {
            if (x < 0 || x >= width_) continue;
            frame_[static_cast<std::size_t>(y) * width_ + x] = pixels[x - x1];
        }
    }
}

void Win32DisplayBackend::present() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Win32DisplayBackend::processEvents() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) quit_ = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !quit_;
}

LRESULT CALLBACK Win32DisplayBackend::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* creating = static_cast<Win32DisplayBackend*>(create->lpCreateParams);
        creating->hwnd_ = hwnd;  // CreateWindowExW が戻る前にメッセージが来るため、ここで保持する
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creating));
    }
    auto* self = reinterpret_cast<Win32DisplayBackend*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->handleMessage(message, wParam, lParam);
}

LRESULT Win32DisplayBackend::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd_);
            updatePointer(lParam);
            pointer_.pressed = true;
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            updatePointer(lParam);
            pointer_.pressed = false;
            return 0;
        case WM_MOUSEMOVE:
            updatePointer(lParam);
            return 0;
        case WM_ERASEBKGND:
            return 1;  // 背景消去によるちらつき防止
        case WM_PAINT:
            paint();
            return 0;
        case WM_DESTROY:
            hwnd_ = nullptr;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void Win32DisplayBackend::updatePointer(LPARAM lParam) {
    pointer_.x = std::clamp(GET_X_LPARAM(lParam) / scale_, 0, width_ - 1);
    pointer_.y = std::clamp(GET_Y_LPARAM(lParam) / scale_, 0, height_ - 1);
}

void Win32DisplayBackend::paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width_;
    info.bmiHeader.biHeight = -height_;  // 負の値 = 上から下へ並んだ画像
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;      // LVGL の lv_color32_t（B,G,R,A）と同じ並び
    info.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(dc, COLORONCOLOR);  // 拡大は最近傍（ぼかさない）
    StretchDIBits(dc, 0, 0, width_ * scale_, height_ * scale_, 0, 0, width_, height_, frame_.data(), &info,
                  DIB_RGB_COLORS, SRCCOPY);
    EndPaint(hwnd_, &ps);
}

}  // namespace infra::lvgl
