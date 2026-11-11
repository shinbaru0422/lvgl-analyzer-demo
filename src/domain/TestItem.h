#pragma once

#include <string>

namespace domain {

enum class Judgement { Normal, Low, High };

// 検査項目（例: GLU）と基準範囲。結果値の高値/低値判定はドメインのルール。
struct TestItem {
    std::string code;
    std::string unit;
    double referenceLow = 0.0;   // 基準範囲の下限（含む）
    double referenceHigh = 0.0;  // 基準範囲の上限（含む）
    int decimals = 0;            // 表示桁数

    Judgement judge(double value) const {
        if (value < referenceLow) return Judgement::Low;
        if (value > referenceHigh) return Judgement::High;
        return Judgement::Normal;
    }
};

}  // namespace domain
