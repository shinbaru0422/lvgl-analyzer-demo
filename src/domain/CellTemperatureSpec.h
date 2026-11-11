#pragma once

namespace domain {

// 反応セルの温度管理規格（37.0 ± 0.2 °C）。
struct CellTemperatureSpec {
    static constexpr double kTarget = 37.0;
    static constexpr double kTolerance = 0.2;
    static constexpr double kLow = kTarget - kTolerance;
    static constexpr double kHigh = kTarget + kTolerance;

    static constexpr bool isWithin(double celsius) { return celsius >= kLow && celsius <= kHigh; }
};

}  // namespace domain
