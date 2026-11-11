// ブラウザ版（WebAssembly）。表示先と入力元を WebDisplayBackend に差し替えた以外は、ウィンドウ版と同じ部品で動く。
// GitHub Pages ではブラウザのマルチスレッド（SharedArrayBuffer）が使えないため、制御スレッドは起動せず、
// 画面の描画 1 回ごとに「制御を進める → LVGL を進める」を同じスレッドで行う（ヘッドレス版と同じ方式）。
//
//   index.html             … 待機中の画面から始める
//   index.html?autostart   … 開いてすぐ測定を始める

#include "app/AnalyzerApp.h"
#include "infra/lvgl/LvglPort.h"
#include "infra/lvgl/WebDisplayBackend.h"

#include <emscripten.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

void logToConsole(const char* line) { std::fputs(line, stdout); }  // ブラウザの開発者ツールのコンソールに出る

struct WebApp {
    infra::lvgl::WebDisplayBackend backend{app::kScreenWidth, app::kScreenHeight, "#screen"};
    infra::lvgl::LvglPort port{backend, backend, &logToConsole};  // lv_init はここ
    app::AnalyzerApp app{lv_scr_act()};                           // 画面を作るのは lv_init の後
    double lastMs = emscripten_get_now();
    double carryMs = 0.0;
};

// requestAnimationFrame ごとに呼ばれる
void onFrame(void* arg) {
    auto* web = static_cast<WebApp*>(arg);
    const double now = emscripten_get_now();
    // 裏のタブから戻ったときに何十秒も一気に進めないよう、1 回の経過は最大 1 秒にする
    const double elapsed = std::min(now - web->lastMs, 1000.0) + web->carryMs;
    web->lastMs = now;
    const auto ms = static_cast<std::uint32_t>(elapsed);
    web->carryMs = elapsed - ms;  // ミリ秒未満の端数は次に回す
    if (ms == 0) return;
    web->app.stepControl(ms);
    web->port.advance(ms);
}

}  // namespace

int main() {
    // main を抜けた後もページを閉じるまで動き続けるので、ヒープに置いて解放しない
    auto* web = new WebApp();
    const bool autostart = EM_ASM_INT({ return new URLSearchParams(location.search).has('autostart') ? 1 : 0; }) != 0;
    if (autostart) web->app.commands().requestStart();
    emscripten_set_main_loop_arg(&onFrame, web, 0, false);  // 0 = ブラウザの描画間隔（requestAnimationFrame）
    return 0;
}
