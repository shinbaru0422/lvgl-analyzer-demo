#include "infra/lvgl/LvglIntervalTimer.h"

#include <utility>

namespace infra::lvgl {

LvglIntervalTimer::LvglIntervalTimer(std::uint32_t periodMs, Handler handler)
    : handler_(std::move(handler)), lastTick_(lv_tick_get()), timer_(lv_timer_create(&LvglIntervalTimer::onTimer, periodMs, this)) {}

LvglIntervalTimer::~LvglIntervalTimer() { lv_timer_del(timer_); }

void LvglIntervalTimer::onTimer(lv_timer_t* timer) {
    auto* self = static_cast<LvglIntervalTimer*>(timer->user_data);
    const std::uint32_t elapsed = lv_tick_elaps(self->lastTick_);
    self->lastTick_ += elapsed;
    self->handler_(elapsed);
}

}  // namespace infra::lvgl
