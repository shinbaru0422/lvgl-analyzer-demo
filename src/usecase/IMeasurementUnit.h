#pragma once

#include "domain/Sample.h"

namespace usecase {

struct MeasurementOutcome {
    bool success = false;
    double value = 0.0;
};

// 装置ハードウェア（分注・測光・温調）を抽象化した出力ポート。
// デモでは乱数の模擬実装、実機では Linux のデバイスドライバや制御プロセスと通信する実装に差し替える。
class IMeasurementUnit {
public:
    virtual ~IMeasurementUnit() = default;
    virtual MeasurementOutcome measure(const domain::Sample& sample) = 0;
    virtual double readCellTemperature() = 0;
};

}  // namespace usecase
