// ウィンドウ版（PC シミュレータ）。マウスをタッチパネルに見立てて操作する。
// GUI スレッド（このスレッド）が LVGL を、装置制御スレッドが検査の進行を受け持つ。
//
//   lvgl_analyzer_demo.exe              … 待機中の画面から始める
//   lvgl_analyzer_demo.exe --autostart  … 起動と同時に測定を始める（デモ用。入力ポートから開始を要求する）

#include "app/AnalyzerApp.h"
#include "infra/lvgl/LvglPort.h"
#include "infra/lvgl/Win32DisplayBackend.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string_view>
#include <thread>

namespace {

void logToDebugger(const char* line) { OutputDebugStringA(line); }  // Visual Studio の出力ウィンドウに出る

int run(bool autostart) {
    infra::lvgl::Win32DisplayBackend backend(app::kScreenWidth, app::kScreenHeight,
                                             L"LVGL 8.3.11 demo - sample analyzer");
    infra::lvgl::LvglPort port(backend, backend, &logToDebugger);
    app::AnalyzerApp app(lv_scr_act());
    app.startControlThread();
    if (autostart) app.commands().requestStart();

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    while (backend.processEvents()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - last);
        last += elapsed;
        port.advance(static_cast<std::uint32_t>(elapsed.count()));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const bool autostart = argc >= 2 && std::string_view(argv[1]) == "--autostart";
    try {
        return run(autostart);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "lvgl_analyzer_demo", MB_ICONERROR);
        return 1;
    }
}
