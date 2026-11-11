#include "usecase/AnalyzerService.h"

#include <algorithm>
#include <utility>

namespace usecase {

AnalyzerService::AnalyzerService(domain::SampleRack rack, IMeasurementUnit& unit, Timing timing)
    : rack_(std::move(rack)), unit_(unit), timing_(timing) {
    // 0 だと advance() のループが進まないため最小 1ms にする
    timing_.measureMsPerSample = std::max<std::uint32_t>(timing_.measureMsPerSample, 1);
    timing_.temperatureIntervalMs = std::max<std::uint32_t>(timing_.temperatureIntervalMs, 1);
}

void AnalyzerService::addObserver(IAnalyzerObserver& observer) { observers_.push_back(&observer); }

void AnalyzerService::publishSnapshot() {
    for (std::size_t i = 0; i < rack_.size(); ++i) notifySample(i);
    setState(state_);
    notifyProgress();
    publishTemperature();
}

void AnalyzerService::advance(std::uint32_t elapsedMs) {
    // 温度監視は状態に関係なく常に行う
    temperatureElapsedMs_ += elapsedMs;
    while (temperatureElapsedMs_ >= timing_.temperatureIntervalMs) {
        temperatureElapsedMs_ -= timing_.temperatureIntervalMs;
        publishTemperature();
    }

    if (state_ != AnalyzerState::Running) return;

    // 大きな経過時間が来ても、検体を1本ずつ順番に処理する
    std::uint32_t remaining = elapsedMs;
    while (state_ == AnalyzerState::Running && remaining > 0) {
        const std::uint32_t needed = timing_.measureMsPerSample - sampleElapsedMs_;
        if (remaining < needed) {
            sampleElapsedMs_ += remaining;
            remaining = 0;
        } else {
            remaining -= needed;
            finishCurrentSample();
        }
    }
    runElapsedMs_ += elapsedMs - remaining;  // 完了後の余りは経過時間に含めない
    notifyProgress();
}

bool AnalyzerService::start() {
    if (state_ == AnalyzerState::Running) return false;
    const auto next = rack_.nextWaitingIndex();
    if (!next) return false;  // 完了後はリセットが必要

    if (state_ == AnalyzerState::Idle) runElapsedMs_ = 0;  // 停止からの再開なら経過時間を引き継ぐ
    beginSample(*next);
    setState(AnalyzerState::Running);
    notifyProgress();
    return true;
}

bool AnalyzerService::stop() {
    if (state_ != AnalyzerState::Running) return false;
    if (current_) {
        const std::size_t index = *current_;
        rack_.at(index).abort();
        current_.reset();
        notifySample(index);
    }
    sampleElapsedMs_ = 0;
    setState(AnalyzerState::Stopped);
    notifyProgress();
    return true;
}

bool AnalyzerService::reset() {
    if (state_ == AnalyzerState::Running) return false;
    rack_.resetAll();
    current_.reset();
    sampleElapsedMs_ = 0;
    runElapsedMs_ = 0;
    for (std::size_t i = 0; i < rack_.size(); ++i) notifySample(i);
    setState(AnalyzerState::Idle);
    notifyProgress();
    return true;
}

void AnalyzerService::beginSample(std::size_t index) {
    rack_.at(index).beginMeasurement();
    current_ = index;
    sampleElapsedMs_ = 0;
    notifySample(index);
}

void AnalyzerService::finishCurrentSample() {
    const std::size_t index = *current_;
    auto& sample = rack_.at(index);
    const MeasurementOutcome outcome = unit_.measure(sample);
    if (outcome.success) {
        sample.complete(outcome.value);
    } else {
        sample.fail();
    }
    current_.reset();
    sampleElapsedMs_ = 0;
    notifySample(index);

    if (const auto next = rack_.nextWaitingIndex()) {
        beginSample(*next);
        return;
    }
    setState(AnalyzerState::Completed);
    const domain::RunSummary summary = rack_.summarize();
    notifyAll([&](IAnalyzerObserver& o) { o.onRunCompleted(summary); });
}

void AnalyzerService::publishTemperature() {
    const double celsius = unit_.readCellTemperature();
    notifyAll([&](IAnalyzerObserver& o) { o.onTemperature(celsius); });
}

void AnalyzerService::setState(AnalyzerState state) {
    state_ = state;
    notifyAll([&](IAnalyzerObserver& o) { o.onStateChanged(state); });
}

void AnalyzerService::notifySample(std::size_t index) {
    const domain::Sample& sample = rack_.at(index);
    notifyAll([&](IAnalyzerObserver& o) { o.onSampleChanged(index, sample); });
}

void AnalyzerService::notifyProgress() {
    const domain::RunSummary summary = rack_.summarize();
    ProgressInfo info;
    info.current = current_ ? &rack_.at(*current_) : nullptr;
    info.sampleRatio = current_ ? static_cast<double>(sampleElapsedMs_) / timing_.measureMsPerSample : 0.0;
    info.processed = summary.processed();
    info.total = summary.total;
    info.runElapsedMs = runElapsedMs_;
    notifyAll([&](IAnalyzerObserver& o) { o.onProgress(info); });
}

}  // namespace usecase
