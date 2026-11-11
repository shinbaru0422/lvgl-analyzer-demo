// domain / usecase / adapter / スレッド（runtime）の単体テスト。LVGL もウィンドウも使わない。

#include "adapter/AnalyzerPresenter.h"
#include "adapter/DispatchingView.h"
#include "domain/SampleRack.h"
#include "infra/runtime/AnalyzerWorker.h"
#include "infra/runtime/UiTaskQueue.h"
#include "usecase/AnalyzerService.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(expr)                                                         \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);   \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

using domain::Judgement;
using domain::Sample;
using domain::SampleRack;
using domain::SampleStatus;
using domain::TestItem;
using usecase::AnalyzerService;
using usecase::AnalyzerState;

const TestItem kGlu{"GLU", "mg/dL", 73.0, 109.0, 0};

SampleRack makeRack(int count) {
    std::vector<Sample> samples;
    for (int i = 1; i <= count; ++i) samples.emplace_back(i, "S-" + std::to_string(i), kGlu);
    return SampleRack(std::move(samples));
}

// 決められた結果を順に返す偽装置
class FakeMeasurementUnit final : public usecase::IMeasurementUnit {
public:
    std::deque<usecase::MeasurementOutcome> outcomes;
    double temperature = 37.0;
    int measureCalls = 0;

    usecase::MeasurementOutcome measure(const Sample&) override {
        ++measureCalls;
        if (outcomes.empty()) return {true, 90.0};
        const auto outcome = outcomes.front();
        outcomes.pop_front();
        return outcome;
    }
    double readCellTemperature() override { return temperature; }
};

class RecordingObserver final : public usecase::IAnalyzerObserver {
public:
    std::vector<AnalyzerState> states;
    std::vector<double> temperatures;
    std::vector<domain::RunSummary> completed;
    usecase::ProgressInfo lastProgress;

    void onStateChanged(AnalyzerState state) override { states.push_back(state); }
    void onSampleChanged(std::size_t, const Sample&) override {}
    void onProgress(const usecase::ProgressInfo& progress) override { lastProgress = progress; }
    void onTemperature(double celsius) override { temperatures.push_back(celsius); }
    void onRunCompleted(const domain::RunSummary& summary) override { completed.push_back(summary); }
};

class FakeView final : public adapter::IAnalyzerView {
public:
    adapter::StatusViewModel status;
    adapter::ControlsViewModel controls;
    std::vector<adapter::SampleRowViewModel> rows;
    adapter::ProgressViewModel progress;
    adapter::TemperatureViewModel temperature;
    std::vector<adapter::MessageViewModel> messages;

    void showStatus(const adapter::StatusViewModel& vm) override { status = vm; }
    void showControls(const adapter::ControlsViewModel& vm) override { controls = vm; }
    void showSampleRow(const adapter::SampleRowViewModel& vm) override { rows.push_back(vm); }
    void showProgress(const adapter::ProgressViewModel& vm) override { progress = vm; }
    void showTemperature(const adapter::TemperatureViewModel& vm) override { temperature = vm; }
    void showMessage(const adapter::MessageViewModel& vm) override { messages.push_back(vm); }
};

constexpr AnalyzerService::Timing kTiming{1000, 500};

// ---------------------------------------------------------------- domain

void testJudgementBoundariesAreInclusive() {
    CHECK(kGlu.judge(72.9) == Judgement::Low);
    CHECK(kGlu.judge(73.0) == Judgement::Normal);
    CHECK(kGlu.judge(109.0) == Judgement::Normal);
    CHECK(kGlu.judge(109.1) == Judgement::High);
}

