#pragma once

#include "usecase/IMeasurementUnit.h"

#include <cstdint>
#include <random>

namespace infra::device {

// 乱数で結果と温度を返す模擬装置。シードを固定しているので毎回同じ結果になる。
class SimulatedMeasurementUnit final : public usecase::IMeasurementUnit {
public:
    struct Profile {
        std::uint32_t seed = 30;     // デモで エラー1件・異常値3件 が出る値
        double failureRate = 0.08;   // エラー（再検）になる割合
        double abnormalRate = 0.25;  // 基準範囲外になる割合
    };

    explicit SimulatedMeasurementUnit(Profile profile);

    usecase::MeasurementOutcome measure(const domain::Sample& sample) override;
    double readCellTemperature() override;

private:
    double uniform(double low, double high);

    Profile profile_;
    std::mt19937 resultRng_;       // 結果用と温度用で乱数系列を分け、結果が時間の進め方に左右されないようにする
    std::mt19937 temperatureRng_;
    double drift_ = 0.0;
    double phase_ = 0.0;
};

}  // namespace infra::device
