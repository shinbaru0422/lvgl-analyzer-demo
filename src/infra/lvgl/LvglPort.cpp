#include "infra/lvgl/LvglPort.h"

#include <cassert>

namespace infra::lvgl {
namespace {

// 部分描画バッファの行数。画面全体（480行）ではなく一部だけ持ち、LVGL は変化した領域を
// この大きさに区切って何回かに分けて描く。RAM の少ない機器ではここを小さくする。
constexpr int kBufferLines = 40;

static_assert(sizeof(lv_color_t) == sizeof(std::uint32_t), "lv_conf.h の LV_COLOR_DEPTH は 32 にすること");

[[maybe_unused]] bool g_initialized = false;  // 二重生成の検出用（assert が有効なデバッグビルドでだけ使う）

}  // namespace

LvglPort::LvglPort(IDisplaySink& display, IPointerSource& pointer, LogSink logSink)
    : display_(display), pointer_(pointer) {
    assert(!g_initialized && "LvglPort は1プロセスに1つだけ");
    g_initialized = true;

    lv_log_register_print_cb(logSink);  // lv_conf.h で LV_LOG_PRINTF 0 にしているので、ログはここに届く
    lv_init();

    const int width = display_.width();
    const int height = display_.height();

    // 1. 表示ドライバ
    drawBuffer_.resize(static_cast<std::size_t>(width) * kBufferLines);
    lv_disp_draw_buf_init(&drawBufferDesc_, drawBuffer_.data(), nullptr, static_cast<std::uint32_t>(drawBuffer_.size()));

    lv_disp_drv_init(&displayDriver_);
    displayDriver_.hor_res = static_cast<lv_coord_t>(width);
    displayDriver_.ver_res = static_cast<lv_coord_t>(height);
    displayDriver_.draw_buf = &drawBufferDesc_;
    displayDriver_.flush_cb = &LvglPort::flushCallback;
    displayDriver_.user_data = this;
    lv_disp_drv_register(&displayDriver_);

    // 2. 入力ドライバ（タッチパネル相当のポインタ）
    lv_indev_drv_init(&inputDriver_);
    inputDriver_.type = LV_INDEV_TYPE_POINTER;
    inputDriver_.read_cb = &LvglPort::readCallback;
    inputDriver_.user_data = this;
    lv_indev_drv_register(&inputDriver_);
}

void LvglPort::advance(std::uint32_t elapsedMs) {
    // 3. 時間。実機では 1ms 割り込みや別スレッドから lv_tick_inc を呼ぶことが多い
    lv_tick_inc(elapsedMs);
    lv_timer_handler();
}

void LvglPort::flushCallback(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    auto* self = static_cast<LvglPort*>(driver->user_data);
    self->display_.writePixels(area->x1, area->y1, area->x2, area->y2, reinterpret_cast<const std::uint32_t*>(colors));
    if (lv_disp_flush_is_last(driver)) self->display_.present();
    // 転送が終わったことを LVGL に知らせる（DMA 転送なら完了割り込みの中で呼ぶ）
    lv_disp_flush_ready(driver);
}

void LvglPort::readCallback(lv_indev_drv_t* driver, lv_indev_data_t* data) {
    const auto* self = static_cast<const LvglPort*>(driver->user_data);
    const PointerState state = self->pointer_.pointer();
    data->point.x = static_cast<lv_coord_t>(state.x);
    data->point.y = static_cast<lv_coord_t>(state.y);
    data->state = state.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace infra::lvgl