void testSampleRejectsInvalidTransition() {
    Sample sample(1, "S-1", kGlu);
    bool threw = false;
    try {
        sample.complete(100.0);  // Waiting から直接 Done にはできない
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(sample.status() == SampleStatus::Waiting);
}

void testSummaryCountsAbnormalAndErrors() {
    SampleRack rack = makeRack(3);
    rack.at(0).beginMeasurement();
    rack.at(0).complete(150.0);  // High
    rack.at(1).beginMeasurement();
    rack.at(1).complete(90.0);   // Normal
    rack.at(2).beginMeasurement();
    rack.at(2).fail();
    const auto summary = rack.summarize();
    CHECK(summary.total == 3);
    CHECK(summary.done == 2);
    CHECK(summary.abnormal == 1);
    CHECK(summary.errors == 1);
    CHECK(summary.processed() == 3);
}

// ---------------------------------------------------------------- usecase

void testStartBeginsFirstSample() {
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(3), unit, kTiming);
    CHECK(service.start());
    CHECK(service.state() == AnalyzerState::Running);
    CHECK(service.rack().at(0).status() == SampleStatus::Measuring);
    CHECK(!service.start());  // 検査中の二重開始は受け付けない
}

void testAdvanceCompletesSampleAndMovesToNext() {
    FakeMeasurementUnit unit;
    unit.outcomes = {{true, 120.0}};
    AnalyzerService service(makeRack(3), unit, kTiming);
    RecordingObserver observer;
    service.addObserver(observer);
    service.start();

    service.advance(400);
    CHECK(service.rack().at(0).status() == SampleStatus::Measuring);
    CHECK(observer.lastProgress.sampleRatio > 0.39 && observer.lastProgress.sampleRatio < 0.41);

    service.advance(600);
    CHECK(service.rack().at(0).status() == SampleStatus::Done);
    CHECK(service.rack().at(0).judgement() == Judgement::High);
    CHECK(service.rack().at(1).status() == SampleStatus::Measuring);
    CHECK(observer.lastProgress.processed == 1);
}

void testFailedMeasurementMarksError() {
    FakeMeasurementUnit unit;
    unit.outcomes = {{false, 0.0}};
    AnalyzerService service(makeRack(2), unit, kTiming);
    service.start();
    service.advance(1000);
    CHECK(service.rack().at(0).status() == SampleStatus::Error);
    CHECK(!service.rack().at(0).value().has_value());
}

void testLargeElapsedProcessesSamplesInOrderAndCompletes() {
    FakeMeasurementUnit unit;
    unit.outcomes = {{true, 50.0}, {false, 0.0}, {true, 90.0}};
    AnalyzerService service(makeRack(3), unit, kTiming);
    RecordingObserver observer;
    service.addObserver(observer);
    service.start();

    service.advance(10'000);  // 3本分を大きく超える経過時間
    CHECK(unit.measureCalls == 3);
    CHECK(service.state() == AnalyzerState::Completed);
    CHECK(observer.completed.size() == 1);
    CHECK(observer.completed.at(0).abnormal == 1);
    CHECK(observer.completed.at(0).errors == 1);
    CHECK(observer.lastProgress.runElapsedMs == 3000);  // 完了後の余りは含めない
    CHECK(!service.start());                            // 完了後はリセットが必要
}

void testStopAbortsCurrentSampleAndResumeKeepsElapsed() {
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(3), unit, kTiming);
    RecordingObserver observer;
    service.addObserver(observer);
    service.start();
    service.advance(1500);  // 1本目完了、2本目が半分

    CHECK(service.stop());
    CHECK(service.state() == AnalyzerState::Stopped);
    CHECK(service.rack().at(1).status() == SampleStatus::Waiting);  // 中断した検体は測り直し
    CHECK(observer.lastProgress.current == nullptr);

    service.advance(5000);  // 停止中は進まない
    CHECK(observer.lastProgress.runElapsedMs == 1500);

    CHECK(service.start());
    CHECK(service.rack().at(1).status() == SampleStatus::Measuring);
    service.advance(100);
    CHECK(observer.lastProgress.runElapsedMs == 1600);
}

