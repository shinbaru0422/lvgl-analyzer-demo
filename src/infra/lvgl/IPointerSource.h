#pragma once

namespace infra::lvgl {

struct PointerState {
    int x = 0;
    int y = 0;
    bool pressed = false;
};

// タッチパネル / マウスの入力元。実機 Linux なら /dev/input/eventX（evdev）を読む実装になる。
class IPointerSource {
public:
    virtual ~IPointerSource() = default;
    virtual PointerState pointer() const = 0;
};

}  // namespace infra::lvgl
