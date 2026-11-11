#pragma once

#include "lvgl.h"

#include <cstdint>
#include <functional>

namespace infra::lvgl {

// lv_timer を C++ から使うための薄いラッパー。前回からの実経過時間（LVGL の tick 基準）を渡す。
// LVGL のタイマーは lv_timer_handler() を呼んだスレッドで動くので、ハンドラ内で lv_* を呼んでよい。
class LvglIntervalTimer {
public:
    using Handler = std::function<void(std::uint32_t elapsedMs)>;

    LvglIntervalTimer(std::uint32_t periodMs, Handler handler);
    ~LvglIntervalTimer();
    LvglIntervalTimer(const LvglIntervalTimer&) = delete;
    LvglIntervalTimer& operator=(const LvglIntervalTimer&) = delete;

private:
    static void onTimer(lv_timer_t* timer);

    Handler handler_;
    std::uint32_t lastTick_;
    lv_timer_t* timer_;
};

}  // namespace infra::lvgl