void testResetIsRefusedWhileRunning() {
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(2), unit, kTiming);
    service.start();
    CHECK(!service.reset());
    service.advance(5000);
    CHECK(service.state() == AnalyzerState::Completed);
    CHECK(service.reset());
    CHECK(service.state() == AnalyzerState::Idle);
    CHECK(service.rack().at(0).status() == SampleStatus::Waiting);
    CHECK(!service.rack().at(0).value().has_value());
}

void testTemperatureIsPublishedEveryInterval() {
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(1), unit, kTiming);
    RecordingObserver observer;
    service.addObserver(observer);
    service.advance(1200);  // 待機中でも温度は監視する
    CHECK(observer.temperatures.size() == 2);
}

// ---------------------------------------------------------------- adapter

void testPresenterControlsFollowState() {
    FakeView view;
    adapter::AnalyzerPresenter presenter(view);

    presenter.onStateChanged(AnalyzerState::Running);
    CHECK(!view.controls.startEnabled && view.controls.stopEnabled && !view.controls.resetEnabled);
    CHECK(view.status.tone == adapter::Tone::Active);

    presenter.onStateChanged(AnalyzerState::Stopped);
    CHECK(view.controls.startEnabled && !view.controls.stopEnabled && view.controls.resetEnabled);
    CHECK(view.controls.startText == "再開");
    CHECK(view.status.stateText == "一時停止");
}

void testPresenterFormatsResults() {
    FakeView view;
    adapter::AnalyzerPresenter presenter(view);

    Sample high(1, "S-1", kGlu);
    high.beginMeasurement();
    high.complete(128.0);
    presenter.onSampleChanged(0, high);
    CHECK(view.rows.back().result == "128 mg/dL H");
    CHECK(view.rows.back().tone == adapter::Tone::Caution);

    Sample error(2, "S-2", kGlu);
    error.beginMeasurement();
    error.fail();
    presenter.onSampleChanged(1, error);
    CHECK(view.rows.back().result == "要再検");
    CHECK(view.rows.back().tone == adapter::Tone::Alert);

    presenter.onTemperature(37.31);
    CHECK(view.temperature.valueText == "37.31 °C");
    CHECK(view.temperature.specText == "管理範囲 37.0±0.2℃");
    CHECK(view.temperature.tone == adapter::Tone::Caution);  // 37.0 ± 0.2 の外
}

// ---------------------------------------------------------------- runtime（スレッド）

void testWorkerAppliesQueuedCommandsInOrderOnStep() {
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(2), unit, kTiming);
    infra::runtime::AnalyzerWorker worker(service);

    worker.requestStart();
    CHECK(service.state() == AnalyzerState::Idle);  // 要求を積んだだけで、まだ実行されない
    worker.step(0);
    CHECK(service.state() == AnalyzerState::Running);

    worker.requestStop();
    worker.requestReset();
    worker.step(0);
    CHECK(service.state() == AnalyzerState::Idle);  // 停止 → リセット の順に実行された
}

// 制御スレッドから通知を受ける側。別スレッドから書かれるので atomic にする
class ThreadAwareObserver final : public usecase::IAnalyzerObserver {
public:
    std::atomic<bool> completed{false};
    std::atomic<std::thread::id> notifiedFrom{};

    void onStateChanged(AnalyzerState) override { notifiedFrom = std::this_thread::get_id(); }
    void onSampleChanged(std::size_t, const Sample&) override {}
    void onProgress(const usecase::ProgressInfo&) override {}
    void onTemperature(double) override {}
    void onRunCompleted(const domain::RunSummary&) override { completed = true; }
};

