#include "adapter/AnalyzerPresenter.h"

#include "domain/CellTemperatureSpec.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace adapter {
namespace {

using domain::Judgement;
using domain::SampleStatus;
using usecase::AnalyzerState;

const char* statusText(SampleStatus status) {
    switch (status) {
        case SampleStatus::Waiting: return "待ち";
        case SampleStatus::Measuring: return "測定中";
        case SampleStatus::Done: return "完了";
        case SampleStatus::Error: return "エラー";
    }
    return "";
}

std::string resultText(const domain::Sample& sample) {
    if (sample.status() == SampleStatus::Error) return "要再検";
    if (!sample.value()) return "-";

    const auto& item = sample.item();
    std::string text = std::format("{:.{}f} {}", *sample.value(), item.decimals, item.unit);
    switch (*sample.judgement()) {
        case Judgement::High: text += " H"; break;
        case Judgement::Low: text += " L"; break;
        case Judgement::Normal: break;
    }
    return text;
}

Tone sampleTone(const domain::Sample& sample) {
    switch (sample.status()) {
        case SampleStatus::Waiting: return Tone::Neutral;
        case SampleStatus::Measuring: return Tone::Active;
        case SampleStatus::Error: return Tone::Alert;
        case SampleStatus::Done: return sample.judgement() == Judgement::Normal ? Tone::Good : Tone::Caution;
    }
    return Tone::Neutral;
}

}  // namespace

AnalyzerPresenter::AnalyzerPresenter(IAnalyzerView& view) : view_(view) {}

void AnalyzerPresenter::onStateChanged(AnalyzerState state) {
    state_ = state;

    StatusViewModel status;
    ControlsViewModel controls;
    controls.startText = "開始";
    switch (state) {
        case AnalyzerState::Idle:
            status = {"待機中", Tone::Neutral};
            controls.startEnabled = true;
            break;
        case AnalyzerState::Running:
            status = {"測定中", Tone::Active};
            controls.stopEnabled = true;
            break;
        case AnalyzerState::Stopped:
            status = {"一時停止", Tone::Caution};
            controls.startEnabled = true;
            controls.resetEnabled = true;
            controls.startText = "再開";
            break;
        case AnalyzerState::Completed:
            status = {"測定完了", Tone::Good};
            controls.resetEnabled = true;
            break;
    }
    view_.showStatus(status);
    view_.showControls(controls);
}

void AnalyzerPresenter::onSampleChanged(std::size_t index, const domain::Sample& sample) {
    SampleRowViewModel row;
    row.row = index;
    row.position = std::to_string(sample.position());
    row.sampleId = sample.sampleId();
    row.testItem = sample.item().code;
    row.status = statusText(sample.status());
    row.result = resultText(sample);
    row.tone = sampleTone(sample);
    row.highlighted = sample.status() == SampleStatus::Measuring;
    view_.showSampleRow(row);
}

void AnalyzerPresenter::onProgress(const usecase::ProgressInfo& progress) {
    ProgressViewModel vm;
    if (progress.current) {
        vm.percent = static_cast<int>(std::lround(std::clamp(progress.sampleRatio, 0.0, 1.0) * 100.0));
        vm.currentSample = progress.current->sampleId() + " / " + progress.current->item().code;
    } else {
        vm.percent = state_ == AnalyzerState::Completed ? 100 : 0;
        vm.currentSample = "-";
    }
    vm.countText = std::format("{} / {} 件", progress.processed, progress.total);
    const std::uint32_t seconds = progress.runElapsedMs / 1000;
    vm.elapsedText = std::format("経過時間 {:02}:{:02}", seconds / 60, seconds % 60);
    view_.showProgress(vm);
}

void AnalyzerPresenter::onTemperature(double celsius) {
    using Spec = domain::CellTemperatureSpec;
    TemperatureViewModel vm;
    vm.celsius = celsius;
    vm.valueText = std::format("{:.2f} °C", celsius);  // 数字用フォント（Montserrat）で表示
    vm.specText = std::format("管理範囲 {:.1f}±{:.1f}℃", Spec::kTarget, Spec::kTolerance);  // 日本語フォントで表示
    vm.tone = Spec::isWithin(celsius) ? Tone::Good : Tone::Caution;
    view_.showTemperature(vm);
}

void AnalyzerPresenter::onRunCompleted(const domain::RunSummary& summary) {
    MessageViewModel vm;
    vm.title = "測定完了";
    vm.body = std::format("全{}検体の測定が完了しました。\n基準範囲外: {}件\nエラー（要再検）: {}件",
                          summary.total, summary.abnormal, summary.errors);
    view_.showMessage(vm);
}

}  // namespace adapter
