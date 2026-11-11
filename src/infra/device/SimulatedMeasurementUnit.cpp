#include "infra/device/SimulatedMeasurementUnit.h"

#include "domain/CellTemperatureSpec.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace infra::device {

SimulatedMeasurementUnit::SimulatedMeasurementUnit(Profile profile)
    : profile_(profile), resultRng_(profile.seed), temperatureRng_(profile.seed + 1) {}

usecase::MeasurementOutcome SimulatedMeasurementUnit::measure(const domain::Sample& sample) {
    if (uniform(0.0, 1.0) < profile_.failureRate) return {false, 0.0};

    const auto& item = sample.item();
    const double span = item.referenceHigh - item.referenceLow;
    double value = 0.0;
    if (uniform(0.0, 1.0) < profile_.abnormalRate) {
        value = uniform(0.0, 1.0) < 0.7 ? item.referenceHigh + span * uniform(0.1, 0.8)
                                        : std::max(0.0, item.referenceLow - span * uniform(0.05, 0.3));
    } else {
        value = item.referenceLow + span * uniform(0.1, 0.9);
    }
    // 装置は表示桁で丸めた値を返す
    const double scale = std::pow(10.0, item.decimals);
    return {true, std::round(value * scale) / scale};
}

double SimulatedMeasurementUnit::readCellTemperature() {
    // ゆっくりした揺れ＋小さなノイズ。ときどき規格（±0.2）の外に出る
    std::normal_distribution<double> noise(0.0, 0.03);
    drift_ = drift_ * 0.92 + noise(temperatureRng_);
    phase_ = std::fmod(phase_ + 0.12, 2.0 * std::numbers::pi);
    return domain::CellTemperatureSpec::kTarget + 0.12 * std::sin(phase_) + drift_;
}

double SimulatedMeasurementUnit::uniform(double low, double high) {
    return std::uniform_real_distribution<double>(low, high)(resultRng_);
}

}  // namespace infra::device
