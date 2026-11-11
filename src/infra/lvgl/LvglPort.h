#pragma once

#include "infra/lvgl/IDisplaySink.h"
#include "infra/lvgl/IPointerSource.h"

#include "lvgl.h"

#include <cstdint>
#include <vector>

namespace infra::lvgl {

// LVGL の「移植層（porting layer）」。LVGL を動かすのに必要な3つを用意する:
//   1. 表示ドライバ（描画バッファと flush_cb）
//   2. 入力ドライバ（read_cb）
//   3. 時間（lv_tick_inc）と定期処理（lv_timer_handler）
// LVGL はグローバル状態を持つため、1プロセスに1つだけ作る。
// ドライバ構造体は LVGL がポインタで保持し続けるので、このオブジェクトはコピー・移動できない。
// LVGL はスレッドセーフではないので、生成と advance() は必ず同じ1つのスレッド（GUI スレッド）で行う。
class LvglPort {
public:
    using LogSink = void (*)(const char* line);  // LVGL のログ1行を受け取る関数

    LvglPort(IDisplaySink& display, IPointerSource& pointer, LogSink logSink);
    LvglPort(const LvglPort&) = delete;
    LvglPort& operator=(const LvglPort&) = delete;

    // 経過時間を LVGL に伝え、タイマー（描画・入力読み取り・アニメーション）を回す
    void advance(std::uint32_t elapsedMs);

private:
    static void flushCallback(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors);
    static void readCallback(lv_indev_drv_t* driver, lv_indev_data_t* data);

    IDisplaySink& display_;
    IPointerSource& pointer_;
    std::vector<lv_color_t> drawBuffer_;
    lv_disp_draw_buf_t drawBufferDesc_{};
    lv_disp_drv_t displayDriver_{};
    lv_indev_drv_t inputDriver_{};
};

}  // namespace infra::lvgl