void testWorkerThreadRunsMeasurementInBackground() {
    using namespace std::chrono_literals;
    FakeMeasurementUnit unit;
    AnalyzerService service(makeRack(3), unit, AnalyzerService::Timing{10, 1000});
    ThreadAwareObserver observer;
    service.addObserver(observer);
    infra::runtime::AnalyzerWorker worker(service);

    worker.startThread(2);
    worker.requestStart();
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!observer.completed && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    worker.stopThread();

    CHECK(observer.completed);
    CHECK(observer.notifiedFrom.load() != std::this_thread::get_id());  // 通知は制御スレッドから来た
    CHECK(service.state() == AnalyzerState::Completed);                // スレッド合流後なので読んでよい
    CHECK(unit.measureCalls == 3);
}

void testDispatchingViewDefersUntilGuiThreadDrains() {
    FakeView view;
    infra::runtime::UiTaskQueue queue;
    adapter::DispatchingView proxy(view, queue);

    proxy.showStatus({"測定中", adapter::Tone::Active});
    CHECK(view.status.stateText.empty());  // drain するまで本物の View は呼ばれない
    CHECK(queue.drain() == 1);
    CHECK(view.status.stateText == "測定中");
}

void testUiTaskQueueKeepsOrderAcrossThreads() {
    using namespace std::chrono_literals;
    FakeView view;  // drain() を呼ぶこのスレッドでしか触らない
    infra::runtime::UiTaskQueue queue;
    adapter::DispatchingView proxy(view, queue);
    constexpr std::size_t kCount = 5000;

    std::thread producer([&] {  // 制御スレッドの代わり
        for (std::size_t i = 0; i < kCount; ++i) {
            adapter::SampleRowViewModel row;
            row.row = i;
            proxy.showSampleRow(row);
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (view.rows.size() < kCount && std::chrono::steady_clock::now() < deadline) {
        queue.drain();  // 送信と並行して GUI スレッド側で処理する
        std::this_thread::yield();
    }
    producer.join();
    queue.drain();

    CHECK(view.rows.size() == kCount);
    bool ordered = true;
    for (std::size_t i = 0; i < view.rows.size(); ++i) ordered = ordered && view.rows[i].row == i;
    CHECK(ordered);
}

struct TestCase {
    const char* name;
    void (*run)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"JudgementBoundariesAreInclusive", testJudgementBoundariesAreInclusive},
        {"SampleRejectsInvalidTransition", testSampleRejectsInvalidTransition},
        {"SummaryCountsAbnormalAndErrors", testSummaryCountsAbnormalAndErrors},
        {"StartBeginsFirstSample", testStartBeginsFirstSample},
        {"AdvanceCompletesSampleAndMovesToNext", testAdvanceCompletesSampleAndMovesToNext},
        {"FailedMeasurementMarksError", testFailedMeasurementMarksError},
        {"LargeElapsedProcessesSamplesInOrderAndCompletes", testLargeElapsedProcessesSamplesInOrderAndCompletes},
        {"StopAbortsCurrentSampleAndResumeKeepsElapsed", testStopAbortsCurrentSampleAndResumeKeepsElapsed},
        {"ResetIsRefusedWhileRunning", testResetIsRefusedWhileRunning},
        {"TemperatureIsPublishedEveryInterval", testTemperatureIsPublishedEveryInterval},
        {"PresenterControlsFollowState", testPresenterControlsFollowState},
        {"PresenterFormatsResults", testPresenterFormatsResults},
        {"WorkerAppliesQueuedCommandsInOrderOnStep", testWorkerAppliesQueuedCommandsInOrderOnStep},
        {"WorkerThreadRunsMeasurementInBackground", testWorkerThreadRunsMeasurementInBackground},
        {"DispatchingViewDefersUntilGuiThreadDrains", testDispatchingViewDefersUntilGuiThreadDrains},
        {"UiTaskQueueKeepsOrderAcrossThreads", testUiTaskQueueKeepsOrderAcrossThreads},
    };
    for (const auto& test : tests) {
        const int before = g_failures;
        test.run();
        std::printf("[%s] %s\n", g_failures == before ? " OK " : "FAIL", test.name);
    }
    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
